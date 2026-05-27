"""
System Tools for Unreal MCP.

Exposes the FUnrealMCPSystemCommands handlers added 2026-05-27:

    execute_python              run arbitrary editor Python without a C++ rebuild
    save_all_dirty_packages     persist MCP edits to .uasset / .umap on disk
    save_package                save a single asset by package path
    reparent_actor_root         swap which scene component is an actor's root
    list_system_commands        introspection — what's wired in the bridge

The execute_python tool is the architectural unlock: it lets future Claude
sessions reach the full `unreal` Python API (Datasmith import, Merge Actors,
DataAsset edits, asset rename/move, etc.) without needing a new C++ handler +
plugin DLL rebuild for every new operation.
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

logger = logging.getLogger("UnrealMCP")


def register_system_tools(mcp: FastMCP):
    """Register system-level tools with the MCP server."""

    @mcp.tool()
    def execute_python(
        ctx: Context,
        script: Optional[str] = None,
        script_file: Optional[str] = None,
        mode: str = "statement",
    ) -> Dict[str, Any]:
        """Execute Python code inside the Unreal Editor's Python environment.

        Exactly one of `script` or `script_file` must be provided.

        Args:
            script: Inline Python source. Multi-line OK in "statement" mode.
            script_file: Path to a .py file. Absolute, project-relative, or
                under the Plugins/UnrealMCP/Python/scripts/ tree.
            mode: One of "statement" (default, multi-line, no return),
                "file" (same as `UnrealEditor-Cmd -run=pythonscript`),
                or "eval" (single expression, returns its string value).

        Returns:
            { "success": bool, "result": str, "log_output": str, "errors": [str, ...] }

        Examples:
            execute_python(script="import unreal; print(unreal.SystemLibrary.get_engine_version())")

            execute_python(script='''
                import unreal
                eas = unreal.EditorAssetLibrary
                for p in eas.list_assets("/Game/Maps", recursive=True):
                    print(p)
            ''')

            execute_python(script_file="dev/import_datasmith.py", mode="file")
        """
        from unreal_mcp_server import get_unreal_connection

        if not script and not script_file:
            return {"success": False, "error": "Provide either 'script' or 'script_file'."}

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Not connected to Unreal Editor."}

            params: Dict[str, Any] = {"mode": mode}
            if script:
                params["script"] = script
            if script_file:
                params["script_file"] = script_file

            response = unreal.send_command("execute_python", params)
            if not response:
                return {"success": False, "error": "No response from Unreal."}

            if response.get("status") == "error":
                return {"success": False, "error": response.get("error", "Unknown error")}

            return response.get("result", response)

        except Exception as e:
            logger.error(f"execute_python failed: {e}")
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def save_all_dirty_packages(
        ctx: Context,
        save_content_packages: bool = True,
        save_map_packages: bool = True,
    ) -> Dict[str, Any]:
        """Save all dirty packages (.uasset + .umap) to disk.

        Use this after a batch of structural MCP edits so the changes
        actually persist to the working tree instead of living only in
        the editor's in-memory world.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Not connected to Unreal Editor."}

            response = unreal.send_command(
                "save_all_dirty_packages",
                {
                    "save_content_packages": save_content_packages,
                    "save_map_packages": save_map_packages,
                    "prompt_user": False,
                },
            )
            if not response:
                return {"success": False, "error": "No response from Unreal."}
            if response.get("status") == "error":
                return {"success": False, "error": response.get("error")}
            return response.get("result", response)
        except Exception as e:
            logger.error(f"save_all_dirty_packages failed: {e}")
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def save_package(ctx: Context, package_path: str) -> Dict[str, Any]:
        """Save a single asset by package path.

        Args:
            package_path: e.g. "/Game/Blueprints/BP_ARPlayerController"
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Not connected to Unreal Editor."}

            response = unreal.send_command("save_package", {"package_path": package_path})
            if not response:
                return {"success": False, "error": "No response from Unreal."}
            if response.get("status") == "error":
                return {"success": False, "error": response.get("error")}
            return response.get("result", response)
        except Exception as e:
            logger.error(f"save_package failed: {e}")
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def reparent_actor_root(
        ctx: Context, actor_name: str, new_root: str
    ) -> Dict[str, Any]:
        """Promote a scene component to be an actor's RootComponent.

        Solves the "MeshComponent is the root, SetActorScale3D clobbers its
        RelativeScale3D" class of bugs (see CLAUDE.md 2026-05-20 BIMMesh entry).

        Args:
            actor_name: Actor label or unique name in the current level.
            new_root: Name of an existing SceneComponent to promote.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Not connected to Unreal Editor."}

            response = unreal.send_command(
                "reparent_actor_root",
                {"actor_name": actor_name, "new_root": new_root},
            )
            if not response:
                return {"success": False, "error": "No response from Unreal."}
            if response.get("status") == "error":
                return {"success": False, "error": response.get("error")}
            return response.get("result", response)
        except Exception as e:
            logger.error(f"reparent_actor_root failed: {e}")
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def list_system_commands(ctx: Context) -> List[str]:
        """List every command wired into the System dispatcher.

        Useful when an `Unknown command` reply suggests a stale-DLL or
        missing-dispatcher-entry bug — confirms what the running plugin
        actually advertises.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return []
            response = unreal.send_command("list_commands", {})
            if not response:
                return []
            result = response.get("result", response)
            return result.get("system_commands", [])
        except Exception as e:
            logger.error(f"list_system_commands failed: {e}")
            return []
