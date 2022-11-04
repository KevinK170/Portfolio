#define _POSIX_SOURCE
#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

int foregroundMode = 0;                               //Global tracker for if foreground-only mode is on or off (0 = off, 1 = on)
int check = 0;                                        //Indicator if ^C or ^Z have been used

char* strdup(const char* str)                         //strdup implementation to get around -std=c99 not including strdup
{
    int n = strlen(str) + 1;
    char* dup = malloc(n);
    if (dup)
    {
        strcpy(dup, str);
    }
    return dup;
}

void handle_SIGINT(int signo)                         //Custom signal handler for SIGINT
{
    fprintf(stdout, "\n");
    fflush(stdout);
    check = 1;
}

void handle_SIGTSTP(int signo)                       //Custom signal handler for SIGTSTP
{
    if (foregroundMode == 0)
    {
        foregroundMode = 1;
        write(1, "\nEntering foreground-only mode (& is now ignored)\n", 51);
        check = 1;
    }
    else
    {
        foregroundMode = 0;
        write(1, "\nExiting foreground-only mode\n", 31);
        check = 1;
    }
}

char* pidConversion(int pid, const char* string)    //Function to convert all instances of $$ in a string to the current process ID
{
    char pidString[30];
    sprintf(pidString, "%d", pid);

    int pidLength = strlen(pidString);                //Calculate length of current process id
    int count = 0;
    int j = 0;
    int i;

    for (int i = 0; i < strlen(string); i++)           //Calculates number of instances of $$ to create new string
    {
        if (strstr(&string[i], "$$") == &string[i])
        {
            count++;
            i++;
        }
    }

    char* replacePid = (char*)malloc(i + count * (pidLength - 2) - 1); //New string that will contain PID

    while (*string)                                   //Rebuild the string using PID instead of $$
    {
        if (strstr(string, "$$") == string)
        {
            strcpy(&replacePid[j], pidString);           //If $$ is found in old string, insert PID in new string
            j += pidLength;
            string += 2;
        }
        else
        {
            replacePid[j++] = *string++;                 //Otherwise, continue
        }
    }

    replacePid[j] = '\0';                            //Complete new string by ending with terminator

    return replacePid;
}


void shell(char* input)
{
    int run = 1;
    int status = 0;
    int background = 0;
    char* inputHolder[2048];
    char tempHolder[512];
    pid_t spawnPid = -10;

    /*The following code about signal handling was derived straight from the explorations.*/

    struct sigaction SIGINT_action = { 0 };

    SIGINT_action.sa_handler = handle_SIGINT;
    sigfillset(&(SIGINT_action.sa_mask));
    SIGINT_action.sa_flags = 0;

    sigaction(SIGINT, &SIGINT_action, NULL);

    struct sigaction SIGTSTP_action = { 0 };

    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&(SIGTSTP_action.sa_mask));
    SIGTSTP_action.sa_flags = 0;

    sigaction(SIGTSTP, &SIGTSTP_action, NULL);



    while (run == 1)                                     //Primary program loop. This is what allows the user to enter commands
    {
        memset(inputHolder, 0, sizeof(inputHolder));
        fprintf(stdout, ": ");
        fflush(stdout);
        fgets(input, 512, stdin);                          //Gets input from the user and stores it in main input storage string

        char* temp = strtok(input, " ");                   //Parse through first argument in command (the command itself)

        int i = 0;                                         //Initialize helper strings/index counter to parse through the input

        char inputf[10000] = { 0 };
        char outputf[10000] = { 0 };                         //Holder strings for I/O redirection

        /**
         * Loop for parsing through input.
         * Reads token "temp" and stores it in string "tempHolder."
         * A duplicate of tempHolder is then created and allocated memory
         * for use in other functions, and copied to array inputHolder.
         * Index counter i is then incremeneted.
         * This process changes depending on the commands encountered.
         */

        while (temp != NULL)
        {
            if (strstr(temp, "$$") != NULL)                   //When "$$" is encountered, convert it to the PID before putting it in main input holder string
            {
                sscanf(temp, "%s", tempHolder);
                inputHolder[i++] = strdup(pidConversion(getpid(), tempHolder));
                temp = strtok(NULL, " ");
            }
            else if (strcmp(temp, "<") == 0)                  //When "<" is encountered, make a new token and put its contents into input file holder string
            {
                temp = strtok(NULL, " ");
                sscanf(temp, "%s", inputf);
                temp = strtok(NULL, " ");
            }
            else if (strcmp(temp, ">") == 0)                  //When ">" is encountered, make a new token and put its contents into output file holder string
            {
                temp = strtok(NULL, " ");
                sscanf(temp, "%s", outputf);
                temp = strtok(NULL, " ");
            }
            else                                             //Otherwise, put contents of token into main input holder string normally.
            {
                sscanf(temp, "%s", tempHolder);
                inputHolder[i] = strdup(tempHolder);
                temp = strtok(NULL, " ");
                i++;
            }
        }

        if (strcmp(inputHolder[i - 1], "&") == 0)           //If & is detected at the end of the command, set background flag and remove it from the command
        {
            inputHolder[i - 1] = NULL;
            background = 1;
        }
        else                                               //Otherwise, reset background flag and make last known index NULL    {
            inputHolder[i] = NULL;
        background = 0;
    }

    if (check == 1)                                     //If ^C or ^Z have been used, reset indicator to avoid seg fault
    {
        check = 0;
    }
    else if (inputHolder[0][0] == '#' || strcmp(inputHolder[0], "") == 0)
    {
        //Comment/Blank line detected, do nothing :)
    }
    else if (strcmp(inputHolder[0], "exit") == 0)      //If user input is "exit" command, exit the shell
    {
        run = 0;
        exit(0);
    }
    else if (strcmp(inputHolder[0], "status") == 0)   //If user input is "status" command, print exit status/terminating signal of previous foreground process
    {
        if (!WIFEXITED(status))                         //Print this message if child was not terminated normally
        {
            fprintf(stdout, "terminated by signal %i\n", status);
            fflush(stdout);
        }
        else                                           //Otherwise, print this message
        {
            fprintf(stdout, "exit value %i\n", WEXITSTATUS(status));
            fflush(stdout);
        }
    }
    else if (strcmp(inputHolder[0], "cd") == 0)      //If user input is "cd" command, change directory to either given path or HOME directory
    {
        if (i > 1)
        {
            chdir(inputHolder[1]);                      //If a path is given, change cwd to that path
        }
        else
        {
            chdir(getenv("HOME"));                      //Otherwise, change cwd to HOME directory
        }
    }
    else                                            //Otherwise, execute other command
    {
        spawnPid = fork();                            //Fork the child process

        switch (spawnPid)
        {
        case -1:                                    //If fork fails, print error
            perror("fork()\n");
            fflush(stdout);
            status = 1;
            break;

        case 0:                                     //Fork succeeds
            if (background == 0 || foregroundMode == 1)
            {
                SIGINT_action.sa_handler = SIG_DFL;
                sigaction(SIGINT, &SIGINT_action, NULL);   //Set default behavior for SIGINT if foreground-only mode is on or background flag is not set
            }

            if (inputf[0] != 0)                       //If < was detected in the command
            {
                int validate;
                if (background == 1 && strcmp(inputHolder[0], "<") == 0)
                {
                    validate = open("/dev/null/", 0);    //Redirect standard input to dev/null if background flag is set and redirection is not specified.
                    dup2(validate, 0);
                }
                else
                {
                    validate = open(inputf, O_RDONLY);   //Otherwise, redirect as normal.
                    if (validate == -1)
                    {
                        fprintf(stdout, "cannot open %s for input\n", inputf);
                        fflush(stdout);
                        _exit(1);
                    }
                    if (dup2(validate, 0) == -1)           //Print errors if file was could not be opened and/or was invalid.
                    {
                        fprintf(stderr, "ERROR: Invalid file\n");
                        fflush(stdout);
                        _exit(1);
                    }
                    close(validate);
                }
            }

            if (outputf[0] != 0)                      //If > was detected in the command
            {
                int validate;
                if (background == 1 && strcmp(inputHolder[0], ">") == 0)
                {
                    validate = open("/dev/null/", 0);     //Redirect standard output to /dev/null if background flag is set and redirection is not specified
                    dup2(validate, 1);
                }
                else
                {
                    validate = open(outputf, O_RDWR | O_CREAT | O_TRUNC, 6040); // Otherwise, redirect as normal
                    if (validate == -1)
                    {
                        fprintf(stdout, "cannot open %s for output\n", outputf);
                        fflush(stdout);
                        _exit(1);
                    }

                    if (dup2(validate, 1) == -1)
                    {
                        fprintf(stderr, "ERROR: Invalid file\n");  //Print errors if file could not be opened and/or file is invalid.
                        fflush(stdout);
                        _exit(1);
                    }
                    close(validate);
                }
            }

            if (execvp(inputHolder[0], inputHolder) < 0)  //Execute command
            {
                fprintf(stdout, "%s: no such file or directory\n", inputHolder[0]);
                fflush(stdout);
                _exit(1);
            }

            break;

        default:                                   //Child process returns to parent, contains child PID
        {
            if (background == 0 || foregroundMode == 1)
            {
                waitpid(spawnPid, &status, 0);      //If foregroundMode is on or background flag is not set, parent will wait for the child.
            }
            else                                  //Otherwise, display background PID
            {
                fprintf(stdout, "background pid is %i\n", spawnPid);
                fflush(stdout);
                usleep(200000);
            }

            break;
        }
        }
    }
    usleep(200000);
    spawnPid = waitpid(-1, &status, WNOHANG);      //Hang parent for child

    while (spawnPid > 0)                            //Inform the user that background process "PID" is completed with exit value or termination signal
    {
        fprintf(stdout, "background pid %i is done: ", spawnPid);
        fflush(stdout);

        if (!WIFEXITED(status))                         //Print this message if child was not terminated normally
        {
            fprintf(stdout, "terminated by signal %i\n", status);
            fflush(stdout);
        }
        else                                           //Otherwise, print this message
        {
            fprintf(stdout, "exit value %i\n", WEXITSTATUS(status));
            fflush(stdout);
        }

        spawnPid = waitpid(-1, &status, WNOHANG);
    }
}
}

int main()
{
    char input[2048];                                   //Initializes shell and passes empty main input storage string
    shell(input);
    return 0;
}