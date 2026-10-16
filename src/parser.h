/*
 * parser.h - Command parsing for mini-shell
 */

#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS         64   /* max arguments per single command */
#define MAX_LINE        1024  /* max characters in one input line */
#define MAX_PIPE_STAGES  10   /* max commands in one pipeline */

/*
 * Represents one simple command, e.g., "grep -i hello".
 *
 * token_buffer stores clean, unquoted argument strings so all
 * pointers in args[], input_file, etc. remain permanently valid.
 */
typedef struct {
    char *args[MAX_ARGS];
    int   argc;
    char *input_file;       /* < filename */
    char *output_file;      /* > or >> filename */
    char *error_file;       /* 2> filename */
    int   append_output;    /* 1 if >>, 0 if > */
    char  token_buffer[MAX_LINE * 2];
} Command;

/*
 * Represents a full pipeline, e.g., "cat f.txt | grep hi | wc -l".
 * A pipeline with no '|' has num_stages == 1.
 *
 * is_background: 1 if user appended '&', 0 for foreground.
 * raw_command:   original command string used for job table display.
 */
typedef struct {
    Command stages[MAX_PIPE_STAGES];
    int     num_stages;
    int     is_background;
    char    raw_command[256];
} Pipeline;

/*
 * parse_line - Parse a single command segment (no pipes).
 * Handles quotes, normalizes redirections, and populates cmd.
 * Returns argc (0 = empty, -1 = syntax error).
 */
int parse_line(char *line, Command *cmd);

/*
 * parse_pipeline - Parse a full input line that may contain '|' and '&'.
 *
 * Checks for background execution '&', splits by '|' outside quotes,
 * and parses each command stage.
 * Returns the number of stages (>= 1), or -1 on error.
 */
int parse_pipeline(char *line, Pipeline *pipeline);

/*
 * expand_variables - Replaces occurrences of $VAR, ${VAR}, $?, and $$ with values.
 * Ignores variables inside single quotes.
 */
void expand_variables(const char *src, char *dest, size_t dest_size, int last_status);

#endif
