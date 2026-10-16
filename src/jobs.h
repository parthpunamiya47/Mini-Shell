/*
 * jobs.h - Background job tracking for mini-shell
 *
 * Maintains a list of background processes so we can:
 *   - Show them with "jobs"
 *   - Bring them to the foreground with "fg"
 *   - Detect when they finish and notify the user
 */

#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 32

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,    /* stopped by Ctrl+Z (will be used in Stage 6) */
    JOB_DONE
} JobStatus;

typedef struct {
    int       job_id;           /* job number shown in [1], [2], etc. */
    pid_t     pid;              /* process ID of the (last) child */
    JobStatus status;
    char      command[256];     /* original command text for display */
    int       active;           /* 1 = slot in use, 0 = free */
} Job;

/*
 * add_job - Register a new background job.
 * Returns the job ID, or -1 if the job table is full.
 */
int add_job(pid_t pid, const char *command);

/*
 * remove_job - Free a slot in the job table.
 */
void remove_job(int job_id);

/*
 * check_jobs - Check for finished/stopped background jobs.
 *
 * Uses waitpid() with WNOHANG to check each job without blocking.
 * Prints a message for any job that has finished and removes it.
 *
 * Call this before each prompt so the user sees completion notifications.
 */
void check_jobs(void);

/*
 * print_jobs - Display all active jobs (for the "jobs" built-in).
 */
void print_jobs(void);

/*
 * get_job - Look up a job by its job ID.
 * Returns a pointer to the Job, or NULL if not found.
 */
Job *get_job(int job_id);

/*
 * get_latest_job - Get the most recently added job.
 * Returns a pointer to the Job, or NULL if no active jobs.
 * Used by "fg" and "bg" when no job number is given.
 */
Job *get_latest_job(void);

#endif
