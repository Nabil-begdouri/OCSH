# OCSH – Operating System Command Shell

**A Mini Shell Developed in C – UIR, Operating Systems Project**

## 1. Project Overview

OCSH is a mini command-line shell developed in the C programming language as part of the Operating Systems course at the International University of Rabat (UIR).

The project aims to understand how a shell works internally through process creation, command execution, pipes, background processes, signals, and environment variables.

## 2. Features

- Command execution using `fork()`, `execvp()` and `waitpid()`.
- Command pipelines using the `->` operator.
- Background process execution using `&`.
- Environment variable management.
- Variable expansion: `$VAR` and `${VAR}`.
- Last command exit status using `$?`.
- Last background process PID using `$!`.
- Background process management using `SIGCHLD`.

## 3. Project Structure

```text
OCSH/
├── code/
│   ├── main.c
│   ├── execution.c
│   ├── execution.h
│   ├── parser.c
│   ├── parser.h
│   ├── pipes.c
│   ├── pipes.h
│   ├── jobs.c
│   └── jobs.h
├── Makefile
├── README.md
└── .gitignore
```

## 4. Compilation

To compile the project, run:

```bash
make
```

To remove generated files:

```bash
make clean
```

## 5. Execution

Start the shell with:

```bash
./ocsh
```

The shell displays the following prompt:

```text
ocsh>
```

## 6. Usage Examples

Execute a command:

```bash
ocsh> pwd
```

Execute a pipeline:

```bash
ocsh> ls -> wc -l
```

Launch a background process:

```bash
ocsh> sleep 10 &
```

Display the last background process PID:

```bash
ocsh> echo $!
```

Use environment variables:

```bash
ocsh> export NOM=NABIL
ocsh> echo $NOM
```

Display the last command status:

```bash
ocsh> echo $?
```

## 7. Technologies

- Language: C
- Operating System: Linux (Ubuntu)
- Compiler: GCC
- Build Tool: Make
- Version Control: Git and GitHub

## 8. Academic Context

**Institution:** International University of Rabat (UIR)  
**Course:** Operating Systems  
**Project:** OCSH – Mini Shell in C