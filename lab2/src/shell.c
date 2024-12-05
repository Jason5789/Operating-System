#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p){
	if (p->in_file) {
        int fd_in = open(p->in_file, O_RDONLY);
        if (fd_in == -1) {
            perror("Error opening input file");
            return; // Handle error appropriately (e.g., return an error code)
        }
        // Duplicate file descriptor for stdin
        if (dup2(fd_in, STDIN_FILENO) == -1) {
            perror("Error redirecting stdin");
            close(fd_in);
            return; // Handle error appropriately
        }
        close(fd_in); // Close the original fd after duplicating
    } else if (p->in != 0) {
        // If there's a pipe input, duplicate it to stdin
        if (dup2(p->in, STDIN_FILENO) == -1) {
            perror("Error redirecting stdin from pipe");
            return; // Handle error appropriately
        }
        close(p->in); // Close the pipe fd after duplicating
    }

    // Redirect standard output if out_file is specified
    if (p->out_file) {
        int fd_out = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out == -1) {
            perror("Error opening output file");
            return; // Handle error appropriately
        }
        // Duplicate file descriptor for stdout
        if (dup2(fd_out, STDOUT_FILENO) == -1) {
            perror("Error redirecting stdout");
            close(fd_out);
            return; // Handle error appropriately
        }
        close(fd_out); // Close the original fd after duplicating
    } else if (p->out != 1) {
        // If there's a pipe output, duplicate it to stdout
        if (dup2(p->out, STDOUT_FILENO) == -1) {
            perror("Error redirecting stdout to pipe");
            return; // Handle error appropriately
        }
        close(p->out); // Close the pipe fd after duplicating
    }
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{
  	pid_t pid;
    int status;

    if ((pid = fork()) == 0) {
        if (p->in_file) {
            int fd_in = open(p->in_file, O_RDONLY);
            dup2(fd_in, 0);
            close(fd_in);
        } else if (p->in != 0) {
            dup2(p->in, 0);
            close(p->in);
        }
        if (p->out_file) {
            int fd_out = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd_out, 1);
            close(fd_out);
        } else if (p->out != 1) {
            dup2(p->out, 1);
            close(p->out);
        }


        if (execvp(p->args[0], p->args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        perror("fork");
        return -1;
    } else {
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return status;
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
	int status = 0;
    int in = 0;  // Initial input for the first command (stdin)
    int fd[2];   // File descriptors for pipes
    struct cmd_node *temp = cmd->head;

    while (temp != NULL) {
        // If there's a next command, create a pipe
        if (temp->next != NULL) {
            if (pipe(fd) == -1) {
                perror("pipe");
                return -1;
            }
            temp->out = fd[1];  // Set the output to the write end of the pipe
            temp->next->in = fd[0];  // Set the next command's input to the read end of the pipe
        } else {
            temp->out = 1;  // For the last command, output is set to stdout
        }

        // Spawn process for current command node
        if ((status = spawn_proc(temp)) == -1) {
            return -1;  // If spawn_proc fails, return -1
        }

        // Close the used pipe ends
        if (temp->in != 0) {
            close(temp->in);
        }
        if (temp->out != 1) {
            close(temp->out);
        }

        // Move to the next command node
        temp = temp->next;
    }

    return status;
}
// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
