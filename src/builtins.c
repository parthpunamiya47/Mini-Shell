/*
 * builtins.c - Built-in shell commands
 *
 * Each built-in is a small function. The dispatcher run_builtin()
 * checks the command name and calls the right one.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <sys/wait.h>
#include "builtins.h"
#include "jobs.h"
#include "signals.h"
#include "history.h"

extern char **environ;

/* Stores the previous working directory for "cd -" */
static char prev_cwd[1024] = "";

/* ------------------------------------------------------------------ */
/*  Helper functions                                                  */
/* ------------------------------------------------------------------ */

static int parse_job_id(const char *str)
{
    if (str == NULL)
        return -1;
    if (str[0] == '%')
        str++;
    return atoi(str);
}

static int is_valid_ident(const char *name)
{
    if (!name || (!isalpha((unsigned char)name[0]) && name[0] != '_'))
        return 0;
    for (int i = 1; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_')
            return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Individual built-in implementations                                */
/* ------------------------------------------------------------------ */

/*
 * builtin_cd - Change the shell's working directory.
 *
 * Supports:
 *   cd           -> go to $HOME
 *   cd ~         -> go to $HOME
 *   cd ~/dir     -> go to $HOME/dir
 *   cd -         -> go to previous working directory (OLDPWD)
 *   cd <path>    -> go to <path>
 */
static int builtin_cd(Command *cmd)
{
    const char *path = NULL;
    char current_cwd[1024];
    static char tilde_path[1024];

    /* Record current directory before changing */
    if (getcwd(current_cwd, sizeof(current_cwd)) == NULL) {
        current_cwd[0] = '\0';
    }

    if (cmd->argc > 2) {
        fprintf(stderr, "mini-shell: cd: too many arguments\n");
        return 1;
    }

    if (cmd->argc < 2) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "mini-shell: cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(cmd->args[1], "-") == 0) {
        if (prev_cwd[0] == '\0') {
            fprintf(stderr, "mini-shell: cd: OLDPWD not set\n");
            return 1;
        }
        path = prev_cwd;
        printf("%s\n", prev_cwd);
    } else if (strcmp(cmd->args[1], "~") == 0) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "mini-shell: cd: HOME not set\n");
            return 1;
        }
    } else if (strncmp(cmd->args[1], "~/", 2) == 0) {
        const char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "mini-shell: cd: HOME not set\n");
            return 1;
        }
        snprintf(tilde_path, sizeof(tilde_path), "%s/%s", home, cmd->args[1] + 2);
        path = tilde_path;
    } else {
        path = cmd->args[1];
    }

    if (chdir(path) != 0) {
        perror("mini-shell: cd");
        return 1;
    }

    /* Update OLDPWD tracking */
    if (current_cwd[0] != '\0') {
        strncpy(prev_cwd, current_cwd, sizeof(prev_cwd) - 1);
        prev_cwd[sizeof(prev_cwd) - 1] = '\0';
        setenv("OLDPWD", current_cwd, 1);
    }

    return 0;
}

/*
 * builtin_pwd - Print the current working directory.
 */
static int builtin_pwd(void)
{
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("mini-shell: pwd");
        return 1;
    }

    return 0;
}

/*
 * builtin_history - Print command history.
 */
static int builtin_history(void)
{
    print_history();
    return 0;
}

/*
 * builtin_export - Set environment variables.
 *
 * Usage:
 *   export               -> prints all current environment variables
 *   export VAR=VAL       -> sets VAR to VAL
 *   export VAR           -> sets VAR to empty string
 */
static int builtin_export(Command *cmd)
{
    if (cmd->argc == 1) {
        for (char **env = environ; *env != NULL; env++) {
            printf("%s\n", *env);
        }
        return 0;
    }

    int ret = 0;
    for (int i = 1; i < cmd->argc; i++) {
        char *arg = cmd->args[i];
        char *eq = strchr(arg, '=');

        if (eq == NULL) {
            if (!is_valid_ident(arg)) {
                fprintf(stderr, "mini-shell: export: '%s': not a valid identifier\n", arg);
                ret = 1;
                continue;
            }
            if (setenv(arg, "", 1) != 0) {
                perror("mini-shell: setenv");
                ret = 1;
            }
        } else {
            *eq = '\0';
            char *name = arg;
            char *val = eq + 1;

            if (!is_valid_ident(name)) {
                fprintf(stderr, "mini-shell: export: '%s': not a valid identifier\n", name);
                *eq = '=';
                ret = 1;
                continue;
            }

            if (setenv(name, val, 1) != 0) {
                perror("mini-shell: setenv");
                ret = 1;
            }
            *eq = '=';
        }
    }

    return ret;
}

/*
 * builtin_help - Display a list of available commands.
 */
static int builtin_help(void)
{
    printf("\n");
    printf("  Mini-Shell — A simple Unix shell\n");
    printf("  ================================\n\n");
    printf("  Built-in commands:\n");
    printf("    cd [dir]       Change directory (supports ~, ~/path, -)\n");
    printf("    pwd            Print current working directory\n");
    printf("    exit [code]    Exit the shell with optional status code\n");
    printf("    help           Show this help message\n");
    printf("    history        Display command history\n");
    printf("    export [K=V]   Set or list environment variables\n");
    printf("    jobs           List background and stopped jobs\n");
    printf("    fg [job_id]    Bring a job to the foreground (e.g. fg 1 or fg %%1)\n");
    printf("    bg [job_id]    Resume a stopped job in the background\n");
    printf("\n");
    printf("  Features supported:\n");
    printf("    - Input/Output redirection: <, >, >>, 2>\n");
    printf("    - Pipelines: cmd1 | cmd2 | cmd3\n");
    printf("    - Background execution: cmd &\n");
    printf("    - Signal handling: Ctrl+C, Ctrl+Z, Ctrl+\\\n");
    printf("    - Variable expansion: $VAR, $?, $$\n\n");

    return 0;
}

/*
 * builtin_jobs - List all active jobs.
 */
static int builtin_jobs(void)
{
    print_jobs();
    return 0;
}

/*
 * builtin_fg - Bring a background or stopped job to the foreground.
 */
static int builtin_fg(Command *cmd)
{
    Job *job = NULL;

    if (cmd->argc >= 2) {
        int jid = parse_job_id(cmd->args[1]);
        job = get_job(jid);
        if (!job) {
            fprintf(stderr, "mini-shell: fg: %s: no such job\n", cmd->args[1]);
            return 1;
        }
    } else {
        job = get_latest_job();
        if (!job) {
            fprintf(stderr, "mini-shell: fg: current: no such job\n");
            return 1;
        }
    }

    printf("%s\n", job->command);

    /* If job was stopped, resume it with SIGCONT */
    if (job->status == JOB_STOPPED) {
        if (kill(job->pid, SIGCONT) < 0) {
            perror("mini-shell: kill (SIGCONT)");
            return 1;
        }
    }
    job->status = JOB_RUNNING;

    /* Wait for the job in the foreground */
    int status;
    set_foreground_pid(job->pid);
    if (waitpid(job->pid, &status, WUNTRACED) < 0) {
        perror("mini-shell: waitpid");
        set_foreground_pid(0);
        return 1;
    }
    set_foreground_pid(0);

    if (WIFSTOPPED(status)) {
        job->status = JOB_STOPPED;
        printf("[%d]  Stopped\t\t%s\n", job->job_id, job->command);
    } else {
        /* Process terminated or signaled */
        remove_job(job->job_id);
    }

    return 0;
}

/*
 * builtin_bg - Resume a stopped job in the background.
 */
static int builtin_bg(Command *cmd)
{
    Job *job = NULL;

    if (cmd->argc >= 2) {
        int jid = parse_job_id(cmd->args[1]);
        job = get_job(jid);
        if (!job) {
            fprintf(stderr, "mini-shell: bg: %s: no such job\n", cmd->args[1]);
            return 1;
        }
    } else {
        job = get_latest_job();
        if (!job) {
            fprintf(stderr, "mini-shell: bg: current: no such job\n");
            return 1;
        }
    }

    if (job->status == JOB_RUNNING) {
        printf("mini-shell: bg: job %d already in background\n", job->job_id);
        return 0;
    }

    /* Send SIGCONT to continue process in background */
    if (kill(job->pid, SIGCONT) < 0) {
        perror("mini-shell: kill (SIGCONT)");
        return 1;
    }

    job->status = JOB_RUNNING;
    printf("[%d]+ %s &\n", job->job_id, job->command);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Dispatcher functions                                               */
/* ------------------------------------------------------------------ */

static const char *builtin_names[] = {
    "cd", "pwd", "exit", "help", "jobs", "fg", "bg", "history", "export",
    NULL
};

int is_builtin(const char *name)
{
    if (name == NULL)
        return 0;
    for (int i = 0; builtin_names[i] != NULL; i++) {
        if (strcmp(name, builtin_names[i]) == 0)
            return 1;
    }
    return 0;
}

int run_builtin(Command *cmd)
{
    if (strcmp(cmd->args[0], "cd") == 0)
        return builtin_cd(cmd);

    if (strcmp(cmd->args[0], "pwd") == 0)
        return builtin_pwd();

    if (strcmp(cmd->args[0], "exit") == 0) {
        printf("Goodbye!\n");
        int code = (cmd->argc >= 2) ? atoi(cmd->args[1]) : 0;
        exit(code);
    }

    if (strcmp(cmd->args[0], "help") == 0)
        return builtin_help();

    if (strcmp(cmd->args[0], "jobs") == 0)
        return builtin_jobs();

    if (strcmp(cmd->args[0], "fg") == 0)
        return builtin_fg(cmd);

    if (strcmp(cmd->args[0], "bg") == 0)
        return builtin_bg(cmd);

    if (strcmp(cmd->args[0], "history") == 0)
        return builtin_history();

    if (strcmp(cmd->args[0], "export") == 0)
        return builtin_export(cmd);

    return 0;
}
