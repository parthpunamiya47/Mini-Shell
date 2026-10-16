/*
 * signals.c - Signal handling for mini-shell
 *
 * Uses sigaction() to configure robust POSIX signal handlers.
 *
 * Rules:
 *   - Ctrl+C (SIGINT): If no foreground child is running, writes a newline
 *     and allows the main REPL loop to redisplay the prompt. If a foreground
 *     child is running, the signal terminates the child, while the shell survives.
 *   - Ctrl+Z (SIGTSTP): Ignored by the shell at the prompt. If a foreground
 *     child is running, it stops the child and allows waitpid() to notice.
 *   - Ctrl+\ (SIGQUIT): Ignored by the shell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "signals.h"

static volatile pid_t foreground_pid = 0;

void set_foreground_pid(pid_t pid)
{
    foreground_pid = pid;
}

pid_t get_foreground_pid(void)
{
    return foreground_pid;
}

/*
 * sigint_handler - Handles SIGINT (Ctrl+C).
 *
 * write() is used instead of printf() because write() is async-signal-safe.
 */
static void sigint_handler(int sig)
{
    (void)sig;
    if (foreground_pid == 0) {
        /* Shell is waiting for input at the prompt */
        write(STDOUT_FILENO, "\n", 1);
    }
}


void init_signals(void)
{
    struct sigaction sa_int;
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0; /* Do not restart syscalls so fgets returns EINTR */
    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa_tstp;
    sa_tstp.sa_handler = SIG_IGN;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    /* Ignore SIGQUIT (Ctrl+\) */
    struct sigaction sa_quit;
    sa_quit.sa_handler = SIG_IGN;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    sigaction(SIGQUIT, &sa_quit, NULL);
}

void reset_child_signals(void)
{
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}
