# msh — Milian Shell

A minimal Unix shell written in C with built-in commands and external program execution.

## Features

- Interactive REPL (Read-Eval-Print Loop)
- Built-in commands: `cd`, `help`, `exit`, `version`
- External command execution via `fork`/`exec`
- Dynamic buffer management for input and tokenization
- X-macro pattern for easy builtin registration

## Build & Run

```bash
gcc -o msh main.c
./msh
```

## Adding a New Builtin

1. Add `X(yourcommand)` to `BUILTIN_LIST`
2. Write `int msh_yourcommand(char **args)` — return 1 to continue, 0 to exit

## Roadmap

- [ ] Show current directory in prompt
- [ ] `pwd` builtin
- [ ] Pipes (`ls | grep .c`)
- [ ] I/O redirection (`echo hello > file.txt`)
- [ ] Job control (`Ctrl+Z`, `bg`, `fg`)
- [ ] Signal handling

/*
TODOs:

• 0.2.0 — add pwd, prompt improvements
• 0.3.0 — pipes
*/

## Requirements

- GCC or Clang
- Unix/macOS/Linux (uses POSIX syscalls)

## License

MIT
