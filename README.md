# Mini Unix Shell in C

A modular, POSIX-compliant Unix command-line shell written in C for an Operating Systems coursework project and live viva examination.

---

## Quick Start: Build & Run in 30 Seconds

### On Windows (using WSL Ubuntu):
```bash
# 1. Open Ubuntu terminal and install build tools (one-time setup)
sudo apt update && sudo apt install build-essential -y

# 2. Go to the project directory
cd "/mnt/c/Users/kusha/.gemini/antigravity/scratch/mini-shell"

# 3. Build and launch
make clean && make
./mini-shell
```

### On Native Linux:
```bash
# 1. Install gcc and make (if not already installed)
sudo apt update && sudo apt install build-essential -y

# 2. Build and launch
cd mini-shell
make clean && make
./mini-shell
```

---

## 1. Project Architecture & File Organization

The shell is structured into decoupled modules, separating the REPL lifecycle, lexical parsing, system process execution, signal management, and job tracking:

```
mini-shell/
├── Makefile          # Build configuration with -Wall -Wextra -g flags
├── README.md         # Comprehensive project documentation & demo guide
└── src/
    ├── main.c        # Shell REPL loop, prompt, history dispatch, variable expansion
    ├── parser.h/c    # Quote-aware tokenizer, pipe & redirection parsing, $VAR expansion
    ├── executor.h/c  # fork(), execvp(), pipes, dup2() redirections, waitpid()
    ├── builtins.h/c  # cd, pwd, exit, help, history, export, jobs, fg, bg
    ├── jobs.h/c      # Background job table, WNOHANG reaper, status tracking
    ├── signals.h/c   # sigaction() handlers for Ctrl+C, Ctrl+Z, Ctrl+\
    └── history.h/c   # Circular buffer recording recent commands
```

### Process Lifecycle Flowchart

```
[User Input Line]
       │
       ▼
[Check & Reap Finished Background Jobs] (waitpid WNOHANG)
       │
       ▼
[Display Prompt] (shows cwd via getcwd)
       │
       ▼
[Read Input Line] (fgets with EINTR signal catch)
       │
       ├── EOF (Ctrl+D)? ──► Print newline & Exit
       ├── Blank line?   ──► Re-display prompt
       │
       ▼
[Record into History] (add_history_entry)
       │
       ▼
[Variable Expansion] ($HOME, $USER, $?, $$, single-quote protected)
       │
       ▼
[Pipeline & Redirection Parser]
       ├── Detect background '&'
       ├── Split by '|' outside quotes
       └── Extract '<', '>', '>>', '2>' and clean argument tokens
       │
       ▼
[Dispatch]
       ├── Single Foreground Built-in?
       │     └── execute_builtin_with_redirection() (dup/dup2 saved FDs)
       │
       └── External Command or Multi-stage Pipeline?
             └── execute_pipeline()
                   ├── pipe() between stages
                   ├── fork() child per stage
                   │     ├── reset_child_signals()
                   │     ├── dup2() pipes & file redirections
                   │     └── execvp() / run_builtin() in subshell
                   │
                   ├── If background ('&'):
                   │     └── add_job(pid, cmd) & return immediately
                   │
                   └── If foreground:
                         ├── set_foreground_pid(last_child_pid)
                         ├── waitpid(WUNTRACED) for all children
                         ├── If stopped (Ctrl+Z): add_job(JOB_STOPPED)
                         └── set_foreground_pid(0)
```

---

## 2. Features Implemented

### 1. External Command Execution
- Executes standard system programs (e.g., `ls`, `grep`, `cat`, `gcc`, `wc`, `mkdir`, `find`).
- Resolves program locations using the system `PATH` via `execvp()`.
- Distinguishes between command not found (exit code 127) and permission denied (exit code 126).

### 2. Built-in Commands (Run In-Process)
- **`cd [dir]`**:
  - `cd`: Navigates to `$HOME`.
  - `cd ~`: Navigates to `$HOME`.
  - `cd ~/folder`: Expands tilde to `$HOME/folder`.
  - `cd -`: Returns to the previous working directory (`$OLDPWD`).
  - `cd <path>`: Navigates to any relative or absolute path.
- **`pwd`**: Prints the absolute path of the current working directory.
- **`exit [code]`**: Exits the shell cleanly with an optional status code (e.g. `exit 0` or `exit 42`).
- **`help`**: Displays built-in help information and supported features.
- **`history`**: Displays a numbered list of up to 100 recent commands.
- **`export [NAME=VAL]`**:
  - `export`: Lists all current environment variables.
  - `export VAR=VALUE`: Sets or updates an environment variable.
  - `export VAR`: Sets an environment variable to an empty string.
  - Validates identifier syntax (must begin with a letter or underscore).
- **`jobs`**: Lists all active background and suspended (stopped) jobs.
- **`fg [job_id]`**: Brings a background or stopped process to the foreground using `SIGCONT` and `waitpid()`.
- **`bg [job_id]`**: Resumes a stopped job in the background using `SIGCONT`.

### 3. Built-in I/O Redirection
Built-in commands fully support input, output, and error redirection through temporary descriptor duplication (`dup()` / `dup2()`):
- `pwd > /tmp/pwd.txt`
- `help > /tmp/help.txt`
- `history > /tmp/history.txt`
- `export > /tmp/environment.txt`

### 4. Full I/O Redirection for External Commands
- **Input (`<`)**: Redirects `stdin` from a file (`open(O_RDONLY)`).
- **Output (`>`)**: Redirects `stdout` to a file, truncating existing content (`O_WRONLY | O_CREAT | O_TRUNC`).
- **Append Output (`>>`)**: Redirects `stdout` to a file, appending new content (`O_WRONLY | O_CREAT | O_APPEND`).
- **Standard Error (`2>`)**: Redirects `stderr` to a file (`dup2(fd, STDERR_FILENO)`).
- **Tolerance for Spacing**: Supports both spaced (`ls > file.txt`) and unspaced (`ls>file.txt`, `cat<file.txt`, `ls 2>err.log`, `ls >>log.txt`) syntax.

### 5. Multi-Stage Pipelines
- Arbitrary pipe chains: `cmd1 | cmd2 | cmd3 | ...`
- Properly closes all read and write pipe ends to prevent deadlock or hanging readers.
- Built-in commands can participate in pipelines (e.g., `history | grep ls`, `help | more`, `jobs | wc -l`).

### 6. Background Execution (`&`)
- Appending `&` at the end of any command or pipeline launches the processes asynchronously.
- The shell immediately outputs `[job_id] PID` and returns to the prompt.
- Background jobs are reaped non-blockingly before each prompt using `waitpid(WNOHANG)`.
- When a job completes, a status notice is displayed: `[1]  Done  sleep 5`.

### 7. Signal Handling (`Ctrl+C`, `Ctrl+Z`, `Ctrl+\`)
- **`Ctrl+C` (`SIGINT`)**:
  - At the shell prompt: Safely caught; prints a newline and refreshes the prompt without terminating the shell.
  - While a foreground process is running: The signal terminates the child process, while the shell remains running.
- **`Ctrl+Z` (`SIGTSTP`)**:
  - At the shell prompt: Ignored.
  - While a foreground process is running: Stops (pauses) the child process. The shell traps `WIFSTOPPED`, registers the child in the job table as `Stopped`, and returns to the prompt.
- **`Ctrl+\` (`SIGQUIT`)**: Ignored by the shell.

### 8. Environment Variable Expansion
- Expands `$VAR` (e.g., `$HOME`, `$USER`, `$PATH`).
- Expands `$$` to the process ID of the shell.
- Expands `$?` to the exit status code of the preceding foreground command.
- Single quotes (`'...'`) inhibit expansion, following POSIX conventions.

### 9. Quoting Support
- Handles arguments wrapped in double quotes (`"..."`) or single quotes (`'...'`), preserving whitespace within tokens:
  `grep "hello world" file.txt`
  `echo 'one two three'`

---

## 3. Important System Calls Used (Viva Cheat Sheet)

| System Call | Header | Signature | Role in Mini-Shell |
|-------------|--------|-----------|--------------------|
| `fork()` | `<unistd.h>` | `pid_t fork(void);` | Clones the shell process. Returns 0 to child, child PID to parent, -1 on failure. |
| `execvp()` | `<unistd.h>` | `int execvp(const char *file, char *const argv[]);` | Replaces child process image with binary, searching directories in `PATH`. |
| `waitpid()` | `<sys/wait.h>` | `pid_t waitpid(pid_t pid, int *status, int options);` | Reaps child processes. `options=0` blocks; `WNOHANG` checks non-blockingly; `WUNTRACED` catches stopped children. |
| `pipe()` | `<unistd.h>` | `int pipe(int pipefd[2]);` | Allocates a unidirectional pipe (`pipefd[0]` = read end, `pipefd[1]` = write end). |
| `dup()` | `<unistd.h>` | `int dup(int oldfd);` | Duplicates an existing file descriptor. Used to preserve original stdin/stdout/stderr before built-in redirection. |
| `dup2()` | `<unistd.h>` | `int dup2(int oldfd, int newfd);` | Clones `oldfd` onto `newfd`. Core mechanism for file and pipe redirection. |
| `open()` | `<fcntl.h>` | `int open(const char *path, int flags, mode_t mode);` | Opens or creates files with `O_RDONLY`, `O_WRONLY`, `O_CREAT`, `O_TRUNC`, `O_APPEND`. |
| `close()` | `<unistd.h>` | `int close(int fd);` | Releases file descriptors. Crucial for pipe ends so readers observe EOF. |
| `sigaction()` | `<signal.h>` | `int sigaction(int sig, const struct sigaction *act, struct sigaction *old);` | Installs reliable signal handlers with controllable system call restart behavior. |
| `kill()` | `<signal.h>` | `int kill(pid_t pid, int sig);` | Sends signals to processes. Used to send `SIGCONT` in `fg` and `bg` to wake suspended jobs. |
| `chdir()` | `<unistd.h>` | `int chdir(const char *path);` | Updates current working directory of the shell. Must be a built-in running in the parent process. |
| `getcwd()` | `<unistd.h>` | `char *getcwd(char *buf, size_t size);` | Retrieves the absolute working directory to render the dynamic prompt. |
| `setenv()` | `<stdlib.h>` | `int setenv(const char *name, const char *value, int overwrite);` | Adds or updates an environment variable for the shell and its future children. |

---

## 4. Installation & Build Instructions

### Method A: Running on Windows via WSL (Recommended)

1. **Install WSL (if not already installed)**:
   Open PowerShell as Administrator and run:
   ```powershell
   wsl --install
   ```
   Restart your PC if prompted, and complete the Ubuntu username/password setup.

2. **Install Build Tools**:
   Inside your Ubuntu/WSL terminal:
   ```bash
   sudo apt update
   sudo apt install build-essential -y
   ```

3. **Navigate to the Project**:
   ```bash
   cd "/mnt/c/Users/kusha/.gemini/antigravity/scratch/mini-shell"
   ```

4. **Compile the Shell**:
   ```bash
   make clean
   make
   ```

5. **Run the Shell**:
   ```bash
   ./mini-shell
   ```

### Method B: Running on Native Linux

1. Ensure `gcc` and `make` are present:
   ```bash
   sudo apt update && sudo apt install build-essential -y
   ```
2. Build and launch:
   ```bash
   cd mini-shell
   make clean
   make
   ./mini-shell
   ```

---

## 5. Live Viva Demonstration Script

Step-by-step verification script to demonstrate every requirement during your examination:

### Step 1: External Commands & PATH Lookup
```bash
# Check working directory
pwd

# List files with arguments
ls -la

# Run commands with arguments
echo Hello, Operating Systems!
whoami
date
```

### Step 2: Built-ins (`cd`, `pwd`, `help`, `history`, `export`)
```bash
# View help
help

# Change directory
cd /tmp
pwd

# Change back to home via tilde and empty argument
cd ~
pwd
cd
pwd

# Return to previous directory using cd -
cd /tmp
cd /var
cd -
pwd

# Export and environment variable expansion
export COURSE=CS201
echo "Course enrolled: $COURSE"
echo "My home directory is $HOME, shell PID is $$, last status is $?"

# View history
history
```

### Step 3: Redirection (`<`, `>`, `>>`, `2>`)
```bash
# Output redirection (truncate)
echo "Line 1" > /tmp/demo.txt
cat /tmp/demo.txt

# Append redirection
echo "Line 2" >> /tmp/demo.txt
cat /tmp/demo.txt

# Input redirection
wc -l < /tmp/demo.txt

# Stderr redirection
ls /nonexistent_directory 2> /tmp/error.log
cat /tmp/error.log

# Redirection on built-ins
pwd > /tmp/my_cwd.txt
cat /tmp/my_cwd.txt
help > /tmp/shell_help.txt
head -10 /tmp/shell_help.txt
```

### Step 4: Pipelines
```bash
# Two-stage pipeline
ls -la | grep "\.c"

# Three-stage pipeline
cat /etc/passwd | cut -d: -f1 | sort | head -5

# Built-in participating in pipeline
history | grep cd
help | grep export
```

### Step 5: Background Execution (`&`) & Job Control (`jobs`, `fg`, `bg`)
```bash
# Start sleep in background
sleep 6 &

# Check job listing
jobs
# Output: [1]  Running    sleep 6

# Wait 6 seconds and hit Enter
# Output: [1]  Done       sleep 6

# Stop and resume a process
sleep 30
# Press Ctrl+Z
# Output: [1]  Stopped    sleep 30

# Resume in background
bg 1
jobs
# Output: [1]  Running    sleep 30

# Bring to foreground
fg 1
# Process is now in foreground waiting for completion
```

### Step 6: Signal Handling (`Ctrl+C`, `Ctrl+Z`)
```bash
# 1. Press Ctrl+C at the empty prompt
# Result: shell prints a newline and refreshes prompt. Shell does NOT exit!

# 2. Run a foreground sleep and terminate with Ctrl+C
sleep 30
# Press Ctrl+C
# Result: sleep process terminates immediately, shell prompt returns safely.
```

### Step 7: Quoting & Syntax Error Handling
```bash
# Quoting with spaces
echo "Hello     World"
echo 'Quotes preserve spaces'

# Syntax error checking
>
| ls
cat < > file.txt

# Clean exit with status code
exit 0
```

---

## 6. Academic Limitations & Design Explanations (For Viva)

When asked about design choices or potential improvements:

1. **Terminal Process Groups (`setpgid` / `tcsetpgrp`)**:
   - *Current Implementation*: Our shell relies on signal routing (`sigaction`, `reset_child_signals`) and process status polling via `waitpid(pid, &status, WUNTRACED)`.
   - *Why*: Full terminal job control requires configuring terminal process groups with `tcsetpgrp()`. For an introductory OS project, direct signal management achieves identical visible behavior (`Ctrl+C`, `Ctrl+Z`, `fg`, `bg`) while keeping the codebase understandable and free of terminal race conditions.

2. **Parsing Model**:
   - *Current Implementation*: A clean two-phase tokenizer that respects single/double quotes, normalizes redirection operators, and expands environment variables.
   - *Why*: Real production shells (like bash) use full context-free grammar parsers generated by Flex/Bison. Our modular tokenizer satisfies all POSIX pipeline and redirection requirements without introducing third-party parser dependencies.
