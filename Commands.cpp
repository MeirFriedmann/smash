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

bool _isBackgroundComamnd(const char *cmd_line) {
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

// TODO: Add your implementation for classes in Commands.h
//TODO change to jobs(new JobsList())
SmallShell::SmallShell() : prompt("smash"),  current_dir(""), last_dir(""),jobs(nullptr), fg_pid(-1)
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
    return new ChangePromptCommand(cmd_line, *this);    
  }
  // else if (firstWord.compare("showpid") == 0) {
  //   return new ShowPidCommand(cmd_line);
  // }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, *this);
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

  char *curr_dir = get_current_dir_name();
  if (curr_dir == nullptr)
  {
    perror("smash error: pwd failed");
    return;
  }
  cout << curr_dir << endl;
  free(curr_dir);
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
    char* curr_dir = get_current_dir_name();
    if (curr_dir == nullptr)
    {
      perror("smash error: getcwd failed");
      for (int i = 0; i < args_len; i++)
      {
        free(args[i]);
      }
      delete[] args;
      return;
    } 
    if (strcmp(args[1], "-") == 0)
    {
      if (SmallShell::getInstance().getLastDir().empty())
      {
        cerr << "smash error: cd: " << endl;
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

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line)
{
}

Command::Command(const char *cmd_line) : cmd_line(cmd_line)
{
}
