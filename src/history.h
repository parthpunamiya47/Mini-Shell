/*
 * history.h - Command history management for mini-shell
 */

#ifndef HISTORY_H
#define HISTORY_H

#define MAX_HISTORY 100

/*
 * add_history_entry - Record a line into the history buffer.
 * Ignores empty lines and consecutive duplicates.
 */
void add_history_entry(const char *line);

/*
 * print_history - Display numbered list of past commands.
 */
void print_history(void);

#endif
