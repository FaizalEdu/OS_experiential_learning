# Secure Configurable Linux Shell: Input/Output Redirection

**Individual Experiential Learning Project — Unit V**  
**Course:** 25PCCS201 — Operating Systems  
**Student:** LODI MOHAMMAD FAIZAL KHAN  
**USN:** 25BTRCL131  
**Section:** AIML-C  
**Assigned context:** Edge-computing Linux device  
**Individual project number:** 07

## 1. Project overview

This project implements a small Linux shell prototype that demonstrates input and output redirection for external commands. It supports standard input redirection with `<`, output redirection with `>`, and append redirection with `>>`. The shell creates a child process for an external command, opens the requested file, connects the file to standard input or standard output, executes the command, and waits for the child to finish.

The prototype focuses on the individual feature assigned in the project statement: input/output redirection and robust file-open and permission error handling. It is a teaching prototype, not a replacement for Bash and not a complete production kiosk shell.

## 2. Features

- `command < input.txt` reads standard input from a file.
- `command > output.txt` creates or truncates an output file.
- `command >> output.txt` creates a file if needed and appends to it.
- Input and output redirection can be used together in one command.
- Quoted arguments containing spaces are supported.
- Missing filenames, unmatched quotation marks, missing input files, directory output targets and permission errors are handled with clear messages.
- External commands are started with `fork()` and `execvp()`.
- File descriptors are connected with `dup2()` and closed after use.
- The parent waits for the child using `waitpid()` and displays parent/child process IDs and the child exit result.
- Built-in commands: `cd` and `exit`.

## 3. Requirements

- Linux environment (for example, Ubuntu or an approved Linux lab environment)
- GCC compiler
- Standard Linux command-line utilities used by the examples, such as `cat`, `wc`, `tr`, `printf` and `echo`

## 4. Compile

Open a terminal in this project folder and run:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 os_io_redirection_shell.c -o os_io_redirection_shell
```

## 5. Run

```bash
./os_io_redirection_shell
```

The prompt is `io-shell>`. Examples:

```text
cat < input.txt
printf 'Hello\n' > output.txt
printf 'Another line\n' >> output.txt
tr a-z A-Z < input.txt > uppercase.txt
exit
```

Use only files in a temporary project folder while testing. Avoid redirecting output to important system files.

## 6. Run the test suite

After compiling, run:

```bash
python3 tests/run_tests.py
```

The script runs nine reproducible tests in temporary directories and saves a separate terminal log for each test under `tests/`. It also creates `tests/test_results.txt` and `tests/test_summary.json`.

The permission test checks a read-only file as an unprivileged user. When the test script is run as root, it launches the test shell with the unprivileged account available in the Linux environment. When run as a regular user, the read-only file is owned by that user and has no write permission.

## 7. Test coverage

| No. | Test | Expected behaviour |
|---:|---|---|
| 1 | Output redirection `>` | New output is written to a file. |
| 2 | Append redirection `>>` | Existing content is preserved and new output is added. |
| 3 | Input redirection `<` | The command reads data from the selected file. |
| 4 | Combined input and output redirection | Data is read from one file and written to another. |
| 5 | Missing input file | A file-open error is shown and the shell continues. |
| 6 | Directory used as output target | The open error is reported and the shell continues. |
| 7 | Missing output filename | Incomplete redirection syntax is rejected. |
| 8 | Quoted argument with spaces | The quoted argument is passed as one argument. |
| 9 | Read-only output file | A permission error is reported for an unprivileged user. |

Actual test outputs are recorded in `tests/test_results.txt` and individual `.log` files. They were produced by running the included source code in a Linux environment in this preparation session. Re-run the tests in the student's approved environment before submission and retain the resulting evidence.

## 8. Operating Systems concepts demonstrated

- **Process creation:** `fork()` creates a child process for an external command.
- **Program execution:** `execvp()` replaces the child process image with the requested command.
- **File descriptors:** standard input and standard output are represented by file descriptors 0 and 1.
- **File operations:** `open()` opens the input or output path with the required access and creation flags.
- **Descriptor duplication:** `dup2()` connects the opened file to standard input or standard output.
- **Process synchronisation:** `waitpid()` allows the parent shell to wait for the child and collect its termination status.
- **Error handling:** `errno` and `strerror()` provide useful explanations when file operations fail.

## 9. Limitations

This is a focused academic prototype. It does not implement pipes, background jobs, command history, environment-variable expansion, wildcard expansion, shell scripting, or the full parsing rules of Bash. Redirection is implemented for external commands; the built-in `cd` is kept in the parent process so that directory changes remain effective. A production edge-computing kiosk would also need a carefully designed command allow-list, access controls, resource limits, logging and security review.

## 10. Evidence to add before final submission

The included package contains actual test logs, test results, source code and build instructions. Add screenshots captured from the student's own approved Linux environment showing:

1. The editor/workspace and terminal where the source file is stored.
2. The GCC compilation command and successful return to the prompt.
3. A command using `<`.
4. A command using `>` and the created file contents.
5. A command using `>>` and the appended file contents.
6. A combined `<` and `>` operation.
7. A file-open or permission error, followed by the shell continuing to accept commands.
8. A process PID message showing the shell parent and child process.

Place genuine screenshots in the `screenshots/` folder. Do not create mock screenshots or edit outputs to make a failed test look successful.

## 11. Source-code policy

The C source file is provided without inline comments as requested. The design and important system calls are explained in `PROJECT_REPORT_MATERIALS.md` so that the implementation remains understandable.

## 12. References

1. Linux man-pages project, `fork(2)`: https://man7.org/linux/man-pages/man2/fork.2.html
2. Linux man-pages project, `open(2)`: https://man7.org/linux/man-pages/man2/open.2.html
3. Linux man-pages project, `dup(2)` / `dup2(2)`: https://man7.org/linux/man-pages/man2/dup.2.html
4. Linux man-pages project, `exec(3)`: https://man7.org/linux/man-pages/man3/exec.3.html
5. Linux man-pages project, `wait(2)` / `waitpid(2)`: https://man7.org/linux/man-pages/man2/waitpid.2.html

---

**AI-use disclosure:** AI assistance was used to help draft and review parts of the code, documentation and report structure. The student should review the source, rerun the program, understand the implementation and edit the reflection to accurately describe their own work before submission.
