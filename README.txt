# C Shell
This is a implementation of a C shell for the UNIX 

This includes the following features in the shell:

## Command Prompt
Users are prompted for input by the ':' character
## Comment and Blank Line handling
All comments (signaled by a leading '#' character) and blank lines are handled by the shell 
## Variable expansion 
Any instance of '$$' is changed into the process ID of the shell
## Coded implementations of exit, cd, and status.
   - exit: exits the shell
   - cd: no arguments passed will return to the home environment variable. Otherwise, will change to path of the directory to be changed to given an argument
   - status: prints to stdout the exit status or the terminating signal of the last foreground process ran by the shell
## Basic linux command handling
If a command is not built-in to the shell, then commands are handled by exec() family functions
## Input/Output redirection
input file redirection via stdin and output file redirection vbia stdout
## Foreground and background process handling 
Foreground proceses are executed by simply typing in the command. A background proces is signaled by & at the end of a command.
## Custom signal behavior for the SIGINT(ctrl-c) and SIGTSTP (ctrl-z)
    - SIGINT: Signals foreground child processes to terminate themselves upon reciept of signal.
    - SIGTSTP: Shell process enters a state where background process cannot be created (& sign is ignored). Can be reverted by sending the signal                     again

Compile using the following in a unix system with the gcc compiler:

gcc -std=gnu99 -o smallsh Shell.c
