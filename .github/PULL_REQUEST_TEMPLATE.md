<!-- Thanks for contributing to libCacheSim! See CONTRIBUTING.md for the full guide. -->

## What does this PR do?

<!-- A short description of the change and the motivation behind it.
     Link any related issue, e.g. "Fixes #123". -->

## Type of change

- [ ] Bug fix
- [ ] New eviction / admission / prefetch algorithm
- [ ] New trace reader or trace format
- [ ] Performance improvement
- [ ] Documentation
- [ ] Build / CI
- [ ] Other:

## How was it tested?

<!-- The commands you ran. Sample traces live in data/, e.g.
     cd _build && ./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb -->

- [ ] `ctest --test-dir _build --output-on-failure` passes
- [ ] Build is warning-free (CI uses `-Wall -Wextra -Werror`)

## Results

<!-- If this changes miss ratios or throughput, paste the before/after numbers
     and the command you used. Delete this section if it does not apply. -->

## Checklist

- [ ] Formatted with `clang-format` (or the pre-commit hook from `scripts/setup_hooks.sh`)
- [ ] Added or updated tests
- [ ] Added or updated documentation
- [ ] New algorithms are registered in the CLI and listed in the [README](../README.md#supported-algorithms)
