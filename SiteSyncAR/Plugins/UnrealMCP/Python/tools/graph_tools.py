"""
Reliable Blueprint Graph Tools for Unreal MCP (added 2026-05-29).

Wraps the FUnrealMCPGraphCommands C++ handler. These exist to replace the
hit-or-miss, one-node-at-a-time node_tools flow that CLAUDE.md flags as the
project's biggest velocity bottleneck.

Three tools:

    build_blueprint_graph    Build a whole event/function graph in ONE call from a
                             nodes + connections spec. Schema-validated wiring
                             (TryCreateConnection), struct-pin splitting, pin
                             literals, and a per-connection success report.
    describe_blueprint_node  Read the REAL pins of a node so you wire by fact, not
                             by guessing pin names. Pass node_id="all" to dump the
                             whole graph.
    split_struct_pin         Split a struct pin (FVector -> X/Y/Z, FHitResult -> ...)
                             — the operation the upstream README says needs manual
                             editor work.

Recommended workflow
---------------------
1. build_blueprint_graph with your nodes + connections.
2. If any connection reports ok=false, call describe_blueprint_node on the
   offending node to read its real pin names, then re-issue just that connection
   (build is additive — nodes already created are reused if you keep their refs,
   or send a connections-only follow-up by re-declaring the same nodes).
3. save_package / save_all_dirty_packages to persist, then compile_blueprint.
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

logger = logging.getLogger("UnrealMCP")


def register_graph_tools(mcp: FastMCP):
    """Register reliable Blueprint graph-building tools with the MCP server."""

    @mcp.tool()
    def build_blueprint_graph(
        ctx: Context,
        blueprint_name: str,
        nodes: List[Dict[str, Any]],
        connections: Optional[List[Dict[str, Any]]] = None,
        graph: str = "EventGraph",
    ) -> Dict[str, Any]:
        """Build a Blueprint graph in one atomic call.

        This is the reliable replacement for the add_node / connect_node round-trip
        flow. All nodes are created first (so refs resolve in any order), then pin
        literals are set, then every connection is wired through the K2 schema's
        TryCreateConnection (same validation the editor UI uses — type checks,
        autocasts, struct/array promotion).

        Args:
            blueprint_name: Target Blueprint (e.g. "BP_ARPlayerController_BIM").
            nodes: List of node specs. Each is a dict:
                {
                  "ref": "begin",                # YOUR alias, used in connections
                  "type": "event",               # see node types below
                  "event_name": "ReceiveBeginPlay",
                  "position": [0, 0],            # optional [x, y]
                  "pin_defaults": {"Duration": "1.0"}   # optional input literals
                }

                Node types:
                  "event"         + event_name (e.g. "ReceiveBeginPlay", "ReceiveTick")
                  "call_function" + function_name [+ target]
                                    target: omit/"self" for own+inherited funcs,
                                    or a class ("GameplayStatics",
                                    "/Script/Engine.GameplayStatics")
                  "variable_get"  + variable_name
                  "variable_set"  + variable_name
                  "self"
                  "branch"        (If/Then/Else)
                  "sequence"      (ExecutionSequence)

            connections: List of wire specs. Each is a dict:
                {
                  "from": "begin", "from_pin": "then",     # output pin
                  "to":   "delay", "to_pin":   "execute"   # input pin
                }
                For split struct pins use dotted syntax: "from_pin": "ReturnValue.X".

            graph: "EventGraph" (default) or a function/macro graph name.

        Returns:
            {
              "success": bool,                 # false if any node or wire failed
              "node_ids": {"<ref>": "<guid>"}, # GUIDs for follow-up describe/split
              "connections": [                 # one entry per requested wire
                  {"from": "begin.then", "to": "delay.execute",
                   "ok": true|false, "error": "...with available pin list..."}
              ],
              "errors": ["node-level errors, if any"]
            }

        Tip: if a connection's ok=false, the error lists the node's real pins —
        call describe_blueprint_node to confirm, then fix the pin name.

        Example — BeginPlay -> Delay(1.0) -> Print "ready":
            build_blueprint_graph(
                blueprint_name="BP_Demo",
                nodes=[
                    {"ref": "begin", "type": "event", "event_name": "ReceiveBeginPlay",
                     "position": [0, 0]},
                    {"ref": "delay", "type": "call_function", "function_name": "Delay",
                     "target": "KismetSystemLibrary", "position": [300, 0],
                     "pin_defaults": {"Duration": "1.0"}},
                    {"ref": "print", "type": "call_function", "function_name": "PrintString",
                     "target": "KismetSystemLibrary", "position": [600, 0],
                     "pin_defaults": {"InString": "ready"}},
                ],
                connections=[
                    {"from": "begin", "from_pin": "then", "to": "delay", "to_pin": "execute"},
                    {"from": "delay", "from_pin": "then",  "to": "print", "to_pin": "execute"},
                ],
            )
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Not connected to Unreal Editor."}

            params: Dict[str, Any] = {
                "blueprint_name": blueprint_name,
                "graph": graph,
                "nodes": nodes,
            }
            if connections:
                params["connections"] = connections

            response = unreal.send_command("build_blueprint_graph", params)
            if not response:
                return {"success": False, "error": "No response from Unreal."}
            if response.get("status") == "error":
                return {"success": False, "error": response.get("error")}
            return response.get("result", response)
        except Exception as e:
            logger.error(f"build_blueprint_graph failed: {e}")
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def describe_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        node_id: str = "all",
        graph: str = "EventGraph",
    ) -> Dict[str, Any]:
        """Read the real pins of a Blueprint graph node (or every node).

        Use this to discover exact pin names/types instead of guessing — the
        introspection the old add/connect flow lacked.

        Args:
            blueprint_name: Target Blueprint.
            node_id: A node GUID, or "all" (default) to dump every node in the graph.
            graph: "EventGraph" (default) or a function/macro graph name.

        Returns:
            {
              "success": true,
              "graph": "EventGraph",
              "nodes": [
                {
                  "node_id": "<guid>", "class": "K2Node_CallFunction",
                  "title": "Delay",
                  "pins": [
                    {"name": "execute", "direction": "input", "category": "exec",
                     "is_array": false, "default_value": "", "link_count": 0,
                     "can_split": false, "sub_pins": ["..."]},
                    ...
                  ]
                }
              ]
            }
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Not connected to Unreal Editor."}

            response = unreal.send_command(
                "describe_blueprint_node",
                {"blueprint_name": blueprint_name, "node_id": node_id, "graph": graph},
            )
            if not response:
                return {"success": False, "error": "No response from Unreal."}
            if response.get("status") == "error":
                return {"success": False, "error": response.get("error")}
            return response.get("result", response)
        except Exception as e:
            logger.error(f"describe_blueprint_node failed: {e}")
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def split_struct_pin(
        ctx: Context,
        blueprint_name: str,
        node_id: str,
        pin: str,
        graph: str = "EventGraph",
    ) -> Dict[str, Any]:
        """Split a struct pin into its component sub-pins.

        e.g. an FVector pin -> X / Y / Z, an FHitResult -> Location / Normal / ...
        After splitting, wire the sub-pins with dotted syntax in build_blueprint_graph
        (e.g. "to_pin": "NewLocation.Z").

        Args:
            blueprint_name: Target Blueprint.
            node_id: GUID of the node owning the pin (from build_blueprint_graph
                node_ids or describe_blueprint_node).
            pin: Name of the struct pin to split.
            graph: "EventGraph" (default) or a function/macro graph name.

        Returns:
            { "success": true, "sub_pins": ["<name>", ...] }
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Not connected to Unreal Editor."}

            response = unreal.send_command(
                "split_struct_pin",
                {"blueprint_name": blueprint_name, "node_id": node_id, "pin": pin, "graph": graph},
            )
            if not response:
                return {"success": False, "error": "No response from Unreal."}
            if response.get("status") == "error":
                return {"success": False, "error": response.get("error")}
            return response.get("result", response)
        except Exception as e:
            logger.error(f"split_struct_pin failed: {e}")
            return {"success": False, "error": str(e)}
