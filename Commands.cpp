#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
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

bool _isBackgroundComamnd(const char *cmd_line)
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

// TODO: Add your implementation for classes in Commands.h

SmallShell::SmallShell() : prompt("smash"), current_dir(""), prev_dir(""), jobs(new JobsList()), fg_pid(-1) {}
// TODO: add your implementation

SmallShell::~SmallShell()
{
  // TODO: add your implementation
  delete jobs;
}

// will continue implementing JobsList
// JobsList::JobEntry::JobEntry(pid_t pid, const string &cmd_line, bool isStopped, int job_id) : pid(pid), cmd_line(cmd_line), isStopped(isStopped), job_id(job_id) {}
// JobsList::JobsList() : max_job_id(0) {}
// void JobsList::addJob(Command *cmd, bool isStopped, pid_t pid)
// {
//   // Add job to list
//   JobEntry *job = new JobEntry(pid, cmd->getCmdLine(), isStopped, ++max_job_id);
//   jobs.push_back(job);
//   delete cmd;
// }   
QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs)
{
  char *args[COMMAND_MAX_ARGS];
  int num_args = _parseCommandLine(cmd_line, args);

  // Check if 'kill' argument exists
  kill_flag = (num_args > 1 && string(args[1]) == "kill");

  // Free allocated memory
  for (int i = 0; i < num_args; i++)
  {
    free(args[i]);
  }
}

void QuitCommand::execute()
{
  if (kill_flag)
  {
    // Get number of running jobs
    int job_count = 0;
    vector<pid_t> job_pids;
    vector<string> job_cmds;

    for (int i = 1; i <= jobs->getMaxJobId(); i++)
    {
      JobsList::JobEntry *job = jobs->getJobById(i);
      if (job)
      {
        job_count++;
        job_pids.push_back(job->getPid());
        job_cmds.push_back(job->getCmdLine());
      }
    }

    // Print kill message
    if (job_count > 0)
    {
      cout << "smash: sending SIGKILL signal to " << job_count << " jobs:" << endl;
      for (size_t i = 0; i < job_pids.size(); i++)
      {
        cout << job_pids[i] << ": " << job_cmds[i] << endl;
      }
      jobs->killAllJobs();
    }
  }

  exit(0);
}
/**
 * Creates and returns a pointer to Command class which matches the given command line (cmd_line)
 */

Command *SmallShell::CreateCommand(const char *cmd_line)
{
  // For example:

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0)
  {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0)
  {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0)
  {
    return new ChangeDirCommand(cmd_line, &prev_dir);
  }
  else if (firstWord.compare("chprompt") == 0)
  {
    return new ChangePromptCommand(cmd_line);
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
  else
  {
    return new ExternalCommand(cmd_line);
  }

  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line)
{
  // TODO: Add your implementation here
  // for example:
  // Command* cmd = CreateCommand(cmd_line);
  // cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}