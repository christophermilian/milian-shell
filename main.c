/*
Made by Christopher Milian, 2026
*/

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define MSH_RL_BUFSIZE 1024
#define MSH_TOK_BUFSIZE 64
#define MSH_TOK_DELIM " \t\r\n\a"
#define MSH_VERSION "0.3.0"
#define MSH_YEAR "2026"

#define BUILTIN_LIST \
    X(cd)            \
    X(help)          \
    X(exit)

// Forward declarations — auto-generated
#define X(name) int msh_##name(char **args);
BUILTIN_LIST
#undef X

// Registration table — auto-generated
typedef struct {
    char *name;
    int (*func)(char **);
} builtin_t;

#define X(name) {#name, msh_##name},
builtin_t builtins[] = {
    BUILTIN_LIST
};
#undef X

/**
 * Returns the number of builtin commands.
 * Uses sizeof trick to count entries in builtin_str array
 * without hardcoding the count.
 */
int msh_num_builtins() {
    return sizeof(builtins) / sizeof(builtin_t);
}

/**
 * Builtin: Change directory.
 * Uses chdir() syscall to change the process's working directory.
 * Prints error if no argument given or if chdir fails (bad path).
 * Returns 1 to keep the shell loop running.
 */
int msh_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "msh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("msh");
    }
  }
  return 1;
}

/**
 * Builtin: Print help info.
 * Lists all available builtin commands.
 * Returns 1 to keep the shell loop running.
 */
int msh_help(char **args)
{
  int i;
  printf("Christopher Milian's msh v%s, %s\n", MSH_VERSION, MSH_YEAR);
  printf("The following are built in:\n");

  for (i = 0; i < msh_num_builtins(); i++) {
    printf("  %s\n", builtins[i].name);
  }
  return 1;
}

/**
 * Builtin: Exit the shell.
 * Returns 0, which signals msh_loop() to stop.
 */
int msh_exit(char **args)
{
  return 0;
}

/**
 * Launch an external program in a child process.
 * 
 * fork() creates a child process (copy of parent).
 * - Child (pid == 0): calls execvp() to replace itself with the
 *   requested program. If execvp fails, the command wasn't found.
 * - Parent (pid > 0): waits for the child to finish using waitpid().
 *   WUNTRACED lets us catch stopped processes (Ctrl+Z).
 *   Loop continues until child exits or is killed by a signal.
 * - pid < 0: fork itself failed (out of memory, process limit, etc.)
 * 
 * Returns 1 to keep the shell loop running.
 */
int msh_launch(char **args)
{
  pid_t pid, wpid;
  int status;

  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("msh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("msh");
  } else {
    // Parent process
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
 * Execute a piped command (e.g., "ls | grep .c").
 * 
 * Creates a pipe connecting two child processes.
 * Child 1 runs left_args with stdout redirected to the pipe write end.
 * Child 2 runs right_args with stdin redirected to the pipe read end.
 * Parent closes both pipe ends and waits for both children to finish.
 * 
 * Returns 1 to keep the shell loop running.
 */
int msh_execute_pipe(char **left_args, char **right_args) {
    // pipe logic here
    int fd[2];
    pipe(fd);

    // Fork first child (ls)
    if (fork() == 0) {
        //duplicate file descriptor
        dup2(fd[1], STDOUT_FILENO);  // stdout → pipe write end
        close(fd[0]);                 // don't need read end
        close(fd[1]);                 // already duped
        // execvp takes the array directly, which is what left_args has
        if (execvp(left_args[0], left_args) == -1) {
          perror("msh");
        }
        exit(EXIT_FAILURE);
    }

    // Fork second child (grep)
    if (fork() == 0) {
        //duplicate file descriptor
        dup2(fd[0], STDIN_FILENO);   // stdin → pipe read end
        close(fd[1]);                 // don't need write end
        close(fd[0]);                 // already duped
        // execvp takes the array directly, which is what right_args has
        if (execvp(right_args[0], right_args) == -1) {
          perror("msh");
        }
        exit(EXIT_FAILURE);
    }

    // Parent closes both ends and waits
    close(fd[0]);
    close(fd[1]);
    wait(NULL);
    wait(NULL);

    return 1;  // keep shell running
}

/**
 * Execute a command — either builtin or external.
 * 
 * Checks if args[0] matches any builtin command name.
 * If found, calls the corresponding builtin function via
 * the function pointer array. Otherwise, falls through
 * to msh_launch() to run it as an external program.
 * 
 * Returns 1 to continue, 0 to exit (only from msh_exit).
 */
int msh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  // Check for pipe
  for (i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], "|") == 0) {
      // Found a pipe — split into left and right
      // ex: [ls, |, grep, doggy]
      // terminate left side. kill the pipe element itself with NULL
      // ex: [ls, NULL, grep, doggy]
      args[i] = NULL;

      // create new pointer that points to start of args array
      // ex: [ls, NULL, ...]
      char **left_args = args; 

      // create new pointer that points to second command element of args array
      // ex: [..., grep, doggy]
      char **right_args = &args[i + 1];
      return msh_execute_pipe(left_args, right_args);
    }
  }

  for (i = 0; i < msh_num_builtins(); i++) {
    if (strcmp(args[0], builtins[i].name) == 0) {
        return builtins[i].func(args);
    }
  }

  return msh_launch(args);
}

/**
 * Split a line into tokens (words) using strtok().
 * Returns an array of strings.
 * 
 * Allocates a buffer of char* pointers. strtok() splits
 * the input string on whitespace delimiters (space, tab,
 * newline, etc). Each token pointer is stored in the array.
 * 
 * If we run out of space, realloc doubles the buffer.
 * The array is NULL-terminated (required by execvp).
 * 
 * Caller must free the returned pointer array.
 */
char **msh_split_line(char *line)
{
  int bufsize = MSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "msh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, MSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += MSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "msh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, MSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

/**
 * Read a line of input from stdin, character by character.
 * 
 * Allocates a buffer and grows it with realloc as needed.
 * Reads until newline or EOF is hit. Returns the line as
 * a null-terminated string.
 * 
 * Note: getline() could replace this, but this manual
 * approach teaches buffer management with malloc/realloc.
 * 
 * Caller must free the returned buffer.
 */
char *msh_read_line(void)
{
  int bufsize = MSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "msh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    c = getchar();

    if (c == EOF || c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    if (position >= bufsize) {
      bufsize += MSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "msh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

/**
 * Main REPL (Read-Eval-Print Loop).
 * 
 * Prints prompt, reads a line, splits it into tokens,
 * executes the command, then frees memory. Repeats
 * until msh_execute returns 0 (from "exit" command).
 * 
 * This is the core loop that makes it a shell.
 */
void msh_loop(void)
{
  /*
  char     → one character: 'h'
  char *   → pointer to chars (a string): "hello"
  char **  → pointer to strings (array of strings): ["ls", "-la", "/home"]
  */
  char *line;
  char **args;
  char cwd[PATH_MAX];
  int status;

  do {
    // Get the current working directory and output it on the CLI line
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        char *dir = strrchr(cwd, '/');
        printf("-> %s > ", dir ? dir + 1 : cwd);
    } else {
        printf("-> unknown_path > ");
    }

    line = msh_read_line();
    // splits what user types into an array of args
    args = msh_split_line(line);
    status = msh_execute(args);

    free(line);
    free(args);
  } while (status);
}

/**
 * Main entry point.
 * Starts the shell loop. Placeholder spots for
 * loading config files and cleanup on shutdown.
 */
int main(int argc, char **argv)
{
  printf("Christopher Milian's msh v%s, %s\n", MSH_VERSION, MSH_YEAR);
  msh_loop();
  return EXIT_SUCCESS;
}
