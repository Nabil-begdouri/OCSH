# OCSH – Operating System Command Shell

**A Mini Shell Developed in C – International University of Rabat (UIR)**

## 1. Project Overview

OCSH is a mini command-line shell developed in the C programming language as part of the Operating Systems course at the International University of Rabat (UIR).

The main objective is to understand the internal mechanisms of a Unix-like shell, including command execution, process management, pipes, redirections, signals, environment variables and script interpretation.

## 2. Features

### Command Execution
- Interactive shell with a command prompt.
- Command execution using `fork()`, `execvp()` and `waitpid()`.
- Command parsing and execution.
- Support for external Linux commands.

### Built-in Commands and Aliases

OCSH provides the following custom commands:

| OCSH Command | Linux Equivalent |
|---|---|
| `go` | `cd` |
| `quit` | `exit` |
| `path` | `pwd` |
| `say` | `echo` |
| `info` | `help` |
| `past` | `history` |
| `list` | `ls` |
| `show` | `cat` |
| `copy` | `cp` |
| `move` | `mv` |
| `del` | `rm` |
| `newdir` | `mkdir` |
| `newfile` | `touch` |

Additional command:
- `osinfo`: displays the operating systems course information.

### Pipes and Redirections
- Command pipelines using the `->` operator.
- Output redirection using `>`.
- Append redirection using `>>`.
- Input redirection using `<`.

Several redirections can be combined in one command, and built-ins
(`say`, `path`, `osinfo`, `past`...) also work inside a pipeline or with `&`.

Example:

```bash
list -> wc -l
say Hello > output.txt
show < output.txt
sort < input.txt > sorted.txt
say hello -> wc -c
```

### Process Management
- Background execution using `&`.
- Foreground process management using `fg`.
- Background process management using `bg`.
- Job listing using `jobs`.
- Signal handling with `SIGINT`, `SIGTSTP` and `SIGCHLD`.
- Process interruption and suspension using keyboard shortcuts.

### Environment Variables
- Variable definition using `export`.
- Variable expansion using `$VAR` and `${VAR}`.
- Last command exit status using `$?`.
- Last background process PID using `$!`.
- Arithmetic expansion using `$((expression))`.

Example:

```bash
export NAME=Nabil
say $NAME
say ${NAME}
```

### History and Command Completion
- Command history using `past` and the up/down arrow keys.
- Command autocompletion using the Readline library.
- Command substitution using `$(...)` and backticks (built-ins and `->` pipes work inside, e.g. `say $(list -> wc -l)`).

Example:

```bash
say $(pwd)
say `whoami`
```

### Script Execution
OCSH supports script execution through:

```bash
./ocsh tests/demo.ocsh
```

The script interpreter supports:
- Variable assignments.
- `if`, `else` and `fi`.
- `for` and `while` loops.
- Arithmetic expansion.
- `break` and `continue`.
- Command execution and pipelines.

## 3. Project Structure

```text
OCSH/
├── code/
│   ├── main.c
│   ├── execution.c
│   ├── execution.h
│   ├── parser.c
│   ├── parser.h
│   ├── variables.c
│   ├── variables.h
│   ├── builtins.c
│   ├── builtins.h
│   ├── redirections.c
│   ├── redirections.h
│   ├── pipes.c
│   ├── pipes.h
│   ├── jobs.c
│   ├── jobs.h
│   ├── history.c
│   ├── history.h
│   ├── command_substitution.c
│   ├── command_substitution.h
│   ├── completion.c
│   ├── completion.h
│   ├── script.c
│   ├── script.h
│   ├── ocshrc.c
│   ├── ocshrc.h
│   └── shell.h
├── tests/
│   ├── demo.ocsh
│   └── test_jobs.py
├── Makefile
├── README.md
├── report.pdf
├── demo.mp4
└── .gitignore
```

Configuration au démarrage : les lignes de `~/.ocshrc` sont exécutées au lancement du shell interactif (exemple : `export NOM=valeur`).

Guillemets : `say "a   b"` conserve les espaces ; `'$HOME'` n'est pas développé.

## 4. Requirements

The following tools are required:

- Linux or Ubuntu.
- GCC compiler.
- GNU Make.
- GNU Readline development library.

Install the required packages on Ubuntu:

```bash
sudo apt update
sudo apt install build-essential libreadline-dev
```

## 5. Compilation

Clone the repository:

```bash
git clone https://github.com/Nabil-begdouri/OCSH.git
cd OCSH
```

Compile the project:

```bash
make
```

Clean generated files:

```bash
make clean
```

## 6. Execution

Start the interactive shell:

```bash
./ocsh
```

The shell displays:

```text
ocsh>
```

To execute a script:

```bash
./ocsh tests/demo.ocsh
```

## 7. Usage Examples

Display the current directory:

```bash
ocsh> path
```

List files:

```bash
ocsh> list
```

Create a directory:

```bash
ocsh> newdir test
```

Execute a pipeline:

```bash
ocsh> list -> wc -l
```

Redirect output:

```bash
ocsh> say Hello World > message.txt
```

Display file contents:

```bash
ocsh> show message.txt
```

Launch a background process:

```bash
ocsh> sleep 10 &
```

Display active jobs:

```bash
ocsh> jobs
```

Manage processes:

```bash
ocsh> fg 1
ocsh> bg 1
```

Use environment variables:

```bash
ocsh> export NOM=Nabil
ocsh> say $NOM
```

Display the last command status:

```bash
ocsh> say $?
```

## 8. Technologies

- Programming Language: C
- Operating System: Linux / Ubuntu
- Compiler: GCC
- Build System: GNU Make
- Command Completion: GNU Readline
- Version Control: Git and GitHub

## 9. Team Contributions

The project was developed collaboratively by three team members.

- **Nabil:** REPL, command parsing, execution and environment variables.
- **Adam:** Pipes, background processes, signal handling and script execution.
- **Taha:** Built-in commands, redirections, history, command substitution and autocompletion.

## 10. Livrables

- `code/` : code source commenté
- `report.pdf` : rapport (approche, difficultés, résultats, liens avec le cours)
- `demo.mp4` : vidéo de démonstration (caméra ouverte)

## 11. Academic Context

**Institution:** International University of Rabat (UIR)
**Course:** Operating Systems
**Project:** OCSH – Operating System Command Shell
**Language:** C

## 12. Tests

```bash
make
./ocsh tests/demo.ocsh              # test du mode script
cd tests && python3 test_jobs.py    # test du job control (pseudo-terminal)
```
