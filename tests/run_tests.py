import json
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROGRAM = ROOT / "os_io_redirection_shell"
results = []


def run_case(name, commands, expected, log_name, setup=None, check=None, run_as_nobody=False):
    with tempfile.TemporaryDirectory(prefix="io_shell_") as tmp:
        work = Path(tmp)
        if setup:
            setup(work)
        if run_as_nobody:
            os.chmod(work, 0o755)
        process = subprocess.run(
            [str(PROGRAM)],
            input=commands,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=work,
            timeout=10,
            preexec_fn=(lambda: os.setuid(65534)) if run_as_nobody else None,
        )
        output = process.stdout
        passed = check(work, output, process.returncode) if check else expected in output
        log_path = ROOT / "tests" / log_name
        log_path.write_text(output, encoding="utf-8")
        results.append({
            "test": name,
            "expected": expected,
            "actual": output.strip().replace("\n", " | "),
            "status": "PASS" if passed else "FAIL",
            "exit_code": process.returncode,
            "log": str(log_path.relative_to(ROOT)),
        })


def setup_input(work):
    (work / "input.txt").write_text("first\nsecond\n", encoding="utf-8")


def check_overwrite(work, output, code):
    return (work / "output.txt").read_text() == "alpha\n" and "Shell closed." in output and code == 0

run_case(
    "Output redirection truncates and writes to a file",
    "printf 'alpha\\n' > output.txt\nexit\n",
    check=check_overwrite,
    expected="output.txt contains alpha followed by a newline",
    log_name="test_01_output_redirection.log",
)


def check_append(work, output, code):
    return (work / "output.txt").read_text() == "old\nnew\n" and code == 0

run_case(
    "Append redirection preserves existing file content",
    "printf 'new\\n' >> output.txt\nexit\n",
    setup=lambda w: (w / "output.txt").write_text("old\n"),
    check=check_append,
    expected="output.txt contains old and new on separate lines",
    log_name="test_02_append_redirection.log",
)


def check_input(work, output, code):
    return "2" in output and "Shell closed." in output and code == 0

run_case(
    "Input redirection supplies a file to standard input",
    "wc -l < input.txt\nexit\n",
    setup=setup_input,
    check=check_input,
    expected="wc reports 2 lines",
    log_name="test_03_input_redirection.log",
)


def check_combined(work, output, code):
    return (work / "upper.txt").read_text() == "FIRST\nSECOND\n" and "Shell closed." in output and code == 0

run_case(
    "Input and output redirection work in one command",
    "tr a-z A-Z < input.txt > upper.txt\nexit\n",
    setup=setup_input,
    check=check_combined,
    expected="upper.txt contains FIRST and SECOND in uppercase",
    log_name="test_04_combined_redirection.log",
)


def check_missing(work, output, code):
    return "Input redirection failed" in output and "No such file or directory" in output and "still-running" in output and code == 0

run_case(
    "Missing input file reports an error and shell continues",
    "cat < missing.txt\necho still-running\nexit\n",
    check=check_missing,
    expected="Clear input-file error followed by still-running",
    log_name="test_05_missing_input_error.log",
)


def check_directory(work, output, code):
    return "Output redirection failed" in output and "Is a directory" in output and code == 0

run_case(
    "Directory used as an output file is rejected",
    "echo blocked > existing_dir\nexit\n",
    setup=lambda w: (w / "existing_dir").mkdir(),
    check=check_directory,
    expected="Output redirection error reports Is a directory",
    log_name="test_06_output_open_error.log",
)


def check_incomplete(work, output, code):
    return "invalid or incomplete redirection syntax" in output and "still-running" in output and code == 0

run_case(
    "Missing output filename is rejected",
    "echo nope >\necho still-running\nexit\n",
    check=check_incomplete,
    expected="Incomplete redirection error followed by still-running",
    log_name="test_07_incomplete_redirection.log",
)


def check_quoted(work, output, code):
    return (work / "quoted.txt").read_text() == "hello world\n" and code == 0

run_case(
    "Quoted argument containing spaces is preserved with redirection",
    "printf '%s\\n' 'hello world' > quoted.txt\nexit\n",
    check=check_quoted,
    expected="quoted.txt contains hello world",
    log_name="test_08_quoted_argument.log",
)

def setup_readonly(work):
    target = work / "read_only.txt"
    target.write_text("protected\n", encoding="utf-8")
    target.chmod(0o444)


def check_permission(work, output, code):
    return "Output redirection failed" in output and "Permission denied" in output and code == 0

run_case(
    "Read-only file produces a permission error for an unprivileged user",
    "echo denied > read_only.txt\nexit\n",
    setup=setup_readonly,
    check=check_permission,
    expected="Output redirection reports Permission denied and shell remains usable",
    log_name="test_09_permission_error.log",
    run_as_nobody=(os.geteuid() == 0),
)

summary = {
    "program": "Secure Configurable Linux Shell: Input/Output Redirection",
    "compiler_command": "gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 os_io_redirection_shell.c -o os_io_redirection_shell",
    "tests_passed": sum(1 for item in results if item["status"] == "PASS"),
    "tests_total": len(results),
    "results": results,
}
(ROOT / "tests" / "test_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

lines = [
    "INPUT/OUTPUT REDIRECTION SHELL - TEST SUMMARY",
    f"Passed: {summary['tests_passed']} / {summary['tests_total']}",
    "",
]
for item in results:
    lines.append(f"{item['status']}: {item['test']}")
    lines.append(f"Expected: {item['expected']}")
    lines.append(f"Actual: {item['actual']}")
    lines.append(f"Log: {item['log']}")
    lines.append("")
(ROOT / "tests" / "test_results.txt").write_text("\n".join(lines), encoding="utf-8")
print("\n".join(lines))
if summary["tests_passed"] != summary["tests_total"]:
    raise SystemExit(1)
