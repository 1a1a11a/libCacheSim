# Debugging libCacheSim

This guide provides detailed information about debugging libCacheSim using the debug script.

## Debug Script Overview

The debug script (`scripts/debug.sh`) is a utility that helps you build and debug libCacheSim with GDB. It automatically:
1. Sets up a debug build with proper compiler flags
2. Builds the project in debug mode
3. Launches GDB with thread exit messages disabled
4. Passes any provided arguments to the cachesim program

## Basic Usage

```bash
# Basic usage - just build and launch GDB
./scripts/debug.sh

# Clean build and debug
./scripts/debug.sh -c

# Debug with program arguments
./scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb

# Clean build and debug with program arguments
./scripts/debug.sh -c -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb
```

## Command Line Options

The debug script supports the following options:
- `-h, --help`: Show help message
- `-c, --clean`: Clean the build directory before building
- `--`: Separator between script options and program arguments

## Example Debug Sessions

### Basic Debug Session
```bash
./scripts/debug.sh
```
This will:
1. Create a debug build in `_build_dbg` directory
2. Build the project with debug symbols
3. Launch GDB with the cachesim binary

### Debug with Clean Build
```bash
./scripts/debug.sh -c
```
This will:
1. Remove the existing `_build_dbg` directory
2. Create a fresh debug build
3. Build the project with debug symbols
4. Launch GDB with the cachesim binary

### Debug with Program Arguments
```bash
./scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb
```
This will:
1. Build the project in debug mode
2. Launch GDB with the cachesim binary
3. Pass the arguments `data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb` to the program

## Debugging Specific Components

### Debugging Cache Algorithms

To debug a specific cache algorithm, set breakpoints in the algorithm's implementation:

```bash
# Start GDB with the debug script
./scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb

# In GDB, set breakpoints in the LRU implementation
(gdb) b LRU_get
(gdb) b LRU_put
(gdb) r
```

### Debugging Trace Readers

To debug trace reading issues:

```bash
# Start GDB with a specific trace
./scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb

# In GDB, set breakpoints in the trace reader
(gdb) b vscsi_read_one_req
(gdb) r
```

### Debugging Admission Policies

To debug admission policy behavior:

```bash
# Start GDB with a specific admission policy
./scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb --admission adaptsize

# In GDB, set breakpoints in the admission policy
(gdb) b adaptsize_admit
(gdb) r
```

## Common Debugging Scenarios
### Segmentation Faults

To debug segmentation faults:

```bash
# Start GDB with the debug script
./scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb

# In GDB, run the program
(gdb) r

# When the segfault occurs, examine the backtrace
(gdb) bt
```

## GDB Commands and Shortcuts

### Essential Commands
- `r` - run/continue
- `c` - continue
- `n` - next (step over)
- `s` - step (step into)
- `p variable` - print variable
- `b function` - set breakpoint
- `bt` - backtrace
- `q` - quit

### Advanced Commands
- `b file.c:line` - set breakpoint at specific line
- `b function if condition` - set conditional breakpoint
- `watch variable` - set watchpoint on variable
- `info break` - list all breakpoints
- `delete breakpoint_number` - delete a breakpoint
- `disable breakpoint_number` - disable a breakpoint
- `enable breakpoint_number` - enable a breakpoint
- `clear file.c:line` - clear breakpoint at line
- `finish` - run until current function returns
- `until line` - run until line is reached
- `return value` - return from current function with value
- `call function(args)` - call a function
- `display variable` - display variable value after each command
- `undisplay display_number` - stop displaying a variable
- `info locals` - show local variables
- `info args` - show function arguments
- `info threads` - show all threads
- `thread thread_number` - switch to thread
- `frame frame_number` - switch stack frame
- `up` - move up one stack frame
- `down` - move down one stack frame
- `set print pretty on` - pretty print structures
- `set print array on` - print arrays in a more readable format
- `set print null-stop on` - stop printing at null character in strings
- `set logging on` - log GDB output to a file
- `set logging off` - stop logging
- `shell command` - execute shell command
- `help command` - get help on a command

### Useful GDB Tips
1. Use `Ctrl+X Ctrl+A` to enter TUI (Text User Interface) mode for a better debugging experience
2. Use `Ctrl+X 2` to split the screen and show source code
3. Use `Ctrl+X 1` to return to single window mode
4. Use `Ctrl+C` to interrupt the program and return to GDB prompt
5. Use `Ctrl+D` to exit GDB
6. Use `Ctrl+L` to refresh the screen
7. Use `Ctrl+R` to reverse-search command history
8. Use `Ctrl+P` to search backward through command history
9. Use `Ctrl+N` to search forward through command history
10. Use `Ctrl+O` to execute the current command and fetch the next one

## Advanced Debugging Techniques

### Using Valgrind to help with debug

For memory-related issues, you can use GDB with Valgrind:

```bash
# Install Valgrind if not already installed
sudo apt install valgrind  # Ubuntu/Debian
brew install valgrind     # macOS

# Run cachesim with Valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind-out.txt ./_build_dbg/bin/cachesim data/cloudPhysicsIO.vscsi vscsi lru 100m,1gb
```




## Troubleshooting

If you encounter any issues:
1. Make sure you have GDB installed
2. Check that all dependencies are installed
3. Try cleaning the build directory with `-c` option
4. Ensure you have proper permissions to execute the script

### Common Issues and Solutions

#### GDB not found
If GDB is not installed, install it using:
```bash
# Ubuntu/Debian
sudo apt install gdb

# CentOS/RHEL
sudo yum install gdb

# macOS
brew install gdb
```

#### Missing debug symbols
If debug symbols are missing, ensure you're using the debug build:
```bash
./scripts/debug.sh -c  # Clean and rebuild with debug symbols
```

#### GDB crashes
If GDB crashes, try:
```bash
# Start with a clean build
./scripts/debug.sh -c

# Or try with a simpler command
./scripts/debug.sh
```

## Debugging with IDE Integration

### VSCode Integration

1. Install the C/C++ extension for VSCode
2. Create a `.vscode/launch.json` file with:
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
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ],
            "preLaunchTask": "build-debug"
        }
    ]
}
```

3. Create a `.vscode/tasks.json` file with:
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build-debug",
            "type": "shell",
            "command": "bash scripts/debug.sh -c",
            "group": {
                "kind": "build",
                "isDefault": true
            }
        }
    ]
}
```

4. Press F5 to start debugging

