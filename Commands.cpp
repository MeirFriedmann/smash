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

SmallShell::SmallShell() : prompt("smash"),  current_dir(""), last_dir(""),jobs(new JobsList()), fg_pid(-1)
{
 
}
SmallShell::~SmallShell()
{
  // delete jobs;
}

/**
 * Creates and returns a pointer to Command class which matches the given command line (cmd_line)
 */
Command *SmallShell::CreateCommand(const char *cmd_line)
{
  // For example:

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

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
  Command *cmd = CreateCommand(cmd_line);
  if (cmd != nullptr)
  {
    cmd->execute();
    delete cmd;
  }
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}
void GetCurrDirCommand::execute()
{
  char buffer[COMMAND_MAX_LENGTH];
  if (getcwd(buffer, sizeof(buffer)) == nullptr) {
    perror("smash error: pwd failed");
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
      perror("smash error: pwd failed");
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

JobsList::JobsList() : max_job_id(0)
{}

void JobsList::addJob(Command* cmd, pid_t pid)
{
  removeFinishedJobs();
  max_job_id++;
  jobs.push_back(JobEntry(max_job_id, pid, cmd->getCmdLine()));
  
}

void JobsList::printJobsList()
{
  removeFinishedJobs();
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
    if (WIFEXITED(status) || WIFSIGNALED(status))
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
{
}

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
{
}

void ExternalCommand::execute()
{
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
  vector<string> commands = {"chprompt", "cd", "showpid", "jobs", "fg", "pwd", "quit", "kill", "alias", "unalias", "whoami", "listdir", "netinfo"};
  if (find(commands.begin(), commands.end(), alias_name) != commands.end()){
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
  if (pid == -1) {perror("smash error: showpid failed"); return;}
  cout << "smash pid is " << pid << endl;
}
