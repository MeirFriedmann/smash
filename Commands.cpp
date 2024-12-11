#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <dirent.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include "Commands.h"

using namespace std;

struct linux_dirent64
{
  ino64_t d_ino;           /* 64-bit inode number */
  off64_t d_off;           /* 64-bit offset to next structure */
  unsigned short d_reclen; /* Size of this dirent */
  unsigned char d_type;    /* File type */
  char d_name[];           /* Filename (null-terminated) */
};

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

string _ltrim(const std::string &s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args)
{
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for (std::string s; iss >> s;)
  {
    args[i] = (char *)malloc(s.length() + 1);
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

bool _isBackgroundCommand(const char *cmd_line)
{
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line)
{
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos)
  {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&')
  {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

SmallShell::SmallShell() : prompt("smash"), current_dir(""), last_dir(""), jobs(new JobsList()), fg_pid(-1), aliases()
{
}
SmallShell::~SmallShell()
{
  // delete jobs;
}

/**
 * Creates and returns a pointer to Command class which matches the given command line (cmd_line)
 */
Command *SmallShell::CreateCommand(char *cmd_line_arg) // i deleted const
{
  char *cmd_line = cmd_line_arg;
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  if (cmd_s[cmd_s.length() - 1] == '&')
  {
    // builtin commands ignore &
    // special commands listdir and netinfo and whoami ignore &?
    for (auto command : reserved_commands)
    {
      if (firstWord.substr(0, firstWord.length() - 1).compare(command) == 0 || firstWord.compare(command) == 0)
      {
        for (int i = strlen(cmd_line) - 1; i >= 0; i--)
        {
          if (cmd_line[i] == '&')
          {
            cmd_line[i] = '\0'; // handling cmd_line with & for bltin commands
            break;
          }
        }
        if (firstWord[firstWord.length() - 1] == '&')
        { // handling ifrst word with & for bltin commands
          firstWord = firstWord.substr(0, firstWord.length() - 1);
        }
        break;
      }
    }
  }
  regex pattern(firstWord + "(.*?) (>>|>) (.*)");
  if (regex_match(cmd_s, pattern))
  {
    cout << "found >> or >" << endl;
    size_t io_index = cmd_s.find_last_of(" ") - 2 == ' ' ? cmd_s.find_last_of(" ") - 1 : cmd_s.find_last_of(" ") - 2;
    string inner_cmd = cmd_s.substr(0, io_index);
    string outer_cmd = cmd_s.substr(io_index);
    char *outer_cmd_copy = new char[outer_cmd.length() + 1];
    char *inner_cmd_copy = new char[inner_cmd.length() + 1];
    strcpy(outer_cmd_copy, outer_cmd.c_str());
    strcpy(inner_cmd_copy, inner_cmd.c_str());
    return new RedirectionCommand(outer_cmd_copy, inner_cmd_copy);
  }
  // if command in alias -> firstWord = alias[firstWord]
  if (getInstance().getAlias(firstWord))
  {
    firstWord = aliases[firstWord];
  }

  if (firstWord.compare("pwd") == 0)
  {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("chprompt") == 0)
  {
    return new ChangePromptCommand(cmd_line, getInstance()); //*this vs getinstance
  }
  else if (firstWord.compare("showpid") == 0)
  {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0)
  {
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
  else if (firstWord.compare("listdir") == 0)
  {
    return new ListDirCommand(cmd_line);
  }
  else if (firstWord.compare("netinfo") == 0)
  {
    return new NetInfoCommand(cmd_line);
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
  char *cmd_line_copy = strdup(cmd_line); // const... should be freed
  Command *cmd = CreateCommand(cmd_line_copy);
  if (dynamic_cast<ExternalCommand *>(cmd) != nullptr)
  {
    pid_t pid = fork();
    if (pid == -1)
    {
      perror("smash error: fork failed");
      return;
    }
    if (pid == 0) // Child process code
    {
      if (setpgrp() == -1)
      {
        perror("smash error: setpgrp failed"); // if processes in the same group CtrlC will kill smash
        exit(1);
      }
      cmd->execute(); // son never returns...
    }
    else if (pid > 0)
    {
      if (!_isBackgroundCommand(cmd_line_copy))
      {
        int status;
        if (waitpid(pid, &status, 0) == -1) // wait till finished
        {
          perror("smash error: waitpid failed"); // waitpid(pid, &status, 0);
        }
        setFgPid(-1); // reset tracking
      }
      else
      {
        jobs->addJob(cmd, pid);
      }
    }
  }
  else if (cmd != nullptr) // not external
  {
    cmd->execute();
  }
  delete cmd;
  free(cmd_line_copy);
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}
void GetCurrDirCommand::execute()
{
  char buffer[COMMAND_MAX_LENGTH];
  if (getcwd(buffer, sizeof(buffer)) == nullptr)
  {
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
  char **args = new char *[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  if (args_len == 1)
  {
    SmallShell::getInstance().setPrompt("smash");
  }
  else
  {
    SmallShell::getInstance().setPrompt(args[1]); 
  }
  for (int j = 0; j < args_len; j++)
  {
    free(args[j]);
  }
  delete[] args;
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, SmallShell &smash) : BuiltInCommand(cmd_line)
{
}
void ChangeDirCommand::execute()
{
  char **args = new char *[COMMAND_MAX_ARGS];
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
    if (getcwd(buffer, sizeof(buffer)) == nullptr)
    {
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

JobsList::JobEntry::JobEntry(int job_id, pid_t pid, const string &cmd_line) : job_id(job_id), prcs_id(pid), cmd_line(cmd_line)
{
}

JobsList::JobsList() : jobs(), max_job_id(0)
{
}

void JobsList::addJob(Command *cmd, pid_t pid)
{

  max_job_id++;
  jobs.push_back(JobEntry(max_job_id, pid, cmd->getCmdLine()));
}

void JobsList::printJobsList()
{

  for (const JobEntry &job : jobs)
  {
    cout << "[" << job.getJobId() << "] " << job.getCmdLine() << endl;
  }
}

void JobsList::killAllJobs()
{
  removeFinishedJobs();
  for (const JobEntry &job : jobs)
  {
    cout << job.getPid() << ": " << job.getCmdLine() << endl;
    if (kill(job.getPid(), SIGKILL) == -1)
      perror("smash error: kill failed");
  }
}

void JobsList::removeFinishedJobs()
{
  auto it = jobs.begin();
  while (it != jobs.end())
  {
    int status;
    int pid = waitpid(it->getPid(), &status, WNOHANG);
    if (pid == -1)
    {
      perror("smash error: waitpid failed");
      return;
    }
    // if (WIFEXITED(status) || WIFSIGNALED(status)) doesnt work for some reason. always true even if running
    if (pid > 0)
    {
      it = jobs.erase(it);
      if (it->getJobId() == max_job_id)
      {
        max_job_id = 0;
        for (const JobEntry &job : jobs)
        {
          max_job_id = std::max(max_job_id, job.getJobId());
        }
      }
      
    }
    else
    {
      ++it;
    }
  }
}
JobsList::JobEntry *JobsList::getJobById(int jobId)
{
  for (JobEntry &job : jobs)
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
  auto it = std::remove_if(jobs.begin(), jobs.end(),
                           [jobId](const JobEntry &job)
                           { return job.getJobId() == jobId; });

  jobs.erase(it, jobs.end());

  max_job_id = 0;
  for (const JobEntry &job : jobs)
  {
    max_job_id = std::max(max_job_id, job.getJobId());
  }
}
JobsList::JobEntry *JobsList::getLastJob() //(int* lastJobId)
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

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{
}

void JobsCommand::execute()
{
  jobs->removeFinishedJobs();

  jobs->printJobsList();
}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{
}

void KillCommand::execute()
{
  char **args = new char *[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  if (args_len != 3 || args[1][0] != '-' || !*args[1])
  {
    cerr << ("smash error: kill: invalid arguments") << endl;
    return;
  } // if theres no dash on -sugnum or signum is only '-'
  auto ptr = ++args[1];
  while (*ptr)
  {
    if (!isdigit(*ptr))
    {
      cerr << ("smash error: kill: invalid arguments") << endl;
      return;
    } // if signum is not digit

    ++ptr;
  }
  ptr = args[2];
  while (*ptr)
  {
    if (!isdigit(*ptr))
    {
      cerr << ("smash error: kill: invalid arguments") << endl;
      return;
    } // if jobid is not digit
    else
      ++ptr;
  }
  if (args[2][0] == '0' || (args[1][1] == '0' && args[1][2] != '\0'))
  {
    cerr << ("smash error: kill: invalid arguments") << endl;
    return;
  } // jobid starts with 0 or signum has 0 and then digit
  int signum = atoi(args[1]);
  int job_id = atoi(args[2]);
  JobsList::JobEntry *job = jobs->getJobById(job_id);
  if (jobs->getJobById(job_id) == nullptr)
  {
    cerr << "smash error: kill: job-id " << (job_id) << " does not exist" << endl;
  }
  else
  {
    int kill_result = kill(job->getPid(), signum);
    if (kill_result == -1)
    {
      perror("smash error: kill failed");
      return;
    }
    else
    {
      // remove job from jobslist
      jobs->removeJobById(job_id);
      cout << "signal number " << signum << " was sent to pid " << job->getPid() << endl;
    }
  }
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{
}

void QuitCommand::execute()
{
  char **args = new char *[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  if (args_len == 1)
  {
    exit(0);
  }
  else if (strcmp(args[1], "kill") == 0)
  {
    cout << "smash: sending SIGKILL signal to " << jobs->length() << " jobs:" << endl;
    jobs->killAllJobs();
    exit(0);
  }
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{
}

void ForegroundCommand::execute()
{
  char **args = new char *[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  JobsList::JobEntry *job = nullptr;
  if (args_len == 1) // fg max job
  {
    job = jobs->getLastJob();
    if (job == nullptr)
    {
      cerr << ("smash error: fg: jobs list is empty") << endl;
      for (int i = 0; i < args_len; i++)
      {
        free(args[i]);
      }
      delete[] args;
      return;
    }
    pid_t pid = job->getPid(); // save proccess id
    string cmd = job->getCmdLine();
    cout << cmd << " " << pid << endl;
    jobs->removeJobById(job->getJobId());
    SmallShell::getInstance().setFgPid(pid);    // for signal handling
    if (waitpid(pid, nullptr, WUNTRACED) == -1) // wait till process is finished
    {
      perror("smash error: waitpid failed");
    }
    SmallShell::getInstance().setFgPid(-1); // reset fg process
    for (int i = 0; i < args_len; i++)
    {
      free(args[i]);
    }
    delete[] args;
  }
  else if (args_len == 2)
  {
    if (args[1][0] == '0')
    {
      cerr << "smash error: fg: invalid arguments" << endl;
      for (int i = 0; i < args_len; i++)
      {
        free(args[i]);
      }
      delete[] args;
      return;
    }
    for (char *p = args[1]; *p; p++)
    {
      if (!isdigit(*p))
      {
        cerr << "smash error: fg: invalid arguments" << endl;
        for (int i = 0; i < args_len; i++)
          free(args[i]);
        delete[] args;
        return;
      }
    }

    for (char *p = args[1]; *p; p++) // contains only digits
    {
      if (!isdigit(*p))
      {
        cerr << "smash error: fg: invalid arguments" << endl;
        for (int i = 0; i < args_len; i++)
        {
          free(args[i]);
        }
        delete[] args;
        return;
      }
    }
    int job_id = atoi(args[1]);
    job = jobs->getJobById(job_id);
    if (!job)
    {
      cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
      for (int i = 0; i < args_len; i++)
      {
        free(args[i]);
      }
      delete[] args;
      return;
    }
    pid_t pid = job->getPid(); // save proccess id
    string cmd = job->getCmdLine();
    cout << cmd << " " << pid << endl;
    jobs->removeJobById(job->getJobId());

    SmallShell::getInstance().setFgPid(pid);    // for signal handling
    if (waitpid(pid, nullptr, WUNTRACED) == -1) // wait till process is finished
    {
      perror("smash error: waitpid failed");
    }

    SmallShell::getInstance().setFgPid(-1); // reset fg process
    for (int i = 0; i < args_len; i++)
    {
      free(args[i]);
    }
    delete[] args;
  }
  else
  {
    cerr << ("smash error: fg: too many arguments") << endl;
    for (int i = 0; i < args_len; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
}
ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line)
{
}

void ExternalCommand::execute()
{
  char *cmd_line_copy = strdup(cmd_line); // should be freed
  if (!cmd_line_copy)
  {
    perror("smash error: strdup failed");
    exit(1);
  }
  _removeBackgroundSign(cmd_line_copy);
  char **args = new char *[COMMAND_MAX_ARGS];
  _parseCommandLine(cmd_line_copy, args);

  // check if complex
  bool is_complex = false;
  for (int i = 0; args[i] != nullptr; i++)
  {
    if (strchr(args[i], '*') || strchr(args[i], '?'))
    {
      is_complex = true;
      break;
    }
  }
  if (is_complex)
  {
    char *bash_arguments[] = {
        (char *)"/bin/bash",
        (char *)"-c",
        cmd_line_copy, nullptr};
    if (execv("/bin/bash", bash_arguments) == -1)
    {
      perror("smash error: execv failed");
    }
  }
  else
  {
    if (execvp(args[0], args) == -1)
    {
      perror("smash error: execvp failed");
    }
  }
  for (int i = 0; args[i] != nullptr; i++)
  {
    free(args[i]);
  }
  delete[] args;
  free(cmd_line_copy);
  exit(1);
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line)
{
}

void PipeCommand::execute()
{
}

AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}

void AliasCommand::execute()
{
  if (regex_match(cmd_line, regex("^alias$"))) // cmd_line == "alias"
  {
    SmallShell::getInstance().printAliases();
    return;
  }
  if (!regex_match(cmd_line, regex("^alias [a-zA-Z0-9_]+='[^']*'$")))
  {
    cerr << "smash error: alias: invalid alias format" << endl;
    return;
  }
  string cmd_s = (string(cmd_line));
  string alias_name = cmd_s.substr(6, cmd_s.find_first_of("=") - 6);                                                          // 6 for length of "alias "
  string alias_cmd_line = cmd_s.substr(cmd_s.find_first_of("'") + 1, cmd_s.find_last_of("'") - cmd_s.find_first_of("'") - 1); // -1 so we wont include last '
  if (SmallShell::getInstance().getAlias(alias_name) != nullptr)
  {
    cerr << "smash error: alias: " << alias_name << " already exists or is a reserved command " << endl;
    return;
  }
  if (find(reserved_commands.begin(), reserved_commands.end(), alias_name) != reserved_commands.end())
  {
    cerr << "smash error: alias: " << alias_name << " already exists or is a reserved command " << endl;
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
  if (pid == -1)
  {
    perror("smash error: getpid failed");
    return;
  }
  cout << "smash pid is " << pid << endl;
}

UnaliasCommand::UnaliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}

void UnaliasCommand::execute()
{
  char **args = new char *[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);
  if (args_len == 1)
  {
    cerr << "smash error: unalias: not enough arguments" << endl;
    return;
  }
  for (int i = 1; i < args_len; i++)
  {
    if (SmallShell::getInstance().getAlias(args[i]) == nullptr)
    {
      cerr << "smash error: unalias: " << args[i] << " alias does not exist" << endl;
      return;
    }
    SmallShell::getInstance().removeAlias(args[i]);
  }
}
void ListDirCommand::printIndentation(int depth)
{
  // Print tab for each level of depth
  for (int i = 0; i < depth; i++)
  {
    cout << "\t";
  }
}
void ListDirCommand::printDirectoryContents(const string &path, int depth)
{
  // Open directory using open() system call
  int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
  if (fd == -1)
  {
    perror("smash error: open failed");
    return;
  }

  vector<string> entries;

  // Buffer for getdents64
  char buffer[4096];
  struct linux_dirent64 *entry;

  // Use getdents64 system call to read directory entries
  int nread;
  while ((nread = syscall(SYS_getdents64, fd, buffer, sizeof(buffer))) > 0)
  {
    for (int pos = 0; pos < nread;)
    {
      entry = (struct linux_dirent64 *)(buffer + pos);
      string name = entry->d_name;

      if (name != "." && name != "..")
      {
        entries.push_back(name);
      }

      pos += entry->d_reclen;
    }
  }

  if (nread == -1)
  {
    perror("smash error: getdents64 failed");
    close(fd);
    return;
  }

  // Sort and print contents
  sort(entries.begin(), entries.end());

  // First print directories
  for (const string &ent : entries)
  {
    string full_path = path + "/" + ent;
    struct stat statbuf;

    if (stat(full_path.c_str(), &statbuf) != -1)
    {
      printIndentation(depth);
      cout << ent << endl;
      if (S_ISDIR(statbuf.st_mode))
      {
        printDirectoryContents(full_path, depth + 1);
      }
    }
    else
    {
      perror("smash error: stat failed");
    }
  }

  close(fd);
}

void ListDirCommand::execute()
{
  char **args = new char *[COMMAND_MAX_ARGS];
  int args_len = _parseCommandLine(cmd_line, args);

  if (args_len > 2)
  {
    cerr << "smash error: listdir: too many arguments" << endl;
    for (int i = 0; i < args_len; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }

  string path = ".";
  if (args_len == 2)
  {
    path = args[1];
  }

  for (int i = 0; i < args_len; i++)
  {
    free(args[i]);
  }
  delete[] args;
  struct stat statbuf;
  vector<string> entries;

  int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
  if (fd == -1)
  {
    perror("smash error: open failed");
    return;
  }

  // Read root level entries first
  char buffer[4096];
  struct linux_dirent64 *entry;

  int nread;
  while ((nread = syscall(SYS_getdents64, fd, buffer, sizeof(buffer))) > 0)
  {
    for (int pos = 0; pos < nread;)
    {
      entry = (struct linux_dirent64 *)(buffer + pos);
      string name = entry->d_name;

      if (name != "." && name != "..")
      {
        entries.push_back(name);
      }
      pos += entry->d_reclen;
    }
  }
  if (nread == -1)
  {
    perror("smash error: getdents64 failed");
    close(fd);
    return;
  }
  close(fd);
  // Sort and process root level entries
  sort(entries.begin(), entries.end());
  for (const string &name : entries)
  {
    string fullPath = path + "/" + name;
    if (stat(fullPath.c_str(), &statbuf) != -1)
    {
      cout << name << endl;
      if (S_ISDIR(statbuf.st_mode))
      {
        printDirectoryContents(fullPath, 1);
      }
    }
  }
}
void RedirectionCommand::execute()
{
  char **args = new char *[COMMAND_MAX_ARGS]; // cmd_line is > filename or >> filename
  _parseCommandLine(cmd_line, args);
  const char *to_file_path = strdup(args[1]); // Remember to free() this later
  pid_t pid = getpid();
  if (pid == -1)
  {
    perror("smash error: getpid failed");
    for (int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
  int to_file_fd;
  if (strcmp(args[0], ">") == 0)
  {
    to_file_fd = open(to_file_path, O_WRONLY | O_TRUNC | O_CREAT, S_IWUSR);
  }
  else
  {
    to_file_fd = open(to_file_path, O_WRONLY | O_APPEND | O_CREAT, S_IWUSR);
  }
  if (to_file_fd == -1)
  {
    perror("smash error: open failed");
    for (int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
  int prev_from_fd = dup(1);
  if (prev_from_fd == -1)
  {
    perror("smash error: dup failed");
    for (int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
  if (dup2(to_file_fd, 1) == -1)
  {
    perror("smash error: dup2 failed");
    for (int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
  SmallShell::getInstance().executeCommand(inner_cmd_line);
  if (dup2(prev_from_fd, 1) == -1)
  {
    perror("smash error: dup2 failed");
    for (int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
  if (close(to_file_fd) == -1)
  {
    perror("smash error: close failed");
    for (int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  } // at the end so file pointer is at place.
  if (close(prev_from_fd) == -1)
  {
    perror("smash error: close failed");
    for (int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
}

void WhoAmICommand::execute()
{
  string etc_passwd_path = "/etc/passwd";
  int fd = open(etc_passwd_path.c_str(), O_RDONLY);
  if (fd == -1)
  {
    perror("smash error: open failed");
    return;
  }
  char buffer[1];
  ssize_t bytes_read;
  string passwd_content;
  while ((bytes_read = read(fd, buffer, 1)) != 0)
  {
    if (bytes_read == -1)
    {
      perror("smash error: read failed");
      return;
    }
    passwd_content += buffer[0];
  }
  string uid_str = to_string(getuid());
  int colon_counter = 0;
  for (size_t i = 0; i < passwd_content.length(); i++)
  {
    if (passwd_content[i] == ':')
    {
      colon_counter++;
      if (colon_counter % 6 == 2)
      {
        if (passwd_content.substr(i + 1, uid_str.length()).compare(uid_str) == 0)
        {
          int user_id_start = colon_counter < 6 ? 0 : passwd_content.find_last_of('\n', i) + 1;
          int user_id_end = passwd_content.find(':', user_id_start);
          string user_id = passwd_content.substr(user_id_start, user_id_end - user_id_start);
          int home_dir_start = i;
          for (int j = 0; j < 3; j++)
          {
            home_dir_start = passwd_content.find(':', home_dir_start + 1);
          }
          int home_dir_end = passwd_content.find(':', home_dir_start + 1);
          home_dir_start++;
          string home_dir = passwd_content.substr(home_dir_start, home_dir_end - home_dir_start);
          cout << user_id << " " << home_dir << endl;
          return;
        }
      }
    }
  }
  if (close(fd) == -1)
    perror("smash error: close failed");
}
std::vector<std::string> NetInfoCommand::getDnsServers()
{
  vector<std::string> dns_servers;
  char buffer[4096];
  int fd = open("/etc/resolv.conf", O_RDONLY);
  if (fd == -1)
  {
    perror("smash error: open failed");
    return dns_servers;
  }
  string content;
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
  while (bytes_read > 0)
  {
    content.append(buffer, bytes_read);
    bytes_read = read(fd, buffer, sizeof(buffer));
  }
  if (bytes_read == -1)
  {
    perror("smash error: read failed");
    close(fd);
    return dns_servers;
  }
  istringstream stream(content);
  string line;
  while (getline(stream, line))
  {
    if (line.substr(0, 10) == "nameserver")
    {
      dns_servers.push_back(line.substr(11));
    }
  }
  close(fd);
  return dns_servers;
}

string NetInfoCommand::getDefaultGateway()
{
  char buffer[4096];
  int fd = open("/proc/net/route", O_RDONLY);
  if (fd == -1)
  {
    perror("smash error: open failed");
    return "";
  }
  string content;
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
  while (bytes_read > 0)
  {
    content.append(buffer, bytes_read);
    bytes_read = read(fd, buffer, sizeof(buffer));
  }
  if (bytes_read == -1)
  {
    perror("smash error: read failed");
    close(fd);
    return "";
  }
  istringstream stream(content);
  string line;
  getline(stream, line); // skip header
  while (getline(stream, line))
  {
    istringstream iss(line);
    string iface, dest, gateway;
    iss >> iface >> dest >> gateway;
    if (dest == "00000000")
    {
      unsigned int addr;
      sscanf(gateway.c_str(), "%x", &addr);
      struct in_addr ip_addr;
      ip_addr.s_addr = addr;
      close(fd);
      return inet_ntoa(ip_addr);
    }
  }
  close(fd);
  return "";
}

void NetInfoCommand::execute()
{
  // Parse interface name from command
  char **args = new char *[COMMAND_MAX_ARGS];
  int args_count = _parseCommandLine(cmd_line, args);

  if (args_count < 2)
  {
    cerr << "smash error: netinfo: interface not specified" << endl;
    for (int j = 0; j < args_count; j++)
    {
      free(args[j]);
    }
    delete[] args;
    return;
  }

  const char *interface = args[1];

  // Create socket for interface queries
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
  {
    perror("smash error: socket failed");
    for (int i = 0; i < args_count; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }

  // Get interface information
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

  // Check if interface exists
  if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
  {
    cerr << "smash error: netinfo: interface " << interface << " does not exist" << endl;
    close(sock);
    for (int i = 0; i < args_count; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }

  // Get IP address
  struct sockaddr_in *ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
  string ip = inet_ntoa(ipaddr->sin_addr);

  // Get subnet mask
  if (ioctl(sock, SIOCGIFNETMASK, &ifr) < 0)
  {
    perror("smash error: ioctl failed");
    close(sock);
    for (int i = 0; i < args_count; i++)
    {
      free(args[i]);
    }
    delete[] args;
    return;
  }
  string netmask = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr);

  // Get default gateway and DNS servers
  string gateway = getDefaultGateway();
  vector<std::string> dns_servers = getDnsServers();

  // Print network information
  cout << "IP Address: " << ip << endl;
  cout << "Subnet Mask: " << netmask << endl;
  cout << "Default Gateway: " << gateway << endl;

  cout << "DNS Servers: ";
  if (!dns_servers.empty())
  {
    cout << dns_servers[0];
    for (size_t i = 1; i < dns_servers.size(); i++)
    {
      cout << ", " << dns_servers[i];
    }
  }
  cout << endl;

  // Cleanup
  close(sock);
  for (int i = 0; i < args_count; i++)
  {
    free(args[i]);
  }
  delete[] args;
}
