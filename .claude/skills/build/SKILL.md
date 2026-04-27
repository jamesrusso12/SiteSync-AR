---
name: build
description: Prompt the user for what they want to build in Unreal Engine, then drive the Unreal MCP server to build it. Use when the user invokes /build, says "build", or asks Claude to build something in Unreal through natural language. Requires the Unreal MCP integration from the "Unreal Engine MCP Setup" skill to already be configured and the Unreal Editor to be open.
---

# Build — Create things in Unreal from a natural-language prompt

When invoked, collect a build description from the user, then translate it into tool calls against the Unreal MCP server (the `unrealMCP` MCP connection).

## Step 1 — Preflight

Before asking the user anything, confirm the integration is live:

1. Check that `unrealMCP` is connected — the Unreal MCP tools (actor spawn, blueprint create, etc.) should be visible in the tool list.
2. If it's not connected, stop and tell the user: *"The Unreal MCP server isn't connected. Run the 'Unreal Engine MCP Setup' skill first, and make sure `MCPGameProject` is open in the Unreal Editor."* Do not attempt to build anything in this state.
3. If connected, do a lightweight ping (e.g., list actors in the current level) so you know the editor is actually reachable on port 55557 before committing to a multi-step build.

## Step 2 — Ask what to build

Use AskUserQuestion if available, otherwise ask directly. Keep it open-ended but give examples anchored in what the MCP plugin can actually do — actors, transforms, Blueprints, components, input mappings, lighting:

> **What do you want to build in Unreal?**
> Examples:
> - "A Stonehenge circle of 12 tall stones with sunset lighting"
> - "A 10×10 maze made from cube walls"
> - "A platformer course with a WASD player pawn and 15 jump platforms"
> - "A domino chain reaction of 50 boxes in an S-curve"

Capture their answer verbatim.

## Step 3 — Plan before executing

Do not immediately fire tool calls. First, draft a short build plan (3–8 steps) and show it to the user:

- Which actors to spawn, and roughly how many
- Any Blueprints to create (player pawns, input mappings, rotators)
- Lighting or camera setup
- An order of operations (spawn geometry → create Blueprints → wire inputs → set lighting)

Ask the user: *"Proceed with this plan, or adjust?"*

Only execute after they confirm. This keeps them in control during a live recording and avoids a 200-actor spawn they didn't want.

## Step 4 — Execute via MCP tools

Translate the plan into concrete MCP tool calls. Prefer:

- **Bulk transforms** — compute positions/rotations in a loop, then issue spawn calls. Don't ask Claude to "be creative" mid-stream; do the math yourself so the layout is deterministic.
- **Small batches** — spawn in groups of ~20 and check success before continuing. If the TCP connection is flaky, large batches fail silently.
- **Named actors** — give actors descriptive names (`Stone_01`, `MazeWall_3_5`, `Platform_End`) so you can reference them later without a level-wide scan.
- **Blueprints last** — geometry first, then Blueprints that reference it. Blueprint compilation errors are easier to diagnose when the scene already exists.

## Step 5 — Verify and offer iteration

After the build:

1. Ask Unreal to focus the viewport on the built actors (the plugin supports viewport focus) so the result is visible on screen.
2. Summarize what was built in one or two sentences: *"Spawned 12 stone actors in a 1000-unit-radius circle, added one directional light."*
3. Offer concrete next tweaks the user might want:
   - "Make the stones taller"
   - "Add a second ring"
   - "Dim the light and add fog"
4. Loop: if the user requests a tweak, go back to Step 3 (plan → confirm → execute).

## Failure handling

- **`connection refused` on port 55557** — editor is closed or crashed. Tell the user to reopen `MCPGameProject.uproject`.
- **Tool call returns an error** — surface the error verbatim, don't paper over it. The user may need to fix something in-editor (e.g., enable the plugin, compile Blueprints).
- **User asks for something outside plugin capabilities** (landscape sculpting, material authoring, Nanite imports, Blender model imports) — say so upfront. Don't try and fail; explain the limitation and suggest an achievable alternative.

## Notes

- This skill is iteration-friendly. A user may invoke it repeatedly in one session to keep adding to a scene. Don't assume a fresh level each time — list existing actors first if the user seems to be extending previous work.
- Keep prompts to the user short; they may be recording a video.
- Do not modify files on disk as part of this skill — all state lives in the Unreal Editor session.
