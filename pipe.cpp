#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <map>

#define ARG_MAX 1024
#define LAST_CMD true
#define NO_PIPE 0
#define PIPE_BOTH 1
#define PIPE_STDOUT 2

using namespace std;

typedef struct pipe_info {
    int read_fd;
    int write_fd;
} pipe_info;
vector<pipe_info> subcmd_pipe_list;

extern long cmd_count;
extern map<int, pipe_info> delay_pipe_table;
vector<pid_t> childpid_list;
int redirect_fd, subcmd_count;

string skip_lead_space(string str) {

    string delimiter = " ";
    while (str.length()!=0 && str.at(0)==' ') {
        try {
            str = str.substr(str.find(delimiter)+1);
        }
        catch (const std::exception& e) {
            cerr << "skip leading spaces:\n" << e.what() << "\n";
        }
    }

    return str;
}

int redirect_handler(string cmd) {  // return pos of '>' symbol if found, otherwise -1
    int pos = cmd.find(">");

    if (pos != -1) {
        string filename = cmd.substr(pos+1);
        filename = skip_lead_space(filename);

        redirect_fd = creat(filename.c_str(), 0666);
        if (redirect_fd == -1) {
            cerr << "open file failed\n" << filename <<endl;
            return -1;
        }
    }

    return pos;
}

void np_exec(string cmd) {
    char *arg[ARG_MAX];
    int arg_cnt = 0, arg_end = 0;
    string arg_delimiter = " ";


    // handle redirect symbol (>)
    redirect_fd = -1;
    int pos;
    if ((pos = redirect_handler(cmd)) != -1) {
        cmd = cmd.substr(0, pos);
        dup2(redirect_fd, STDOUT_FILENO);
    }

    // parse arguments
    string parse_arg(cmd);
    while ((arg_end = parse_arg.find(arg_delimiter)) != string::npos) {
        string cur_arg = parse_arg.substr(0, arg_end);
        if (cur_arg.length())
            arg[arg_cnt++] = strdup(cur_arg.c_str());

        parse_arg = parse_arg.substr(arg_end+1);
        parse_arg = skip_lead_space(parse_arg); 
    }
    if (parse_arg.length())
        arg[arg_cnt++] = strdup(parse_arg.c_str());
    arg[arg_cnt] = NULL;

    if (execvp(arg[0], arg) == -1) {
        cerr << "Unknown command: [" << cmd << "].\n";
        exit(0);
    }

}

void np_fork(string cmd, bool last_cmd) {

    pid_t child_pid;

    // create pipe for subcmd and store in vector list
    int subcmd_pipe[2];
    pipe_info tmp_pipe;
    pipe(subcmd_pipe);
    tmp_pipe.read_fd = subcmd_pipe[0];
    tmp_pipe.write_fd = subcmd_pipe[1];
    subcmd_pipe_list.push_back(tmp_pipe);

    // number (delay) pipe
    long target_cmd = -1;
    int delay_pipe[2], numpipe_type;
    int pos;
    pipe_info pipe_out;

    if ((pos = cmd.find("|")) != string::npos) {
        long delay = stol(cmd.substr(pos+1));
        target_cmd = delay + cmd_count;
        cmd = cmd.substr(0, pos);
        numpipe_type = PIPE_STDOUT;
    }
    else if ((pos = cmd.find("!")) != string::npos) {
        long delay = stol(cmd.substr(pos+1));
        target_cmd = delay + cmd_count;
        cmd = cmd.substr(0, pos);
        numpipe_type = PIPE_BOTH;
    }
    else {
        numpipe_type = NO_PIPE;
    }

    // find if target number pipe has existed
    if (numpipe_type && (delay_pipe_table.find(target_cmd) == delay_pipe_table.end())) {   // not exist
        pipe(delay_pipe);
        pipe_out.read_fd = delay_pipe[0];
        pipe_out.write_fd = delay_pipe[1];
        delay_pipe_table.insert(pair<int, pipe_info>(target_cmd, pipe_out));
    }

    while ((child_pid = fork())<0) {
        int status;
        waitpid(-1, &status, 0);
    }

    if (child_pid == 0) {   // child process

        // connect with previous commands output (number pipe)
        if (delay_pipe_table.find(cmd_count)!=delay_pipe_table.end() && !subcmd_count) {
            dup2(delay_pipe_table.find(cmd_count)->second.read_fd, STDIN_FILENO);
            close(delay_pipe_table.find(cmd_count)->second.read_fd);
            close(delay_pipe_table.find(cmd_count)->second.write_fd);
        }

        // connect with subcmd pipes
        if (!subcmd_count && last_cmd && (numpipe_type == NO_PIPE)) {    // only 1 subcmd
            ;
        }
        else if (!subcmd_count) {           // first subcmd
            dup2((subcmd_pipe_list.at(subcmd_count)).write_fd, STDOUT_FILENO);
        }
        else if (last_cmd) {                // last subcmd
            dup2((subcmd_pipe_list.at(subcmd_count-1)).read_fd, STDIN_FILENO);
        }
        else {                              // others
            dup2((subcmd_pipe_list.at(subcmd_count-1)).read_fd, STDIN_FILENO);
            dup2((subcmd_pipe_list.at(subcmd_count)).write_fd, STDOUT_FILENO);
        }

        if (numpipe_type == PIPE_STDOUT) 
            dup2(delay_pipe_table.find(target_cmd)->second.write_fd, STDOUT_FILENO);
        else if (numpipe_type == PIPE_BOTH) {
            dup2(delay_pipe_table.find(target_cmd)->second.write_fd, STDOUT_FILENO);
            dup2(delay_pipe_table.find(target_cmd)->second.write_fd, STDERR_FILENO);
        }

        for (const auto& it: subcmd_pipe_list) {
            close(it.read_fd);
            close(it.write_fd);
        }

        np_exec(cmd);
    }
    else {  // parent process
        if (!numpipe_type)
            childpid_list.push_back(child_pid);
        auto pip_ele = delay_pipe_table.find(cmd_count);
        if (pip_ele != delay_pipe_table.end()) {
            close(pip_ele->second.read_fd);
            close(pip_ele->second.write_fd);
        }
    }

}

void execute_cmd(string cmd) {

    string pipe_delimiter = "| ", space_delimiter = " ";
    size_t new_end = 0;
    childpid_list.clear();
    subcmd_count = 0;
    subcmd_pipe_list.clear();

    // split cmd into subcmd by pipe delimiter (|)
    // eg, a | b | c |1 -> a, b, c |1
    while ((new_end = cmd.find(pipe_delimiter)) != string::npos) {
        string cur_cmd = cmd.substr(0, new_end);
        np_fork(cur_cmd, !LAST_CMD);
        subcmd_count++;

        cmd = cmd.substr(new_end+1);
        cmd = skip_lead_space(cmd);
    }
    np_fork(cmd, LAST_CMD);
    subcmd_count++;

    for (const auto& it: subcmd_pipe_list) {
        close(it.read_fd);
        close(it.write_fd);
    }

    auto pip_ele = delay_pipe_table.find(cmd_count);
    if (pip_ele != delay_pipe_table.end()) {
        close(pip_ele->second.read_fd);
        close(pip_ele->second.write_fd);
    }

    for (const auto& pid: childpid_list) {
        int status;
        waitpid(pid, &status, 0);
    }
}
