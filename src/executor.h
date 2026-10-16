/*
 * executor.h - Command execution for mini-shell
 */

#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

/*
 * execute_command - Run a single external command (convenience wrapper).
 */
int execute_command(Command *cmd);

/*
 * execute_pipeline - Run a full pipeline (one or more commands).
 * Handles fork(), pipe() chaining, dup2() redirections, and background execution.
 *
 * Returns the exit status of the last command in the pipeline.
 */
int execute_pipeline(Pipeline *pipeline);

/*
 * execute_builtin_with_redirection - Execute a built-in command with I/O redirections.
 * Temporarily redirects stdin, stdout, or stderr if requested, then restores them.
 *
 * Returns the exit code of the built-in command, or -1 on shell exit.
 */
int execute_builtin_with_redirection(Command *cmd);

#endif
