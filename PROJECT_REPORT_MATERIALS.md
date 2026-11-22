# Report Materials — Input/Output Redirection Shell

## Assigned project statement

Design and implement a shell prototype for a Linux-based edge-computing device. The individual feature is input/output redirection. Implement input, output and append redirection with robust file-open and permission error handling.

## Problem statement

A Linux command-line environment needs a way to send data to a command from a file and save command output to files. Without redirection, users generally read input from the keyboard and see output in the terminal. This prototype adds the three basic redirection operators so that commands can read input from files, create or overwrite output files, and append output without deleting existing content. It also reports file-open and permission errors rather than allowing the shell to stop unexpectedly.

## Objectives

1. Implement input redirection using `<`.
2. Implement output redirection using `>`.
3. Implement append redirection using `>>`.
4. Use Linux file descriptors and system calls to connect files to standard input and output.
5. Report missing-file, invalid-syntax, directory-target and permission errors clearly.
6. Demonstrate process creation and completion using parent and child process identifiers.
7. Test normal, boundary and error conditions with recorded results.

## Development environment

- Operating system: Linux environment used for compilation and tests.
- Language: C (POSIX interfaces).
- Compiler: GCC.
- Core interfaces: `fork()`, `execvp()`, `waitpid()`, `open()`, `dup2()`, `close()`, `chdir()`.
- Test utilities: Python 3 test runner and standard Linux commands.

The student must add a screenshot from their own approved Linux environment and state the exact distribution/version and GCC version shown there. Do not guess these details in the final report.

## Algorithm

1. Start the shell and display the prompt.
2. Read a command line from the user.
3. Check the command length and parse tokens, preserving quoted arguments.
4. Identify `<`, `>` and `>>` operators and their file names.
5. Reject missing filenames, duplicate redirection operators or invalid syntax.
6. If the command is `cd` or `exit`, handle the built-in operation in the shell process.
7. Otherwise call `fork()` to create a child process.
8. In the child, open the input file with read-only access when `<` is used.
9. In the child, open the output file with create-and-truncate flags for `>` or create-and-append flags for `>>`.
10. Use `dup2()` to connect the opened file to standard input or standard output, then close the extra descriptor.
11. Call `execvp()` to run the requested external command.
12. In the parent, call `waitpid()` and report the child process ID and termination result.
13. Return to the prompt and repeat until `exit` or end-of-input.

## Plain-text program flow

Start shell → Display prompt → Read command → Validate and tokenize input → Parse redirection operators → Reject invalid syntax or continue → Fork child process → Open requested file → Connect descriptor using dup2 → Execute command using execvp → Parent waits using waitpid → Display child status → Return to prompt or exit.

## Important source-code segments and explanation

### 1. Opening the file for input

```c
int fd = open(cmd->input_path, O_RDONLY);
```

`open()` requests read-only access to the input file. If the file does not exist or permission is denied, it returns an error and sets `errno`.

### 2. Redirecting standard input

```c
if (dup2(fd, STDIN_FILENO) < 0) {
    fprintf(stderr, "Could not redirect standard input: %s\\n", strerror(errno));
}
```

`dup2()` makes the opened file descriptor become the process's standard input descriptor. The external command then reads from the file as if it were receiving input from the keyboard.

### 3. Opening output in truncate or append mode

```c
int flags = O_WRONLY | O_CREAT | (cmd->append_output ? O_APPEND : O_TRUNC);
int fd = open(cmd->output_path, flags, 0644);
```

Both output operators create the file if it does not exist. `>` uses `O_TRUNC` to remove earlier file contents, while `>>` uses `O_APPEND` so new data is added at the end.

### 4. Creating and running the child process

```c
pid_t pid = fork();
if (pid == 0) {
    if (apply_redirections(cmd) != 0) _exit(126);
    execvp(cmd->argv[0], cmd->argv);
}
```

`fork()` creates a child process. The child applies the requested redirection before `execvp()` replaces the child program with the external command. If setup fails, the child exits with an error status.

### 5. Waiting for the child

```c
waited = waitpid(pid, &status, 0);
```

The parent shell waits for the child to finish and checks whether it exited normally or was terminated by a signal. This provides process-completion evidence and prevents the shell from leaving an uncollected child process.

## Expected-versus-actual test table

Actual results below are from the included nine-case automated run. Full terminal logs are stored in `tests/`.

| No. | Test input / condition | Expected result | Actual result | Status |
|---:|---|---|---|---|
| 1 | `printf 'alpha\\n' > output.txt` | File is created and contains `alpha` plus a newline. | File content matched exactly; child exited with status 0. | PASS |
| 2 | `printf 'new\\n' >> output.txt` where the file already contains `old` | Existing content remains and `new` is added. | File contained `old` and `new` on separate lines. | PASS |
| 3 | `wc -l < input.txt` with two lines in the input file | Command reports 2 lines. | Output reported 2. | PASS |
| 4 | `tr a-z A-Z < input.txt > upper.txt` | Input is read from one file and uppercase output is written to another. | Output file contained `FIRST` and `SECOND` on separate lines. | PASS |
| 5 | `cat < missing.txt` | Clear file-open error; shell remains available. | `No such file or directory` was reported and the next command ran. | PASS |
| 6 | `echo blocked > existing_dir` | Directory output target is rejected with a clear error. | `Is a directory` was reported. | PASS |
| 7 | `echo nope >` | Incomplete redirection syntax is rejected. | Invalid/incomplete syntax message appeared and the next command ran. | PASS |
| 8 | `printf '%s\\n' 'hello world' > quoted.txt` | Quoted phrase is preserved as one argument. | File contained `hello world`. | PASS |
| 9 | Write to a read-only file as an unprivileged user | Permission error is reported and shell remains stable. | `Permission denied` was reported; child exited with status 126. | PASS |

## Process observations

The shell reports its own PID and the child PID for each external command. The parent waits for that child using `waitpid()`. Successful commands in the test run returned exit status 0. File-open or redirection setup errors returned status 126. These observations demonstrate process creation, execution and synchronisation relevant to the assigned shell feature.

## Limitations

The prototype intentionally focuses on redirection. It does not implement full Bash syntax, pipes, background jobs, command history, variable expansion, wildcards or shell scripts. It uses a simple tokeniser and supports quoted strings, but is not intended to reproduce every shell parsing rule. A production edge-computing kiosk would require a command allow-list, privilege separation, resource limits, audit logging and security review.

## Reflection draft — personalise before submission

Working on this project helped me understand how a shell handles commands beyond simply displaying output on the terminal. I learned how input and output redirection work and how Linux uses file descriptors to connect a file to a command. The most useful part was implementing the three operators, because `>`, `>>` and `<` look similar but behave differently when files are opened. I also learned how `fork()` creates a child process and how `execvp()` runs the selected command. One challenge was handling file errors properly. A missing input file, an invalid output path and a file without write permission all need different error messages. I found it useful to see that the parent shell can continue running even after a child command fails. If I improve the project, I would add support for pipes and background execution, and make the command parser handle more shell syntax. Overall, the project gave me practical experience with Linux process handling, file descriptors and system calls.

This is a draft reflection, not a claim about personal experiences. The student should edit it to match what they actually did and learned. It is approximately 160 words.

## Report evidence still to collect from the student's own environment

- Screenshot showing the student's Linux editor/workspace and project files.
- Screenshot showing GCC version and the exact compilation command completing successfully.
- Screenshot demonstrating `<` input redirection.
- Screenshot demonstrating `>` output creation/truncation and displaying the created file.
- Screenshot demonstrating `>>` appending to an existing file.
- Screenshot demonstrating combined input and output redirection.
- Screenshot demonstrating the missing-file or permission-denied error and the shell continuing afterward.
- Screenshot showing parent and child PIDs.

The included automated logs are actual runs of the included source code in the preparation Linux environment. For a strong individual submission, rerun them in the student's approved lab/Codespaces environment and share the student's own screenshots.
