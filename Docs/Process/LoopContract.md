# Loop contract (bounded development loop)

Replaces the open-ended "Gauntlet loop" (2026-09-25). A loop is only useful when it is bounded. Critics may find
problems; they may not change the objective.

## Every sprint is a contract, written before any work
| Field | Rule |
|---|---|
| Objective | One Backlog item, stated as the user-visible outcome. Fixed for the whole sprint. |
| Acceptance | 2–6 checkable criteria (a test, a smoke check, a measured number, a named screenshot). Written up front, never edited mid-sprint. |
| Writable scope | Explicit file/folder list. Anything else is read-only. Parallel agents never share writable files. |
| Limits | Max 3 build/fix attempts per failing check; max 2 look-and-iterate rounds for visuals; one agent per sprint unless files are disjoint; max 3 WRITING agents at once (read-only analysis/design agents that only write their own new doc files don't count, max 6 of those). |
| Plateau | Two rounds without measurable improvement on an acceptance metric = stop and report the item as open. |
| Stop | Stop when the acceptance criteria pass. Not when "it could be better". |

## Invariants: what already works must keep working
`powershell -File Tools/Gauntlet/run_invariants.ps1` (headless, no windows):
- I1 build + all automation tests (`run_gates.ps1`)
- I2 v2 map verify: layout, build, clearances (`Tools/Level/verify_v2_build.py`)
- I3 network smokes: chat, emote, fish, chore
- I4 whole match with bots reaches a decided winner (`Tools/Gauntlet/kg_match_smoke.ps1`)

Run it before the sprint (baseline) and after. A sprint that breaks an invariant is not done. Fix it within the attempt
limit, or revert the sprint.

## Rollback
- A snapshot before each sprint: `D:\KillGodot_Snapshots\<stamp>\` (Source, Tools, Docs, Config).
- The git repo exists but has no commits yet. Commits happen only when the user approves (CLAUDE.md).
- Content (.uasset/.umap) is not snapshotted. Asset-producing sprints write to NEW paths and never overwrite shipped
  assets in place.

## Scope changes need the user
- Critics, reviewers and agents report findings as **proposals** in their final report.
- Proposals go to `Docs/Backlog.md` under "Proposed (needs approval)".
- Nothing moves into the active objective without the user's OK.

## Verification debt
- Features built headless but never seen in a real session are listed in `Docs/Process/VerificationDebt.md`.
- New feature sprints pause when that list grows past ~10 unverified items; a verification sprint comes first.

## Usage budget (user, 2026-09-25: "the 5-hour limit runs out in 40 minutes")
- Max **2 agents running at once**, one of them writing code. No parallel workflows. Design and research work runs one
  at a time, after the code sprints.
- Pick the cheapest model that can do the job: code sprints inherit the main model at medium effort; docs, research,
  asset lists and mechanical fixes use `model: sonnet`.
- One reviewer per design, not three. No critic loops on docs.
- Agents read only the files their contract names (grep before reading whole files, never dump big logs) and keep
  final reports short: a table plus file paths.
- User requests are batched: new asks go into the Backlog queue and are planned in one pass, not one design workflow
  per message.
