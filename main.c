#include "shell.h"

job *head=NULL;

//not certain if these are working as intended, job control seems to wirking fine however
void sig_handler(int sig){
    if (sig==SIGTSTP){ //ctrl-z
        if (head!=NULL){
            printf("putting %s:%d in the bg\n",head->command,head->pgid); fflush(stdout);
            killpg(head->pgid,SIGTSTP);
            head->bg=1;
            move_to_bg(head);
            return;
        }
        else{
            printf("\nNo process in the background\n"); fflush(stdout);
        }
    }
    else if (sig==SIGINT){ //ctrl-c
        if (head!=NULL){
            printf("Killing process group %d\n",head->pgid); fflush(stdout);
            killpg(head->pgid,SIGINT);
            tcsetpgrp(shell_terminal,shell_pgid);
            tcgetattr(shell_terminal,&head->tmodes);
            tcsetattr(shell_terminal,TCSADRAIN,&shell_tmodes);
        }
        else{
          printf("nothing to kill\n");
        }
    }
}

void init_shell() {
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty(shell_terminal); //checks if terminal is running interactively
    if (shell_is_interactive) {
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp())) //kills until it is in the foreground
            kill(-shell_pgid, SIGTTIN);

        //activates signal handler and ignores
        signal(SIGINT,sig_handler);
        signal(SIGTSTP, sig_handler);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        
        shell_pgid = getpid();
        setpgid(shell_pgid,shell_pgid); //make terminal its own process group
        tcsetpgrp(shell_terminal, shell_pgid); //puts shell process group in the foreground
        tcgetattr(shell_terminal, &shell_tmodes_OLD); //gets the shell terminal modes
        
        //set the stdin to raw, unbuffered input
        memcpy(&shell_tmodes,&shell_tmodes_OLD,sizeof(struct termios));
        shell_tmodes.c_lflag &= ~(ICANON | ECHO);
        shell_tmodes.c_cc[VTIME]=0;
        shell_tmodes.c_cc[VMIN]=1;
        tcsetattr(shell_terminal,TCSANOW,&shell_tmodes);
        atexit(exit_func); //calls exit_func on exit

        if ((command_history=malloc(sizeof(char*) * MAX_HISTORY)) == NULL){
            fprintf(stderr,"Internal error malloc; line ~60; Exiting;");
            exit(-1);
        }
        history_size=0;
    }
}

//prints all jobs in list
void print_all(){
    job *tmp;
    int i=0;
    printf("%sIndex\tPGID\tCommand%s\n",_UNDERLINE_,_RESET_);
    for (tmp=head;tmp;tmp=tmp->next){
        printf("%d\t%d\t%s\n",i++,tmp->pgid,tmp->command);
    }
    printf("\n");
    free(tmp);
}

/* Pushes job onto job stack */
void push(job* j){
    job *tmp=copy_job(j);
    tmp->next=head;
    head=tmp;
}

/* Removes head from job stack */
void pop(){
    job *tmp;
    tmp=head;
    head=head->next;
    if (head!=NULL && head->bg){
        if (!is_alive())
            pop();
    }
    free(tmp);
}

/* checks if head is alive */
int is_alive(){
    int status;
    pid_t return_pid=waitpid(head->pgid,&status,WNOHANG);
    if (return_pid==0){
        return 1;
    }
    else{
        return 0;
    }
}

/* Function to help copy process list for each job */
void process_push(process **headp,char **argv,pid_t pid){
    process *tmp;
    if ((tmp=(process*)malloc(sizeof(process))) == NULL){
        fprintf(stderr,"Internal error malloc; line ~112; Exiting;");
        exit(-1);
    }
    tmp->pid=pid;
    int i=0;
    while (argv[i]!=NULL)
        i++;
    if ((tmp->argv=malloc(sizeof(char*)*i))==NULL){
        fprintf(stderr,"Internal error malloc; line ~120; Exiting;");
        exit(-1);
    }
    for (int j=0;j<i;j++){
        if ((tmp->argv[j]=malloc(sizeof(strlen(argv[j]))+1))==NULL){
            fprintf(stderr,"Internal error malloc; line ~125; Exiting;");
            exit(-1);
        }
        tmp->argv[j]=malloc(sizeof(char*)*strlen(argv[j])+1);
        strcpy(tmp->argv[j],argv[j]);
        tmp->argv[j][strlen(argv[j])]='\0';
    }
    tmp->next=*headp;
    *headp=tmp;
}

/* copies the entire process linked list, given the head of the process 
   returns pointer to the head of the new list */
process *copy_process_list(process *p){
    process *tail=NULL;
    process *new_process=NULL;
    process *cur=p;
    while (cur!=NULL){
        if (new_process==NULL){
            process_push(&new_process,cur->argv,cur->pid);
            tail=new_process;
        }
        else{
            process_push(&(tail->next),cur->argv,cur->pid);
            tail=tail->next;
        }
        cur=cur->next;
    }
    free(cur);
    return new_process;
}

/* Does a deep copy of the given job pointer */
job *copy_job(job *j){
    job *new;
    if ((new=malloc(sizeof(job)))==NULL){
        fprintf(stderr,"Internal error malloc; line ~159; Exiting;");
        exit(-1);
    }
    *(new)=*(j);
    new->command=strdup(j->command);
    new->first_process=copy_process_list(j->first_process);
    if (j->file!=NULL)
        new->file=strdup(j->file);
    return new;
}

/* sets proper pid and pgid for job/process
   also sets in in the foreground if needed */
void set_pid(job *j,pid_t pgid, int bg){
    pid_t pid;
    pid=getpid();
    if (pgid==0)
        pgid=pid;
    setpgid(pid,pgid);
    if (!bg)
        tcsetpgrp(shell_terminal,pgid);
    j->pgid=pgid;
}

/* Launches process with proper input and output streams */
int run_process(process *p,int in,int out){
    pid_t pid;
    pid=fork();
    if (pid==0){
        if (in!=0){
            dup2(in,0);
            close(in);
        }
        if (out!=1){
            dup2(out,1);
            close(out);
        }
        if (execvp(p->argv[0],p->argv)<0){
            fprintf(stderr,"%s: %s\n",p->argv[0],strerror(errno));
            exit(-1);
        }
    }
    return pid;
}

void move_to_bg(job *j){
    tcsetpgrp(shell_terminal,shell_pgid);
    tcgetattr(shell_terminal,&head->tmodes);
    tcsetattr(shell_terminal,TCSADRAIN,&shell_tmodes);
    j->bg=1;
}

int move_to_fg(job *j){
    tcsetpgrp(shell_terminal,j->pgid);
    tcsetattr(shell_terminal,TCSANOW,&shell_tmodes_OLD);
    j->bg=0;
    int status=wait_for_job(j);
    return status;
}

int wait_for_job(job *j){
    int status;
    waitpid(head->pgid,&status,WUNTRACED);
    if (!WIFSTOPPED(status)){
        pop();
        tcsetattr(shell_terminal,0,&shell_tmodes);
        tcsetpgrp(shell_terminal,shell_pgid);
    }
    return status;
}

//parses input and gets it ready to run as a command
void parse_command(job *j, char *in,char **args){
        process *p;
        if ((p=malloc(sizeof(process)))==NULL){
            fprintf(stderr,"Internal error malloc; line ~234; Exiting;");
            exit(-1);
        }
        p->next=NULL;
        p->argv=&args[0];
        j->first_process=p;
        int i=0;
        char* tok=strtok(in," ");
        while (tok!=NULL){
            args[i++]=tok;
            if (!strcmp(tok,"|")){
                args[i-1]=NULL;
                if ((p->next=malloc(sizeof(process)))==NULL){
                    fprintf(stderr,"Internal error malloc; line ~247; Exiting;");
                    exit(-1);
                }
                p=p->next;
                p->argv=&args[i];
                p->next=NULL;
            }
            if (!strcmp(tok,">")){
                args[i--]=NULL;
                tok=strtok(NULL," ");
                j->file=tok;
                break;
            }
            tok=strtok(NULL," ");
        }
        if (!strcmp(args[i-1],"&")){
            j->bg=1;
            i--;
        }
        args[i]=NULL;
        p=NULL;
        free(p);
}

/* Runs a job */
void run_job(job *j){
    int in,fd[2];
    in=0;
    pid_t pid;
    process *p;
    signal(SIGTSTP,sig_handler);
    pid=fork();
    if (pid==0){
        for (p=j->first_process;p;p=p->next){
            if (p->next){
                pipe(fd);
                set_pid(j,j->pgid,j->bg);
                run_process(p,in,fd[1]);
                close(fd[1]);
                in=fd[0];
            }
            else{
                if (in!=0)
                    dup2(in,0);
                if (j->file!=NULL){
                    j->file_fd=open(j->file, O_CREAT | O_WRONLY, 0644);
                    if (j->file_fd<0){
                        fprintf(stderr,"Could not open/create file %s\n",j->file);
                        exit(-1);
                    }
                    dup2(j->file_fd,1);
                }
                if (execvp(p->argv[0],p->argv)<0){
                    fprintf(stderr,"%s: %s\n",p->argv[0],strerror(errno));
                    exit(-1);
                }
            }
        }
    }
    else{
        if (shell_is_interactive){
            if (!j->pgid){
                j->pgid=pid;
            }
            setpgid(pid,j->pgid);
        }
    }
    if (j->bg){
        push(j);
        move_to_bg(head);
    }
    else{
        push(j);
        move_to_fg(j);
        tcgetattr(shell_terminal,&j->tmodes);
        tcsetattr(shell_terminal,TCSADRAIN,&shell_tmodes);
        tcsetpgrp(shell_terminal,shell_pgid);
    }
}


int main(){
	char *args[80];
    init_shell();
    char in[256];
	while (1){
        memset(in,0,128);
        job j;
        j.bg=0;
        j.pgid=0;
        j.next=NULL;
        j.file=NULL;
        print_prompt();
        input_buffer(in,128);
        if (strlen(in)==0) continue; //empty string
        j.command=in;
        //adds to command history; if at max, moves memory down one
        command_history[history_size]=strdup(in);
        if (history_size==MAX_HISTORY){//out of bounds for array
            memmove(&command_history[0],&command_history[1],sizeof(char*)*MAX_HISTORY-1);
        }
        else history_size++;
        parse_command(&j,in,args);
        if (!strcmp(args[0],"exit")){
            exit(0);
        }
        if (!strcmp("cd",args[0])){
            char tmp[128];
            getcwd(tmp,sizeof(tmp));
            if (args[1]==NULL) args[1]=pw->pw_dir;
            else if (!strcmp(args[1],"~")) args[1]=pw->pw_dir;
            else if (!strcmp(args[1],"-")) args[1]=last_dir;
            if (chdir(args[1])==-1){
                printf("%s\n",strerror(errno));
            }
            else{
                strcpy(last_dir,tmp);
            }
            continue;
        }
        if (!strcmp(args[0],"fg")){
            if (head!=NULL){
                printf("putting %s:%d in the fg\n",head->command,head->pgid);
                killpg(head->pgid,SIGCONT);
                tcsetattr(shell_terminal,TCSADRAIN,&head->tmodes);
                int status=move_to_fg(head);
                if (WIFSTOPPED(status)){ //if status recieved ctrl-z
                    printf("putting %s:%d in the bg\n",head->command,head->pgid); fflush(stdout);
                    killpg(head->pgid,SIGTSTP); //sends stop signal
                    move_to_bg(head);
                }
            }
            continue;
        }
        if (!strcmp(args[0],"jobs")){
            print_all();
            continue;
        }
        if (!strcmp(args[0],"history")){
            int i;
            for (i=0;i<history_size;i++){
                printf("%d.\t%s\n",i,command_history[i]);
            }
            continue;
        }
        run_job(&j);
        if (head!=NULL){
            if (!is_alive()) pop();
        }
	}
}
