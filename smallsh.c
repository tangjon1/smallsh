#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>

char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub, int first_instance_only);
void getInput(char **arg_array, int *arg_count, int *exit_fg_value, char *pid_bg_value);
void runCommand(char **arg_array, int *arg_count, int *exit_fg_value, char *pid_bg_value); 

// SIGINT handler that does nothing
void handle_SIGINT(int signo) {}

int main(void) {
  // Set up signal handlers for SIGTSTP and SIGINT
  struct sigaction SIGTSTP_action = {0};
  SIGTSTP_action.sa_handler = SIG_IGN;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);
  
  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = handle_SIGINT;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;
  sigaction(SIGINT, &SIGINT_action, NULL);

  // Track foreground exit value and background process PID
  int exit_fg_value = 0;
  char pid_bg_value[32] = {'\0'};

  // Ready shell for user input
  int arg_count;
  char *arg_array[512];
  for (;;) {
    // Reset the argument array for new input
    arg_count = 0;
    memset(arg_array, '\0', 512);
    
    // Catch SIGINT while receiving input
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Receive user input to populate argument array
    getInput(arg_array, &arg_count, &exit_fg_value, pid_bg_value);
    if (arg_count < 1) {
      continue;
    }

    // Run the command using the provided arguments 
    runCommand(arg_array, &arg_count, &exit_fg_value, pid_bg_value);
    
    // Ignore SIGINT until next input
    SIGINT_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_action, NULL);
  }
  return 0;
}

void getInput(char **arg_array, int *arg_count, int *exit_fg_value, char *pid_bg_value) {
  char *word;

  char *line_buffer[4096];
  size_t line_len;

  char sub[2048];
  int comment_word_exists = 0;
  int comment_word_index = 0;

  // Prompt using the current PS1 enviornment variable value
  if (getenv("PS1") == NULL) {
    printf("");
  } else {
    printf("%s", getenv("PS1"));
  }

  // Receive user input
  if (getline(line_buffer, &line_len, stdin) == -1) {
    fprintf(stderr, "getline() error");
  };
  
  // Find the environment delimiter variable to use
  char *delim = getenv("IFS");
  if (delim == NULL) {
    delim = " \t\n"; 
  }

  // Separate the input into individual strings 
  word = strtok(*line_buffer, delim);
  while (word != NULL) {
    arg_array[*arg_count] = strdup(word);
    *arg_count += 1;
    word = strtok(NULL, delim); 
  }
  
  int i;
  for (i = 0; i < *arg_count; i++) {
    char *current_word = arg_array[i];
    // Expand the '~/' characters into the environment home variable value
    if (strlen(current_word) > 1) {
      if ((current_word[0] == '~') && (current_word[1] == '/')) {
        sprintf(sub, "%s", getenv("HOME")); 
        str_gsub(&(current_word), "~", sub, 1);
      }
    }
    // Expand the '$$' characters to the PID
    sprintf(sub, "%d", getpid());
    str_gsub(&(current_word), "$$", sub, 0);

    // Expand the '$?' characters to the foreground exit value
    sprintf(sub, "%d", *exit_fg_value);
    str_gsub(&(current_word), "$?", sub, 0);

    // Expand the '$!' characters to the background PID
    sub[0] = *pid_bg_value;
    str_gsub(&(current_word), "$!", sub, 0);

    // Check for '#' comments in the arguments
    if ((strlen(current_word) == 1) && (*current_word == '#')) {
      comment_word_exists = 1; 
      comment_word_index = i;
    }
  }
  // Ignore the '#' character and all subsequent arguments after it
  if (comment_word_exists == 1) {
    arg_array[comment_word_index] = NULL;
    *arg_count = comment_word_index;
  }
}

void runCommand(char **arg_array, int *arg_count, int *exit_fg_value, char *pid_bg_value) {
  int is_background = 0;
  
  int check_infile = 0;
  int check_outfile = 0;

  int child_status;

  // Run built-in 'exit' command, if applicable
  if (strcmp(arg_array[0], "exit") == 0) {
    // No additional arguments: exit with the current foreground exit value
    if (*arg_count == 1) {
      fprintf(stderr, "\nexit\n");
      exit(*exit_fg_value);
    }
    // One argument: exit with the specified value of the argument
    if (*arg_count < 3) {
      if (strcmp(arg_array[1], "0") == 0) {
        fprintf(stderr, "\nexit\n");
        *exit_fg_value = 0;
        exit(0);
      }
      int exit_val;
      exit_val = atoi(arg_array[1]);
      if (exit_val != 0) {
        fprintf(stderr, "\nexit\n");
        *exit_fg_value = exit_val;
        exit(exit_val);
      }
    } else {
      fprintf(stderr, "'exit' argument error");
      return;
    }
  }

  // Run built-in 'cd' command, if applicable
  if (strcmp(arg_array[0], "cd") == 0) {
    if (*arg_count > 2) {
      fprintf(stderr, "'cd' argument error");
      return;
    // chdir to the specified directory
    } else if (*arg_count == 2) {
      if (chdir(arg_array[1]) != 0) {
        fprintf(stderr, "'cd' argument error");
        return;
      }
    // chdir to the directory specified by the "HOME" environment variable
    } else if (*arg_count == 1) {
      if (chdir(getenv("HOME")) != 0) {
        fprintf(stderr, "'cd' argument error");
        return;
      }
    }
    return;
  }

  // Check for the background operator as the last argument
  if ((strlen(arg_array[*arg_count - 1]) == 1) && (arg_array[*arg_count - 1][0] == '&')) {
  is_background = 1;
    arg_array[*arg_count - 1] = NULL;
    *arg_count -= 1;
    if (*arg_count < 1) {
      exit(1);
    }
  }

  // fork a new process
  pid_t spawn_pid = fork();
  switch (spawn_pid) {
    case -1:
      fprintf(stderr, "fork() error");
      return;
      break;
    case 0:
      // Detect '<' and '>' characters to redirect stdin and stdout using dup2
      for (int i = 0; i < 2; i++) {
        if ((check_infile == 0) && (*arg_count > 1) && (strlen(arg_array[*arg_count - 2]) == 1) && (arg_array[*arg_count - 2][0] == '<')) {
          int sourceFD = open(arg_array[*arg_count - 1], O_RDONLY);
          if (sourceFD == -1) {
            fprintf(stderr, "open() infile error");
            exit(1);
          }  
          int result = dup2(sourceFD, 0);
          if (result == -1) {
            fprintf(stderr, "dup2() infile error");
            exit(1);
          }
          close(sourceFD);
          arg_array[*arg_count - 2] = NULL;
          *arg_count -= 2;
          if (*arg_count < 1) {
            exit(1);
          }
          check_infile = 1;
        }

        if ((check_outfile == 0) && (*arg_count > 1) && (strlen(arg_array[*arg_count - 2]) == 1) && (arg_array[*arg_count - 2][0] == '>')) {
          int targetFD = open(arg_array[*arg_count - 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
          if (targetFD == -1) {
            fprintf(stderr, "open() outfile error");
            exit(1);
          }
          int result = dup2(targetFD, 1);
          if (result == -1) {
            fprintf(stderr, "dup2() outfile error");
            exit(1);
          }
          close(targetFD);
          arg_array[*arg_count - 2] = NULL;
          *arg_count -= 2;
          if (*arg_count < 1) {
            exit(1);
          }
          check_outfile = 1;
        }
      }

      // Pass the remaining arguments to execvp for the shell to execute
      if (execvp(arg_array[0], arg_array) == -1) {
        fprintf(stderr, "command not recognized: %s\n", arg_array[0]);
        exit(1);
      }
      break;
    default:
      if (is_background == 1) {
        // Set the background PID value
        sprintf(pid_bg_value, "%d", spawn_pid);
      } else {
        // For foreground commands, wait for the child process to exit
        waitpid(spawn_pid, &child_status, 0); 
        if (WIFEXITED(child_status)) {
          *exit_fg_value = WEXITSTATUS(child_status);
        }
       
        while ( spawn_pid != -1 ) {
          // Wait for remaining child processes
          spawn_pid = waitpid(-1, &child_status, WNOHANG | WUNTRACED);
          if (spawn_pid > 0 && WIFEXITED(child_status)) {
            fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) spawn_pid, WEXITSTATUS(child_status));
          } else if (spawn_pid > 0 && WIFSIGNALED(child_status)) {
            fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) spawn_pid, 128 + WTERMSIG(child_status));
          }         
        }
      }
      break;
  }   
}

