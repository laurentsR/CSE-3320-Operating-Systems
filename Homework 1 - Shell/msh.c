/*

    Name: Ryan Laurents
    ID:   1000763099

*/

// The MIT License (MIT)
// 
// Copyright (c) 2016, 2017, 2020 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 11    // Mav shell only supports ten arguments

int main()
{
    char *cmd_str = (char*) malloc(MAX_COMMAND_SIZE);

    int pid_index = 0;
    int pid_history[15] = {0};
    bool pid_over_15 = false;

    int history_index = 0;
    char history[15][MAX_COMMAND_SIZE];
    bool history_over_15 = false;

    while(1)
    {
        // Print out the msh prompt
        printf ("msh> ");

        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while(!fgets(cmd_str, MAX_COMMAND_SIZE, stdin));

        // This will start a new line if the user enters blank input
        if(cmd_str[0] == '\n')
        {
            continue;
        }

        /* Parse input */
        char *token[MAX_NUM_ARGUMENTS];

        int   token_count = 0;                                 
                                                           
        // Pointer to point to the token
        // parsed by strsep
        char *argument_ptr;                                         
                                                           
        // We save the command history here before it gets tokenized
        strncpy(history[history_index], cmd_str, MAX_COMMAND_SIZE);
        
        // If the user enters the !n command, we must remove the !
        // before processing the number.
        if(cmd_str[0] == '!')
        {   
            // Copy over the command string without the !
            strncpy(cmd_str, &cmd_str[1], MAX_COMMAND_SIZE);

            int hist_num = -1;
            hist_num = atoi(cmd_str);

            // hist_num will equal 0 with invalid input only. Since our history
            // begins at 1, it will not cause an issue.
            if(hist_num == 0)
            {
                printf("Invalid number.\n");
                continue;
            }

            else if(hist_num > 15)
            {
                printf("Please enter a number between 1-15.\n");
                continue;
            }

            else if(hist_num > history_index && !history_over_15)
            {
                printf("Command not in history.\n");
                continue;
            }

            // We are simply copying the nth command into the command string and passing
            // it as if it was the original command entered.
            else if(history_index < 15 && !history_over_15)
            {
                strncpy(cmd_str, history[hist_num - 1], MAX_COMMAND_SIZE);
            }

            // If there are more than 15 commands entered, we need to ensure we grab the
            // correct one. We start at history_index and add the number they entered.
            // I set this up in a loop to ensure I didn't mess up the math.
            else
            {
                int hist_i = history_index;
                int i;
                for(i = 0; i < 15; i++)
                {
                    if(i == hist_num)
                    {
                        strncpy(cmd_str, history[hist_i - 1], MAX_COMMAND_SIZE);
                    }
                    hist_i++;
                    if(hist_i > 14)
                    {
                        hist_i = 0;
                    }
                }
            }
        }

        // Move this after we change the cmd_str above. If we did this before, we would get
        // a segfault/core dump
        char *working_str  = strdup(cmd_str);
        
        // Increment index after the fact to make it easier to use above
        history_index++;
        if(history_index > 14) 
        {
            history_index = 0;
            history_over_15 = true;
        }
        
        // we are going to move the working_str pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        char *working_root = working_str;

        // Tokenize the input strings with whitespace used as the delimiter
        while (((argument_ptr = strsep(&working_str, WHITESPACE)) != NULL) && (token_count<MAX_NUM_ARGUMENTS))
        {
            token[token_count] = strndup(argument_ptr, MAX_COMMAND_SIZE);
            if(strlen(token[token_count]) == 0)
            {
                token[token_count] = NULL;
            }
            token_count++;
        }

        //***********************************************
        // We will include our built-ins before the fork.
        //***********************************************

        // I decided to use strcasecmp to ensure it will still work with the wrong case
        if(strcasecmp(token[0], "quit") == 0 || strcasecmp(token[0], "exit") == 0)
        {
            exit(0);
        }

        else if(strcasecmp(token[0], "cd") == 0)
        {
            // token[1] is the intended directory to CD into
            chdir(token[1]);
        }

        else if(strcasecmp(token[0], "showpids") == 0)
        {
            // pid_over_15 is a bool that changes to true once we have more than 15 PIDs in the pid_index
            if(pid_index == 0 && !pid_over_15)
            {
                printf("There have been no processes spawned by the shell yet.\n");
            }    

            // With more than 15 PIDS we need to ensure we start printing at the correct index. Our storage
            // model is rotating over the same 15 indexes in an array, so printing it we will need to rotate
            // past the 15th slot back to the first and continue printing.
            else if(pid_over_15)
            {
                int index = pid_index;
                int i = 0;
                while(i < 15)
                {
                    printf("%d: %d\n", i + 1, pid_history[index]);
                    index++;
                    i++;
                    if(index > 14) 
                    {
                        index = 0;
                    }
                }
            }

            // Under 15 PIDS print here. We are choosing to start our numbering at 1 to match the history.
            else
            {
                int counter = 0;
                while(counter < pid_index)
                {
                    printf("%d: %d\n", counter + 1, pid_history[counter]);
                    counter++;
                }
            }       
        }

        else if(strcasecmp(token[0], "history") == 0)
        {
            if(history_over_15)
            {
                int index = history_index;
                int i;
                for(i = 0; i < 15; i++)
                {
                    printf("%d: %s", i + 1, history[index]);
                    index++;
                    if(index > 14)
                    {
                        index = 0;
                    }
                }
            }

            else 
            {
                int hist_counter = 0;
                while(hist_counter < history_index)
                {
                    printf("%d: %s", hist_counter + 1, history[hist_counter]);
                    hist_counter++;
                }
            }           
        }

        //****************************************************************
        // This is the fork/wait that will handle the rest of the commands
        //****************************************************************
        else
        {
            int ret;
            pid_history[pid_index] = fork();        

            // Only the child will execute this portion
            if(pid_history[pid_index] == 0)
            {
                ret = execvp(token[0], &token[0]);
                if(ret == -1)
                {
                    printf("%s : Command not found.\n", token[0]);    
                    // This exit will ensure you don't need multiple quit calls to exit the program.
                    exit(0);
                }                
            }

            // The parent waits here for the child to exit.
            else
            {
                int status;
                wait(&status);
            }

            pid_index++;
            if(pid_index > 14)
            {
                pid_index = 0;
                pid_over_15 = true;
            }
        }
        free(working_root);
    }
    return 0;
}