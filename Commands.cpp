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
SmallShell::SmallShell() : prompt("smash"), jobs(nullptr), fg_pid(-1), last_dir("")
{
  // TODO: add your implementation
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
    return new ChangePromptCommand(cmd_line, getInstance());    
  }
  // else if (firstWord.compare("showpid") == 0) {
  //   return new ShowPidCommand(cmd_line);
  // }
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
  Command *cmd = CreateCommand(cmd_line);
  cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
{
}

void GetCurrDirCommand::execute()
{

  char *currDir = get_current_dir_name();
  cout << currDir << endl;
  free(currDir);
}

/* TODO: Add your implementation here::::::::::MEIR*/
ChangeDirCommand::ChangeDirCommand(const char *cmd_line, SmallShell &smash) : BuiltInCommand(cmd_line), smash(smash)
{
}
void ChangeDirCommand::execute()
{
  char **args = (char **)malloc(sizeof(char *) * 21);
  int len = _parseCommandLine(cmd_line, args);
  if (len == 1)
  {
    free(args[0]);
    free(args);
    return;
  }
  if (len == 2)
  {
    string currDir = string(get_current_dir_name());
    if (strcmp(args[1], "-") == 0)
    {
      if (smash.getLastDir() == "")
      {
        perror("smash error: cd: OLDPWD not set");
        return;
      }
      else
      {
        if (chdir(smash.getLastDir().c_str()) != 0)
        {
          perror("smash error: chdir failed");
        }
        else
        {
          smash.setLastDir(currDir);
        }
      };
    }
    else if (strcmp(args[1], "..") == 0)
    {
      if (chdir("..") != 0)
      {
        perror("smash error: chdir failed");
      }
      else
      {
        smash.setLastDir(currDir);
      }
    }
    else if (chdir(args[1]) != 0)
    {
      perror("smash error: chdir failed");
    }
    else
    {
      smash.setLastDir(currDir);
    }
  }
  else
  {
    perror("smash error: cd: too many arguments");
  }

  for (int i = 1; args[i] != NULL; i++)
  {
    free(args[i]);
  }
  free(args);
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line)
{
}

Command::Command(const char *cmd_line) : cmd_line(cmd_line)
{
}

ChangePromptCommand::ChangePromptCommand(const char *cmd_line, SmallShell &smash) : BuiltInCommand(cmd_line), smash(smash)
{
}

void ChangePromptCommand::execute()
{
  char **args = (char **)malloc(sizeof(char *) * 21);
  int len = _parseCommandLine(cmd_line, args);
  if (len == 1)
  {
    free(args[0]);
    free(args);
    smash.setPrompt("smash");
    return;
  }

  string prompt = string(args[1]);
  for (int i = 1; args[i] != NULL; i++)
  {
    free(args[i]);
  }
  free(args);
  smash.setPrompt((prompt));
}
