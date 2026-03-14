/*
TODOs:
• 0.1.0 — basic REPL with builtins
• 0.2.0 — add pwd, prompt improvements
• 0.3.0 — pipes
*/

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define MSH_VERSION "0.1.0"
#define MSH_YEAR "2026"

#define BUILTIN_LIST \
    X(cd)            \
    X(help)          \
    X(exit)

// Forward declarations — auto-generated
#define X(name) int lsh_##name(char **args);
BUILTIN_LIST
#undef X

// Registration table — auto-generated
typedef struct {
    char *name;
    int (*func)(char **);
} builtin_t;

#define X(name) {#name, lsh_##name},
builtin_t builtins[] = {
    BUILTIN_LIST
};
#undef X

/**
 * Returns the number of builtin commands.
 * Uses sizeof trick to count entries in builtin_str array
 * without hardcoding the count.
 */
int lsh_num_builtins() {
    return sizeof(builtins) / sizeof(builtin_t);
}

/**
 * Builtin: Change directory.
 * Uses chdir() syscall to change the process's working directory.
 * Prints error if no argument given or if chdir fails (bad path).
 * Returns 1 to keep the shell loop running.
 */
int lsh_cd(char **args)
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
int lsh_help(char **args)
{
  int i;
  printf("Christopher Milian's msh v%s, %s\n", MSH_VERSION, MSH_YEAR);
  printf("The following are built in:\n");

  for (i = 0; i < lsh_num_builtins(); i++) {
    printf("  %s\n", builtins[i].name);
  }
  return 1;
}

/**
 * Builtin: Exit the shell.
 * Returns 0, which signals lsh_loop() to stop.
 */
int lsh_exit(char **args)
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
int lsh_launch(char **args)
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
 * Execute a command — either builtin or external.
 * 
 * Checks if args[0] matches any builtin command name.
 * If found, calls the corresponding builtin function via
 * the function pointer array. Otherwise, falls through
 * to lsh_launch() to run it as an external program.
 * 
 * Returns 1 to continue, 0 to exit (only from lsh_exit).
 */
int lsh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(args[0], builtins[i].name) == 0) {
        return builtins[i].func(args);
    }
  }

  return lsh_launch(args);
}

/**
 * Split a line into tokens (words) using strtok().
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
char **lsh_split_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "msh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "msh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_TOK_DELIM);
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
char *lsh_read_line(void)
{
  int bufsize = LSH_RL_BUFSIZE;
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
      bufsize += LSH_RL_BUFSIZE;
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
 * until lsh_execute returns 0 (from "exit" command).
 * 
 * This is the core loop that makes it a shell.
 */
void lsh_loop(void)
{
  char *line;
  char **args;
  int status;

  do {
    printf("> ");
    line = lsh_read_line();
    args = lsh_split_line(line);
    status = lsh_execute(args);

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
  lsh_loop();
  return EXIT_SUCCESS;
}
