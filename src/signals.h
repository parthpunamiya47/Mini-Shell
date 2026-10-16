/*
 * signals.h - Signal handling for mini-shell
 *
 * Handles SIGINT (Ctrl+C) and SIGTSTP (Ctrl+Z) so the shell does not
 * terminate or stop unintentionally, while allowing child processes
 * in the foreground to receive these signals.
 */

#ifndef SIGNALS_H
#define SIGNALS_H

#include <sys/types.h>

/*
 * init_signals - Configure signal handlers for the shell process.
 */
void init_signals(void);

/*
 * reset_child_signals - Restore default signal actions for child processes.
 * Called in the child process immediately after fork() before execvp().
 */
void reset_child_signals(void);

/*
 * Foreground process tracking.
 * Allows the signal handlers to know whether a child is currently running.
 */
void set_foreground_pid(pid_t pid);
pid_t get_foreground_pid(void);

#endif
