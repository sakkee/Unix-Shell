#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define LOGOUT 60
#define MAXNUM 40
#define MAXLEN 160


/*Auto-exit after a specific idle time*/
void sighandler(int sig)
{
	switch (sig) {
		case SIGALRM:
			printf("\nautologout\n");
			exit(0);
		default:
			break;
	}
	return;
}
/*Function to handle the piping, given as a parameter a list of original commands,
i.e. {"cat", "-b", "|", "wc", "-l"}*/
void pipeFunction(char *args[]){
    
    int pipefd[2];
	char *cmd[MAXNUM];
	int pid;
	int rounds = 0;
	int j = 0;
	int k = 0;
	/*Does 2 times the cycle, for the left and right parts of the command*/
	while (rounds<2){
		k = 0;
        /*Splits the original command list*/
		while (strcmp(args[j],"|") != 0){
			cmd[k] = args[j];
			j++;
            k++;            
			if (args[j] == NULL)
                break;
		}
        /*Adds the required NULL to the end of the temporary command list*/
		cmd[k] = NULL;
        /*Open the pipe on the first round*/
        if (rounds==0)
            pipe(pipefd);
        /*Do the dup2() and execvp() each time the pid is 0*/
		switch (pid=fork()) {
            case -1:
                perror("fork");
                return;
            case 0:
                if(rounds==0) {
                    dup2(pipefd[1],1);
                }
                else {
                    dup2(pipefd[0],0);
                }
                execvp(cmd[0],cmd);
                perror("execvp");
                exit(1);
        }
        /*Close the pipefd based on the round*/
        if (rounds==0)
            close(pipefd[1]);
        else
            close(pipefd[0]);
        /*Wait for state change*/
		waitpid(pid,NULL,0);
        j++;
		rounds++;	
	}
}

/*Function to handle the redirection, given as a parameter a list of original commands
i.e. {"ls", "-l", "-a", "-S", ">", "output.txt"} */
void redirectFunction(char *args[]) {
    /*Compile a line of the command list*/
    char string[MAXLEN];
    int j =0;
    while (args[j] != '\0') {
        j++;
    }
    int i =0;
    for (i = 0; i < j; i++)    {
        if (i>0) {
            strcat(strcat(string, " "), args[i]);
        }
        else {
            strcat(string,args[i]);
        }
        
    }
    /*Using popen(), do the redirection*/
    FILE *in;
    char buff[512];
    if(!(in = popen(string, "w"))){
        exit(1);
    }
    while(fgets(buff, sizeof(buff), in)!=NULL){
        printf("%s", buff);
    }
    pclose(in);
}

/*Function to check whether something is in the history file, returns 0 or 1 based on the finding*/
int checkIfInHistory(int command, char filePath[]) {
    static char tmp[256];
    int lineCount=0;
    int found=0;
    FILE* file;
    file = fopen(filePath, "r");
    while (fgets(tmp, sizeof(tmp)-1,file)!= NULL) {
        if (lineCount==command) {
            found=1;
            break;
        }
        lineCount++;
    }
    fclose(file);
    return found;
}
/*Gets the line count of the history file, used for history editing*/
char *getEndOfHistoryFile(char filePath[]) {
    static char tmp[256];
    static char returnNumber[6];
    int lineCount=0;
    FILE* file;
    file = fopen(filePath, "r");
    while (fgets(tmp, sizeof(tmp)-1,file)!= NULL) {
        lineCount++;
    }
    fclose(file);
    sprintf(returnNumber,"+%d", lineCount);
    return returnNumber;
}
/*Returns the command from the history file*/
const char * getCommandFromHistory(int command, char filePath[]) {
    static char tmp[256];
    int lineCount=0;
    FILE* file;
    file = fopen(filePath, "r");
    while (fgets(tmp, sizeof(tmp)-1,file)!= NULL) {
        if (lineCount==command) {
            break;
        }
        lineCount++;
    }
    fclose(file);
    int tempInt;
    sscanf(tmp, "%d %[^\n]\n",&tempInt,tmp);
    return tmp;
}
/*Function to save the history*/
void saveHistory(char command[], char filePath[]) {
    FILE* file;
    int temp,lineCount=0;
    file = fopen(filePath, "r");
    /*Checks if the history file exists and if not, creates one*/
    /*Reads the line count of the file*/
    if (file==NULL) {
        file = fopen(filePath,"w");
        fclose(file);
        file = fopen(filePath, "r");
    }
    do {
        temp = fgetc(file);
        if (temp== '\n') {
            lineCount++;
        }
    } while(temp!= EOF);
    
    fclose(file);
    /*Appends a new string, with the linecount in the beginning, to the file*/
    file = fopen(filePath,"a");
    fprintf(file, " %d %s\n", lineCount, command);
    fclose(file);
    
}
int main(void)
{
	char * cmd, line[MAXLEN], * args[MAXNUM];
	char directory[256], fileName[MAXLEN];
	int background, i, redirecting, piping;
	int pid;
	char defaultHistoryPath[256];
    
    /*Defines the history file path when the program is ran*/
    getcwd(defaultHistoryPath,sizeof(defaultHistoryPath));
    strcat(defaultHistoryPath,"/history.txt");
    
    /*Defines the timer*/
	signal(SIGALRM, sighandler);
    signal(SIGINT, sighandler);
	
	while (1) {
        
		background = 0;
        redirecting=0;
        piping = 0;
        
        /*Prints the current directory on the shell*/
		if (getcwd(directory, sizeof(directory))!=NULL) {
            /* print the prompt */
            printf("$ %s> ", directory);
		}
        else {
            printf("$ >");
        }
		/* set the timeout for alarm signal (autologout) */
		alarm(LOGOUT);
		
		/* read the users command */
		if (fgets(line,MAXLEN,stdin) == NULL) {
			printf("\nlogout\n");
			exit(0);
		}
        /*Removes the last character (\n) from the line*/
		line[strlen(line) - 1] = '\0';
		
		if (strlen(line) == 0)
			continue;
		/* start to background? */
		if (line[strlen(line)-1] == '&') {
			line[strlen(line)-1]=0;
			background = 1;
		}
        /*Case, when a command from the history is called (having ! as the first character of the line)*/
		if (line[0]=='!') {
            memmove(line,line+1,strlen(line));
            int commend = atoi(line);
            /*Checks whether the command exists and if so, copies it to the line*/
            if (checkIfInHistory(commend,defaultHistoryPath))
                strcpy(line,getCommandFromHistory(commend,defaultHistoryPath));
            else
                printf("Invalid command!\n");
        }
        
        /*Save the command to the history*/
        saveHistory(line,defaultHistoryPath);
        
		/* split the command line, check for special cases (>, < and |) */
		cmd = line;
        i = 0;
		while ( (args[i] = strtok(cmd, " ")) != NULL) {
            
            if (strcmp(args[i],">")==0){
                redirecting=1;
            }
            else if (strcmp(args[i],"<")==0) {
                /*Note! On my 64-bit Debian, I needed to take the < character off
                for the function (md5sum for example) to work. I am not sure what's the case
                on other distros, hence calling it null and replacing it with the next word in the line.
                This might need to be removed for the call (md5sum < output.txt) to work.*/
                args[i] = NULL;
                i--;
            }
            else if (strcmp(args[i],"|")==0) {
                piping = 1;
            }
            i++;
			cmd = NULL;
		}
		if (strcmp(args[0],"exit")==0) {
			exit(0);
		}
		else if (strcmp(args[0], "cd")==0) {
			if (!args[1]) {
				chdir(getenv("HOME"));
			}
			else {
				chdir(args[1]);
			}
			continue;
		}
        else if (strcmp(args[0], "history")==0) {
            /*I use nano as a default line editing tool, converting the args list to
            i.e. {"nano", "+100", "history.txt"}*/
            args[0] = "nano";
            args[1] = getEndOfHistoryFile(defaultHistoryPath);
            args[2] = defaultHistoryPath;
        }
		
        if (redirecting) {
            redirectFunction(args);
        }
        else if (piping) {
            pipeFunction(args);
        }
        else {
            /* fork to run the command */
            switch (pid = fork()) {
                case -1:
                    /* error */
                    perror("fork");
                    continue;
                case 0:
                    /* child process */
                    execvp(args[0], args);
                    perror("execvp");
                    
                    exit(1);
                    
                default:
                    /* parent (shell) */
                    if (!background) {
                        alarm(0);
                        //waitpid(pid, NULL, 0);
                        while (wait(NULL)!=pid)
                            printf("some other child process exited\n");
                    }
                    break;
            }
        }
	}
	return 0;
}

/*Sources:
Weekly coding exercises (pretty much every feature was heavily influenced by them)
Jim Glenn: http://www.cs.loyola.edu/~jglenn/702/F2004/Examples/index.html (tips on the pipe function)
Tips on the popen() used on redirecting: http://pubs.opengroup.org/onlinepubs/009695399/functions/popen.html*/

