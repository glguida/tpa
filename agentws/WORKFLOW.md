# TPA Cleanup Workflow

## Job Flow

Each feature follows this pipeline:

```
plan → code → review → commit
                ↓
         (code job for fixes)
```

## Job Types and Agents

### 1. Planning Agent (`type: plan`)
```bash
pi --model opus "You are a plan agent. Read roles/planner.md. Work on jobs of type plan."
```
- Breaks down high-level goals into concrete tasks
- Creates detailed specifications for coders
- Defines acceptance criteria and dependencies

### 2. Coding Agent (`type: code`)
```bash
pi --model sonnet "You are a code agent. Read roles/coder.md. Work on jobs of type code."
```
- Implements features according to spec
- Creates git branches for each task
- Ensures code builds and tests pass
- When done: Creates review job

### 3. Review Agent (`type: review`)
```bash
pi --model sonnet "You are a review agent. Read roles/reviewer.md. Work on jobs of type review."
```
- Reviews code quality and architecture
- Checks adherence to spec
- Verifies tests and documentation
- Decision: Approve (→commit) or Request changes (→rework)

### 4. Commit Agent (`type: commit`)
```bash
pi --model sonnet "You are a commit agent. Read roles/committer.md. Work on jobs of type commit."
```
- Merges approved code to main branch
- Runs final integration tests
- Updates documentation if needed
- Ensures clean git history

## Execution Phases

### Phase 1: HAL Interface (Foundation)
1. `hal-interface` (code)
2. `hal-interface-review` (review)
3. `hal-interface-commit` (commit)

**Must complete before Phase 2 & 3**

### Phase 2: Core Extraction (Parallel)
Can run in parallel after HAL is committed:
- `extract-scheduler` → review → commit
- `extract-channels` → review → commit
- `extract-process` → review → commit

### Phase 3: Platform Implementations (Parallel)
After HAL is committed:
- `platform-erbium` → review → commit
- `platform-etsoc1` → review → commit

### Phase 4: Integration
After all above are committed:
- `simplify-build` → review → commit
- `update-examples` → review → commit

## Monitoring Progress

```bash
# See all jobs
bin/job-list

# See what's running
bin/job-list running

# See what needs review
bin/job-list pending | grep review

# Watch for new review jobs
bin/job-watch pending | grep review
```

## Quality Gates

Each review checks:
1. **Code quality**: Clean, maintainable, follows patterns
2. **Specification adherence**: Does what was asked
3. **Tests**: All tests pass, new tests added where needed
4. **Documentation**: Comments and docs updated
5. **No regressions**: Existing functionality preserved
6. **Build**: Clean compile with no warnings

## Handling Review Feedback

When review requests changes:
1. Review agent creates: `bin/job-create {task}-fix-{issue} -t code`
2. Code agent picks it up like any other code job
3. Code agent creates new review job when done
4. Process repeats until approved

Example: `hal-interface-fix-memory-barriers` or `extract-scheduler-fix-atomics`

## Git Branch Strategy

- `main`: Stable, all tests passing
- `feature/{task-name}`: One branch per code job
- Commits are atomic and well-documented
- No force pushes after review starts
- Clean history (squash if needed during commit)

## Success Metrics

- All 8 core tasks completed and merged
- Zero platform-specific code in lib/
- Each file < 500 lines
- All tests passing
- Clean build on both platforms
- Documentation complete