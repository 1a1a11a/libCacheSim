# Claude Code Skills for libCacheSim

This file documents the Claude Code slash-command skills available to agents working in this repository. Skills are pre-built behaviors that extend Claude Code; invoke them with the `/skill-name` syntax in any Claude Code session.

## Quick Reference

| Skill | Command | Purpose |
|---|---|---|
| `update-config` | `/update-config` | Configure the Claude Code harness via `settings.json` |
| `session-start-hook` | `/session-start-hook` | Set up `SessionStart` hooks (build checks, linters) |
| `simplify` | `/simplify` | Review changed code for quality, reuse, and efficiency |
| `loop` | `/loop [interval] [cmd]` | Run a command repeatedly on a timed interval |
| `schedule` | `/schedule` | Create scheduled remote agents on a cron schedule |
| `claude-api` | `/claude-api` | Build tools or integrations using the Anthropic SDK |
| `keybindings-help` | `/keybindings-help` | Customize keyboard shortcuts |

---

## Skills

### `update-config` — Configure the harness

Use this when you want to add automated behaviors: "before every commit run the linter", "after each edit rebuild", etc. Automated behaviors are executed by the **harness** (not by Claude at prompt time), so they must be registered in `settings.json` via hooks—asking Claude to remember something is not sufficient.

```
/update-config
```

**When to use:**
- Setting up pre/post-tool hooks (e.g., run `cmake --build` after file edits)
- Storing project-level defaults or allowed tool lists
- Enabling or disabling specific capabilities for automated sessions

---

### `session-start-hook` — Ensure the project is ready at session start

Sets up a `SessionStart` hook so every new Claude Code session begins by verifying the project is in a known-good state: dependencies installed, build passing, tests green.

```
/session-start-hook
```

**When to use:**
- First-time setup of a repo for Claude Code on the web
- Ensuring `cmake` configuration and build succeed before an agent starts editing C code
- Running `ctest` at session start to confirm no pre-existing failures

**libCacheSim example hook:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

---

### `simplify` — Code review after changes

After making edits, invoke this skill to review the changed code for redundancy, reuse opportunities, and inefficiencies. The skill finds issues and fixes them in place.

```
/simplify
```

**When to use:**
- After adding a new eviction algorithm to check for duplicated logic
- After editing `CMakeLists.txt` to verify build targets aren't redundant
- As a final pass before committing

---

### `loop` — Recurring tasks

Runs a prompt or slash command on a repeating interval. Defaults to every 10 minutes if no interval is specified.

```
/loop [interval] [command or prompt]
```

**Examples:**
```
/loop 5m /simplify
/loop 2m check if the cmake build is still passing and report any errors
```

**When to use:**
- Watching a long `ctest` run and summarizing new failures as they appear
- Periodically re-running the trace analyzer on a data file and diffing results

---

### `schedule` — Cron-based remote agents

Creates, updates, lists, or runs scheduled remote agents that execute on a cron schedule. Unlike `/loop` (which runs in the current session), scheduled agents persist and run independently.

```
/schedule
```

**When to use:**
- Nightly performance regression checks against reference traces in `data/`
- Weekly sweeps to ensure all example projects still build cleanly
- Automated benchmark runs on a fixed schedule

---

### `claude-api` — Build tools with the Anthropic SDK

Triggered automatically when code imports `anthropic` or `@anthropic-ai/sdk`. Also invoke manually when you want to build a script or integration that calls Claude programmatically.

```
/claude-api
```

**When to use in libCacheSim context:**
- Writing a Python script that uses Claude to analyze cache trace output and suggest algorithm tuning
- Building a Node.js tool on top of `libcachesim-node` that calls Claude to interpret miss-ratio curves
- Generating synthetic trace files with LLM assistance

---

### `keybindings-help` — Customize keyboard shortcuts

Modifies `~/.claude/keybindings.json` to rebind keys, add chord shortcuts, or change the submit key.

```
/keybindings-help
```

**When to use:**
- Rebinding a key that conflicts with your terminal emulator
- Adding a chord shortcut for a frequently used command like `/simplify`

---

## Hooks vs. Asking Claude

A common confusion: asking Claude "always run the linter after you edit a file" does **not** persist beyond the current session. To make a behavior automatic and durable, it must be registered as a hook in `settings.json`. Use `/update-config` or `/session-start-hook` to set this up correctly.

| Goal | Right approach |
|---|---|
| Run `cmake --build` after every file edit | `/update-config` → PostToolUse hook |
| Verify build at the start of every session | `/session-start-hook` |
| One-off code review | `/simplify` |
| Recurring check during a session | `/loop` |
| Persistent scheduled agent | `/schedule` |
