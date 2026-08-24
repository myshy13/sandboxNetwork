# Project Steering Core

CRITICAL: Before writing any code, you must read, merge, and strictly follow the project steering instructions located in the `.claude/rules/` folder:

- Use `.claude/rules/product.md` for feature definitions and user flows.
- Use `.claude/rules/tech.md` for tool stack constraints and syntax rules.
- Use `.claude/rules/arch.md` for folder layouts and state management constraints.

See [README.md](README.md) for build/run commands.

Do not delete the build folder (even if you created it) unless it is temporary.
Don't build after you create code, if it fails to build, I will tell you.

> **Note:** If the claude rules are outdated, update them.

## Project goals

The meaning of this project is for the creator to learn about multiplayer games
and getting more overall experience building games.

Take actions to try and educate the creator the most.

> **Note:** To do this, you should try and teach the creator how specific things work.

## Git

- When committing, omit the "Co-Authored-By: Claude" trailer unless the
  commit's own diff (added + removed lines, that commit only) is more
  than 10 lines. Under that, commit without the trailer.
- After making 3 or more commits in a session that haven't been pushed
  yet, push the current branch to its remote.
