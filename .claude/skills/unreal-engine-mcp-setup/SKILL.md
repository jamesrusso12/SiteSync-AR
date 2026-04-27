---
name: Unreal Engine MCP Setup
description: Interactively set up the chongdashu/unreal-mcp server and connect it to Claude Code from scratch. Walks a first-time user through Epic Games Launcher, Unreal Engine 5.5+, C++ build tools (Xcode/Visual Studio), Python 3.12+, uv, cloning the repo, compiling the plugin, and wiring the MCP config. Use when the user says "set up Unreal MCP", "connect Unreal to Claude Code", or asks to start the Unreal MCP integration from zero.
---

# Unreal Engine MCP Setup

You are guiding a user — likely a complete Unreal beginner — through installing and connecting [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp) to Claude Code. Proceed gate-by-gate. Do not advance until the current gate verifies.

## Operating principles

- **One gate at a time.** Announce the gate, run its checks, report results, then ask permission to proceed or to install what's missing.
- **Check before installing.** Every tool below has a verification command. Run it first; only install if it fails.
- **Never auto-install large or system-wide software** (Epic Games Launcher, Unreal Engine, Xcode, Visual Studio). Give the user the link/command and wait for them to confirm completion.
- **Small tools you may install after explicit user approval:** Homebrew packages, `uv`, Python via pyenv/brew.
- **Absolute paths always** when writing the MCP config — `uv --directory` breaks on relative paths.
- **Platform-aware.** Detect macOS vs. Windows via the environment and branch accordingly. The user's platform string is visible to you from the environment (e.g., `darwin`).
- **Use TodoWrite** to track the 5 gates as you go, marking each complete the moment it passes.

## Prerequisites check — run this first

Before any gate, run the following in parallel and report a summary table (tool → installed? → version). This gives the user and you a shared map of what's missing.

| Tool | Check command | Minimum |
|---|---|---|
| git | `git --version` | any |
| python3 | `python3 --version` | 3.12 |
| uv | `uv --version` | any |
| Xcode CLT (mac) | `xcode-select -p` | any path |
| Visual Studio (win) | check `where cl` or ask | 2022 w/ C++ workload |
| Epic Launcher | ask user — no CLI check | installed |
| Unreal Engine 5.5+ | ask user — no CLI check | 5.5+ |

After the scan, present a checklist to the user and ask which gate to start with (default: Gate 1).

---

## Gate 1 — Epic Games Launcher + Unreal Engine 5.5+

**Goal:** User has UE 5.5 or newer installed and launchable.

1. Ask the user if they already have the Epic Games Launcher and UE 5.5+ installed.
2. If no:
   - Tell them to download the launcher from `https://www.epicgames.com/store/en-US/download` and sign in with a free Epic account.
   - In the launcher: **Unreal Engine tab → Library → + → install 5.5 or later.** Size is ~40–80 GB; this runs in the background while you do Gates 2–4.
3. Verify by asking the user to confirm the install completed. There's no CLI check for UE.
4. **Do not gate the whole flow on this finishing** — move to Gate 2 while UE downloads.

---

## Gate 2 — C++ build tools

**Goal:** The user can compile a UE plugin locally. The UnrealMCP plugin is C++ and must be built.

### On macOS
1. Check: `xcode-select -p`
2. If it errors or returns nothing:
   - Install Xcode Command Line Tools: `xcode-select --install` (opens a GUI prompt — user clicks Install).
   - For newer UE versions, full Xcode from the App Store may be required. Mention this; recommend installing full Xcode if CLT alone isn't enough once they reach Gate 5 build.
3. Verify: `xcode-select -p` returns a path, and `clang --version` runs.

### On Windows
1. Check: `where cl` in PowerShell.
2. If missing:
   - Direct the user to install **Visual Studio 2022 Community** with the **"Game development with C++"** workload and the **"Desktop development with C++"** workload checked.
   - Link: `https://visualstudio.microsoft.com/downloads/`
3. Verify: `where cl` returns a path after a new terminal session.

---

## Gate 3 — Python 3.12+ and uv

**Goal:** `python3 --version` ≥ 3.12 AND `uv --version` works.

1. Check Python:
   - `python3 --version` — if < 3.12 or missing, install.
   - macOS: `brew install python@3.12` (ask first).
   - Windows: direct user to `https://www.python.org/downloads/` for 3.12+.
2. Check uv:
   - `uv --version`
   - If missing:
     - macOS/Linux: `curl -LsSf https://astral.sh/uv/install.sh | sh` (ask first, then source shell).
     - Windows: `powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"`
3. After install, remind the user they may need to open a new terminal for PATH updates.

---

## Gate 4 — Clone the repo and set up the Python server

**Goal:** `uv sync` succeeds inside `unreal-mcp/Python/` and `uv run unreal_mcp_server.py` launches cleanly.

1. Clone into the current project directory:
   ```
   git clone https://github.com/chongdashu/unreal-mcp.git
   ```
   (If the user already has it cloned elsewhere, ask for the path and use that — do not re-clone.)

2. Install the Python dependencies:
   ```
   cd unreal-mcp/Python
   uv sync
   ```

3. **Smoke-test the server standalone** (it will fail to connect to the UE plugin — that's expected; we just want import errors to surface now):
   ```
   uv run unreal_mcp_server.py
   ```
   Expect output showing the MCP server starting and likely a "connection refused" toward UE's TCP port. That failure is fine at this gate. Kill with Ctrl+C.

4. Record the **absolute path** to `unreal-mcp/Python` — you'll need it in Gate 6.

---

## Gate 5 — Build the UnrealMCP plugin

**Goal:** `MCPGameProject` opens in the Unreal Editor with the UnrealMCP plugin loaded.

1. Confirm UE 5.5+ from Gate 1 is now installed.
2. In the cloned repo there is a pre-wired sample project: `unreal-mcp/MCPGameProject/MCPGameProject.uproject`. **Use this project first** — don't try to integrate into a fresh project on the first run; it's a common failure path.
3. Generate IDE project files:
   - **macOS**: right-click `MCPGameProject.uproject` in Finder → "Services" → "Generate Xcode project files." If the option isn't there, run it from the Epic Launcher's context menu, or use the UE CLI: `"/Users/Shared/Epic Games/UE_5.5/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" -project="<abs path to .uproject>" -game`.
   - **Windows**: right-click `.uproject` → "Generate Visual Studio project files."
4. Build:
   - **macOS**: open the generated `.xcworkspace` in Xcode → scheme: `MCPGameProjectEditor` → target: **My Mac** → Product → Build. First build takes 10–30 min.
   - **Windows**: open the generated `.sln` → configuration: **Development Editor** → Build Solution.
5. Launch the editor by double-clicking `MCPGameProject.uproject`. If prompted to rebuild modules, click Yes.
6. Verify plugin loaded: **Edit → Plugins → search "UnrealMCP"** — should show Enabled. If not, enable it and restart the editor.
7. **The UE plugin opens a TCP server on port 55557 automatically when the editor loads.** Confirm nothing else is using that port:
   - macOS: `lsof -iTCP:55557 -sTCP:LISTEN` — expect a `UnrealEditor` process.
   - Windows: `netstat -ano | findstr 55557`.

---

## Gate 6 — Wire the MCP server into Claude Code

**Goal:** Claude Code sees `unrealMCP` as an MCP server and can list tools.

1. Use the Claude Code CLI with the **absolute path** from Gate 4:
   ```
   claude mcp add unrealMCP -- uv --directory /ABSOLUTE/PATH/TO/unreal-mcp/Python run unreal_mcp_server.py
   ```
   Scope it to the current project (default) unless the user wants it user-wide (`--scope user`).

2. Alternative: write `.claude/mcp.json` in the project root:
   ```json
   {
     "mcpServers": {
       "unrealMCP": {
         "command": "uv",
         "args": [
           "--directory",
           "/ABSOLUTE/PATH/TO/unreal-mcp/Python",
           "run",
           "unreal_mcp_server.py"
         ]
       }
     }
   }
   ```
   Substitute the real absolute path. Do not commit this file if the path is machine-specific.

3. Verify with:
   ```
   claude mcp list
   ```
   Expect `unrealMCP` to show as connected.

---

## Gate 7 — Live connection test

**Goal:** Claude executes a round-trip command against the running editor.

**Order of operations matters:**
1. Confirm `MCPGameProject` is open in the Unreal Editor (so the TCP server on 55557 is live).
2. Start a fresh `claude` session in the project directory.
3. Ask Claude something concrete, e.g.:
   - *"List all actors in the current level."*
   - *"Spawn a cube at world origin."*
4. Confirm you see the action take effect in the editor's viewport.

If this works, the integration is complete.

## Troubleshooting playbook

Hit any of these, stop and walk the user through the fix before moving on.

| Symptom | Likely cause | Fix |
|---|---|---|
| `uv: command not found` after install | PATH not refreshed | New terminal window, or `source ~/.zshrc` |
| `claude mcp list` shows unrealMCP as "failed" | Relative path in `--directory` | Rewrite config with absolute path |
| `connection refused` on port 55557 | UE editor not open, or plugin disabled | Open `MCPGameProject.uproject`; verify plugin in Edit → Plugins |
| Port 55557 already in use by non-UE process | Another app claimed the port | `lsof -iTCP:55557` → kill offender or close the app |
| Xcode build fails with "no such module" | Command Line Tools only, needs full Xcode | Install Xcode from App Store, run `sudo xcode-select -s /Applications/Xcode.app` |
| Editor prompts to rebuild modules and fails | Mismatched UE version vs. project | Confirm UE 5.5+ installed; update project's engine association |
| Claude lists the server but has no tools | MCP handshake failed | Check `uv run unreal_mcp_server.py` logs manually, look for Python traceback |

## When setup completes

After Gate 7 passes:
1. Congratulate the user. The integration is live.
2. Suggest a first-build prompt. Good starter prompts for a beginner:
   - *"Arrange 12 tall stone-colored cube actors in a circle of radius 1000 around the origin, each rotated to face inward. Add a directional light at a sunset angle."* (Stonehenge — instant visual payoff)
   - *"Generate a 10×10 grid maze from cube wall actors, 200 units per cell, with an entrance on one side and exit on the opposite."*
   - *"Create a player pawn Blueprint with WASD movement and spacebar jump, and spawn 15 cube platforms at varying heights from X=0 to X=5000."*
3. Remind the user the plugin is experimental — pin the commit SHA they cloned, so their YouTube video stays reproducible:
   ```
   cd unreal-mcp && git rev-parse HEAD
   ```

## Notes for Claude running this skill

- If the user is recording a video, they may want to rehearse. Offer to do a dry-run (walk through without actually installing) before the live attempt.
- This skill is destructive only in the sense that it installs software. Always get explicit approval before running `brew install`, `curl | sh`, or similar.
- If a gate fails twice after a fix attempt, stop looping — surface the error to the user and let them decide whether to skip, debug, or abort.
