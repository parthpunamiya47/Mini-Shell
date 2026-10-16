/*
 * main.c - Entry point for mini-shell
 *
 * REPL loop: prompt → read → history → expand vars → parse pipeline → dispatch → repeat
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include "parser.h"
#include "executor.h"
#include "builtins.h"
#include "jobs.h"
#include "signals.h"
#include "history.h"

static void print_prompt(void)
{
    char cwd[1024];
    const char *home = getenv("HOME");

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        if (home != NULL && strcmp(cwd, home) == 0) {
            printf("mini-shell:~$ ");
        } else if (home != NULL && strncmp(cwd, home, strlen(home)) == 0 && cwd[strlen(home)] == '/') {
            printf("mini-shell:~%s$ ", cwd + strlen(home));
        } else {
            printf("mini-shell:%s$ ", cwd);
        }
    } else {
        printf("mini-shell$ ");
    }
    fflush(stdout);
}

/*
 * read_input - Read a line from stdin.
 *
 * Returns:
 *   1 on success
 *   0 on EOF (Ctrl+D)
 *  -1 on signal interruption (e.g., Ctrl+C), indicating prompt should refresh
 */
static int read_input(char *buffer, int size)
{
    if (fgets(buffer, size, stdin) == NULL) {
        if (errno == EINTR) {
            clearerr(stdin);
            return -1;
        }
        printf("\n");
        return 0;
    }
    return 1;
}

int main(void)
{
    char line[MAX_LINE];
    char expanded_line[MAX_LINE * 2];
    Pipeline pipeline;
    int last_status = 0;

    /* Install signal handlers for Ctrl+C, Ctrl+Z, Ctrl+\ */
    init_signals();

    printf("Welcome to mini-shell!  Type 'help' for available commands.\n");

    while (1) {
        /* Check for finished or stopped background jobs */
        check_jobs();

        print_prompt();

        int res = read_input(line, sizeof(line));
        if (res == 0)
            break;   /* EOF (Ctrl+D) */
        if (res < 0)
            continue; /* Interrupted by signal (e.g. Ctrl+C), redisplay prompt */

        /* Trim newline for history storage and empty check */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        /* Check if line is purely whitespace */
        int is_blank = 1;
        for (size_t i = 0; i < len; i++) {
            if (!isspace((unsigned char)line[i])) {
                is_blank = 0;
                break;
            }
        }
        if (is_blank)
            continue;

        /* Record valid command into history */
        add_history_entry(line);

        /* Expand environment variables ($VAR, $?, $$) */
        expand_variables(line, expanded_line, sizeof(expanded_line), last_status);

        /* Parse the full line (may contain pipes, quotes, or '&') */
        int nstages = parse_pipeline(expanded_line, &pipeline);
        if (nstages <= 0)
            continue;  /* syntax error */

        /*
         * Built-ins only run in the shell process when there's a single command
         * (no pipes and not background).
         */
        if (nstages == 1 && !pipeline.is_background &&
            pipeline.stages[0].argc > 0 &&
            is_builtin(pipeline.stages[0].args[0])) {

            int result = execute_builtin_with_redirection(&pipeline.stages[0]);
            if (result == -1)
                break;  /* exit */
            last_status = result;
            continue;
        }

        /* Execute the pipeline (single command or multi-stage) */
        last_status = execute_pipeline(&pipeline);
    }

    printf("Goodbye!\n");
    return 0;
}
