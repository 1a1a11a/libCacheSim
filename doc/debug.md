# Debugging libCacheSim

This page shows how to get `cachesim` into a debugger. You'll need it when a simulation crashes or a new algorithm misbehaves, or when you want to step through a trace reader to see what it actually parses.

The short version, run from the repository root (trace paths are relative to the directory you run the script from):

```bash
./scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb
```

That builds a debug copy of libCacheSim and starts `cachesim` under GDB with the arguments after `--`.

## What `debug.sh` does

[`scripts/debug.sh`](/scripts/debug.sh) saves you from setting up a debug build by hand. When you run it, it:

1. Moves to the repository root, so the build always lands in the same place.
2. Configures a **Debug** build in `_build_dbg/`. It has full debug symbols and the same strict warning flags as CI (`-Wall -Wextra -Werror` and more), so a warning stops the build.
3. Builds it with Ninja.
4. Starts GDB on `_build_dbg/bin/cachesim`, but only if you passed program arguments. GDB runs the program straight away and hides the noisy thread start/exit messages.

With no program arguments, the script just builds and prints the GDB command you can run yourself.

The debug build skips tcmalloc, because tcmalloc gets in the way of Valgrind. It is also much slower than a release build, so don't use it to measure performance.

## Options

| Option | What it does |
| --- | --- |
| `-h`, `--help` | Show the help message. |
| `-c`, `--clean` | Delete `_build_dbg/` and build from scratch. |
| `-d`, `--default` | Run a ready-made example: `data/cloudPhysicsIO.vscsi vscsi lru,s3fifo 100m,1gb`. |
| `--` | Everything after this is passed to `cachesim`. |

Some typical runs:

```bash
# build only, then print how to start gdb
./scripts/debug.sh

# rebuild from scratch, then debug the sample trace
./scripts/debug.sh -c -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb

# quickest way to see the debugger working
./scripts/debug.sh -d
```

> [!NOTE]
> The script `cd`s to the repository root before building, but `cachesim` runs in the directory you started the script from. Trace paths are therefore relative to where *you* are. The examples above assume you are at the repository root.

## Breaking inside the code you care about

Start the session as usual, press `Ctrl+C` to get back to the `(gdb)` prompt (or let it crash), set your breakpoints, and run again with `r`.

**An eviction algorithm.** Each algorithm's functions are named after it, for example `LRU_get`, `LRU_insert` and `LRU_evict` in [LRU.c](/libCacheSim/cache/eviction/LRU.c):

```bash
./scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb
```

```text
(gdb) b LRU_evict
(gdb) r
```

**A trace reader.** Every reader has a `*_read_one_req` function. For the vscsi reader:

```text
(gdb) b vscsi_read_one_req
(gdb) r
```

**An admission policy.** Pass the policy to `cachesim` and break on its admit function:

```bash
./scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb --admission adaptsize
```

```text
(gdb) b adaptsize_admit
(gdb) r
```

**A segmentation fault.** Just run it. When it crashes, `bt` shows where:

```text
(gdb) r
...
Program received signal SIGSEGV, Segmentation fault.
(gdb) bt
```

## GDB commands you'll actually use

| Command | What it does |
| --- | --- |
| `r` | Run (or restart) the program. |
| `c` | Continue until the next breakpoint. |
| `n` / `s` | Step over / step into the next line. |
| `finish` | Run until the current function returns. |
| `b func` or `b file.c:123` | Set a breakpoint. |
| `b func if req->obj_id == 42` | Break only when a condition holds. |
| `watch expr` | Stop when a value changes. |
| `info break`, `delete N` | List breakpoints, delete one. |
| `bt`, `up`, `down`, `frame N` | Look at the call stack and move through it. |
| `p expr`, `p *req` | Print a value or a whole struct. |
| `info locals`, `info args` | Show local variables or function arguments. |
| `info threads`, `thread N` | List threads and switch between them. `cachesim` runs one thread per simulation. |
| `set print pretty on` | Print structs one field per line. |
| `q` | Quit. |

`Ctrl+X A` toggles GDB's text UI, which shows the source next to the prompt.

## Finding memory bugs with Valgrind

Valgrind is better than GDB at catching leaks, use-after-free bugs and reads of uninitialized memory. Use the debug build, because tcmalloc is left out of it:

```bash
sudo apt install valgrind   # Ubuntu/Debian

./scripts/debug.sh          # build _build_dbg/ first
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --log-file=valgrind-out.txt \
    ./_build_dbg/bin/cachesim data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb
```

Then read `valgrind-out.txt`. Valgrind is slow, so start with a small trace or cap the run with `--num-req`.

For memory *usage* rather than memory bugs, see [memory_usage_profiling.md](memory_usage_profiling.md).

## Debugging from VS Code

Install the C/C++ extension, then add two files to `.vscode/`.

`.vscode/tasks.json` builds the debug binary:

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build-debug",
            "type": "shell",
            "command": "bash scripts/debug.sh",
            "group": { "kind": "build", "isDefault": true }
        }
    ]
}
```

`.vscode/launch.json` runs it under GDB:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug libCacheSim",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/_build_dbg/bin/cachesim",
            "args": ["data/cloudPhysicsIO.vscsi", "vscsi", "lru", "100m,1gb"],
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "setupCommands": [
                { "text": "-enable-pretty-printing", "ignoreFailures": true }
            ],
            "preLaunchTask": "build-debug"
        }
    ]
}
```

Press F5 to build and start debugging. Change `args` to the trace and settings you want.

## When something goes wrong

**`gdb: command not found`.** Install it: `sudo apt install gdb` (Ubuntu/Debian), `sudo yum install gdb` (CentOS/RHEL) or `brew install gdb` (macOS).

**The build fails on a warning.** The debug build treats warnings as errors, just like CI. Fix the warning. It would fail CI anyway.

**Breakpoints don't bind, or variables show as `<optimized out>`.** You are probably running a release binary. Make sure you are debugging `_build_dbg/bin/cachesim`, and rebuild with `./scripts/debug.sh -c` if in doubt.

**Odd build errors after switching branches.** Do a clean rebuild with `./scripts/debug.sh -c`.
