#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <map>
#include <string>
using std::string;
#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)




class Command
{
protected:
    const char *cmd_line;
    bool is_background;
    pid_t prcs_id;

public:
    Command(const char *cmd_line);

    virtual ~Command();

    virtual void execute() = 0;

    pid_t getPid() const
    {
        return prcs_id;
    } // not modifing class members
    const char *getCmdLine() const
    {
        return cmd_line;
    }
    bool isBackground() const
    {
        return is_background;
    }

    // virtual void prepare();
    // virtual void cleanup();
};

class JobsList
{
public:
    class JobEntry
    {
        // TODO: Add your data members
        int job_id; // unique id
        pid_t prcs_id;
        std::string cmd_line;

    public:
        JobEntry(int job_id, pid_t pid, const string &cmd);
        ~JobEntry() = default;
        pid_t getPid() const
        {
            return prcs_id;
        }
        string getCmdLine() const
        {
            return cmd_line;
        }
    };

    // TODO: Add your data members
private:
    std::vector<JobEntry> jobs;
    int max_job_id;

public:
    JobsList();

    ~JobsList();

    void addJob(Command *cmd, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    int getMaxJobId() const
    {
        return max_job_id;
    }

    // TODO: Add extra methods or modify exisitng ones as needed
};


class SmallShell
{
private:
    // TODO: Add your data members
    string prompt;                    // current shell prompt
    string current_dir;               // current working directory
    string last_dir;                  // previous working directory
    JobsList *jobs;                   // Jobs list
    pid_t fg_pid;                     // current foreground process id
    std::map<string, string> aliases; // aliases

    SmallShell();
    
    static SmallShell* instance; 

public:
    Command *CreateCommand(const char *cmd_line);
    SmallShell(SmallShell const &) = delete;     // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance()             // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    // TODO: add extra methods as needed
    const string &getPrompt() const
    {
        return prompt;
    }
    void setPrompt(const string &p)
    {
        prompt = p;
    }
    JobsList *getJobsList()
    {
        return jobs;
    }
    void setFgPid(pid_t pid)
    {
        fg_pid = pid;
    }
    pid_t getFgPid() const
    {
        return fg_pid;
    }
    const string& getLastDir() const{
        return last_dir;
    }
    void setLastDir(const string& dir)
    {
        last_dir = dir;
    }
};



class BuiltInCommand : public Command
{
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand()
    {
    }
};

class ExternalCommand : public Command
{
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand()
    {
    }

    void execute() override;
};

class PipeCommand : public Command
{
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand()
    {
    }

    void execute() override;
};
class ChangePromptCommand : public BuiltInCommand
{
public:
    ChangePromptCommand(const char *cmd_line, SmallShell &smash);
    virtual ~ChangePromptCommand() = default;
    
    void execute() override;
} ;
class RedirectionCommand : public Command
{
    // TODO: Add your data members
private:
    string cmd;
    string filename;
    bool is_append;

public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() = default;

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand
{
    // TODO: Add your data members public:
    string *last_pwd;
    const char *cmd_line;

public:
    ChangeDirCommand(const char *cmd_line, SmallShell& smash);

    virtual ~ChangeDirCommand() = default;

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand
{
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand()
    {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand
{
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand()
    {
    }

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand
{
private:
    JobsList *jobs;
    bool kill_flag;

public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand()
    {
    }

    void execute() override;
};

class JobsCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    JobsCommand(const char *cmd_line, JobsList *jobs);

    virtual ~JobsCommand()
    {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand()
    {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand()
    {
    }

    void execute() override;
};

class ListDirCommand : public Command
{
public:
    ListDirCommand(const char *cmd_line);

    virtual ~ListDirCommand()
    {
    }

    void execute() override;
};

class WhoAmICommand : public Command
{
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand()
    {
    }

    void execute() override;
};

class NetInfo : public Command
{
    // TODO: Add your data members
public:
    NetInfo(const char *cmd_line);

    virtual ~NetInfo()
    {
    }

    void execute() override;
};

class aliasCommand : public BuiltInCommand
{
public:
    aliasCommand(const char *cmd_line);

    virtual ~aliasCommand()
    {
    }

    void execute() override;
};

class unaliasCommand : public BuiltInCommand
{
public:
    unaliasCommand(const char *cmd_line);

    virtual ~unaliasCommand()
    {
    }

    void execute() override;
};



#endif // SMASH_COMMAND_H_
