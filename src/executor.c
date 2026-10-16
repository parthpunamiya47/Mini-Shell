/*
 * executor.c - Command execution for mini-shell
 *
 * Handles single commands, redirections, multi-stage pipelines,
 * background execution, built-in redirections, and signal integration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include "executor.h"
#include "jobs.h"
#include "signals.h"
#include "builtins.h"

/* ------------------------------------------------------------------ */
/*  Redirection setup                                                  */
/* ------------------------------------------------------------------ */

static int setup_redirections(Command *cmd)
{
    int fd;

    if (cmd->input_file != NULL) {
        fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "mini-shell: %s: %s\n", cmd->input_file, strerror(errno));
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->output_file != NULL) {
        if (cmd->append_output)
            fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        else
            fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "mini-shell: %s: %s\n", cmd->output_file, strerror(errno));
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (cmd->error_file != NULL) {
        fd = open(cmd->error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "mini-shell: %s: %s\n", cmd->error_file, strerror(errno));
            return -1;
        }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Built-in command with redirection                                  */
/* ------------------------------------------------------------------ */

int execute_builtin_with_redirection(Command *cmd)
{
    int saved_stdin = -1;
    int saved_stdout = -1;
    int saved_stderr = -1;
    int ret;

    /* Save original file descriptors if redirection is requested */
    if (cmd->input_file != NULL)
        saved_stdin = dup(STDIN_FILENO);
    if (cmd->output_file != NULL)
        saved_stdout = dup(STDOUT_FILENO);
    if (cmd->error_file != NULL)
        saved_stderr = dup(STDERR_FILENO);

    /* Apply redirections in the shell process */
    if (setup_redirections(cmd) != 0) {
        if (saved_stdin != -1)  { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); }
        if (saved_stdout != -1) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
        if (saved_stderr != -1) { dup2(saved_stderr, STDERR_FILENO); close(saved_stderr); }
        return 1;
    }

    /* Execute the built-in */
    ret = run_builtin(cmd);

    /* Flush stdio buffers before restoring file descriptors */
    fflush(stdout);
    fflush(stderr);

    /* Restore original file descriptors */
    if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    if (saved_stdout != -1) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (saved_stderr != -1) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }

    return ret;
}

/* ------------------------------------------------------------------ */
/*  Single command execution (convenience wrapper)                     */
/* ------------------------------------------------------------------ */

int execute_command(Command *cmd)
{
    Pipeline pipeline;
    pipeline.stages[0] = *cmd;
    pipeline.num_stages = 1;
    pipeline.is_background = 0;
    pipeline.raw_command[0] = '\0';
    return execute_pipeline(&pipeline);
}

/* ------------------------------------------------------------------ */
/*  Pipeline execution                                                 */
/* ------------------------------------------------------------------ */

int execute_pipeline(Pipeline *pipeline)
{
    int n = pipeline->num_stages;
    int i;
    int pipe_fd[2];         /* current pipe: [0]=read, [1]=write */
    int prev_read_fd = -1;  /* read end of the previous pipe */
    pid_t pids[MAX_PIPE_STAGES];
    int status;

    if (n == 0)
        return 0;

    for (i = 0; i < n; i++) {
        /* Create pipe for all stages except the last one */
        if (i < n - 1) {
            if (pipe(pipe_fd) < 0) {
                perror("mini-shell: pipe");
                if (prev_read_fd != -1)
                    close(prev_read_fd);
                for (int k = 0; k < i; k++)
                    waitpid(pids[k], NULL, 0);
                return -1;
            }
        }

        pids[i] = fork();
        if (pids[i] < 0) {
            perror("mini-shell: fork");
            if (i < n - 1) {
                close(pipe_fd[0]);
                close(pipe_fd[1]);
            }
            if (prev_read_fd != -1)
                close(prev_read_fd);
            for (int k = 0; k < i; k++)
                waitpid(pids[k], NULL, 0);
            return -1;
        }

        if (pids[i] == 0) {
            /* --- Child process for stage i --- */

            /* Restore default signal handlers for child process */
            reset_child_signals();

            /* Connect stdin to previous pipe's read end */
            if (prev_read_fd != -1) {
                dup2(prev_read_fd, STDIN_FILENO);
                close(prev_read_fd);
            }

            /* Connect stdout to current pipe's write end */
            if (i < n - 1) {
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
                close(pipe_fd[0]);
            }

            /* Apply file redirections */
            if (setup_redirections(&pipeline->stages[i]) != 0)
                exit(EXIT_FAILURE);

            /* If the command in this stage is a built-in, run it directly in child */
            if (is_builtin(pipeline->stages[i].args[0])) {
                int ret = run_builtin(&pipeline->stages[i]);
                exit(ret < 0 ? 0 : ret);
            }

            execvp(pipeline->stages[i].args[0], pipeline->stages[i].args);

            if (errno == ENOENT) {
                fprintf(stderr, "mini-shell: %s: command not found\n",
                        pipeline->stages[i].args[0]);
                exit(127);
            } else {
                perror(pipeline->stages[i].args[0]);
                exit(126);
            }
        }

        /* --- Parent process --- */

        /* Close read end from previous pipe stage */
        if (prev_read_fd != -1)
            close(prev_read_fd);

        /* Close write end and save read end for next stage */
        if (i < n - 1) {
            close(pipe_fd[1]);
            prev_read_fd = pipe_fd[0];
        }
    }

    /* If running in the background, register job and do NOT wait */
    if (pipeline->is_background) {
        int jid = add_job(pids[n - 1], pipeline->raw_command);
        if (jid != -1) {
            printf("[%d] %d\n", jid, (int)pids[n - 1]);
        }
        return 0;
    }

    /* Foreground execution: track foreground PID so signals are handled properly */
    set_foreground_pid(pids[n - 1]);

    int last_status = 0;
    int was_stopped = 0;

    for (i = 0; i < n; i++) {
        if (waitpid(pids[i], &status, WUNTRACED) == -1) {
            if (errno != ECHILD)
                perror("mini-shell: waitpid");
        }

        if (WIFSTOPPED(status)) {
            was_stopped = 1;
        }

        if (i == n - 1) {
            if (WIFEXITED(status))
                last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                last_status = 128 + WTERMSIG(status);
            else
                last_status = -1;
        }
    }

    /* If the process was stopped (e.g. by Ctrl+Z), register it in the job table */
    if (was_stopped) {
        int jid = add_job(pids[n - 1], pipeline->raw_command);
        Job *job = get_job(jid);
        if (job)
            job->status = JOB_STOPPED;
        printf("\n[%d]  Stopped\t\t%s\n", jid, pipeline->raw_command);
    }

    /* Reset foreground tracking after children finish or stop */
    set_foreground_pid(0);

    return last_status;
}
