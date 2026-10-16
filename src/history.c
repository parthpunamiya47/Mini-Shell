/*
 * history.c - Command history management for mini-shell
 *
 * Implements a fixed-capacity circular buffer storing up to MAX_HISTORY entries.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "history.h"

static char history_table[MAX_HISTORY][256];
static int history_count = 0;

void add_history_entry(const char *line)
{
    if (line == NULL || line[0] == '\0')
        return;

    /* Avoid adding identical consecutive commands */
    if (history_count > 0) {
        int last_idx = (history_count - 1) % MAX_HISTORY;
        if (strcmp(history_table[last_idx], line) == 0)
            return;
    }

    int idx = history_count % MAX_HISTORY;
    strncpy(history_table[idx], line, sizeof(history_table[idx]) - 1);
    history_table[idx][sizeof(history_table[idx]) - 1] = '\0';
    history_count++;
}

void print_history(void)
{
    int start = 0;
    int total = history_count;

    if (total > MAX_HISTORY)
        start = total - MAX_HISTORY;

    for (int i = start; i < total; i++) {
        int idx = i % MAX_HISTORY;
        printf("  %4d  %s\n", i + 1, history_table[idx]);
    }
}
