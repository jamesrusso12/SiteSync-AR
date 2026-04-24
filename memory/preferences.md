# Preferences & Collaboration Style

## Response Format
- Blueprint work → ordered node-by-node walkthroughs with pin types and connection order
- C++ → minimal files only; always show `BlueprintCallable` declaration alongside implementation
- MCP prompts → copy-paste-ready natural language blocks
- Every appended prompt block must carry a labeled header so James can route it at a glance:
  - `## Mac Prompt` — Mac / iOS compile / Xcode / wired deploy / Homebrew / gh on Mac
  - `## PC Prompt` — Windows PC / UE5 on PC / VS2022 / Cursor / MCP server
  - `## UE5 Prompt` — genuinely machine-agnostic UE5 editor work (James picks the machine)
  - `## Note` — content intended for CLAUDE.md / README.md / in-repo docs (not executed)
- If work spans machines, split into separate labeled blocks — never combine
- Clarify ambiguous Node requirements before starting work

## Code Style
- No comments explaining WHAT code does — only WHY if non-obvious
- No speculative features, abstractions, or error handling for impossible states
- Blueprint graphs organized into Functions/Macros with descriptive comment nodes

## Git
- Conventional Commits: `feat:`, `fix:`, `docs:`, `chore:`
- Branch naming: `feature/node-X.Y-short-description`
- LFS required for binary assets — always run `git lfs install` + `git lfs pull` on new machines

## Performance Non-Negotiables
- 60fps on iPhone 16 Pro
- LiDAR mesh updates throttled to 200ms minimum
- Flag any operation that must move off the game thread
- LOD strategy required for all Datasmith assets (Phase 2)
