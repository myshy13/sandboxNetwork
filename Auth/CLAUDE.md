# Auth service: working agreement

This file applies only inside `Auth/` and overrides the root `CLAUDE.md` where they disagree (the C++ formatting
rules there don't apply; the git rules still do). Stack and decisions: `plan.md` "Accounts"; route order:
`routes-backlog.md`.

## Who writes what

- **The creator writes all production code** in `Auth/` (db, passwords, tickets, routes, scripts, config).
- **Claude writes the tests** (`*.test.ts`, `node --test` via `pnpm tsx --test`), and runs them when asked.
- Claude edits non-test files only when the creator asks for that specific edit.
- Claude does not write production code "as a demo" or as a fix during a review: point at the problem instead.

## Who does the problem solving

The goal is that the creator designs the solution and Claude does not translate it for them. So, for a piece the creator
is about to write:

- Claude states **what it must do** (the contract: inputs, outputs, status codes, what must never happen) and the
  **constraints** (security rules, the ticket byte layout, rate limits). It does not give the steps or the call sequence.
- The creator proposes the design (files, classes, order of operations). Claude then reviews the proposal before any
  code is written: confirms it, or names the flaw and says why it is one.
- Hints escalate only when asked or when the creator is stuck: first a question ("what happens if two requests
  register the same name?"), then the area to look at, then the API or doc to read. Never straight to the answer.
- Library questions ("how do I do X in Fastify?") are fine to answer directly with the docs-level fact. The design
  around it stays the creator's.
- On review, Claude reports problems with the file and line and the **why**, and leaves the fix to the creator.
  Say "this returns early without sending, so the next line still runs" and not the corrected line.
  A must-fix list is still ordered by severity, and a bug that is a typo-level slip may be named with its fix.

## Tests

- Written by Claude after the creator says a piece is done (or before, if the creator asks for tests first).
- One `*.test.ts` next to the code it covers. `app.inject()` against `new Db(":memory:")` for routes; a PEM from
  `generateKeyPairSync("ed25519")` for `TicketSigner`. No fixtures, no frameworks beyond `node:test`.
- Include the rejection cases, and at least one test that proves the success path, so a function that always
  fails can't pass the suite.
- When a test fails, say whether the test or the code is wrong and show the evidence. Don't edit the creator's code to
  make it pass.

## Commits

One commit per route, so a route can be reviewed or reverted alone. `authRoutes.ts`, `docs.md`, `routes-backlog.md`
and `Db/db.ts` hold several routes each, so a route's commit takes only its own hunks of those files.

- **Together in one commit:** routes that cannot work or be tested without each other (login + register share the
  accounts table and the name rules), or hunks that share a change (constructor, a shared schema or helper).
  When unsure, split, and say how the split was made.
- **Per route commit:** its handler and schema in `authRoutes.ts`, its entry in `docs.md`, its ticked line in
  `routes-backlog.md`, and any `Db` method only it uses. Tests are a separate commit after it (Claude wrote them, so
  that commit gets the trailer; the route commit follows the root rule on trailers by who wrote most of it).
- **Staging hunks without an interactive prompt**, in order of preference:
  1. `git diff <file>` to read the hunks in order, then pipe the answers: `printf 'y\nn\n' | git add -p <file>`.
     Use `s` to split a hunk; if two routes sit in one unsplittable hunk, use method 2.
  2. Build the wanted version in the index only: copy `git show :<file>` to the scratchpad, apply just the chosen
     edits there, `git hash-object -w` it, then `git update-index --cacheinfo 100644,<hash>,<path>`. The working
     tree is never touched, so the creator's unsaved IDE edits are safe. Do not swap parts of the working file in
     and out (the `[~]` trick) when the creator may be editing it.
- **Always check before committing:** `git diff --cached` shows only that route, and `git diff` (what stays unstaged)
  shows the rest. Repeat for each route. Never `git add -A` or `git commit -a` here.
- Push only when asked.

## Docs

Whenever a route is created, Claude adds it to `docs.md` in the same turn: method and path, what it is for, rate
limit, the body schema (field, type, rules), and every response (status, body, when). When a route's schema or
responses change, update its entry. A route that is still being written is marked "(in progress)".

## Security rules that tests and reviews check

Same-error for unknown name and wrong password; the dummy hash on unknown names; no passwords or tickets in logs; the
body schema has `additionalProperties: false`; `isAdmin` never comes from a request body; the private key never in
the repo; listen on `127.0.0.1` only (`trustProxy` depends on it).

## Reminders

- Imports of own TS files use the `.js` extension (NodeNext). The folder on disk is `Db`, not `DB`.
- Run `pnpm tsx` from `Auth/` so dotenv finds `.env`.
- `*.pem`, `*.db`, `.env` stay git-ignored; look at `git status` before every `git add`.
