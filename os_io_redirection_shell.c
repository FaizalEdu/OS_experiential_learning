#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INPUT_LIMIT 4096
#define TOKEN_LIMIT 128
#define STORAGE_LIMIT 4096

struct command {
    char *argv[TOKEN_LIMIT];
    int argc;
    char *input_path;
    char *output_path;
    int append_output;
};

static int tokenize(const char *line, char *storage, char *tokens[], int *count) {
    size_t i = 0;
    size_t out = 0;
    int n = 0;

    while (line[i] != '\0') {
        while (isspace((unsigned char)line[i])) i++;
        if (line[i] == '\0') break;
        if (n >= TOKEN_LIMIT || out >= STORAGE_LIMIT - 2) return -1;

        tokens[n++] = &storage[out];

        if (line[i] == '<') {
            storage[out++] = '<';
            i++;
            storage[out++] = '\0';
            continue;
        }
        if (line[i] == '>') {
            storage[out++] = '>';
            i++;
            if (line[i] == '>') {
                storage[out++] = '>';
                i++;
            }
            storage[out++] = '\0';
            continue;
        }

        char quote = '\0';
        while (line[i] != '\0') {
            char ch = line[i];
            if (quote == '\0' && (isspace((unsigned char)ch) || ch == '<' || ch == '>')) break;
            if ((ch == '\'' || ch == '"') && (quote == '\0' || quote == ch)) {
                quote = quote == '\0' ? ch : '\0';
                i++;
                continue;
            }
            if (ch == '\\' && line[i + 1] != '\0' && quote != '\'') {
                i++;
                ch = line[i];
            }
            if (out >= STORAGE_LIMIT - 2) return -1;
            storage[out++] = ch;
            i++;
        }
        if (quote != '\0') return -2;
        storage[out++] = '\0';
    }

    tokens[n] = NULL;
    *count = n;
    return 0;
}

static int parse_command(char *tokens[], int token_count, struct command *cmd) {
    memset(cmd, 0, sizeof(*cmd));

    for (int i = 0; i < token_count; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            if (i + 1 >= token_count || cmd->input_path != NULL) return -1;
            cmd->input_path = tokens[++i];
        } else if (strcmp(tokens[i], ">") == 0 || strcmp(tokens[i], ">>") == 0) {
            if (i + 1 >= token_count || cmd->output_path != NULL) return -1;
            cmd->append_output = strcmp(tokens[i], ">>") == 0;
            cmd->output_path = tokens[++i];
        } else {
            if (cmd->argc >= TOKEN_LIMIT - 1) return -1;
            cmd->argv[cmd->argc++] = tokens[i];
        }
    }

    cmd->argv[cmd->argc] = NULL;
    if (cmd->argc == 0 && (cmd->input_path != NULL || cmd->output_path != NULL)) return -1;
    return 0;
}

static int apply_redirections(const struct command *cmd) {
    if (cmd->input_path != NULL) {
        int fd = open(cmd->input_path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Input redirection failed for '%s': %s\n", cmd->input_path, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            fprintf(stderr, "Could not redirect standard input: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (cmd->output_path != NULL) {
        int flags = O_WRONLY | O_CREAT | (cmd->append_output ? O_APPEND : O_TRUNC);
        int fd = open(cmd->output_path, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "Output redirection failed for '%s': %s\n", cmd->output_path, strerror(errno));
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            fprintf(stderr, "Could not redirect standard output: %s\n", strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

static int run_builtin(const struct command *cmd, int *should_exit) {
    if (strcmp(cmd->argv[0], "exit") == 0) {
        if (cmd->argc > 1) {
            fprintf(stderr, "Usage: exit\n");
            return 1;
        }
        *should_exit = 1;
        return 1;
    }

    if (strcmp(cmd->argv[0], "cd") == 0) {
        if (cmd->input_path != NULL || cmd->output_path != NULL) {
            fprintf(stderr, "Redirection is supported for external commands; use an external command with redirection.\n");
            return 1;
        }
        const char *target = cmd->argc > 1 ? cmd->argv[1] : getenv("HOME");
        if (target == NULL || cmd->argc > 2) {
            fprintf(stderr, "Usage: cd [directory]\n");
            return 1;
        }
        if (chdir(target) != 0) fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
        return 1;
    }

    return 0;
}

static void execute_command(const struct command *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return;
    }

    if (pid == 0) {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, NULL);
        sigaction(SIGQUIT, &action, NULL);

        if (apply_redirections(cmd) != 0) _exit(126);
        execvp(cmd->argv[0], cmd->argv);
        fprintf(stderr, "Command execution failed for '%s': %s\n", cmd->argv[0], strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }

    fprintf(stderr, "[process] shell PID %ld started child PID %ld\n", (long)getpid(), (long)pid);

    int status;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited < 0) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
    } else if (WIFEXITED(status)) {
        fprintf(stderr, "[process] child PID %ld exited with status %d\n", (long)pid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "[process] child PID %ld ended by signal %d\n", (long)pid, WTERMSIG(status));
    }
}

int main(void) {
    char line[INPUT_LIMIT];
    char storage[STORAGE_LIMIT];
    char *tokens[TOKEN_LIMIT + 1];
    int should_exit = 0;

    struct sigaction ignore_action;
    memset(&ignore_action, 0, sizeof(ignore_action));
    ignore_action.sa_handler = SIG_IGN;
    sigemptyset(&ignore_action.sa_mask);
    sigaction(SIGINT, &ignore_action, NULL);

    printf("Secure Configurable Linux Shell\n");
    printf("Input/output redirection: <, >, >> | Built-ins: cd, exit\n");

    while (!should_exit) {
        printf("io-shell> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            if (feof(stdin)) break;
            if (ferror(stdin)) {
                fprintf(stderr, "Input error: %s\n", strerror(errno));
                clearerr(stdin);
                continue;
            }
        }

        size_t length = strlen(line);
        if (length > 0 && line[length - 1] != '\n' && !feof(stdin)) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            fprintf(stderr, "Input rejected: command exceeds %d characters.\n", INPUT_LIMIT - 1);
            continue;
        }

        int token_count = 0;
        int token_status = tokenize(line, storage, tokens, &token_count);
        if (token_status == -2) {
            fprintf(stderr, "Input rejected: unmatched quotation mark.\n");
            continue;
        }
        if (token_status != 0) {
            fprintf(stderr, "Input rejected: too many tokens or command too large.\n");
            continue;
        }
        if (token_count == 0) continue;

        struct command cmd;
        if (parse_command(tokens, token_count, &cmd) != 0) {
            fprintf(stderr, "Input rejected: invalid or incomplete redirection syntax.\n");
            continue;
        }

        if (run_builtin(&cmd, &should_exit)) continue;
        execute_command(&cmd);
    }

    printf("Shell closed.\n");
    return 0;
}
