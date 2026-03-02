# Shell
A shell implementation developed as part of the **Operating Systems** course at **Theoretical Computer Science, Jagiellonian University, Cracow**.

## Getting Started

### Prerequisites
```txt
gcc make byacc lex
```

### Building Shell
```bash
cd shell
make 
```

### Running Shell
```bash
./bin/mshell
```


## Custom Commands
- `lcd [path]` – changes the current working directory to the specified path. If no path is provided, it changes to the user's home directory.  

- `lkill [ -signal_number ] pid` – sends the signal `signal_number` to the process/group with the given `pid`. The default signal number is `SIGTERM`.  

- `lls` – prints the names of files in the current directory (similar to `ls -1` without any options).

## Authors
- Mateusz Wojaczek