# C2 - Git and GitHub Basics

Four people and an AI agent can change firmware faster than one person. Without version control, they can also lose a working robot faster.

This ten-minute guide gives your team one shared history, a simple sprint workflow and a safe route back when the mouse worked twenty minutes ago but does not work now.

By the end, you will be able to:

- clone the team repository;
- work on the correct branch;
- commit and share a small change;
- pull a teammate's work;
- resolve a common merge conflict; and
- restore working behaviour without deleting shared history.

## 1. Understand the two systems

**Git** records versions on your computer. **GitHub** stores the shared remote copy used by the team.

```text
Your computer                GitHub
working files
     ↓
local commits   -- push -->  shared commits
                <-- pull --
```

A saved file is not automatically a Git version. A **commit** is the named checkpoint. A **push** uploads that checkpoint to GitHub.

{: .warning}
> Never commit passwords, access tokens, Wi-Fi credentials or private keys. Removing them in a later commit does not remove them from the earlier Git history.

## 2. Follow the first-time workflow

The commands below use a placeholder repository. Replace the URL with the repository supplied by your team.

### Step 1 - Clone

Clone is used once to create the local copy:

```powershell
git clone https://github.com/your-team/micromouse-firmware.git
cd micromouse-firmware
```

Confirm that you are in the correct repository:

```powershell
git remote -v
git status
```

### Step 2 - Select the sprint branch

For a six-hour event, this guide recommends one shared branch:

```powershell
git switch -c sprint/micromouse
git push -u origin sprint/micromouse
```

Only the team integrator should create and publish it. Everyone else joins the existing branch:

```powershell
git fetch origin
git switch --track origin/sprint/micromouse
```

Check the branch before editing:

```powershell
git branch --show-current
```

The result should be:

```text
sprint/micromouse
```

### Step 3 - Commit a tested change

Inspect the files before staging them:

```powershell
git status
git diff
```

Add only the files that belong to this change:

```powershell
git add src/sensors.cpp
git commit -m "fix: initialise right distance sensor"
```

A useful commit describes one outcome. Avoid messages such as `update`, `changes` or `final`.

### Step 4 - Push

Upload your commit:

```powershell
git push
```

Push shares commits. It does not automatically prove the firmware works, and it does not merge a separate branch into `master`.

### Step 5 - Pull

Download and integrate the team's latest commits:

```powershell
git pull origin sprint/micromouse
```

After the first-time sequence, the normal daily loop is:

```text
Pull → edit one task → test on hardware → commit → push
```

## 3. Use the six-hour sprint workflow

A full pull-request workflow can be too slow during a short, supervised build sprint. Use one shared branch with strict small-change rules.

| Time | Team action |
|---|---|
| Start | Clone, join `sprint/micromouse` and assign file ownership |
| Before each task | Pull, check the branch and announce which files you will edit |
| Every 15-30 minutes | Test one small result, commit it and push it |
| Before changing owner | Push and write a short handoff note |
| Before the final run | Stop feature work, pull once, build, flash and test together |
| After the event | A maintainer reviews the stable result before merging to `master` |

Recommended ownership:

```text
Person A: sensors.cpp
Person B: motors.cpp
Person C: navigation.cpp
Person D: integration, logs and testing
```

If two people need the same file, agree who edits first. Do not ask two AI agents to rewrite the same file at the same time.

Good sprint commits:

```text
feat: read left encoder
fix: correct right motor direction
test: log both distance sensors
checkpoint: mouse drives forward correctly
```

## 4. Understand a rejected push

Suppose Alex and Blair both start from the same commit:

```text
Alex  → commit → push succeeds
Blair → commit → push rejected
```

Git rejects Blair's push because GitHub now contains Alex's new commit. Git is protecting Alex's work from being overwritten.

Blair should not force push. Blair runs:

```powershell
git pull origin sprint/micromouse
```

If the changes affect different lines, Git normally merges them automatically. Blair tests the combined code and then runs:

```powershell
git push
```

If both people changed the same lines, Git pauses for a merge conflict.

## 5. Resolve a merge conflict

Imagine both people changed the distance-sensor timeout. After pulling, `src/sensors.cpp` contains:

```cpp
// Your local version (HEAD)
sensor.setTimeout(100);

// The incoming version (origin/sprint/micromouse)
sensor.setTimeout(200);
```

When Git reports a conflict, the actual file separates these two alternatives with conflict markers. They mean:

- between `<<<<<<< HEAD` and `=======` is your local version;
- between `=======` and `>>>>>>>` is the incoming version; and
- Git cannot decide which behaviour is correct.

Do not blindly keep "ours" or "theirs." Ask why each value changed and decide what the robot should actually use.

If the team tests and chooses 150 ms, replace the entire marked block with:

```cpp
sensor.setTimeout(150);
```

Then check that no conflict markers remain:

```powershell
git status
git diff
```

Build, flash and test the real mouse. When the combined result works:

```powershell
git add src/sensors.cpp
git commit -m "merge: resolve sensor timeout conflict"
git push
```

If you are unsure and have not committed the merge, stop it safely:

```powershell
git merge --abort
```

This returns to the state before the attempted merge. Ask the file owner to help before pulling again.

{: .warning}
> A file with conflict markers is not finished code. Never remove the markers, commit an untested guess and assume the conflict is solved.

## 6. Save checkpoints without copying folders

You do not need to create folders named `backup1`, `backup2` and `final-final`. A small tested commit is the checkpoint.

Use checkpoints after meaningful results:

```text
sensor reads correctly       → commit
both motors turn correctly   → commit
mouse drives one square      → commit
maze decision works          → commit
```

Before a risky AI-assisted change:

```powershell
git status
git diff
git add src/sensors.cpp
git commit -m "checkpoint: both sensors working"
git push
```

Now the entire team can identify the last known-good state.

## 7. Recover when the mouse stops working

Start by finding recent checkpoints:

```powershell
git log --oneline --decorate -10
```

Example:

```text
91c7a20 fix: change motor timing
52ab410 checkpoint: mouse drives correctly
```

Inspect what changed:

```powershell
git diff 52ab410..91c7a20
```

### Undo a committed change on the shared branch

If `91c7a20` is the bad commit, create a new commit that reverses it:

```powershell
git revert 91c7a20
```

Build, flash and test. If the working behaviour returns:

```powershell
git push
```

`git revert` preserves the shared history, so teammates can see both the original change and its reversal.

### Test an old checkpoint without deleting current work

Create a temporary local diagnostic branch:

```powershell
git switch -c diagnose/known-good 52ab410
```

Build and flash this version. When testing is finished, return to the team branch:

```powershell
git switch sprint/micromouse
```

### Protect unfinished local work

If your current edits are not ready for a commit, store them temporarily:

```powershell
git stash push -m "temporary sensor experiment"
```

Restore them later:

```powershell
git stash pop
```

If you definitely want to discard an uncommitted file change:

```powershell
git restore src/sensors.cpp
```

{: .warning}
> `git restore` discards the selected uncommitted changes. Check `git diff` first. On a shared branch, prefer `git revert` over `git reset --hard`, and never force push unless the whole team has deliberately agreed to rewrite history.

## 8. Use the sprint command card

### Start a task

```powershell
git branch --show-current
git pull origin sprint/micromouse
git status
```

### Review and share a tested result

```powershell
git diff
git add <files-you-changed>
git commit -m "type: clear outcome"
git push
```

### Investigate a regression

```powershell
git log --oneline -10
git diff <known-good-commit>..HEAD
git revert <bad-commit>
```

## Team checklist

- [ ] Everyone is on `sprint/micromouse`.
- [ ] Each active file has one owner.
- [ ] Pull before starting a task.
- [ ] Test on real hardware before committing.
- [ ] Keep each commit small and descriptive.
- [ ] Push checkpoints frequently.
- [ ] Resolve conflicts with the other file owner.
- [ ] Never force push the shared sprint branch.
- [ ] Record which commit was last known to work.

## Key takeaway

Git does not remove the need for communication or hardware testing. It gives the team named checkpoints, prevents silent overwrites and provides a safe path back when an experiment fails.