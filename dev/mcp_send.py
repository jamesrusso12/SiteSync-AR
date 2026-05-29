#!/usr/bin/env python3
"""
Tiny CLI for the UnrealMCP TCP server (port 55557) on the LIVE editor.
Lets this Claude session drive the Mac editor without the MCP tool layer.

Usage:
    # run an editor-python file in the live editor:
    python3 dev/mcp_send.py exec_file dev/some_op.py

    # run an inline statement:
    python3 dev/mcp_send.py exec 'import unreal; print(unreal.SystemLibrary.get_engine_version())'

    # save all dirty packages (persist edits to disk):
    python3 dev/mcp_send.py save_all

    # raw command + json params:
    python3 dev/mcp_send.py raw list_commands '{}'

Wire protocol (per CLAUDE.md): connect 127.0.0.1:55557, send
{"type": <cmd>, "params": {...}} as raw UTF-8, read until JSON parses,
server closes after each command.
"""
import socket, json, sys

HOST, PORT = "127.0.0.1", 55557


def send(cmd, params=None, timeout=120):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((HOST, PORT))
    s.sendall(json.dumps({"type": cmd, "params": params or {}}).encode("utf-8"))
    buf = b""
    while True:
        try:
            chunk = s.recv(65536)
        except socket.timeout:
            break
        if not chunk:
            break
        buf += chunk
        try:
            json.loads(buf.decode("utf-8"))
            break
        except Exception:
            continue
    s.close()
    return buf.decode("utf-8", "replace")


def main():
    if len(sys.argv) < 2:
        print("usage: mcp_send.py {exec|exec_file|save_all|save|raw} ...")
        sys.exit(2)
    op = sys.argv[1]
    if op == "exec":
        out = send("execute_python", {"script": sys.argv[2], "mode": "statement"})
    elif op == "exec_file":
        out = send("execute_python", {"script_file": sys.argv[2], "mode": "file"})
    elif op == "save_all":
        out = send("save_all_dirty_packages", {})
    elif op == "save":
        out = send("save_package", {"package_path": sys.argv[2]})
    elif op == "raw":
        params = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
        out = send(sys.argv[2], params)
    else:
        print("unknown op: " + op)
        sys.exit(2)
    print(out)


if __name__ == "__main__":
    main()
