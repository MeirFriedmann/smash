#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <algorithm>
#include <regex>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";
vector<string> reserved_commands = {"chprompt", "cd", "showpid", "jobs", "fg", "pwd", "quit", "kill", "alias", "unalias", "whoami", "listdir", "netinfo"};


#if 0
#define FUNC_ENTRY() \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;
#define FUNC_EXIT() \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

Command::Command(const char *cmd_line) : cmd_line(cmd_line)
{
}
BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line)
{
}



bool _isBackgroundCommand(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

SmallShell::SmallShell() : prompt("smash"),  current_dir(""), last_dir(""),jobs(new JobsList()), fg_pid(-1), aliases()
{
}
SmallShell::~SmallShell()
{
  // delete jobs;
}

/**
 * Creates and returns a pointer to Command class which matches the given command line (cmd_line)
 */
Command *SmallShell::CreateCommand(char *cmd_line_arg) //i deleted const
{
  char* cmd_line = cmd_line_arg;
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  if (cmd_s[cmd_s.length() - 1] == '&') {
    //builtin commands ignore &
    //special commands listdir and netinfo and whoami ignore &?
    for (auto command : reserved_commands) {
      if(firstWord.substr(0, firstWord.length() - 1).compare(command) == 0 || firstWord.compare(command) == 0) {
        for (int i = strlen(cmd_line) - 1 ; i >= 0 ; i--) {
          if (cmd_line[i] == '&') {
            cmd_line[i] = '\0';                         //handling cmd_line with & for bltin commands
            break;
          }          
        }
        if (firstWord[firstWord.length() - 1] == '&') { //handling ifrst word with & for bltin commands
          firstWord = firstWord.substr(0, firstWord.length() - 1);
          }
        break;        
      }
    }
  }

  if (reserved_commands.end() != find(reserved_commands.begin(), reserved_commands.end(), firstWord)) {
    regex pattern(firstWord + "* [>]{1,2} [^ ]+");
    if(regex_match(cmd_s, pattern)){
      int index_of_io  = cmd_s.find_last_of(" ") - 2 == ' ' ? cmd_s.find_last_of(" ") - 1 : cmd_s.find_last_of(" ") - 2;
      printf("index of io: %d\n", index_of_io);
      const char* inner_cmd_line = cmd_s.substr(index_of_io, cmd_s.length()-index_of_io).c_str(); //sorry for bad naming
      const char* outer_cmd_line = cmd_s.substr(0, index_of_io).c_str();
      return new RedirectionCommand(outer_cmd_line, inner_cmd_line);
    }

  }

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("chprompt") == 0) {
    // pass cmd_line without first word
    string new_prompt = cmd_s.substr(cmd_s.find_first_of(" \n") + 1);
    // convert new
    return new ChangePromptCommand(cmd_line, getInstance());     //*this vs getinstance
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, getInstance());
  }
  else if (firstWord.compare("jobs") == 0)
  {
    return new JobsCommand(cmd_line, jobs);
  }
  else if (firstWord.compare("fg") == 0)
  {
    return new ForegroundCommand(cmd_line, jobs);
  }
  else if (firstWord.compare("quit") == 0)
  {
    return new QuitCommand(cmd_line, jobs);
  }
  else if (firstWord.compare("kill") == 0)
  {
    return new KillCommand(cmd_line, jobs);
  }
  else if (firstWord.compare("alias") == 0)
  {
    return new AliasCommand(cmd_line);
  }
  else if (firstWord.compare("unalias") == 0)
  {
    return new UnaliasCommand(cmd_line);
  }
  else if (firstWord.compare("whoami") == 0)
  {
    return new WhoAmICommand(cmd_line);
  }
  // else if (firstWord.compare("listdir") == 0)
  // {
  //   return new ListDirCommand(cmd_line);
  // }
  // else if (firstWord.compare("netinfo") == 0)
  // {
  //   return new NetInfoCommand(cmd_line);
  // }
  else
  {
    return new ExternalCommand(cmd_line);
  }

  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line)
{
  if (jobs != nullptr)
  {
    jobs->removeFinishedJobs();
  }
  char * cmd_line_copy = strdup(cmd_line); //const... should be freed
  Command *cmd = CreateCommand(cmd_line_copy);
  if (dynamic_cast<ExternalCommand*>(cmd) != nullptr) {
    pid_t pid = fork();
    if (pid == -1) {
      perror("smash error: fork failed");
      return;
    }
    if (pid == 0) {
      if (setpgrp() == -1) perror("smash error: setpgrp failed"); //needed?
      cmd->execute(); //son never returns...
    }
    else if (pid > 0) {
      if (!_isBackgroundCommand(cmd_line_copy)) {
        int status;
        if (waitpid(pid, &status, 0) == -1) perror("smash error: waitpid failed"); //waitpid(pid, &status, 0);
      }
      else{
        jobs->addJob(cmd, pid);
      }
    }
  }
  else if (cmd != nullptr) //not external
  {
    cmd->execute();
  }
  delete cmd;
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}
void GetCurrDirCommand::execute()
{
  char buffer[COMMAND_MAX_LENGTH];
  if (getcwd(buffer, sizeof(buffer)) == nullptr) {
    perror("smash error: getcwd failed");
    return;
  }
  cout << buffer << endl;
}

ChangePromptCommand::ChangePromptCommand(const char *cmd_line, SmallShell &smash) : BuiltInCommand(cmd_line)
{
}
void ChangePromptCommand::execute()
{
  char **args = new char*[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  if (args_len == 1)
  {
    SmallShell::getInstance().setPrompt("smash");
  }
  else if (args_len == 2)
  {
    SmallShell::getInstance().setPrompt(args[1]);
  }
  for (int i = 0; i < args_len; i++)
  {
    free(args[i]);
  }
  delete[] args;
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, SmallShell& smash) : BuiltInCommand(cmd_line)
{
}
void ChangeDirCommand::execute()
{
  char** args = new char*[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);

  // no arguments
  if (args_len == 1)
  {
    free(args[0]);
    delete[] args;
    return;
  }
  // handle cd
  if (args_len == 2)
  {
    char buffer[COMMAND_MAX_LENGTH];
    if (getcwd(buffer, sizeof(buffer)) == nullptr) {
      perror("smash error: getcwd failed");
      return;
    }
    string curr_dir = buffer;
    if (strcmp(args[1], "-") == 0)
    {
      if (SmallShell::getInstance().getLastDir().empty())
      {
        cerr << "smash error: cd: OLDPWD not set" << endl;
      }
      else
      {
        if (chdir(SmallShell::getInstance().getLastDir().c_str()) == -1)
        {
          perror("smash error: chdir failed");
        }
        else
        {
          SmallShell::getInstance().setLastDir(string(curr_dir));
        }
      }
    }
    else if (strcmp(args[1], "..") == 0)
    {
      if (chdir("..") != 0)
      {
        perror("smash error: chdir failed");
      }
      else
      {
        SmallShell::getInstance().setLastDir(curr_dir);
      }
    }
    else if (chdir(args[1]) != 0)
    {
      perror("smash error: chdir failed");
    }
    else
    {
      SmallShell::getInstance().setLastDir(curr_dir);
    }
    for (int i = 0; i < args_len; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
  else // args_len > 2
  {
    cerr << "smash error: cd: too many arguments" << endl;
    for (int i = 0; i < args_len; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
}


JobsList::JobEntry::JobEntry(int job_id, pid_t pid, const string& cmd_line) : job_id(job_id), prcs_id(pid), cmd_line(cmd_line)
{}

JobsList::JobsList() : jobs(), max_job_id(0)
{}

void JobsList::addJob(Command* cmd, pid_t pid)
{
  // removeFinishedJobs();
  max_job_id++;
  jobs.push_back(JobEntry(max_job_id, pid, cmd->getCmdLine()));
  
}

void JobsList::printJobsList()
{
  // removeFinishedJobs();
  for (const JobEntry& job : jobs)
  {
    cout << "[" << job.getPid() << "] " << job.getCmdLine() <<endl;
  }
}

void JobsList::killAllJobs()
{
    removeFinishedJobs();
    for (const JobEntry& job : jobs){
      cout << job.getPid() << ": " << job.getCmdLine() << endl;
      kill(job.getPid(), SIGKILL);
    }
}

void JobsList::removeFinishedJobs(){
  auto it = jobs.begin();
  while (it != jobs.end())
  {
    int status;
    int pid = waitpid(it->getPid(), &status, WNOHANG);
    if (pid == -1) {perror("smash error: waitpid failed"); return;}
    // if (WIFEXITED(status) || WIFSIGNALED(status)) doesnt work for some reason. always true even if running
    if (pid > 0)
    {
      it = jobs.erase(it);
    }
    else
    {
      ++it;
    }
  }
}
JobsList::JobEntry* JobsList::getJobById(int jobId)
{
  for (JobEntry& job : jobs)
  {
    if (job.getJobId() == jobId)
    {
      return &job;
    }
  }
  return nullptr;
}

void JobsList::removeJobById(int jobId)
{
  jobs.erase(std::remove_if(jobs.begin(), jobs.end(),                    //added include algorithms
    [jobId](const JobEntry& job) { return job.getJobId() == jobId; }),
    jobs.end());
}
JobsList::JobEntry* JobsList::getLastJob() //(int* lastJobId)
{
  if (jobs.empty())
  {
    return nullptr;
  }
  // *lastJobId = jobs.back().getJobId(); //why is it needed?
  return &jobs.back();
}

int JobsList::length()
{
    return jobs.size();
}

JobsCommand::JobsCommand(const char *cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}

void JobsCommand::execute()
{
  jobs->printJobsList();
}

KillCommand::KillCommand(const char *cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{}

void KillCommand::execute()
{
  char **args = new char*[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  if (args_len != 3 || args[1][0] != '-' || !*args[1]) {cerr<<("smash error: kill: invalid arguments")<<endl;return;} //if theres no dash on -sugnum or signum is only '-'
  auto ptr = args[1];
  while (*++ptr) {
    if (!isdigit(*ptr)) {cerr<<("smash error: kill: invalid arguments")<<endl;return;} //if signum is not digit
    else ++ptr; }
  ptr = args[2];
  while (*ptr) {
    if (!isdigit(*ptr)) {cerr<<("smash error: kill: invalid arguments")<<endl;return;} //if jobid is not digit
    else ++ptr; }
  if (args[2][0] == '0' || (args[1][1] == '0' && args[1][2] != '\0')) {cerr<<("smash error: kill: invalid arguments")<<endl;return;} //jobid starts with 0 or signum has 0 and then digit  
  int signum = atoi(args[1]);
  int job_id = atoi(args[2]);
  if (jobs->getJobById(job_id) == nullptr){
    cerr<<"smash error: kill: job-id " << (job_id) << " does not exist" << endl;
  } 
  else if (kill(jobs->getJobById(job_id)->getPid(), signum) == -1) {perror(" smash error: kill failed");return;}
  else cout << "signal number " << signum << " was sent to pid " << jobs->getJobById(job_id)->getPid() << endl;
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{
}

void QuitCommand::execute()
{
  char **args = new char*[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  if (args_len == 1)
  {
    exit(0);
  }
  else if (strcmp(args[1],"kill") == 0){
    cout << "smash: sending SIGKILL signal to " << jobs->length() << " jobs:" << endl;
    jobs->killAllJobs();

  }
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{
}

void ForegroundCommand::execute()
{
  char **args = new char*[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  if (args_len == 1)
  {
    if (jobs->getLastJob() == nullptr)
    {
      cerr<<("smash error: fg: jobs list is empty")<<endl; return;
    }
    else
    {
      cout << jobs->getLastJob()->getCmdLine() << " " << jobs->getLastJob()->getPid() << endl;
      jobs->removeJobById(jobs->getLastJob()->getJobId());
      waitpid(jobs->getLastJob()->getPid(), nullptr, WUNTRACED);
      return;
    }
  }
  else if (args_len == 2)
  {
    if (args[1][0] == '0'){
      cout<<("smash error: fg: invalid arguments")<<endl; return;
    }
    auto ptr = args[1];
    while (*ptr) {
    if (!isdigit(*ptr)) {cout<<("smash error: fg: invalid arguments")<<endl; return;}
    else ++ptr; }
    JobsList::JobEntry* job = jobs->getJobById(atoi(args[1]));
    if (job == nullptr)
    {
      cerr<<("smash error: fg: job does not exist")<<endl;
      return;
    }
    else
    {
      cout << job->getCmdLine() << " " << job->getPid() << endl;
      jobs->removeJobById(atoi(args[1]));
      waitpid(job->getPid(), nullptr, WUNTRACED);
      return;
    }
  }
  else
  {
    cerr<<("smash error: fg: too many arguments")<<endl;
  }

}

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line)
{}

void ExternalCommand::execute(){
  char * cmd_line_copy = strdup(cmd_line); //should be freed
  _removeBackgroundSign(cmd_line_copy);
  char **args = new char*[COMMAND_MAX_ARGS];
  // int args_len = 
  _parseCommandLine(cmd_line_copy, args);
  
  if (execvp(args[0], args) == -1) {                            //execvp handles both PATH and path execution
      perror("smash error: execvp failed");
      exit(1);
  }
  
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line)
{
}

void PipeCommand::execute()
{
}

AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{}

void AliasCommand::execute()
{
  if(regex_match(cmd_line, regex("^alias$"))){
    SmallShell::getInstance().printAliases();
    return;
  }
  if(!regex_match(cmd_line, regex("^alias [a-zA-Z0-9_]+='[^']*'$"))){cerr<<"smash error: alias: invalid alias format"<<endl; return;}
  string cmd_s = (string(cmd_line));
  string alias_name = cmd_s.substr(6, cmd_s.find_first_of("=") - 6); // 6 for length of "alias "
  string alias_cmd_line = cmd_s.substr(cmd_s.find_first_of("'") + 1, cmd_s.find_last_of("'") - cmd_s.find_first_of("'") -1); // -1 so we wont include last '
  if (SmallShell::getInstance().getAlias(alias_name) != nullptr){
    cerr<<"smash error: alias: " << alias_name << " already exists or is a reserved command "<<endl;
    return;
  }
  if (find(reserved_commands.begin(), reserved_commands.end(), alias_name) != reserved_commands.end()){
    cerr<<"smash error: alias: " << alias_name << " already exists or is a reserved command "<<endl;
    return;
  }
  SmallShell::getInstance().setAlias(alias_name, alias_cmd_line);
}


ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}

void ShowPidCommand::execute()
{
  pid_t pid = getpid();
  if (pid == -1) {perror("smash error: getpid failed"); return;}
  cout << "smash pid is " << pid << endl;
}

UnaliasCommand::UnaliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}

void UnaliasCommand::execute()
{
  char **args = new char*[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  if (args_len == 1){
    cerr<<"smash error: unalias: not enough arguments"<<endl;
    return;
  }
  for (int i = 1; i < args_len; i++){
    if (SmallShell::getInstance().getAlias(args[i]) == nullptr){
      cerr<<"smash error: unalias: " << args[i] << " alias does not exist"<<endl;
      return;
    }
    SmallShell::getInstance().removeAlias(args[i]);
  }  
}

void RedirectionCommand::execute()
{
  char **args = new char*[COMMAND_MAX_ARGS]; //cmd_line is > filename or >> filename
  _parseCommandLine(cmd_line, args);
  char to_file_path [strlen(args[1])+2];
  strcpy(to_file_path, "\"");
  strcat(to_file_path, args[1]);
  strcat(to_file_path, "\"");
  pid_t pid = getpid();
  if (pid == -1) {perror("smash error: getpid failed"); return;}
  int to_file_fd;
  if (strcmp(args[0], ">") == 0){
    to_file_fd = open(to_file_path, O_WRONLY | O_TRUNC | O_CREAT);
  }
  else {
    to_file_fd = open(to_file_path, O_WRONLY | O_APPEND | O_CREAT);
  }
  if (to_file_fd == -1) {perror("smash error: open failed"); return;}
  int from_fd = open(("/proc"+to_string(pid)+"/fd/1").c_str(), O_WRONLY | O_TRUNC); //contents of file will be overwritten
  if (from_fd == -1) {perror("smash error: open failed"); return;}
  int prev_from_fd = dup(from_fd);
  if (prev_from_fd == -1) {perror("smash error: dup failed"); return;}
  if(dup2(to_file_fd, from_fd) == -1) {perror("smash error: dup2 failed"); return;}
  //running actual inner command
  char * inner_cmd_line_copy = strdup(inner_cmd_line); //const... should be freed
  Command* cmd = SmallShell::getInstance().CreateCommand(inner_cmd_line_copy); 
  SmallShell::getInstance().executeCommand(cmd->getCmdLine()); //or instead cmd->execute ?
  //
  if (close(to_file_fd) == -1) {perror("smash error: close failed"); return;} //at the end so file pointer is at place.
  if (dup2(prev_from_fd, from_fd) == -1) {perror("smash error: dup2 failed"); return;}

}

void WhoAmICommand::execute()
{
  string etc_passwd_path = "/etc/passwd";
  int fd = open(etc_passwd_path.c_str(), O_RDONLY);
  if (fd == -1){perror("smash error: open failed");return;}
  char buffer[1];
  ssize_t bytes_read;
  string passwd_content;
  while ((bytes_read = read(fd, buffer, 1)) != 0) {
    if (bytes_read == -1){perror("smash error: read failed"); return;}
    passwd_content += buffer[0];
  }
  // char buffer[10000];
  // ssize_t bytes_read = read(fd, buffer, 10000);
  // if (bytes_read == -1){perror("smash error: read failed"); return;}
  // string passwd_content;
  // for (int i = 0; i < bytes_read; ++i) {
  //   passwd_content += buffer[i];
  // }
  string uid_str = to_string(getuid());
  int colon_counter = 0;
  for (size_t i = 0; i < passwd_content.length(); i++){
    if (passwd_content[i] == ':') {
      colon_counter++;
      if (colon_counter % 6 == 2){
        if (passwd_content.substr(i+1, uid_str.length()).compare(uid_str) == 0) {
          int user_id_start = colon_counter < 6 ? 0 : passwd_content.find_last_of('\n', i) + 1;
          int user_id_end = passwd_content.find(':', user_id_start);
          string user_id = passwd_content.substr(user_id_start, user_id_end - user_id_start);
          int home_dir_start = i;
          for (int j = 0; j < 3; j++){
            home_dir_start = passwd_content.find(':', home_dir_start + 1);
          }
          int home_dir_end = passwd_content.find(':', home_dir_start+1);
          home_dir_start++;
          string home_dir = passwd_content.substr(home_dir_start, home_dir_end - home_dir_start);
          cout << user_id << " " << home_dir << endl;
          return;
            }  
        }
    }

  }


// size_t pos = 0;
// int colon_count = 0;
// string passwd_row = passwd_content.substr(0, passwd_content.find('\n'));
// while (pos < passwd_content.length()) {
// cout<<passwd_row<<endl;

//   while (colon_count < 2) {
//     if (passwd_content[pos] == ':') {
//         colon_count++;
//       }
//       pos++;
//     }
//   if (passwd_row.substr(pos, uid_str.length()) == uid_str) {
//     cout << "smash: user is " << endl;
//     return;
// }
// else{
//   colon_count = 0;
//   size_t prev_pos = pos;
//   pos = passwd_content.find('\n',pos)+1;
//   passwd_row = passwd_content.substr(pos,
//    passwd_content.find('\n',pos) - passwd_content.find('\n',prev_pos));
// }


// }
  if (close(fd) == -1)
  {
    perror("smash error: close failed");
  }
}

