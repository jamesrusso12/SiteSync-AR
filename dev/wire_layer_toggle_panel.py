"""
Wire the layer toggle panel into BP_ARPlayerController_BIM.

Prereqs (must be done first in UE editor):
  1. WBP_LayerRow created with StoredComponent (StaticMeshComponent var)
     and OnCheckStateChanged → SetVisibility logic
  2. WBP_LayerTogglePanel created with LayerScrollBox, PopulateFromActor function,
     and TogglePanelWidget variable exposed
  3. BP_ARPlayerController_BIM has TogglePanelWidget variable
     (type: WBP_LayerTogglePanel, Instance Editable: false)
  4. BP_ClashTestRig placed in SiteSync_BIMTest (via place_clash_test_rig.py)
  5. Mac editor rebuilt with custom_event support:
       Build.sh SiteSyncAREditor Mac Development -project=...

Adds two things to BP_ARPlayerController_BIM:
  A) Custom event "ShowLayerPanel" — find ClashTestRig, create panel, populate, add to viewport
  B) Custom event "HideLayerPanel" — remove panel from viewport, clear variable

Then James calls ShowLayerPanel from the end of his existing BeginPlay chain (one wire).

Run with:
    python3 dev/mcp_send.py raw build_blueprint_graph '<SHOW_GRAPH_JSON>'
    python3 dev/mcp_send.py raw build_blueprint_graph '<HIDE_GRAPH_JSON>'

Or paste the JSON directly from this file into mcp_send.py raw calls.
"""

SHOW_GRAPH = {
    "blueprint_name": "BP_ARPlayerController_BIM",
    "graph": "EventGraph",
    "nodes": [
        {
            "ref": "show_evt",
            "type": "custom_event",
            "event_name": "ShowLayerPanel",
            "position": [2000, 400]
        },
        {
            "ref": "get_rig",
            "type": "call_function",
            "function_name": "GetActorOfClass",
            "target": "GameplayStatics",
            "position": [2200, 400]
        },
        {
            "ref": "is_valid",
            "type": "branch",
            "position": [2480, 400]
        },
        {
            "ref": "create_panel",
            "type": "call_function",
            "function_name": "CreateWidget",
            "target": "WidgetBlueprintLibrary",
            "pin_defaults": {
                "WidgetType": "WBP_LayerTogglePanel"
            },
            "position": [2700, 350]
        },
        {
            "ref": "set_panel",
            "type": "variable_set",
            "variable_name": "TogglePanelWidget",
            "position": [2980, 350]
        },
        {
            "ref": "populate",
            "type": "call_function",
            "function_name": "PopulateFromActor",
            "position": [3200, 350]
        },
        {
            "ref": "add_viewport",
            "type": "call_function",
            "function_name": "AddToViewport",
            "position": [3450, 350]
        }
    ],
    "connections": [
        {"from": "show_evt", "from_pin": "then", "to": "get_rig", "to_pin": "execute"},
        {"from": "get_rig", "from_pin": "then", "to": "is_valid", "to_pin": "execute"},
        {"from": "get_rig", "from_pin": "ReturnValue", "to": "is_valid", "to_pin": "Condition"},
        {"from": "is_valid", "from_pin": "True", "to": "create_panel", "to_pin": "execute"},
        {"from": "create_panel", "from_pin": "then", "to": "set_panel", "to_pin": "execute"},
        {"from": "create_panel", "from_pin": "ReturnValue", "to": "set_panel", "to_pin": "TogglePanelWidget"},
        {"from": "set_panel", "from_pin": "then", "to": "populate", "to_pin": "execute"},
        {"from": "set_panel", "from_pin": "TogglePanelWidget", "to": "populate", "to_pin": "Target"},
        {"from": "get_rig", "from_pin": "ReturnValue", "to": "populate", "to_pin": "InActor"},
        {"from": "populate", "from_pin": "then", "to": "add_viewport", "to_pin": "execute"},
        {"from": "set_panel", "from_pin": "TogglePanelWidget", "to": "add_viewport", "to_pin": "Target"}
    ]
}

HIDE_GRAPH = {
    "blueprint_name": "BP_ARPlayerController_BIM",
    "graph": "EventGraph",
    "nodes": [
        {
            "ref": "hide_evt",
            "type": "custom_event",
            "event_name": "HideLayerPanel",
            "position": [2000, 700]
        },
        {
            "ref": "get_panel",
            "type": "variable_get",
            "variable_name": "TogglePanelWidget",
            "position": [2200, 700]
        },
        {
            "ref": "is_valid_panel",
            "type": "branch",
            "position": [2380, 700]
        },
        {
            "ref": "remove",
            "type": "call_function",
            "function_name": "RemoveFromParent",
            "position": [2600, 650]
        },
        {
            "ref": "clear_panel",
            "type": "variable_set",
            "variable_name": "TogglePanelWidget",
            "position": [2800, 650]
        }
    ],
    "connections": [
        {"from": "hide_evt", "from_pin": "then", "to": "is_valid_panel", "to_pin": "execute"},
        {"from": "get_panel", "from_pin": "TogglePanelWidget", "to": "is_valid_panel", "to_pin": "Condition"},
        {"from": "is_valid_panel", "from_pin": "True", "to": "remove", "to_pin": "execute"},
        {"from": "get_panel", "from_pin": "TogglePanelWidget", "to": "remove", "to_pin": "Target"},
        {"from": "remove", "from_pin": "then", "to": "clear_panel", "to_pin": "execute"}
    ]
}

if __name__ == "__main__":
    import json
    print("=== SHOW_GRAPH ===")
    print(json.dumps(SHOW_GRAPH))
    print()
    print("=== HIDE_GRAPH ===")
    print(json.dumps(HIDE_GRAPH))
