#include <termios.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <dirent.h>
#include <pwd.h>

struct passwd *pw; //current pw
char *current_dir[]={ ".", NULL };
char *binaries[]={ "/bin/", "/usr/bin/", "/usr/local/bin/", NULL };
char *binaries_cwd[]={ "/bin/", "/usr/bin/", "/usr/local/bin/", ".", NULL };
void super_tab2(char *buffer, char **dirs, int all);

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

//int get_cursor_pos(int *cx, unsigned short *tx) {
//    char buf[30]={0};
//    int ret, i, pow;
//    char ch;
//    *cx = 0;
//    struct termios term, restore;
//    tcgetattr(0, &term);
//    tcgetattr(0, &restore);
//    term.c_lflag &= ~(ICANON|ECHO);
//    tcsetattr(0, TCSANOW, &term);
//    write(1, "\033[6n", 4);
//    for( i = 0, ch = 0; ch != 'R'; i++ )
//    {
//        ret = read(0, &ch, 1);
//        if ( !ret ) {
//            fprintf(stderr, "getpos: error reading response!\n");
//            return 1;
//        }
//    buf[i] = ch;
//    }
//    for( i -= 2, pow = 1; buf[i] != ';'; i--, pow *= 10)
//        *cx = *cx + ( buf[i] - '0' ) * pow;
//    tcsetattr(0, TCSANOW, &restore);
//
//
//    struct winsize w;
//    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
//    *tx=w.ws_col;
//    return 0;  // make sure your main returns int
//}

void super_tab1(char *buffer){
    int last_index=strlen(buffer)-1;
    int target=last_index;
    char c=buffer[target];
    while (c!=' ' && target!=0){
        c=buffer[target--];
    }
    if (strlen(buffer)==0){ //tab on nothing
        return;
    }
    if (target==0){ //is pressing tab on the first word in command
        super_tab2(buffer,binaries,0);
        return;
    }
    else{ //is pressing tab on last word of multi word command
        target+=2;
        if (buffer[strlen(buffer)-1]==' '){ //ends on a space -> print cwd
            super_tab2(buffer,current_dir,1);
            return;
        }
        char *sub_buffer=buffer+target;
        super_tab2(sub_buffer,current_dir,0);
        return;
    }

}


void super_tab2(char *buffer, char **dirs, int all){
    char **result=malloc(sizeof(char*)*20);
    int size=0;
    int dir_count=0;
    while (dirs[dir_count]){ //while directory list is not NULL
        DIR *d;
        d=opendir(dirs[dir_count]);
        struct dirent *dir;
        if (all){ //print all items
            while ((dir=readdir(d))!=NULL){ //while there is still items in directory to read
                if ( in_array(result,dir->d_name,size)==0 ) continue;
                result[size++]=strdup(dir->d_name);
                if (size%20==0){
                    if ( (result=realloc(result,sizeof(char*)*(size*2))) == NULL ){
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
                    if (size%20==0){
                        if ( (result=realloc(result,sizeof(char*)*(size*2))) == NULL ){
                            fprintf(stderr,"\nerror on realloc\n");
                            exit(-1);
                        }
                    }
                }
            }
        }
        dir_count++;
    }

    qsort( result, size, sizeof(char*), compare_stings); //sort array
    int j;
    if (size==1){
        strcpy(buffer,result[0]);
        return;
    }
    printf("\n");
    for (j=0;j<size;j++){
        printf("%s  ",result[j]);
        free(result[j]);
    }
    printf("\n");
    free(result);

}

/* Prints the shell prompt, including user and current directory */
void print_prompt(){
        pw=getpwuid(getuid());
        char cwd[128];
        getcwd(cwd,128);
        char *cmp=malloc(sizeof(char)*strlen(pw->pw_dir)+1);
        strncpy(cmp,cwd,strlen(pw->pw_dir));
        cmp[strlen(pw->pw_dir)]='\0';
        if ( !strcmp( pw->pw_dir,  cmp) ){
            cwd[0]='~';
            strcpy(cwd+1,cwd+strlen(pw->pw_dir));
        }
		printf("%s:%s> ",pw->pw_name,cwd);
        free(cmp);
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

/* Gets key presses from keyboard and stores them
   in a buffer. Returns on Enter */
int input_buffer(char *buffer,size_t size){
    int i=0;
    while (!key_press()){
        char c;
        c=getchar();
        if (c=='\n'){
            printf("%c",c);
            return 0;
        }
        if (c=='\t'){
            if (buffer[strlen(buffer)-1]==' ' || i==0){
                super_tab1(buffer);
            }
            else{
                char *x[]={
                    ".",
                    NULL
                };
                super_tab1(buffer);
            }
            clear_input_field();
            print_prompt();
            printf("%s",buffer);
            i=strlen(buffer)-1;
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
                        i--;
                        break;
                    case 'B': //down
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

