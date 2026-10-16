/*
 * jobs.c - Background job tracking for mini-shell
 *
 * Maintains a table of background processes and reaps completed jobs.
 */

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include "jobs.h"

static Job job_table[MAX_JOBS];
static int next_job_id = 1;

int add_job(pid_t pid, const char *command)
{
    /* Reset job counter if table is empty */
    int active_count = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].active)
            active_count++;
    }
    if (active_count == 0)
        next_job_id = 1;

    /* Find a free slot */
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!job_table[i].active) {
            job_table[i].job_id = next_job_id++;
            job_table[i].pid = pid;
            job_table[i].status = JOB_RUNNING;
            job_table[i].active = 1;

            strncpy(job_table[i].command, command, sizeof(job_table[i].command) - 1);
            job_table[i].command[sizeof(job_table[i].command) - 1] = '\0';

            return job_table[i].job_id;
        }
    }

    fprintf(stderr, "mini-shell: too many background jobs\n");
    return -1;
}

void remove_job(int job_id)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].active && job_table[i].job_id == job_id) {
            job_table[i].active = 0;
            return;
        }
    }
}

/*
 * check_jobs - Non-blocking reaper for finished or stopped background jobs.
 */
void check_jobs(void)
{
    int status;
    pid_t result;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (!job_table[i].active)
            continue;

        if (job_table[i].status == JOB_DONE)
            continue;

        result = waitpid(job_table[i].pid, &status, WNOHANG | WUNTRACED);

        if (result == 0) {
            /* Child is still running */
            continue;
        }

        if (result < 0) {
            /* Child process already reaped or doesn't exist */
            job_table[i].active = 0;
            continue;
        }

        /* Child state changed */
        if (WIFEXITED(status)) {
            printf("[%d]  Done\t\t%s\n", job_table[i].job_id,
                   job_table[i].command);
            job_table[i].active = 0;
        } else if (WIFSIGNALED(status)) {
            printf("[%d]  Terminated\t%s\n", job_table[i].job_id,
                   job_table[i].command);
            job_table[i].active = 0;
        } else if (WIFSTOPPED(status)) {
            printf("[%d]  Stopped\t\t%s\n", job_table[i].job_id,
                   job_table[i].command);
            job_table[i].status = JOB_STOPPED;
        }
    }
}

/*
 * print_jobs - Show all active background and stopped jobs.
 */
void print_jobs(void)
{
    int found = 0;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (!job_table[i].active)
            continue;

        const char *status_str;
        switch (job_table[i].status) {
            case JOB_RUNNING: status_str = "Running"; break;
            case JOB_STOPPED: status_str = "Stopped"; break;
            case JOB_DONE:    status_str = "Done";    break;
            default:          status_str = "Unknown"; break;
        }

        printf("[%d]  %s\t\t%s\n", job_table[i].job_id, status_str,
               job_table[i].command);
        found = 1;
    }

    if (!found)
        printf("No active jobs.\n");
}

Job *get_job(int job_id)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].active && job_table[i].job_id == job_id)
            return &job_table[i];
    }
    return NULL;
}

Job *get_latest_job(void)
{
    Job *latest = NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].active) {
            if (latest == NULL || job_table[i].job_id > latest->job_id)
                latest = &job_table[i];
        }
    }
    return latest;
}
