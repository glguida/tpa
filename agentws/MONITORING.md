# TPA Cleanup Monitoring Guide

## Current Phase: Phase 1 - HAL Interface
- `hal-interface` - RUNNING (claimed by agent)

## Monitoring Commands

```bash
# Watch current status
bin/job-list

# Check specific job progress
cat jobs/hal-interface/log.md

# Watch for review jobs
bin/job-list | grep review

# Check for commits
ls jobs/*-commit/status 2>/dev/null
```

## Expected Flow

### Phase 1 (NOW)
1. ✓ `tpa-cleanup-plan` - DONE
2. ⏳ `hal-interface` - RUNNING
3. ⏸️ `hal-interface-review` - Will be created by coder
4. ⏸️ `hal-interface-commit` - Will be created by reviewer

### Phase 2 (After HAL commit)
Will be created by `hal-interface-commit` job:
- `extract-scheduler`
- `extract-channels`
- `extract-process`
- `platform-erbium`
- `platform-etsoc1`

### Phase 3 (After Phase 2 commits)
Will be created by phase monitoring job:
- `simplify-build`
- `update-examples`

## Repository Structure

The agents will create in `/home/glguida/think/new-tpa-structured/`:
```
tpa/
├── lib/
│   ├── include/tpa/
│   │   ├── tpa.h        # Public API
│   │   ├── hal.h        # HAL interface (Phase 1)
│   │   ├── channel.h    # Channel abstraction (Phase 2)
│   │   └── process.h    # Process abstraction (Phase 2)
│   └── src/
│       ├── scheduler.c  # (Phase 2)
│       ├── channel.c    # (Phase 2)
│       ├── process.c    # (Phase 2)
│       └── runtime.c    # (Phase 2)
├── hal/
│   ├── erbium/          # (Phase 3)
│   └── etsoc1/          # (Phase 3)
└── CMakeLists.txt       # (Phase 4)
```

## Git Branches

Each task creates its own branch:
- `hal-interface` branch for HAL work
- `extract-scheduler` branch for scheduler extraction
- etc.

## Success Indicators

- [ ] HAL interface defined with no platform dependencies
- [ ] Core library < 500 lines per file
- [ ] All tests passing
- [ ] Clean build on both platforms
- [ ] No abstraction leaks