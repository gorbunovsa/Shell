#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

enum{
    next_line=1,
    end_of_file=2,
    space_word=3,
    background_mode=4,
    foreground_mode=5,
    error=-1,
    no_redirection=-2,
};

char *amp=(char *)(-1); /*ampersand (&)*/
char *lab=(char *)(-2); /*left angle bracket (<) */
char *rab=(char *)(-3); /*right angle bracket (>) */
char *dab=(char *)(-4); /*double angle brackets (>>) */
char *ssl=(char *)(-5); /* straight slash | */
char ***ssl_syntax_error=(char ***)(-6);

struct item{
    char *word;
    struct item *next;
};

char *resize(int size, char *old)
{
    int i;
    char *new;
    size+=1;
    new=malloc(size*sizeof(*new));
    for(i=0;i<size-1;i++)
        new[i]=old[i];
    free(old);
    return new;
}

int checkForSeparator(char *str)
{
    return (str==amp||str==lab||str==rab||str==dab||str==ssl);
}

char *separator(int c)
{
    int ch;
    if(c=='&')
        return amp;
    if(c=='|')
        return ssl;
    if(c=='<')
        return lab;
    ch=getchar();
    if(ch=='>')
        return dab;
    ungetc(ch,stdin);
    return rab;
}

void newItem(struct item **p, int c)
{
    (*p)->next=malloc(sizeof(struct item));
    *p=(*p)->next;
    (*p)->word=separator(c);
    (*p)->next=NULL;
}

int getWord(struct item **p)
{
    int c,i=0,quote_flag=0,slash_flag=0;
    char *str;
    str=malloc(sizeof(*str));
    while((c=getchar())!=' ' || quote_flag){
        if (c=='&' || c=='<' || c=='>' || c=='|'){
            if(i){
                str[i]=0;
                (*p)->word=str;
                newItem(p,c);
            }else{
                (*p)->word=separator(c);
            }
            return 0;
        }
        switch(c){
            case '/':
                slash_flag=1;
                break;
            case '"':
                if(slash_flag){
                    str[i-1]=c;
                    slash_flag=0;
                }else{
                    quote_flag=!quote_flag;
                }
                continue;
            case '\n':
                if(i){
                    str[i]=0;
                    (*p)->word=str;
                }else{
                    (*p)->word=NULL;
                    free(str);
                }
                return next_line;
            case EOF:
                (*p)->word=NULL;
                free(str);
                return end_of_file;
            default:
                slash_flag=0;
        }
        str=resize(i,str);
        str[i]=c;
        i++;
    }
    if (i){
        str[i]=0;
        (*p)->word=str;
    }else{
        free(str);
        return space_word;
    }
    return 0;
}

void delList(struct item *p)
{
    struct item *tmp;
    while(p){
        tmp=p->next;
        if(!checkForSeparator(p->word))
            free(p->word);
        free(p);
        p=tmp;
    }
}

int countArgc(struct item *p)
{
    int k=0;
    while(p && p->word){
        k++;
        p=p->next;
    }
    return k;
}

char **buildArgv(struct item *p, int argc)
{
    int i=0;
    char **argv;
    if(p==NULL || argc==0){
        return NULL;
    }else{
        argv=malloc((argc+1)*sizeof(*argv));
        while(p){
            if(p->word){
                argv[i]=p->word;
                i++;
            }
            p=p->next;
        }
    }
    argv[i]=NULL;
    return argv;
}

int checkForCd(char *str)
{
    return (str[0]=='c' && str[1]=='d' && str[2]==0);
}

void run_cd(int argc, char **argv)
{
    if(argc<=2){
        if(argv[1]){
            if(checkForSeparator(argv[1])){
                fputs("cd: syntax error\n",stderr);
            }else{
                int err;
                err=chdir(argv[1]);
                if(err==-1)
                perror(argv[1]);
            }
        }else{
            fputs("cd: no arguments\n",stderr);
        }
    }else{
        fputs("cd: to many arguments\n",stderr);
    }
}

int setMode(int argc,char **argv)
{
    int i;
    for(i=0;i<argc+1;i++){
        if(argv[i]==amp){
            if(argv[i+1]){
                fputs("Incorrect '&' token use\n",stderr);
                return error;
            }
            return background_mode;
        }
    }
    return foreground_mode;

}

void prepareArgv(int argc,char **argv)
{
    int i;
    for(i=0;i<argc+1;i++)
        if(argv[i]==amp)
            argv[i]=NULL;
}

char ***buildSuperArgv(int argc, char **argv)
{
    int i=0,j=0,k=0,t;
    char ***superArgv;
    superArgv=malloc((argc+1)*sizeof(**superArgv));
    while(i<argc){
        superArgv[k]=malloc((argc+1)*sizeof(char *));
        while(argv[i]!=ssl && argv[i]!=NULL){
            superArgv[k][j]=argv[i];
            j++;
            i++;
        }
        if(i==0 || (argv[i]==ssl && (argv[i+1]==ssl || argv[i+1]==NULL))){
            for(t=0;t<=k;t++)
                free(superArgv[t]);
            free(superArgv);
            fputs("Syntax error\n",stderr);
            return ssl_syntax_error;
        }
        superArgv[k][j]=NULL;
        k++;
        i++;
        j=0;
    }
    superArgv[k]=NULL;
    return superArgv;
}

void delSuperArgv(char ***superArgv)
{
    int i=0;
    while(superArgv[i]){
        free(superArgv[i]);
        i++;
    }
    free(superArgv);
}

int redirectOutput(int argc, char ***argv)
{
    int i=0,j=0,fd,k=0;
    char **new_argv;
    char *file,*sep=NULL;
    new_argv=malloc((argc+1)*sizeof(*new_argv));
    while((*argv)[i]){
        if((*argv)[i]==rab || (*argv)[i]==dab){
            if((*argv)[i+1]==NULL || checkForSeparator((*argv)[i+1])){
                fputs("Syntax error\n",stderr);
                return error;
            }
            k++;
            if(k>1){
                fputs("Too many redirections\n",stderr);
                return error;
            }
            file=(*argv)[i+1];
            sep=(*argv)[i];
            i+=2;
        }else{
            new_argv[j]=(*argv)[i];
            i++;
            j++;
        }
    }
    new_argv[j]=NULL;
    if(sep==NULL){
        free(new_argv);
        return no_redirection;
    }
    free(*argv);
    *argv=new_argv;
    if(sep==rab)
        fd=open(file,O_WRONLY|O_CREAT|O_TRUNC,0666);
    else
        fd=open(file,O_WRONLY|O_CREAT|O_APPEND,0666);
    if(fd==-1){
        perror(file);
        return error;
    }
    return fd;
}

int redirectInput(int argc, char ***argv)
{
    int i=0,j=0,fd,k=0;
    char **new_argv;
    char *file,*sep=NULL;
    new_argv=malloc((argc+1)*sizeof(*new_argv));
    while((*argv)[i]){
        if((*argv)[i]==lab){
            if(!(*argv)[i+1] || checkForSeparator((*argv)[i+1])){
                fputs("Syntax error\n",stderr);
                return error;
            }
            k++;
            if(k>1){
                fputs("Too many redirections\n",stderr);
                return error;
            }
            file=(*argv)[i+1];
            sep=(*argv)[i];
            i+=2;
        }else{
            new_argv[j]=(*argv)[i];
            i++;
            j++;
        }
    }
    new_argv[j]=NULL;
    if(sep==NULL){
        free(new_argv);
        return no_redirection;
    }
    free(*argv);
    *argv=new_argv;
    fd=open(file,O_RDONLY);
    if(fd==-1){
        perror(file);
        return error;
    }
    return fd;
}

int countSuperArgc(char ***superArgv)
{
    int i=0;
    while(superArgv[i])
        i++;
    return i;
}

int *makePipe(char ***superArgv, int fd0, int fd1)
{
    int i,prev_out,superArgc;
    int fd[2];
    int *pid_arr;
    superArgc=countSuperArgc(superArgv);
    pid_arr=malloc(superArgc*sizeof(int));
    for(i=0;i<superArgc;i++){
        if(superArgv[i+1])
            pipe(fd);
        pid_arr[i]=fork();
        if(pid_arr[i]==0){/*child*/
            if(superArgv[i+1]){
                close(fd[0]);
                dup2(fd[1],1);
                close(fd[1]);
            }else{
                if(fd1!=no_redirection)
                    dup2(fd1,1);
            }
            if(fd1!=no_redirection)
                    close(fd1);
            if(i==0){
                if(fd0!=no_redirection)
                    dup2(fd0,0);
            }else{
                dup2(prev_out,0);
                close(prev_out);
            }
            if(fd0!=no_redirection)
                close(fd0);
            execvp(superArgv[i][0],superArgv[i]);
            perror(superArgv[i][0]);
            exit(1);
        }
        if(i!=0)
            close(prev_out);
        if(superArgv[i+1]){
            prev_out=fd[0];
            close(fd[1]);
        }
    }
    if(fd0!=no_redirection)
        close(fd0);
    if(fd1!=no_redirection)
        close(fd1);
    return pid_arr;
}

void separatorAnalyzer(int argc, char **argv)
{
    int *pid_arr;
    int amp_mode,fd0,fd1,i;
    char ***superArgv;
    if(!argv)
        return;
    if(!checkForSeparator(argv[0]) && checkForCd(argv[0])){
        run_cd(argc,argv);
        return;
    }
    for(i=0;i<argc;i++){
        if(!checkForSeparator(argv[i]) && checkForCd(argv[i])){
            fputs("cd: syntax error\n",stderr);
            return;
        }
    }
    amp_mode=setMode(argc,argv);
    prepareArgv(argc,argv);
    fd0=redirectInput(argc,&argv);
    fd1=redirectOutput(argc,&argv);
    if(fd0==error || fd1==error || amp_mode==error)
        return;
    for(argc=0;argv[argc];argc++){
    }
    superArgv=buildSuperArgv(argc,argv);
    if(superArgv==ssl_syntax_error)
        return;
    pid_arr=makePipe(superArgv,fd0,fd1);
    if(amp_mode==foreground_mode)
        for(i=0;superArgv[i];i++)
            waitpid(pid_arr[i],NULL,0);
    free(pid_arr);
    delSuperArgv(superArgv);
}

void zombieCleaner()
{
    int w;
    while ((w=waitpid(-1,NULL,WNOHANG))&&w!=-1){
    }
}

int main()
{
    int gw=0,argc;
    char **argv;
    struct item *first=NULL,*last=NULL;
    printf("Enter command:\n");
    for(;;){
        if (gw==space_word){
            gw=getWord(&last);
        }else{
            if(first)
                last->next=malloc(sizeof(struct item));
            else
                first=malloc(sizeof(struct item));
            last=((last)? last->next:first);
            gw=getWord(&last);
            last->next=NULL;
        }
        if(gw==next_line){
            argc=countArgc(first);
            argv=buildArgv(first,argc);
            separatorAnalyzer(argc,argv);
            zombieCleaner();
            delList(first);
            first=NULL;
            last=NULL;
            printf("Enter command:\n");
        }else{
            if(gw==end_of_file){
                delList(first);
                printf("\n");
                break;
            }
        }
    }
    return 0;
}
