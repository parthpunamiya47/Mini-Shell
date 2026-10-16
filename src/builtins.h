/*
 * builtins.h - Built-in shell commands
 *
 * Built-in commands run inside the shell process itself (no fork).
 * This is necessary for commands like "cd" which must modify the
 * shell's own state — a child process can't change the parent's
 * working directory.
 */

#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

/*
 * is_builtin - Check if a command name is a built-in.
 * Returns 1 if it is, 0 if it's an external command.
 */
int is_builtin(const char *name);

/*
 * run_builtin - Execute a built-in command.
 * Returns the exit status (0 = success), or -1 if "exit" was called
 * (signals main loop to quit).
 */
int run_builtin(Command *cmd);

#endif
