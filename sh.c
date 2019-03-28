#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "sh.h"
#include <glob.h>
#include "linked_list.h"
#include <utmpx.h>
#include <assert.h>

#define BUFFER_SIZE 1000
#define MAX_COMMAND_HISTORY 999
#define MAX_ALIAS 10
//void run_command(int , char** , char*, int , char** );

void handle_sigchild(int sig) {
    while (waitpid((pid_t) (-1), 0, WNOHANG) > 0) {}
}

char *commands[] = {
    "exit",
    "which",
    "where",
    "cd",
    "pwd",
    "list",
    "pid",
    "kill",
    "prompt",
    "printenv",
    "setenv"
};

pthread_mutex_t lock;

    int uid, i, status, argsct, go = 1;
    struct passwd *password_entry;
    char *homedir;
    struct pathelement *pathlist;

        char *command, *arg, *commandpath, *p, *pwd, *owd, *cwd;
            char *prompt_prefix = NULL;
            int len;
    char buf[BUFFER_SIZE];

    pthread_t watchuser_threadid;

int sh(int argc, char **argv, char **envp) {
    
    prompt_prefix = (char *) malloc(0);
    char *prompt = calloc(PROMPTMAX, sizeof(char));
    char **args = calloc(MAXARGS, sizeof(char *));

    uid = getuid();
   password_entry = getpwuid(uid);
   homedir = password_entry->pw_dir;

    if ((pwd = getcwd(buf, BUFFER_SIZE + 1)) == NULL) {
        perror("getcwd");
        exit(2);
    }

    cwd = calloc(strlen(pwd) + 1, sizeof(char));
    owd = calloc(strlen(pwd) + 1, sizeof(char));
    memcpy(owd, pwd, strlen(pwd));
    memcpy(cwd, pwd, strlen(pwd));
    prompt[0] = ' ';
    prompt[1] = '\0';

    pathlist = get_path();

    int len;
    char *string_input;

    sigignore(SIGINT);
    sigignore(SIGTERM);
    sigignore(SIGTSTP);
    signal(SIGCHLD, handle_sigchild);

    while (go) {

        for (int j = 0; j < MAXARGS; j++) {
            args[j] = NULL;
        }

        printf("%s[%s] >>", prompt_prefix, cwd);

        fgets(buf, BUFFER_SIZE, stdin);
        len = (int) strlen(buf);

        if (len >= 2) {
            buf[len - 1] = '\0';
            string_input = (char *) malloc(len);
            char *string_input_alias_find = (char *) malloc(len);
            strcpy(string_input, buf);
            strcpy(string_input_alias_find, buf);

            char * pch;
            char * command = "";
            pch = strtok (buf," ");
            int args_len = 0;
            while (pch != NULL)
            {
                if(command == ""){
                    command = pch;
                }
                args[args_len] = (char *) malloc(len);
                args[args_len] = pch;
                args_len++;
                pch = strtok (NULL, " ");
                args[args_len] = (char *) malloc(len);
                //strcpy(BUFFER, );
                //strcpy(args[args_len], BUFFER);
            }
          /*  if (alias_find_result != NULL) {
                //We found it in our alias table, so we have to replace it.
                char *alias_token = strtok(alias_find_result, " ");

                //Fill up arg array with alias substitution
                //Preshifts argumets if we have a token
                while (alias_token) {
                    len = (int) strlen(alias_token);
                    args[args_len] = (char *) malloc(len);
                    strcpy(BUFFER, alias_token);
                    strcpy(args[args_len], BUFFER);
                    alias_token = strtok(NULL, " ");
                    args_len++;
                }

                free(alias_token);
            }*/

            int piped = 0;
            int pipeindex = -1;
            int pipe_error = 0;

            for(int i = 0; i < args_len; i++){
                if(strcmp(args[i], "|") == 0 || strcmp(args[i], "|&") == 0){
                    if(strcmp(args[i], "|&") == 0){
                        pipe_error = 1;
                    }
                    piped = 1;
                    pipeindex = i;
                    break;
                }
            }

            char** piped_args = NULL;
            int piped_arg_nums = 0;

            int pfds[2];
            int first_child = -1;
            int second_child = -1;
            int command_index = 99;
            int flag = 0;

          //  for (command_index = 0; command_index < command_count; ++command_index) {
          //      if (strcmp(args[0], command_strings[command_index]) == 0) {
          //          break;
          //      }
          //  }
            
            for(int ind = 0; ind < 11; ind++){
                if(strcmp(commands[ind], command) == 0){
                    command_index = ind;
                }
            }

            if(piped){
                char** piped_args = calloc(MAXARGS, sizeof(char *));

                for(int i = 0; i < MAXARGS; i++){
                    piped_args[i] = NULL;
                }
                
                free(args[pipeindex]);

                for(int i = pipeindex+1; i < MAXARGS; i++){
                    if(args[i] != NULL){
                        piped_args[i-pipeindex-1] = args[i];
                        piped_arg_nums++;
                    }else{
                        break;
                    }
                }

                for(int j = pipeindex; j < MAXARGS; j++) {
                    if(args[j] == NULL){
                        break;
                    }
                    args[j] = NULL;
                    args_len--;
                }

                pipe(pfds);
                first_child = fork();

                if(first_child == 0){
                    if(pipe_error){
                        close(STDERR_FILENO);
                        dup(pfds[1]);
                    }

                    close(STDOUT_FILENO);
                    dup(pfds[1]);
                    close(pfds[0]);



                    run_command(command_index, args, pathlist, args_len, envp);
                    //execute_external(args, pathlist, args_len, envp, 0);

                    exit(0);
                }else{
                    //We are parent
                    second_child = fork();
                    //printf("%i\n",second_child);

                    if(second_child == 0){

                        //Left of pipe
                        //printf("SECOND CHILD!\n");
                        close(STDIN_FILENO);
                        dup(pfds[0]);
                        close(pfds[1]);
                        
                       // execute_external(piped_args, pathlist, piped_arg_nums, envp, 1);

                        exit(0);
                    }else{
                        close(pfds[0]);
                        close(pfds[1]);
                        int c_status;
                        waitpid(second_child, &c_status, 0);

                        for (int j = 0; j < MAXARGS; j++) {
                            if(piped_args[j] != NULL){
                                free(piped_args[j]);
                            }
                        }

                        free(piped_args);
                    }
                }
               
            }
            
            if(piped == 0){
                run_command(command_index, args, pathlist, args_len, envp);
            }
        }
    }
    
    free(prompt);
    free(owd);
    free(cwd);
    free(args);
    free(prompt_prefix);

    struct pathelement *current;
    current = pathlist;
    free(current->element);

    while (current != NULL) {
        free(current);
        current = current->next;
    }

    return 0;
} /* sh() */

void run_command(int command_index, char** args, char* pathlist, int args_len, char** envp){
    switch (command_index) {
        //Exit the shell
        case 0: // exit
            go = 0;
            break;
        case 1: // which
            if (args_len == 1) {
                printf("%s", "which: Too few arguments.\n");
            } else {
                //Iterate though all following args
                //Print out more than one if it exits
                for (int i = 1; i < args_len; i++) {
                    if (args[i] != NULL) {
                        char *result = which(args[i], pathlist);
                        if (result != NULL) {
                            printf("%s\n", result);
                            free(result);
                        } else {
                            printf("%s not found\n", args[i]);
                        }
                    } else {
                        break;
                    }
                }
            }
            
            printf("I am getting to this line...");
            break;
        case 2: // where
            if (args[1] == NULL) {
                printf("%s", "where: Too few arguments.\n");
            } else {
                //Iterate though all following args
                //Print out more than one if it exits
                for (int i = 1; i < args_len; i++) {
                    if (args[i] != NULL) {
                        char *result = where(args[i], pathlist);
                        if (result != NULL) {
                            printf("%s\n", result);
                            free(result);
                        } else {
                            printf("%s not found\n", args[i]);
                        }
                    } else {
                        break;
                    }
                }
            }
            break;
        case 3: // cd
            //Change Directories
            printf("");
            char *cd_path = args[1];

            if (args_len > 2) {
                perror("cd: Too many arguments");
            } else {
                //If we have one arg change to the home directory
                if (args_len == 1) {
                    cd_path = homedir;
                    //Else change to the directory we give it
                } else if (args_len == 2) {
                    cd_path = args[1];
                }


                //In theory we just want to swap the owd with the cwd
                //But lots of code for swapping two strings because everyone loves C
                if ((pwd = getcwd(buf, BUFFER_SIZE + 1)) == NULL) {
                    perror("getcwd");
                    exit(2);
                }

                if (cd_path[0] == '-') {
                    if (chdir(owd) < 0) {
                        printf("Invalid Directory: %d\n", errno);
                    } else {
                        free(cwd);
                        cwd = malloc((int) strlen(owd));
                        strcpy(cwd, owd);


                        free(owd);
                        owd = malloc((int) strlen(buf));
                        strcpy(owd, buf);
                    }
                } else {
                    if (chdir(cd_path) < 0) {
                        printf("Invalid Directory: %d\n", errno);
                    } else {
                        free(owd);
                        owd = malloc((int) strlen(buf));
                        strcpy(owd, buf);

                        if ((pwd = getcwd(buf, BUFFER_SIZE + 1)) == NULL) {
                            perror("getcwd");
                            exit(2);
                        }

                        free(cwd);
                        cwd = malloc((int) strlen(buf));
                        strcpy(cwd, buf);
                    }
                }
            }
            break;
        case 4: // pwd
            //Print working directory
            printf("%s\n", cwd);
            break;
        case 5: // list
            //List current directory for one arg
            if (args_len == 1) {
                list(cwd);
                //List all of them for the others
            } else {
                for (int i = 1; i < MAXARGS; i++) {
                    if (args[i] != NULL) {
                        printf("[%s]:\n", args[i]);
                        list(args[i]);
                    }
                }
            }
            break;
        case 6: // pid
            printf("");
            int pid = getpid();
            printf("%d\n", pid);
            break;
        case 7: // kill
            if (args_len == 3) {
                char *pid_str = args[2];
                char *signal_str = args[1];

                char *end;
                long pid_num;
                long sig_num;

                //using strtol because it supports error catching
                pid_num = strtol(pid_str, &end, 10);
                //converting pid
                if (end == pid_str) {
                    printf("%s\n", "Cannot convert string to number");
                }
                //get rid of the - flag
                signal_str[0] = ' ';
                sig_num = strtol(signal_str, &end, 10);

                if (end == signal_str) {
                    printf("%s\n", "Cannot convert string to number");
                }

                int id = (int) pid_num;
                int sig = (int) sig_num;
                kill(id, sig_num);
                //If its two args just send the default SIGTERM
            } else if (args_len == 2) {
                char *pid_str = args[1];
                char *end;
                long num;
                num = strtol(pid_str, &end, 10);
                if (end == pid_str) {
                    printf("%s\n", "Cannot convert string to number");
                }
                int id = (int) num;
                kill(id, SIGTERM);
            } else {
                printf("%s\n", "kill: Incorrect amount of arguments");
            }
            break;
        case 8: // prompt
            //Switch the prompt to a new string
            free(prompt_prefix);
            if (args_len == 1) {
                fgets(buf, BUFFER_SIZE, stdin);
                len = (int) strlen(buf);
                buf[len - 1] = '\0';
                prompt_prefix = (char *) malloc(len);
                strcpy(prompt_prefix, buf);
            } else if (args_len == 2) {
                prompt_prefix = (char *) malloc(strlen(args[1]));
                strcpy(prompt_prefix, args[1]);
            }
            break;
        case 9: // printenv
            //Print our enviornment
            printenv(args_len, envp, args);
            break;
        case 10: // setenv
            //Print env if zero args
            if (args_len == 1) {
                printenv(args_len, envp, args);
            } else if (args_len == 2) {
                //Set to empty
                setenv(args[1], "", 1);
            } else if (args_len == 3) {
                //Reset vars
                setenv(args[1], args[2], 1);

                //special care for home and path
                if (strcmp(args[1], "HOME") == 0) {
                    homedir = getenv("HOME");
                } else if (strcmp(args[1], "PATH") == 0) {
                    pathlist = get_path();
                }
            } else {
                printf("%s\n", "setenv: Incorrect amount of arguments");
            }
            break;
        default:
             printf("Command [%s] does not exist!\n", command);
            //Assume user wants to run an actual command
            
           // execute_external(args, pathlist, args_len, envp, supress_output);
            //free(cmd_path);
    }
}

//Print enviornment variables
void printenv(int args_len, char **envp, char **args) {
    if (args_len == 1) {
        int i = 0;
        while (envp[i] != NULL) {
            printf("%s\n", envp[i]);
            i++;
        }
    } else if (args_len == 2) {
        char *env_str = getenv(args[1]);
        if (env_str != NULL) {
            printf("%s\n", env_str);
        }
    }
}


char *which(char *command, struct pathelement *pathlist) {
    /* loop through pathlist until finding command and return it.  Return
    NULL when not found. */

    //A buffer for concatting strings
    char CAT_BUFFER[BUFFER_SIZE];
    struct pathelement *current = pathlist;

    DIR *dr;
    struct dirent *de;

    //Go though all our PATH_ENVs
    while (current != NULL) {

        char *path = current->element;

        //vars for looking though the directories
        dr = opendir(path);

        if(dr){

        //in each path, look at all of it's files
            while ((de = readdir(dr)) != NULL) {


                //for each file in the directory, check if it's the one we want
                if (strcmp(de->d_name, command) == 0) {
                    //cat together the full filename
                    strcpy(CAT_BUFFER, path);
                    strcat(CAT_BUFFER, "/");
                    strcat(CAT_BUFFER, de->d_name);

                    //create a string pointer and return it
                    int len = (int) strlen(CAT_BUFFER);
                    char *p = (char *) malloc(len);
                    strcpy(p, CAT_BUFFER);

                    closedir(dr);

                    return p;
                }
            }
        }
        closedir(dr);
        current = current->next;
    }


    //Return null if we haven't found one 
    return NULL;

}

char *where(char *command, struct pathelement *pathlist) {
    char CAT_BUFFER[BUFFER_SIZE];
    struct pathelement *current = pathlist;

    DIR *dr;
    struct dirent *de;
    strcpy(CAT_BUFFER, "");

    //Go though all our PATH_ENVs
    while (current != NULL) {

        char *path = current->element;

        //vars for looking though the directories
        dr = opendir(path);
        // printf("Dir closed");

        if(dr){
        //in each path, look at all of it's files
            while ((de = readdir(dr)) != NULL) {


                //for each file in the directory, check if it's the one we want
                if (strcmp(de->d_name, command) == 0) {
                    //If it is add it to the buffer
                    strcat(CAT_BUFFER, path);
                    strcat(CAT_BUFFER, "/");
                    strcat(CAT_BUFFER, de->d_name);
                    strcat(CAT_BUFFER, "\n");
                }
            }

        }
        closedir(dr);

        current = current->next;
    }

    int len = (int) strlen(CAT_BUFFER);
    char *p = (char *) malloc(len);

    //replace last '\n' with null terminator
    CAT_BUFFER[len - 1] = '\0';
    strcpy(p, CAT_BUFFER);

    return p;
} /* where() */


//List all files in given dir
void list(char *dir) {

    DIR *dr;
    struct dirent *de;
    dr = opendir(dir);
    if (dr == NULL) {
        printf("Cannot open %s\n", dir);
    } else {
        while ((de = readdir(dr)) != NULL) {
            printf("%s\n", de->d_name);
        }
    }


    closedir(dr);
} /* list() */

