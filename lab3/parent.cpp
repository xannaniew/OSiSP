#define _XOPEN_SOURCE 700
#include <signal.h>
#include <iostream>
#include <sys/ucontext.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <map>
#include <limits>
#include <termios.h>
#include <ctime>

using namespace std;

#define FIFO_NAME "child_info_holder"
#define ALLOW 1
#define DENY 0
#define REQUEST_INFO 2
#define IS_READY 3

#define BUF_SIZE 1024

class ChildInfo
{
    const char *name;
    bool is_allowed;

public:
    ChildInfo(const char *name)
    {
        is_allowed = false;
        this->name = new char[strlen(name) + 1];
        strcpy((char *)this->name, name);
    }

    ~ChildInfo()
    {
        delete this->name;
    }

    const char *get_name()
    {
        return this->name;
    }

    void set_is_allowed(bool value)
    {
        is_allowed = value;
    }

    bool get_is_allowed() const
    {
        return is_allowed;
    }
};

typedef enum NAME_OPERATIONS
{
    GENERATE = 0,
    GET = 1
} NAME_OPERATIONS;

typedef enum OPERATIONS
{
    CREATE_CHILD = '+',
    KILL_CHILD = '-',
    QUIT = 'q',
    KILL_ALL_CHILDREN = 'k',
    LIST = 'l',
    ALL_ALLOWED = 'g',
    ALL_DENYED = 's'
} OPERATIONS;

volatile sig_atomic_t last_child_process;
volatile sig_atomic_t print_request = false;
volatile sig_atomic_t is_closed = true;
volatile sig_atomic_t fifo_fd;

pid_t child_pid;
bool is_time_limit = false;

map<pid_t, ChildInfo *> children;

void display_info();
void handle_child_request(int, siginfo_t *, void *);
char *generate_child_name(NAME_OPERATIONS);
void list_processes(map<pid_t, ChildInfo *> *);
void open_child_info_holder();
void close_child_info_holder();
void create_child();
void kill_child();
void manage_allow_flag(map<pid_t, ChildInfo *> *, int);
void display_info();
void delete_ChildInfo();
void error_occured();
bool parse_complex_input(string, string, string, int &);
bool manage_input();

void handle_child_request(int signum, siginfo_t *info, void *context)
{
    sigval response;

    if (info->si_value.sival_int == IS_READY)
    {
        display_info();
        return;
    }

    map<pid_t, ChildInfo *>::const_iterator it = children.find(info->si_pid);
    if (it != children.end())
    {
        if (it->second->get_is_allowed())
        {
            response.sival_int = ALLOW;
            print_request = true;
            sigqueue(it->first, SIGUSR2, response);
        }
        else
        {
            response.sival_int = DENY;
            sigqueue(it->first, SIGUSR2, response);
        }
    }
}

char *generate_child_name(NAME_OPERATIONS operation) // генерируем имя потомка
{
    static int counter = 0;
    static char last_child_name[sizeof("C_kXX")];
    if (operation == GET)
        return last_child_name;

    string name = "C_";
    name.append(to_string(counter));
    counter++;

    for (int i = 0; i < name.size(); i++)
        last_child_name[i] = name[i];

    return last_child_name;
}

void list_processes(map<pid_t, ChildInfo *> *processes)
{
    for (auto i : *processes)
    {
        printf("\nchild: pid = %d\tname = %s", i.first, i.second->get_name());
        if (i.second->get_is_allowed())
            printf("\tis allowed to print info");
        else
            printf("\tis prohibited to print info");
    }
    printf("\nparent: pid = %d", getpid());
}

void open_child_info_holder()
{
    if (is_closed)
    {
        if ((fifo_fd = open(FIFO_NAME, O_RDONLY)) == -1)
        {
            perror("open");
            exit(EXIT_FAILURE);
        }
        is_closed = false;
    }
}

void close_child_info_holder()
{
    if (is_closed)
        return;
    else
    {
        if (close(fifo_fd) == -1)
        {
            perror("close");
            exit(EXIT_FAILURE);
        }
        is_closed = true;
    }
}

void create_child()
{
    pid_t child_pid;
    int status;

    char *generated_name = generate_child_name(GENERATE);

    last_child_process = fork();
    if (last_child_process == -1) // если родителю вернуло -1 -> ошибка
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (last_child_process == 0)
    {
        execl("/home/xannaniew/projects-cpp/OSiSP/lab3/child", generated_name, NULL);
    }
    else
    {
        open_child_info_holder();
        return;
    }

    return;
}

void kill_child()
{
    if (kill(last_child_process, SIGKILL) == -1)
    {
        perror("kill");
        exit(EXIT_FAILURE);
    }
}

void manage_allow_flag(map<pid_t, ChildInfo *> *children, int operation)
{
    if (operation == ALLOW)
    {
        for (auto i : *children)
            i.second->set_is_allowed(ALLOW);
    }
    else if (operation == DENY)
    {
        for (auto i : *children)
            i.second->set_is_allowed(DENY);
    }
}

void display_info()
{
    char buffer[BUF_SIZE] = {0};
    ssize_t bytes_received;

    bytes_received = read(fifo_fd, buffer, BUF_SIZE);

    if (bytes_received == -1)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
    else if (bytes_received == 0)
        return;

    printf("\nPARENT: child info: %s", buffer);

    print_request = false;
}

void delete_ChildInfo()
{
    for (auto i : children)
    {
        delete i.second;
    }
}

void error_occured()
{
    printf("\nERROR: operation is undefined");
    if (!children.empty())
        delete_ChildInfo();
    kill(getpgid(getpid()), SIGKILL);

    exit(EXIT_FAILURE);
}

bool parse_complex_input(string begin, string end, string input, int &number)
{
    size_t begin_position = 0;
    size_t end_position = 0;
    if ((begin_position = input.find(begin)) != string::npos)
    {
        if ((end_position = input.find(end, begin_position + 1)) != string::npos)
        {
            for (int i = begin_position + 2; i < end_position; i++)
            {
                if (input[i] > '9' || input[i] < '0')
                    return false;
            }
            number = stoi(input.substr(begin_position + 2, end_position - 1));
            return true;
        }
        return false;
    }
    return false;
}

bool manage_input()
{
    time_t start_time;
    time_t end_time;
    while (true)
    {
        string input;
        printf("\nInput operation: ");

        // if (is_time_limit)
        // {
        //     printf("\nInput operation: ");
        //     while (start_time < end_time)
        //     {
        //         if (cin >> input)
        //             break;
        //     }
        // }

        cin >> input;
        cin.ignore();

        int number;

        if (input.find_first_of(CREATE_CHILD) != string::npos)
        {
            printf("\nYour operation: %c", CREATE_CHILD);
            create_child();
            child_pid = last_child_process;
            children.emplace(child_pid, new ChildInfo(generate_child_name(GET)));
        }

        else if (input.find_first_of(KILL_CHILD) != string::npos)
        {
            printf("\nYour operation: %c", KILL_CHILD);
            if (children.size() == 1)
                close_child_info_holder();
            kill_child();
            child_pid = last_child_process;
            children.erase(child_pid);
        }

        else if (parse_complex_input("s<", ">", input, number))
        {
            if (children.size() >= number)
            {
                printf("\nYour operation: s<%d>", number);
                string name = "C_" + to_string(number);
                for (auto i : children)
                {
                    if (i.second->get_name() == name.c_str())
                        i.second->set_is_allowed(DENY);
                }
            }
        }

        else if (parse_complex_input("g<", ">", input, number))
        {
            if (children.size() >= number)
            {
                printf("\nYour operation: g<%d>", number);
                string name = "C_" + to_string(number);
                for (auto i : children)
                {
                    if (!strcmp(i.second->get_name(), name.c_str()))
                        i.second->set_is_allowed(ALLOW);
                }
            }
        }

        else if (parse_complex_input("p<", ">", input, number))
        {
            if (children.size() < number)
                return false;
            printf("\nYour operation: p<%d>", number);
            sigval request;
            request.sival_int = REQUEST_INFO;
            manage_allow_flag(&children, DENY);
            string name = "C_" + to_string(number);
            for (auto i : children)
            {
                if (!strcmp(i.second->get_name(), name.c_str()))
                    sigqueue(i.first, SIGUSR2, request);
            }
            // start_time = time(nullptr);
            // end_time = start_time + 5;
            is_time_limit = true;
            continue;
        }

        else if (input.find_first_of(LIST) != string::npos)
        {
            printf("\nYour operation: %c", LIST);
            list_processes(&children);
        }

        else if (input.find_first_of(ALL_ALLOWED) != string::npos)
        {
            if (!children.empty())
            {
                printf("\nYour operation: %c", ALL_ALLOWED);
                manage_allow_flag(&children, ALLOW);
            }
        }

        else if (input.find_first_of(QUIT) != string::npos)
        {
            if (!children.empty())
            {
                printf("\nYour operation: %c", QUIT);
                close_child_info_holder();
                delete_ChildInfo();
                for (auto i : children)
                    kill(i.first, SIGKILL);
                printf("\nINFO: all children were killed");
            }
            return true;
        }

        else if (input.find_first_of(ALL_DENYED) != string::npos)
        {
            if (!children.empty())
            {
                printf("\nYour operation: %c", ALL_DENYED);
                manage_allow_flag(&children, DENY);
            }
        }

        else if (input.find_first_of(KILL_ALL_CHILDREN) != string::npos)
        {
            if (!children.empty())
            {
                printf("\nYour operation: %c", KILL_ALL_CHILDREN);
                for (auto i : children)
                    kill(i.first, SIGKILL);
                delete_ChildInfo();
            }
        }

        else
        {
            return false;
        }
    }
}

// export CHILD_PATH=/home/xannaniew/projects-cpp/OSiSP/lab3/child

int main(int argc, char **argv)
{
    struct sigaction action_sigusr1;
    sigemptyset(&action_sigusr1.sa_mask);
    action_sigusr1.sa_flags = SA_SIGINFO | SA_RESTART; // используем обработчик типа sa_sigaction
    action_sigusr1.sa_sigaction = handle_child_request;

    if (sigaction(SIGUSR1, &action_sigusr1, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    system("rm child_info_holder");
    system("cls");

    if (mkfifo(FIFO_NAME, 0777) == -1 && errno != EEXIST)
    {
        perror("mkfifo");
        exit(1);
    }

    if (!manage_input())
        error_occured();

    return EXIT_SUCCESS;
}
