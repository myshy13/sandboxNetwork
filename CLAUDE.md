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

## Git

- When committing, omit the "Co-Authored-By: Claude" trailer unless the
  amount that you contributed is over ~50% of the code in that 1 commit.
- After making 3 or more commits in a session that haven't been pushed
  yet, push the current branch to its remote.

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
