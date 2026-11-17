# Unix Shell in C

A small Unix command shell written in C on top of POSIX system calls (`fork`, `execvp`, `pipe`, `dup2`, `waitpid`, `signal`).
Built for the Operating Systems course at Ariel University, starting from a minimal skeleton (`starter/shell-basic.c`)
and extended to the full implementation in `shell.c`.

## Features

- **Pipelines** of any length: `ls -l | grep .c | wc -l`
- **I/O redirection**: `>` (overwrite), `>>` (append), `<` (input), `2>` (stderr)
- **Background jobs** with `&`; finished children are reaped by a `SIGCHLD` handler (no zombie processes), while `SIGCHLD` is blocked during foreground waits so exit statuses are never lost
- **Ctrl-D** (end of input) exits cleanly
- **Ctrl-C handling**: the shell itself ignores `SIGINT` and prints a message, while foreground children get the default behaviour
- **History**: `!!` repeats the last command, the ↑ arrow key recalls previous commands
- **Built-ins**:
  - `cd <dir>` — change directory
  - `status` — exit status of the last command
  - `prompt = <text>` — change the prompt
  - `quit` — exit the shell

## Build & run (Linux / WSL)

```bash
make
./myshell
sh tests/run_tests.sh   # smoke tests
```

## Example

```
hello: ls -l | grep .c > c_files.txt
hello: cat < c_files.txt
hello: sleep 5 &
hello: status
hello: prompt = myshell>
myshell> !!
```

## Tech
C · POSIX processes, pipes and file descriptors · signal handling · Linux
