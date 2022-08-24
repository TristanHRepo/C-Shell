#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 2048
#define MAX_ARGS 512
#define UNUSED(x) (void)(x)

int foreground_only = 0;

// prototypes
void cd_com(char **args, int *status);
void status_com(char **args, int *status);
void exit_com(char **args, int *status);

void handle_SIGTSTP(int signo){
    
    // check if not in foreground only mode already
    if (foreground_only == 0){
        char* message = "Entering foreground-only mode (& is now ignored)\n";
	    write(STDOUT_FILENO, message, strlen(message));
        foreground_only = 1;
    } else if (foreground_only == 1){
        char* message = "Exiting foreground-only mode\n";
	    write(STDOUT_FILENO, message, strlen(message));
        foreground_only = 0;
    }

}

// A struct to hold function names and functions
struct built_com{
    char *command;
    void (*func)(char **args, int *status);
};

// struct array that holds each function
struct built_com built_coms[] = {
    {"cd", cd_com},
    {"status", status_com},
    {"exit", exit_com},
};

char* read_placeholder(){

    char *command = malloc(MAX_LINE_LENGTH * sizeof(char));   

    // get command from stdin
    fgets(command, MAX_LINE_LENGTH, stdin);
    command[strlen(command) - 1] = '\0';

    if (feof(stdin)) exit(0);

    // Setup strings needed
    char ph[] = "$$";

    // check if there is something to replace in the command
    char *check;
    check = strstr(command, ph);

    if (check) {
        // Get pid as a string
        int pid = getpid();
        char *pid_str = malloc(sizeof(long long) + 1);
        sprintf(pid_str, "%d", pid);

        // Get length of each string for dynamic mem allocation
        int command_len = strlen(command);
        int ph_len = strlen(ph);
        int pid_str_len = strlen(pid_str);

        // calculate difference between ph and pid length
        int difference = pid_str_len - ph_len;

        int i = 0;
        int num_ph = 0;                 // number of times the placeholder is in the argument
        while (i < command_len) {     
            // check for substring from the incremented position and compare with the substring. Allows us to go past finding first occurance.     
            if (strstr(&command[i], ph) == &command[i]) {             
                num_ph++;
                i += ph_len;            // increment index past the placeholder to look for another placeholder                 
            } else {
                i++;
            }
        }

        // calculate new str length
        int new_command_len = command_len + (difference * num_ph);

        // allocate memory for new str
        char *new_command = malloc((new_command_len + 1) * sizeof(char));

        // create new str with placeholders filled in
        i = 0;
        int j = 0;                      // will keep up with our position in the new line
        while (i < command_len) {
            if (strstr(&command[i], ph) == &command[i]) {                               
                strcpy(&new_command[j], pid_str);
                i += ph_len;
                j += pid_str_len;
            } else {
                new_command[j] = command[i];
                i++;
                j++;
            }
        }
        new_command[j] = '\0';

        free (command);
        free (pid_str);
        return new_command;
    } else {
        return command;
    }
}

char** parse_command(char *command, char** inputf, char** outputf, int *bg){

    char **args = calloc(MAX_ARGS, sizeof(char*));
    
    // make copy of command line input since strtok destroys string
    char commandcpy[MAX_LINE_LENGTH];
    strcpy(commandcpy, command);

    // start breaking up input
    char *word = strtok(commandcpy, " \t\r\n");

    // count arguments
    int count = 0;

    // add tokens to array and count arguments
    while (word != NULL) {

        // if input file operator found, break loop
        if (strcmp(word, "<") == 0) {
            break;
        // if output file operator found, break loop
        } else if (strcmp(word, ">") == 0) {
            break;
        // check if this is an end of file &
        } else if (strcmp(word, "&") == 0) {
            char *curr_word; 
            curr_word = word;
            // get next token
            word = strtok(NULL, " \n");
            // check if next token is end of line
            if (word == NULL) {
                // otherwise, this is end & and indicates a bg process

                // check if in foreground only mode
                if (foreground_only != 1){
                    *bg = 1;
                }
                break;
            } else {
                // if not end, continue as normal
                args[count] = curr_word;
                count ++;
                continue;
            }
        }

        args[count] = word;
        count++;

        word = strtok(NULL, " \n");

    }

    // get rest of command if is present
    while (word != NULL) {
        if (strcmp(word, "<") == 0) {
            word = strtok(NULL, " \n");
            *inputf = word;
            word = strtok(NULL, " \n");
        } else if (strcmp(word, ">") == 0) {
            word = strtok(NULL, " \n");
            *outputf = word;
            word = strtok(NULL, " \n");
        } else if (strcmp(word, "&") == 0){
            // check if in foreground only mode
            if (foreground_only != 1){
                *bg = 1;
            }
            word = strtok(NULL, " \n");
        }
    }

    return args;
}

int check_builtin(char **args, int *stat){

    // check if any command was given in first place
    if (args[0] == NULL) {
        return 0;
    }

    /*
    This method was  found here: 
    https://stackoverflow.com/questions/5309859/how-to-define-an-array-of-functions-in-c 
    
    Iterates over the functions and checks if the command matches any of the built in function
    names.
    */

    for (int i = 0; i < 3; i++) {
        if (strcmp(args[0], built_coms[i].command) == 0) {
            built_coms[i].func(args, stat);
            return 0;
        }
    }

    return 1;
}   

int main(){

    int status = 0;
    int bg_pids[5];
    for (int i=0; i < 5; i++){
        bg_pids[i] = -1;
    }

    // change signal handlers
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    while (1) {
        
        // check for any background processes that may have terminated
        for (int j=0; j < 5; j++){
            if (bg_pids[j] == -1) {
                continue;
            }
            int bg_stat;
            if(waitpid(bg_pids[j], &bg_stat, WNOHANG) > 0) {
                printf("background pid %d is done: ", bg_pids[j]);
                fflush(stdout);
                bg_pids[j] = -1;
                if(WIFEXITED(bg_stat)){
                    printf("exit value %d\n", WEXITSTATUS(bg_stat));
                    fflush(stdout);
                } else{
                    printf("terminated by signal %d\n", WTERMSIG(bg_stat));
                    fflush(stdout);
                }
            }
        }

        //prompt user
        printf(": ");
        fflush(stdout);

        // setup variables     
        char *inputf = NULL;
        char *outputf = NULL;
        int bg = 0;

        // this function reads in command line and replaces placeholders
        char *command = read_placeholder();

        // check if input is a comment
        if (command[0] == '#') {
            continue;
        }

        // this function parses the command and looks for special characters
        char **args = parse_command(command, &inputf, &outputf, &bg);
        
        // calls check_builtin function. If builtin command is passed in, then returns 0. Else will open exec functions
        if (check_builtin(args, &status)) {

            // create child process
            pid_t childPid = fork();
            
            if (childPid == -1) {
                perror("fork() failed\n");
            
            } else if (childPid == 0 && bg == 0) {
                // Execute as a foreground process

                // change signal handlers
                struct sigaction default_action = {0}, ignore_action = {0};
                default_action.sa_handler = SIG_DFL;
                ignore_action.sa_handler = SIG_IGN;
                sigaction(SIGINT, &default_action, NULL);
                sigaction(SIGTSTP, &ignore_action, NULL);

                // check if a redirect of stdout has been called
                if (outputf != NULL) {
                    // open output file. If it does not exist, create it
                    int output = open(outputf, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    // error check if open worked
                    if (output == -1) { 
                        perror("output open() failed"); 
                        exit(1); 
                    }
                    // redirect output to designated file
                    int check_out = dup2(output, 1);
                    // error check if dup2() function call worked
                    if (check_out == -1) { 
                        perror("redirect out dup2() failed"); 
                        exit(2); 
                    }
                    fcntl(output, F_SETFD, FD_CLOEXEC);
                }

                // check if a redirect of stdin has been called
                if (inputf != NULL) {
                    // open input file.
                    int input = open(inputf, O_RDONLY);
                    // error check if open worked
                    if (input == -1) { 
                        perror("input open() failed"); 
                        exit(1); 
                    }
                    // redirect input to designated file
                    int check_in = dup2(input, 0);
                    // error check if dup2() function call worked
                    if (check_in == -1) { 
                        perror("redirect in dup2() failed"); 
                        exit(2); 
                    }
                    fcntl(input, F_SETFD, FD_CLOEXEC);
                }

                // execute command
                execvp(args[0], args);
                perror("execvp error");
                // terminate
                exit(1);
            
            } else if (childPid == 0 && bg == 1) { 
                // Execute as a background process

                // change signal handlers
                struct sigaction ignore_action = {0};
                ignore_action.sa_handler = SIG_IGN;
                sigaction(SIGINT, &ignore_action, NULL);
                sigaction(SIGTSTP, &ignore_action, NULL);

                // check if a redirect of stdout has been called
                if (outputf != NULL) {
                    // open output file. If it does not exist, create it
                    int output = open(outputf, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    // error check if open worked
                    if (output == -1) { 
                        perror("output open() failed"); 
                        exit(1); 
                    }
                    // redirect output to designated file
                    int check_out = dup2(output, 1);
                    // error check if dup2() function call worked
                    if (check_out == -1) { 
                        perror("redirect out dup2() failed"); 
                        exit(2); 
                    }
                    fcntl(output, F_SETFD, FD_CLOEXEC);
                } else {
                    // open /dev/null
                    int devnull_out = open("/dev/null", O_WRONLY);
                    // error check if open worked
                    if (devnull_out == -1) { 
                        perror("output open() failed"); 
                        exit(1); 
                    }
                    // redirect output to /dev/null
                    int check_out = dup2(devnull_out, 1);
                    // error check if dup2() function call worked
                    if (check_out == -1) { 
                        perror("redirect out dup2() failed"); 
                        exit(2); 
                    }
                }

                // check if a redirect of stdin has been called
                if (inputf != NULL) {
                    // open input file.
                    int input = open(inputf, O_RDONLY);
                    // error check if open worked
                    if (input == -1) { 
                        perror("input open() failed"); 
                        exit(1); 
                    }
                    // redirect input to designated file
                    int check_in = dup2(input, 0);
                    // error check if dup2() function call worked
                    if (check_in == -1) { 
                        perror("redirect in dup2() failed"); 
                        exit(2); 
                    }
                    fcntl(input, F_SETFD, FD_CLOEXEC);
                } else {
                    // open /dev/null
                    int devnull_in = open("/dev/null", O_RDONLY);
                    // error check if open worked
                    if (devnull_in == -1) { 
                        perror("input open() failed"); 
                        exit(1); 
                    }
                    // redirect input to /dev/null
                    int check_in = dup2(devnull_in, 0);
                    // error check if dup2() function call worked
                    if (check_in == -1) { 
                        perror("redirect in dup2() failed"); 
                        exit(2); 
                    }
                }

                // execute command
                execvp(args[0], args);
                perror("execvp error");
                // terminate
                exit(1);

            } else {
                struct sigaction ignore_action = {0};
                ignore_action.sa_handler = SIG_IGN;
                sigaction(SIGINT, &ignore_action, NULL);

                // check if a background process was executed
                if (bg == 1){
                    int stat;
                    // print bg process
                    printf("background pid is %d\n", childPid);
                    fflush(stdout);
                    // store in array at an empty spot
                    for (int k=0; k < 5; k++){
                        if (bg_pids[k] == -1) {
                            bg_pids[k] = childPid;
                            break;
                        }
                    }
                    // WNOHANG process
                    childPid = waitpid(childPid, &stat, WNOHANG);
                } else {
                    childPid = waitpid(childPid, &status, 0);
                    if(WIFSIGNALED(status)){
                        printf("terminated by signal %d\n", WTERMSIG(status));
                        fflush(stdout);
                        }   
                }
             }
        }
        
        free(args);
        args = NULL;
        free(command);
        command = NULL;
    }
    return 0;
}

void cd_com(char **args, int *status){

    UNUSED(status);

    // move to home directory
    if (args[1] == NULL) {
        char *home = getenv("HOME");
        chdir(home);
    // move to specified path
    } else {
        if (chdir(args[1]) != 0){
            perror("cd");
        }
    }

}

void status_com(char **args, int *status){
    
    UNUSED(args);

    if(WIFEXITED(*status)){
        printf("exit value %d\n", WEXITSTATUS(*status));
    } else{
        printf("terminated by signal %d\n", WTERMSIG(*status));
    }

}

void exit_com(char **args, int *status){
    UNUSED(status);
    UNUSED(args);
    exit(0);
}
