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
#define MAX_LINE 80

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
} job;

pid_t shell_pgid;
int shell_is_interactive;
int shell_terminal;
struct termios shell_tmodes, shell_tmodes_OLD;
char last_dir[128];


//main functions
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
void print_prompt();
void clear_input_field();
void delete_word(char *buffer);
void reposition_cursor(int i,int x);
int input_buffer(char *buffer,size_t size);