/*
 * parser.c - Command parsing for mini-shell
 *
 * Implements:
 *   - Quote-aware tokenization ("..." and '...') with quotes stripped
 *   - Embedded quote handling (e.g. VAR="hello world")
 *   - Separation of redirection operators (<, >, >>, 2>)
 *   - Multi-stage pipe parsing (|) outside quotes
 *   - Background execution detection (&) outside quotes
 *   - Environment variable expansion ($VAR, ${VAR}, $?, $$)
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "parser.h"

/* ------------------------------------------------------------------ */
/*  Helper functions                                                  */
/* ------------------------------------------------------------------ */

static int is_redir_token(const char *tok)
{
    if (tok == NULL)
        return 0;
    return (strcmp(tok, "<") == 0 || strcmp(tok, ">") == 0 ||
            strcmp(tok, ">>") == 0 || strcmp(tok, "2>") == 0);
}

/*
 * normalize_spacing - Ensures redirection operators (<, >, >>, 2>)
 * have whitespace around them, except when inside quotes.
 */
static void normalize_spacing(const char *src, char *dst, size_t dst_size)
{
    size_t d = 0;
    const char *s = src;
    int in_dquote = 0;
    int in_squote = 0;

    while (*s != '\0' && d + 4 < dst_size) {
        if (*s == '"' && !in_squote) {
            in_dquote = !in_dquote;
            dst[d++] = *s++;
        } else if (*s == '\'' && !in_dquote) {
            in_squote = !in_squote;
            dst[d++] = *s++;
        } else if (!in_dquote && !in_squote) {
            if ((s == src || isspace((unsigned char)*(s - 1))) &&
                s[0] == '2' && s[1] == '>' && s[2] != '>') {
                dst[d++] = ' ';
                dst[d++] = '2';
                dst[d++] = '>';
                dst[d++] = ' ';
                s += 2;
            } else if (s[0] == '>' && s[1] == '>') {
                dst[d++] = ' ';
                dst[d++] = '>';
                dst[d++] = '>';
                dst[d++] = ' ';
                s += 2;
            } else if (s[0] == '>' || s[0] == '<') {
                dst[d++] = ' ';
                dst[d++] = *s++;
                dst[d++] = ' ';
            } else {
                dst[d++] = *s++;
            }
        } else {
            dst[d++] = *s++;
        }
    }
    dst[d] = '\0';
}

/*
 * extract_token - Extracts the next whitespace-delimited token from *src_ptr.
 * Quotes ('...' and "...") are processed and removed.
 * Returns:
 *   1 if a token was extracted
 *   0 if end of string reached
 *  -1 on syntax error (unclosed quote)
 */
static int extract_token(char **src_ptr, char *dst, size_t dst_size)
{
    char *s = *src_ptr;
    size_t d = 0;
    int in_dquote = 0;
    int in_squote = 0;

    /* Skip leading whitespace */
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;

    if (*s == '\0') {
        *src_ptr = s;
        return 0;
    }

    while (*s != '\0') {
        if (*s == '"' && !in_squote) {
            in_dquote = !in_dquote;
            s++;
        } else if (*s == '\'' && !in_dquote) {
            in_squote = !in_squote;
            s++;
        } else if ((*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') &&
                   !in_dquote && !in_squote) {
            break;
        } else {
            if (d + 1 < dst_size) {
                dst[d++] = *s;
            }
            s++;
        }
    }

    if (in_dquote || in_squote) {
        fprintf(stderr, "mini-shell: syntax error: unclosed quote\n");
        return -1;
    }

    dst[d] = '\0';
    *src_ptr = s;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Command & Pipeline Parsing                                        */
/* ------------------------------------------------------------------ */

/*
 * parse_line - Tokenizes one command stage, extracts redirections,
 * and fills cmd->args[].
 */
int parse_line(char *line, Command *cmd)
{
    char *tokens[MAX_ARGS];
    int num_tokens = 0;
    char *p = line;
    size_t buf_offset = 0;
    int i, arg_index;

    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->error_file = NULL;
    cmd->append_output = 0;
    cmd->argc = 0;

    /* Extract tokens into cmd->token_buffer */
    while (num_tokens < MAX_ARGS - 1) {
        if (buf_offset >= sizeof(cmd->token_buffer) - 1)
            break;

        char *token_dst = &cmd->token_buffer[buf_offset];
        size_t max_token_len = sizeof(cmd->token_buffer) - buf_offset;

        int res = extract_token(&p, token_dst, max_token_len);
        if (res < 0)
            return -1;  /* unclosed quote */
        if (res == 0)
            break;      /* end of string */

        tokens[num_tokens++] = token_dst;
        buf_offset += strlen(token_dst) + 1;
    }

    if (num_tokens == 0)
        return 0;

    arg_index = 0;
    for (i = 0; i < num_tokens; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            i++;
            if (i >= num_tokens || is_redir_token(tokens[i])) {
                fprintf(stderr, "mini-shell: syntax error near unexpected token '%s'\n",
                        (i < num_tokens) ? tokens[i] : "newline");
                return -1;
            }
            cmd->input_file = tokens[i];
        } else if (strcmp(tokens[i], ">>") == 0) {
            i++;
            if (i >= num_tokens || is_redir_token(tokens[i])) {
                fprintf(stderr, "mini-shell: syntax error near unexpected token '%s'\n",
                        (i < num_tokens) ? tokens[i] : "newline");
                return -1;
            }
            cmd->output_file = tokens[i];
            cmd->append_output = 1;
        } else if (strcmp(tokens[i], "2>") == 0) {
            i++;
            if (i >= num_tokens || is_redir_token(tokens[i])) {
                fprintf(stderr, "mini-shell: syntax error near unexpected token '%s'\n",
                        (i < num_tokens) ? tokens[i] : "newline");
                return -1;
            }
            cmd->error_file = tokens[i];
        } else if (strcmp(tokens[i], ">") == 0) {
            i++;
            if (i >= num_tokens || is_redir_token(tokens[i])) {
                fprintf(stderr, "mini-shell: syntax error near unexpected token '%s'\n",
                        (i < num_tokens) ? tokens[i] : "newline");
                return -1;
            }
            cmd->output_file = tokens[i];
            cmd->append_output = 0;
        } else {
            cmd->args[arg_index++] = tokens[i];
        }
    }

    cmd->args[arg_index] = NULL;
    cmd->argc = arg_index;
    return arg_index;
}

/*
 * parse_pipeline - Check for '&', split by '|' outside quotes,
 * and parse each command stage.
 */
int parse_pipeline(char *line, Pipeline *pipeline)
{
    static char normalized[MAX_LINE * 2];
    char *segments[MAX_PIPE_STAGES];
    int num_segments = 0;
    char *p;
    int i, result;

    pipeline->is_background = 0;
    pipeline->raw_command[0] = '\0';

    /* Save clean command string for job display */
    strncpy(pipeline->raw_command, line, sizeof(pipeline->raw_command) - 1);
    pipeline->raw_command[sizeof(pipeline->raw_command) - 1] = '\0';
    size_t raw_len = strlen(pipeline->raw_command);
    while (raw_len > 0 && (pipeline->raw_command[raw_len - 1] == '\n' ||
                           pipeline->raw_command[raw_len - 1] == '\r')) {
        pipeline->raw_command[--raw_len] = '\0';
    }

    /* Normalize spacing around redirection operators */
    normalize_spacing(line, normalized, sizeof(normalized));

    /* Trim trailing whitespace */
    int len = (int)strlen(normalized);
    while (len > 0 && isspace((unsigned char)normalized[len - 1])) {
        normalized[--len] = '\0';
    }

    if (len == 0)
        return 0;

    /* Detect trailing '&' outside of quotes */
    int in_dquote = 0;
    int in_squote = 0;
    int last_amp_pos = -1;

    for (i = 0; i < len; i++) {
        if (normalized[i] == '"' && !in_squote)
            in_dquote = !in_dquote;
        else if (normalized[i] == '\'' && !in_dquote)
            in_squote = !in_squote;
        else if (normalized[i] == '&' && !in_dquote && !in_squote) {
            /* Check if this '&' is the last non-whitespace character in string */
            int trailing = 1;
            for (int k = i + 1; k < len; k++) {
                if (!isspace((unsigned char)normalized[k])) {
                    trailing = 0;
                    break;
                }
            }
            if (trailing) {
                last_amp_pos = i;
                break;
            }
        }
    }

    if (last_amp_pos != -1) {
        pipeline->is_background = 1;
        normalized[last_amp_pos] = '\0';
        len = last_amp_pos;
        while (len > 0 && isspace((unsigned char)normalized[len - 1])) {
            normalized[--len] = '\0';
        }
        if (len == 0) {
            fprintf(stderr, "mini-shell: syntax error near unexpected token '&'\n");
            return -1;
        }
    }

    /* Split at '|' characters that are outside quotes */
    segments[0] = normalized;
    num_segments = 1;

    in_dquote = 0;
    in_squote = 0;

    for (p = normalized; *p != '\0'; p++) {
        if (*p == '"' && !in_squote) {
            in_dquote = !in_dquote;
        } else if (*p == '\'' && !in_dquote) {
            in_squote = !in_squote;
        } else if (*p == '|' && !in_dquote && !in_squote) {
            if (num_segments >= MAX_PIPE_STAGES) {
                fprintf(stderr, "mini-shell: too many pipe stages (max %d)\n",
                        MAX_PIPE_STAGES);
                return -1;
            }
            *p = '\0';
            segments[num_segments] = p + 1;
            num_segments++;
        }
    }

    if (in_dquote || in_squote) {
        fprintf(stderr, "mini-shell: syntax error: unclosed quote\n");
        return -1;
    }

    /* Parse each segment into a Command */
    for (i = 0; i < num_segments; i++) {
        result = parse_line(segments[i], &pipeline->stages[i]);
        if (result < 0)
            return -1;
        if (pipeline->stages[i].argc == 0) {
            fprintf(stderr, "mini-shell: syntax error: empty command in pipeline\n");
            return -1;
        }
    }

    pipeline->num_stages = num_segments;
    return num_segments;
}

/* ------------------------------------------------------------------ */
/*  Environment Variable Expansion                                    */
/* ------------------------------------------------------------------ */

void expand_variables(const char *src, char *dest, size_t dest_size, int last_status)
{
    size_t d = 0;
    const char *s = src;
    int in_dquote = 0;
    int in_squote = 0;

    if (dest_size == 0)
        return;

    while (*s != '\0' && d + 1 < dest_size) {
        /* Track quotes: single quotes inhibit expansion */
        if (*s == '"' && !in_squote) {
            in_dquote = !in_dquote;
            dest[d++] = *s++;
            continue;
        }
        if (*s == '\'' && !in_dquote) {
            in_squote = !in_squote;
            dest[d++] = *s++;
            continue;
        }

        /* Variable expansion triggered by '$' outside single quotes */
        if (*s == '$' && !in_squote) {
            s++;
            if (*s == '?') {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", last_status);
                for (int i = 0; buf[i] != '\0' && d + 1 < dest_size; i++) {
                    dest[d++] = buf[i];
                }
                s++;
            } else if (*s == '$') {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", (int)getpid());
                for (int i = 0; buf[i] != '\0' && d + 1 < dest_size; i++) {
                    dest[d++] = buf[i];
                }
                s++;
            } else if (*s == '{') {
                /* Handle ${VAR} syntax */
                s++;
                char varname[128];
                int vi = 0;
                while (*s != '\0' && *s != '}' && vi < (int)sizeof(varname) - 1) {
                    varname[vi++] = *s++;
                }
                varname[vi] = '\0';
                if (*s == '}')
                    s++;

                const char *val = getenv(varname);
                if (val != NULL) {
                    while (*val != '\0' && d + 1 < dest_size) {
                        dest[d++] = *val++;
                    }
                }
            } else if (isalpha((unsigned char)*s) || *s == '_') {
                /* Handle $VAR syntax */
                char varname[128];
                int vi = 0;
                while ((isalnum((unsigned char)*s) || *s == '_') && vi < (int)sizeof(varname) - 1) {
                    varname[vi++] = *s++;
                }
                varname[vi] = '\0';

                const char *val = getenv(varname);
                if (val != NULL) {
                    while (*val != '\0' && d + 1 < dest_size) {
                        dest[d++] = *val++;
                    }
                }
            } else {
                dest[d++] = '$';
                if (*s != '\0' && d + 1 < dest_size) {
                    dest[d++] = *s++;
                }
            }
        } else {
            dest[d++] = *s++;
        }
    }

    dest[d] = '\0';
}
