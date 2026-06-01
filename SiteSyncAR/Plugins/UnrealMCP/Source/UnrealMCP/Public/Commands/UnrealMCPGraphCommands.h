#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/**
 * Handler class for high-level, RELIABLE Blueprint graph editing.
 *
 * Why this exists
 * ---------------
 * The original chongdashu BlueprintNode handler edits graphs one node at a time:
 * add a node -> get its GUID back -> add the next -> connect-by-GUID-and-pin-name.
 * That round-trip model is the documented "hit-or-miss" pain point in CLAUDE.md.
 * It fails because:
 *   - The caller has to GUESS pin names (e.g. "exec", "then", "Target", "ReturnValue")
 *     with no way to read the real pins before wiring.
 *   - Raw UEdGraphPin::MakeLinkTo bypasses the K2 schema, so type-incompatible or
 *     struct-split links silently no-op or leave the graph in a half-wired state.
 *   - Struct pins (FVector .X/.Y/.Z, hit-result breaks) can't be split at all.
 *   - State is spread across many tool calls, so a single mis-typed pin name strands
 *     the whole graph and there's no atomic rollback.
 *
 * This handler fixes all four with three commands:
 *
 *   build_blueprint_graph   ONE call takes a full {nodes:[...], connections:[...]}
 *                           spec, creates every node, resolves pins by intent, splits
 *                           struct pins, sets literals, and wires everything via the
 *                           K2 schema's TryCreateConnection (the same path the editor
 *                           UI uses — autocasts, type checks, array promotion). Returns
 *                           a node_id map + a per-connection success report so a partial
 *                           failure is visible instead of silent.
 *
 *   describe_blueprint_node Read the REAL pins of a node (name, direction, type,
 *                           sub-pins). Lets the caller introspect instead of guess.
 *                           Accepts a node GUID, or a node "ref" used in a prior
 *                           build spec, or "all" to dump every node in a graph.
 *
 *   split_struct_pin        Split a struct pin into its component sub-pins
 *                           (FVector -> X/Y/Z, FHitResult -> Location/Normal/...),
 *                           the one operation the upstream README calls out as
 *                           requiring manual editor work.
 *
 * The single static GetSupportedCommands() accessor is the same registry pattern the
 * System handler uses, so the bridge dispatcher needs exactly one line for the whole
 * handler — no per-command dispatcher edit (the two-edit fragility documented in
 * CLAUDE.md "Unwired dispatcher").
 */
class UNREALMCP_API FUnrealMCPGraphCommands
{
public:
    FUnrealMCPGraphCommands();

    /** Single source of truth for which commands this handler dispatches. */
    static const TArray<FString>& GetSupportedCommands();

    /** Entry point from the bridge dispatcher. */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleBuildGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDescribeNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSplitStructPin(const TSharedPtr<FJsonObject>& Params);

    // ---- internal helpers (graph building) ---------------------------------

    /** Resolve which graph a spec targets: "EventGraph" (default) or a named function graph. */
    UEdGraph* ResolveGraph(UBlueprint* Blueprint, const FString& GraphName, FString& OutError);

    /**
     * Create a single node from a spec object. Supported "type" values:
     *   event            { "event_name": "ReceiveBeginPlay" | "ReceiveTick" | ... }
     *   call_function    { "function_name": "...", "target": "self|<Class>|<var ref>" }
     *   variable_get     { "variable_name": "..." }
     *   variable_set     { "variable_name": "..." }
     *   self
     *   branch           (UK2Node_IfThenElse)
     *   sequence         (UK2Node_ExecutionSequence)
     * Optional on every node: "ref" (caller alias for connections), "position":[x,y].
     */
    UEdGraphNode* CreateNodeFromSpec(UBlueprint* Blueprint, UEdGraph* Graph,
                                     const TSharedPtr<FJsonObject>& NodeSpec, FString& OutError);

    /** Schema-correct connection. Returns false + reason on type / direction mismatch. */
    bool ConnectPins(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, FString& OutError);

    /** Find a pin, optionally a split sub-pin via "Parent.Sub" dotted syntax. */
    UEdGraphPin* ResolvePin(UEdGraphNode* Node, const FString& PinPath,
                            EEdGraphPinDirection Direction, FString& OutError);

    /** JSON dump of a single pin (name, direction, type, default, sub-pins). */
    TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin) const;

    /** JSON dump of a node (guid, class, title, pins[]). */
    TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* Node) const;
};
