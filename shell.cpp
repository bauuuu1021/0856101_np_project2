#include <iostream>
#include <stdio.h>
#include <string>
#include <signal.h>
#include <sys/wait.h>
#include <map>

using namespace std;

typedef struct pipe_info {
    int read_fd;
    int write_fd;
} pipe_info;

extern void execute_cmd(string cmd);
extern string skip_lead_space(string str);
long cmd_count;
map<int, pipe_info> delay_pipe_table;

void waitChildHandler(int signo) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
}

void printenv(string cmd) {

    char *ret;

    try {
        string env_name = cmd.substr(9, string::npos);
        ret = getenv(env_name.c_str());
    }
    catch (const std::exception& e) {
        cerr << "[printenv] error:\n" << e.what() << "\n";
        return;
    }

    if (ret)
        printf("%s\n", ret);
}

void setenv(string cmd) {

    try {
        string delimiter = " ";
        cmd = cmd.substr(cmd.find(delimiter)+1);
        string env_name = cmd.substr(0, cmd.find(delimiter));
        string env_value = cmd.substr(cmd.find(delimiter)+1, string::npos);
        setenv(env_name.c_str(), env_value.c_str(), !0);
    }
    catch (const std::exception& e) {
        cerr << "[setenv] error:\n" << e.what() << "\n";
        return;
    }
}

int main () {

    signal(SIGCHLD, waitChildHandler);
    setenv("PATH", "bin:.", !0);
    cmd_count = 0;
    delay_pipe_table.clear();

    string cmd, delimiter = " ";
    cout << "% ";
    while (getline(cin, cmd) && cmd.compare("exit")) {

        cmd = skip_lead_space(cmd);         

        if (!cmd.compare(0, 8, "printenv")) {
            printenv(cmd);
            cmd_count++;
        }
        else if (!cmd.compare(0, 6, "setenv")) {
            setenv(cmd);
            cmd_count++;
        }
        else if (cmd.length()==0) {     // empty command
            ;
        }
        else {
            execute_cmd(cmd);
            cmd_count++;
        }

        cout << "% ";
    }
}
