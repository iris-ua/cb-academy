
No human reads this file. It is the map, the reuse index, and the scars.

When reporting information to me, be succinct and concise.
If my request is ambiguous, ask when the cost of being wrong is high, decide-and-mention when it's cheap.
Preplan your tool calls, and group in batches where it makes sense, wait for them to all return before reading any.

## Tests

Add a test only when it protects behavior that matters: new behavior, a bug fix,
or a regression not already covered. A useful test fails when that behavior
break and keeps passing when wording, names or internals change.

Don't add tests for non-behavioral edits (message or doc wording, renames,
refactors), for inputs the code can't receive, or for one more case in a guard
whose behavior is already tested.

## Git Commit Messages

Use short, single-line commit messages following Conventional Commits.

**Format:** `<type>: <description>`

**Prefixes:**
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation
- `refactor:` Code restructuring
- `perf:` Performance improvement
- `test:` Tests
- `style:` Formatting
- `build:` Build or dependencies
- `ci:` CI/CD changes
- `chore:` Maintenance

**Rules:**
- Prefer under 50 characters.
- Use imperative mood (e.g., "add", not "added").
- No trailing period.
- Be specific and avoid unnecessary details.
- Add a scope only when useful: `feat(auth): add login`.
