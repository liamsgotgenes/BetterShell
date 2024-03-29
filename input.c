#include "shell.h"

char *current_dir[]={ ".", NULL };
char *binaries[]={ "/bin/", "/usr/bin/", "/usr/local/bin/", NULL };
char *binaries_cwd[]={ "/bin/", "/usr/bin/", "/usr/local/bin/", ".", NULL };
int history_index=0;

/* Detects a key press from the keyboard
   without the need for enter to be pressed */
static int key_press(void){
    struct timeval timeout;
    fd_set read_handles;
    int status;
    FD_ZERO(&read_handles);
    FD_SET(0,&read_handles);
    timeout.tv_sec=timeout.tv_usec=0;
    status=select(1,&read_handles,NULL,NULL,&timeout);
    return status;
}

int compare_stings(const void *a, const void *b){
    const char *c=a;
    const char *d=b;
    return strcmp( *(const char**)a, *(const char**)b );
}

int in_array(char **buffer,char *string,int size){
    int i;
    for (i=0;i<size;i++){
        if (strcmp(buffer[i],string)==0){
            return 0;
        }
    }
    return 1;
}

/* prints out tab-completion in formatted columns */
void print_result(char **result, const int size){
    printf("\n");
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    int col=w.ws_col;
    int row=w.ws_row;
    int loc=0;
    int longest=0;
    int i;
    int lines=0;
    for (i=0;i<size;i++){
        if (strlen(result[i])>longest){
            longest=strlen(result[i]);
        }
    }
    for (i=0;i<size;i++){
        loc+=longest+1;
        if (loc>=col-2){
            printf("\n");
            lines++;
            if (lines>=row-2){
                char c;
                while (!key_press()){
                    printf("--%d of %d--",i,size);
                    c=getchar();
                    clear_input_field();
                    if (c=='\n') break;
                    if (c=='\033' || c=='q') return; //esc
                }
            }
            loc=0;
            loc+=longest+1;
        }
        if (result[i][strlen(result[i])-1]=='/')
            printf(_BLUE_"%-*s"_RESET_,longest+1,result[i]);
        else
            printf("%-*s",longest+1,result[i]);
    }
    printf("\n");
}

//allows completion when nested in directories
int is_dir(char *buffer){
    int len;
    for (len=strlen(buffer)-1;len>=0;len--){
        char c=buffer[len];
        if (c=='/'){
            char *sub_buffer=buffer+len+1;
            char *dir;
            if ((dir=malloc(sizeof(char)*strlen(buffer)-len))==NULL){
                fprintf(stderr,"Input Error; malloc; Line 46\n");
                exit(-1);
            }
            strncpy(dir,buffer,len+1);
            dir[len+1]='\0';
            char *dirs[]={dir,NULL};
            super_tab2(sub_buffer,dirs,0);
            return 1; //return 1 if is a dir. returns 0 if not
        }
    }
    return 0;
}

/* figures out which directories and section of the input buffer to use for the tab completion */
void super_tab1(char *buffer){
    int last_index=strlen(buffer)-1;
    int target=last_index;
    char c=buffer[target];
    char **dirs;
    while (c!=' ' && target!=0){
        c=buffer[target--];
    }
    if (strlen(buffer)==0){ //tab on nothing
        return;
    }
    if (target==0){ //is pressing tab on the first word in command
        if (is_dir(buffer)==0)
            super_tab2(buffer,binaries,0);
        return;
    }
    else{ //is pressing tab on last word of multi word command
        target+=2;
        if (buffer[strlen(buffer)-1]==' '){ //ends on a space -> print cwd
            char *sub_buffer=buffer+strlen(buffer); //making sure it only hands super_tab2 what it needs; if handed full buffer, tab completion overwrites previous word
            super_tab2(sub_buffer,current_dir,1);
            return;
        }
        char *sub_buffer=buffer+target;
        if (is_dir(sub_buffer)==0)
            super_tab2(sub_buffer,current_dir,0);
        return;
    }
}

void free_result(char **result,int cap){
    int i;
    for (i=0;i<cap;i++){
        free(result[i]);
    }
    free(result);
}

void super_tab2(char *buffer, char **dirs, int all){
    int size=0;
    int cap=20;
    int dir_count=0;
    char **result=malloc(sizeof(char*)*cap);
    while (dirs[dir_count]){ //while directory list is not NULL
        DIR *d;
        d=opendir(dirs[dir_count]);
        if (d==NULL) return; //is not a directory
        struct dirent *dir;
        if (all){ //print all items
            while ((dir=readdir(d))!=NULL){ //while there is still items in directory to read
                if ( in_array(result,dir->d_name,size)==0 ) continue;
                if (!strcmp(dir->d_name,"..")) continue;
                if (!strcmp(dir->d_name,".")) continue;
                result[size++]=strdup(dir->d_name);
                if (dir->d_type==DT_DIR){
                    int len=strlen(result[size-1]);
                    char *new=malloc(sizeof(char)+len+2);
                    strcpy(new,result[size-1]);
                    new[len]='/';
                    new[len+1]='\0';
                    char *temp=result[size-1];
                    result[size-1]=new;
                    free(temp);
                }
                if (size==cap){
                    cap*=2;
                    if ( (result=realloc(result,sizeof(char*)*(cap))) == NULL ){
                        fprintf(stderr,"\nerror on realloc\n");
                        exit(-1);
                    }
                }
            }
        }
        else{
            while ((dir=readdir(d))!=NULL){ //while there is still items in directory to read
                if ( in_array(result,dir->d_name,size)==0 ) continue;
                int match=strncmp(buffer,dir->d_name,strlen(buffer));
                if (match==0){
                    result[size++]=strdup(dir->d_name);
                    if (dir->d_type==DT_DIR){                    
                        int len=strlen(result[size-1]);
                        char *new=malloc(sizeof(char)+len+2);
                        strcpy(new,result[size-1]);
                        new[len]='/';
                        new[len+1]='\0';
                        char *temp=result[size-1];
                        result[size-1]=new;
                        free(temp);
                    }
                    if (size==cap){
                        cap*=2;
                        if ( (result=realloc(result,sizeof(char*)*(cap))) == NULL ){
                            fprintf(stderr,"\nerror on realloc\n");
                            exit(-1);
                        }
                    }
                }
            }
        }
        dir_count++;
        free(d);
    }
    if (size==0){
        free_result(result,size);
        return;
    }

    qsort( result, size, sizeof(char*), compare_stings); //sort array

    if (size==1){ //autofills if only one match
        strcpy(buffer,result[0]);
        free_result(result,size);
        return;
    }

    int match_attempt=strlen(buffer); //does partial completion on buffer
    int changed=0;
    int match=1;
    int j;
    while (1 && size!=0){
        for (j=1;j<size;j++){
            if (result[j-1][match_attempt] != result[j][match_attempt]){
                match=0;
                break;
            }
        }
        if (!match) break;
        match_attempt++;
        changed=1;
    }
    if (changed){ //avoids printing new lines and all options on a partial completion
        strncpy(buffer,result[0],match_attempt);
        free_result(result,size);
        return;
    }
    print_result(result,size);
    free_result(result,size);

}

/* Prints the shell prompt, including user and current directory */
void print_prompt(){
        uid_t uid=getuid();
        pw=NULL;
        pw=getpwuid(uid);
        char cwd[128];
        getcwd(cwd,128);
        if (!strncmp(cwd,pw->pw_dir,strlen(pw->pw_dir))){
            int len=strlen(cwd)-strlen(pw->pw_dir);
            cwd[0]='~';
            memmove(&cwd[1],&cwd[strlen(pw->pw_dir)],len+1);
            cwd[len+2]='\0';
        }
		printf("%s:%s> ",pw->pw_name,cwd);
}

/* Clear all input visually on terminal */
void clear_input_field(){
    printf("%c[2K", 27);
    printf("\r");
}

/* Deletes a singular word */
void delete_word(char *buffer){
    int last_index=strlen(buffer)-1;
    int target=last_index;
    char c=buffer[last_index];
    if (c==' '){ //ensures will still delete word even if there is trailing whitespace
        while (c==' '&&target>=0){
            c=buffer[target--];
        }
    }
    while (c!=' '&&target>=0){
        target--;
        c=buffer[target];
    }
    target++; //want to keep the space
    memset(buffer+target,'\0',strlen(buffer)-target);
}

void reposition_cursor(int i,int x){
    while (x!=i+1){
        printf("\b");
        x--;
    }
}

void swap_history(char *buffer,int flag){
    if (flag){ //up
        if (history_index>history_size)history_index=history_size;
        if (history_index==0) return;
        history_index--;
        strcpy(buffer,command_history[history_index]);
    }
    else{ //down
        history_index++;
        if (history_index>=history_size){
            memset(buffer,0,strlen(buffer));
            return;
        }
        strcpy(buffer,command_history[history_index]);
    }
}

/* Gets key presses from keyboard and stores them
   in a buffer. Returns on Enter */
int input_buffer(char *buffer,size_t size){
    int i=0;
    history_index=history_size;
    while (!key_press()){
        char c;
        c=getchar();
        if (c=='\n'){
            //buffer[i]='\0';
            printf("%c",c);
            return 0;
        }
        if (c=='\t'){
            if (buffer[strlen(buffer)-1]==' ' || i==0){
                super_tab1(buffer);
            }
            else{
                super_tab1(buffer);
            }
            clear_input_field();
            print_prompt();
            i=strlen(buffer)-1;
            printf("%s",buffer);
            reposition_cursor(i,strlen(buffer));
        }
        else{
            if (c==127){ //delete (or backspace)
                if (i==0)
                    i--;
                else{
                    memmove(&buffer[i-1],&buffer[i],strlen(buffer)-i+1);
                    clear_input_field();
                    print_prompt();
                    printf("%s",buffer);
                    i--;
                    i--;
                    reposition_cursor(i,strlen(buffer));
                }
            }
            else if (c==23){ //ctrl-w; delete a word
                delete_word(buffer);
                clear_input_field();
                print_prompt();
                i=strlen(buffer)-1;
                printf("%s",buffer);
            }
            else if (c==21){ //ctrl-u; delete all input
                memset(buffer,0,strlen(buffer));
                clear_input_field();
                print_prompt();
                i=strlen(buffer)-1;
                printf("%s",buffer);
            }
            else if (c==1){ //ctrl-a; go to start of input
                i=-1;
                clear_input_field();
                print_prompt();
                printf("%s",buffer);
                reposition_cursor(i,strlen(buffer));
            }
            else if (c==5){ //ctrl-e; go to end of input
                i=strlen(buffer)-1;
                clear_input_field();
                print_prompt();
                printf("%s",buffer);
                reposition_cursor(i,strlen(buffer));
            }
            else if (c=='\033'){ //arrow keys; each arrow key spits out a value \033[_
                getchar(); //skips the [
                c=getchar();
                switch (c){
                    case 'A': //up
                        swap_history(buffer,1);
                        i=strlen(buffer);
                        clear_input_field();
                        print_prompt();
                        printf("%s",buffer);
                        i--;
                    break;
                    case 'B': //down
                        swap_history(buffer,0);
                        i=strlen(buffer);
                        clear_input_field();
                        print_prompt();
                        printf("%s",buffer);
                        i--;
                    break;
                    case 'C': //right
                        if (i==strlen(buffer)){
                            i--;
                            break;
                        }
                        if (i<strlen(buffer)+1){
                            clear_input_field();
                            print_prompt();
                            printf("%s",buffer);
                            reposition_cursor(i,strlen(buffer));
                        }
                    break;
                    case 'D': //left
                        if (i>0){
                            printf("\b");
                            i--;
                        }
                        i--;
                    break;
                    default: //escape
                        i--;
                    break;
                }
            }
            else{
                if (i==strlen(buffer)) {
                    buffer[i]=c;
                    printf("%c",c);
                }
                else{
                    memmove(&buffer[i+1],&buffer[i],strlen(buffer)-i);
                    buffer[i]=c;
                    clear_input_field();
                    print_prompt();
                    printf("%s",buffer);
                    reposition_cursor(i,strlen(buffer));
                }
            }
        }
        i++;
    }
}

