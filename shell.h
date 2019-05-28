#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/ioctl.h>

/*
   main.c
*/
#define MAX_HISTORY 50

//ANSI color codes for better output
#define _RED_       "\x1b[31m"
#define _GREEN_     "\x1b[32m"
#define _YELLOW_    "\x1b[33m"
#define _BLUE_      "\x1b[34m"
#define _MAGENTA_   "\x1b[35m"
#define _CYAN_      "\x1b[36m"
#define _UNDERLINE_ "\x1b[4m"
#define _RESET_     "\x1b[0m"


typedef struct process {
    struct process *next;
    char **argv;
    pid_t pid;
} process;

typedef struct job {
    struct job *next;
    char *command;
    process *first_process;
    pid_t pgid;
    struct termios tmodes;
    int stdin,stdout,stderr;
    int bg;
    char *file;
    int file_fd;
    int file_mode;
} job;

pid_t shell_pgid;
int shell_is_interactive;
int shell_terminal;
struct termios shell_tmodes, shell_tmodes_OLD;
char last_dir[128];
char **command_history;
int history_size;

//main functions
void add_history(char *in);
job *copy_job(job *);
process *copy_process_list(process *p);
void init_shell();
int is_alive();
void move_to_bg(job *j);
int move_to_fg(job *j);
void parse_command(job *j,char *in,char **args);
void pop();
void print_all();
void process_push(process **headp,char **argv,pid_t pid);
void push(job *j);
void run_job(job *j);
int run_process(process *p,int in,int out);
void set_pid(job *j,pid_t pgid,int bg);
void sig_handler(int sig);
int wait_for_job(job *j);

static void exit_func(void){
    tcsetattr(shell_terminal,TCSANOW,&shell_tmodes_OLD);
}

/*
   input.c
*/
struct passwd *pw; //current pw


//input funtions
int compare_stings(const void *a, const void *b);
int in_array(char **buffer,char *string,int size);
int get_cursor_pos(int *cx, unsigned short *tx);
int is_dir(char *buffer);
void super_tab1(char *buffer);
void super_tab2(char *buffer, char **dirs, int all);
void print_completion(char **result,int size);
void print_prompt();
void clear_input_field();
void delete_word(char *buffer);
void reposition_cursor(int i,int x);
int input_buffer(char *buffer,size_t size);
