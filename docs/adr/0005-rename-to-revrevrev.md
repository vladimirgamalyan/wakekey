# 0005. Rename project to RevRevRev

- Status: Accepted
- Date: 2026-07-20

## Context

The project was named WakeKey from its initial commit. A new name was chosen:
RevRevRev, after the military wake-up call "Reveille! Reveille! Reveille!" —
traditionally called three times over a ship's or camp's PA system to rouse
everyone. "Rev" is short for reveille, said three times, giving RevRevRev. The
call's meaning — wake up — matches exactly what this device does to a sleeping
host, which is the whole of its function.

## Decision

Rename the project from WakeKey to RevRevRev in documentation (README, ADRs)
and in all future references.

Two references to the old name sat outside this repository's tracked files:

- The GitHub remote, `vladimirgamalyan/wakekey`. Renamed to
  `vladimirgamalyan/revrevrev` via `gh repo rename`; the local `origin` remote
  URL was updated to match. GitHub redirects the old URL, so existing clones
  and forks keep working.
- The local working directory, `wakekey` on disk. Renaming it required
  exclusive access to the directory, which an active session against that
  path could not release from within itself (both the Bash and PowerShell
  tool shells held it as their current directory). Done manually outside an
  active session: `Rename-Item D:\projects\wakekey D:\projects\revrevrev`.

## Consequences

Easier:

- The name now carries a clear, on-the-nose provenance instead of a generic
  descriptive one.

Harder:

- Anyone with an existing clone should point it at the new remote URL and
  rename their local folder to match; GitHub's redirect covers the URL, but
  the local path is not automatic.
