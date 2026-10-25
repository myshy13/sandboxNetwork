# Project Steering Core

CRITICAL: Before writing any code, you must read, merge, and strictly follow the project steering instructions located in the `.claude/rules/` folder:

- Use `.claude/rules/product.md` for feature definitions and user flows.
- Use `.claude/rules/tech.md` for tool stack constraints and syntax rules.
- Use `.claude/rules/arch.md` for folder layouts and state management constraints.

At the start of a session, read `context.md` (repo root) for where the last session stopped. If it doesn't exist,
create it from `plan.md`, `git status` and `git log`, then carry on. Update it before the creator leaves.

See [README.md](README.md) for build/run commands.

Do not delete the build folder (even if you created it) unless it is temporary.
Don't build after you create code, if it fails to build, I will tell you.

> **Note:** If the claude rules are outdated, update them before you do anything else.

## Project goals

The meaning of this project is for the creator to learn about multiplayer games
and getting more overall experience building games.

Take actions to try and educate the creator the most.
Do not do any code unless you have concrete evidence that they already would know how.

> **Note:** To do this, you should try and teach the creator how specific things work.

## Working agreement

- By default the creator writes the code. Claude explains the design first, then reviews what they wrote
  (correctness, edge cases, the formatting rules below). Claude writes a piece only when asked for that piece.
- Explain how to implement something as pseudocode-level steps: name the functions, data structures and order
  of operations, but leave the real C++ to the creator. No finished code and no long prose walkthroughs.
- Point at the tricky part (an ordering, an edge case, an invariant) and say plainly why it is tricky, without writing the fix.
  Ask a question only when there is a real design choice for the creator to make, never as a quiz.
- The creator trusts Claude's design judgement: give one recommendation with the reasoning, not a menu of options.
- When an explanation uses a name that isn't defined in the code (a made-up variable like `toSun` or `D`), say what it
  means and where it comes from. Never assume the creator knows.
- Plans for larger work live in `plan.md`; keep it current as steps finish.
- Known bugs live in `bugs.md` (numbered, repo root); "bug #N" means that list. Remove an entry once fixed. Update the numbering if you update the list

## Git

- Branch names are `type/what-it-is` in kebab-case: `feature/block-registry`, `bug/shadow-hop`.
- Small changes (docs, backlog/plan/context updates, a typo or one-line fix, tuning a constant) may be committed straight
  on `main`. Anything medium or bigger (what would need a minor version bump) and any change to gameplay mechanics gets
  its own branch. If a change looks like it qualifies for its own branch, ask the creator before branching or
  reminding; the creator decides.
- Merging a branch: major changes and any change to gameplay mechanics, however small, go through a GitHub PR
  (`gh pr create`, then `gh pr merge --merge`); the rest get a regular local `git merge --no-ff` into `main`, then push.
  Always a merge commit, never a fast-forward or a squash.
- When committing, omit the "Co-Authored-By: Claude" trailer unless the
  amount that you contributed is over ~50% of the code in that 1 commit.
- After making 3 or more commits in a session that haven't been pushed
  yet, ask the creator whether to push the current branch to its remote.
- Never use git worktrees (no EnterWorktree, no `isolation: "worktree"`). Edit in the main checkout, on the current
  branch, after checking `git status`. If a tool forces a worktree, stop and tell the creator instead.
- Before writing any code, you should check with the creator, but you should also check the git status for uncommited code.
- If told to commit while the commit contains a bug already known (found in review or by a failing test), question it
  before committing: name the bug and ask whether to fix it first or commit anyway. Don't commit it silently.
- Remind the user of things before it is too late, for example, if they are in a branch and they made changes that shouldn't be in that branch. Or they made too many changes and forgot to commit.
  One or two small unrelated commits on a branch are fine; don't remind about those. At 3 or more, remind them to move
  the unrelated work: cherry-pick onto `main` if it is small, or its own branch if it is big enough (same small-vs-big
  rules as the Git section above).

## Formatting guide

- Limit comments to one line.
- Preferably OOP.
- Section banners: `// ==== name ==== //`.
- Align runs of consecutive `=` assignments (clang-format `AlignConsecutiveAssignments`).
- New protocol messages: a struct in `protocol.hpp` with an inline
  `template <class A> void serialize(A &ar)`; add a matching `Type` enum entry.
- Wrap a platform API (ENet, Emscripten sockets) behind an interface with one
  class per backend; keep `#ifdef` out of game code (see `Transport`).
- Server reads identity from the connection, never from the message payload.
- `constexpr` constants over `#define`; no `static` functions in headers
  (`inline` or `constexpr` instead).
- Don't delete an explanatory comment to satisfy "one line" — shorten it.
- Formatting wise, the creator is always correct

> **Note:** Edit this file if it needs to be updated
