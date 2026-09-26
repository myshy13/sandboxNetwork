# Project Steering Core

CRITICAL: Before writing any code, you must read, merge, and strictly follow the project steering instructions located in the `.claude/rules/` folder:

- Use `.claude/rules/product.md` for feature definitions and user flows.
- Use `.claude/rules/tech.md` for tool stack constraints and syntax rules.
- Use `.claude/rules/arch.md` for folder layouts and state management constraints.

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
- Plans for larger work live in `plan.md`; keep it current as steps finish.
- Known bugs live in `bugs.md` (numbered, repo root); "bug #N" means that list. Remove an entry once fixed.

## Git

- When committing, omit the "Co-Authored-By: Claude" trailer unless the
  amount that you contributed is over ~50% of the code in that 1 commit.
- After making 3 or more commits in a session that haven't been pushed
  yet, push the current branch to its remote.
- Before writing any code, you should check with the creator, but you should also check the git status for uncommited code.
- Remind the user of things before it is too late, for example, if they are in a branch and they made changes that shouldn't be in that branch. Or they made too many changes and forgot to commit.

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
