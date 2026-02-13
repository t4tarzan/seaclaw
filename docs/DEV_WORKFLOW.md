# Sea-Claw Development Workflow

## Branch Strategy

```
main  ← public release (installer pulls from here)
  │
  └── dev  ← Docker-based feature development & testing
```

- **`main`** — Production. The public installer (`install.sh`) clones from this branch. Only validated, tested features land here.
- **`dev`** — Development. All new features are built and tested here inside Docker. Cherry-pick to `main` when ready.

## Quick Reference

```bash
# Build dev container
docker build -f Dockerfile.dev -t seaclaw-dev .

# Run interactive TUI
docker run -it --rm seaclaw-dev

# Run all tests (needs --privileged for ASan)
docker run --rm --privileged seaclaw-dev test

# Run doctor diagnostic
docker run --rm seaclaw-dev doctor

# Drop into shell for manual debugging
docker run -it --rm seaclaw-dev shell

# Rebuild inside container
docker run -it --rm -v $(pwd):/seaclaw seaclaw-dev build
```

## Feature Development Flow

### 1. Start on `dev`

```bash
git checkout dev
git pull origin dev
```

### 2. Develop & Test in Docker

```bash
# Edit code on host, rebuild in Docker
docker build -f Dockerfile.dev -t seaclaw-dev .

# Run tests
docker run --rm --privileged seaclaw-dev test

# Manual testing
docker run -it --rm seaclaw-dev
```

### 3. Commit to `dev`

```bash
git add -A
git commit -m "feat: description of feature"
git push origin dev
```

### 4. Validate

- All 12 test suites pass in Docker
- `--doctor` reports clean
- Manual TUI testing confirms feature works
- No regressions in existing commands

### 5. Cherry-Pick to `main`

```bash
# Note the commit hash from dev
git log --oneline -5

# Switch to main
git checkout main
git pull origin main

# Cherry-pick the validated commit(s)
git cherry-pick <commit-hash>

# Verify it builds clean on main
make clean && make release && make test

# Push — this is what the public installer pulls
git push origin main
```

### 6. Switch Back to `dev`

```bash
git checkout dev
git merge main  # Keep dev in sync
```

## Dockerfiles

| File | Purpose | Image Size |
|------|---------|------------|
| `Dockerfile` | Production runtime (multi-stage, slim) | ~90 MB |
| `Dockerfile.dev` | Development (full toolchain, tests) | ~300 MB |

## Rules

1. **Never push untested code to `main`** — people are installing from it
2. **All features start on `dev`** — no direct commits to `main`
3. **Tests must pass in Docker** before cherry-picking
4. **One feature per commit** — makes cherry-picking clean
5. **Keep `dev` synced with `main`** after each cherry-pick
