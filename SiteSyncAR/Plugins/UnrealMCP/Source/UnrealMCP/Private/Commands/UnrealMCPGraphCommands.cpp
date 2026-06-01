#include "Commands/UnrealMCPGraphCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Self.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCPGraph, Log, All);

FUnrealMCPGraphCommands::FUnrealMCPGraphCommands()
{
}

const TArray<FString>& FUnrealMCPGraphCommands::GetSupportedCommands()
{
    static const TArray<FString> Commands = {
        TEXT("build_blueprint_graph"),
        TEXT("describe_blueprint_node"),
        TEXT("split_struct_pin"),
    };
    return Commands;
}

TSharedPtr<FJsonObject> FUnrealMCPGraphCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("build_blueprint_graph"))    return HandleBuildGraph(Params);
    if (CommandType == TEXT("describe_blueprint_node"))  return HandleDescribeNode(Params);
    if (CommandType == TEXT("split_struct_pin"))         return HandleSplitStructPin(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown graph command: %s"), *CommandType));
}

// =============================================================================
// Helpers
// =============================================================================

UEdGraph* FUnrealMCPGraphCommands::ResolveGraph(UBlueprint* Blueprint, const FString& GraphName, FString& OutError)
{
    if (GraphName.IsEmpty() || GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
    {
        UEdGraph* EventGraph = FUnrealMCPCommonUtils::FindOrCreateEventGraph(Blueprint);
        if (!EventGraph)
        {
            OutError = TEXT("Failed to resolve the EventGraph");
        }
        return EventGraph;
    }

    // Named function / macro graph.
    for (UEdGraph* G : Blueprint->FunctionGraphs)
    {
        if (G && G->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            return G;
        }
    }
    for (UEdGraph* G : Blueprint->UbergraphPages)
    {
        if (G && G->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            return G;
        }
    }

    OutError = FString::Printf(TEXT("Graph not found: %s"), *GraphName);
    return nullptr;
}

UEdGraphNode* FUnrealMCPGraphCommands::CreateNodeFromSpec(UBlueprint* Blueprint, UEdGraph* Graph,
                                                         const TSharedPtr<FJsonObject>& NodeSpec, FString& OutError)
{
    FString NodeType;
    if (!NodeSpec->TryGetStringField(TEXT("type"), NodeType))
    {
        OutError = TEXT("node spec missing 'type'");
        return nullptr;
    }

    FVector2D Position(0.f, 0.f);
    if (NodeSpec->HasField(TEXT("position")))
    {
        Position = FUnrealMCPCommonUtils::GetVector2DFromJson(NodeSpec, TEXT("position"));
    }

    NodeType = NodeType.ToLower();

    if (NodeType == TEXT("event"))
    {
        FString EventName;
        NodeSpec->TryGetStringField(TEXT("event_name"), EventName);
        if (EventName.IsEmpty())
        {
            OutError = TEXT("event node requires 'event_name'");
            return nullptr;
        }
        UK2Node_Event* Node = FUnrealMCPCommonUtils::CreateEventNode(Graph, EventName, Position);
        if (!Node) { OutError = FString::Printf(TEXT("could not create event node '%s'"), *EventName); }
        return Node;
    }

    if (NodeType == TEXT("self"))
    {
        return FUnrealMCPCommonUtils::CreateSelfReferenceNode(Graph, Position);
    }

    if (NodeType == TEXT("variable_get"))
    {
        FString VarName;
        NodeSpec->TryGetStringField(TEXT("variable_name"), VarName);
        UK2Node_VariableGet* Node = FUnrealMCPCommonUtils::CreateVariableGetNode(Graph, Blueprint, VarName, Position);
        if (!Node) { OutError = FString::Printf(TEXT("variable not found for get: '%s'"), *VarName); }
        return Node;
    }

    if (NodeType == TEXT("variable_set"))
    {
        FString VarName;
        NodeSpec->TryGetStringField(TEXT("variable_name"), VarName);
        UK2Node_VariableSet* Node = FUnrealMCPCommonUtils::CreateVariableSetNode(Graph, Blueprint, VarName, Position);
        if (!Node) { OutError = FString::Printf(TEXT("variable not found for set: '%s'"), *VarName); }
        return Node;
    }

    if (NodeType == TEXT("branch"))
    {
        UK2Node_IfThenElse* Node = NewObject<UK2Node_IfThenElse>(Graph);
        Node->NodePosX = Position.X;
        Node->NodePosY = Position.Y;
        Graph->AddNode(Node, true);
        Node->CreateNewGuid();
        Node->PostPlacedNewNode();
        Node->AllocateDefaultPins();
        return Node;
    }

    if (NodeType == TEXT("sequence"))
    {
        UK2Node_ExecutionSequence* Node = NewObject<UK2Node_ExecutionSequence>(Graph);
        Node->NodePosX = Position.X;
        Node->NodePosY = Position.Y;
        Graph->AddNode(Node, true);
        Node->CreateNewGuid();
        Node->PostPlacedNewNode();
        Node->AllocateDefaultPins();
        return Node;
    }

    if (NodeType == TEXT("call_function"))
    {
        FString FunctionName;
        NodeSpec->TryGetStringField(TEXT("function_name"), FunctionName);
        if (FunctionName.IsEmpty())
        {
            OutError = TEXT("call_function node requires 'function_name'");
            return nullptr;
        }

        FString Target;
        NodeSpec->TryGetStringField(TEXT("target"), Target);

        UFunction* Function = nullptr;

        if (!Target.IsEmpty() && !Target.Equals(TEXT("self"), ESearchCase::IgnoreCase))
        {
            // Try resolve as an external class. Accept both raw ("GameplayStatics")
            // and script-path ("/Script/Engine.GameplayStatics") forms.
            UClass* TargetClass = nullptr;
            if (Target.StartsWith(TEXT("/Script/")))
            {
                TargetClass = LoadObject<UClass>(nullptr, *Target);
            }
            if (!TargetClass)
            {
                TargetClass = FindFirstObject<UClass>(*Target, EFindFirstObjectOptions::None);
            }
            if (!TargetClass && !Target.StartsWith(TEXT("U")))
            {
                TargetClass = FindFirstObject<UClass>(*(FString(TEXT("U")) + Target), EFindFirstObjectOptions::None);
            }

            if (TargetClass)
            {
                for (UClass* C = TargetClass; C && !Function; C = C->GetSuperClass())
                {
                    Function = C->FindFunctionByName(FName(*FunctionName));
                    if (!Function)
                    {
                        for (TFieldIterator<UFunction> It(C, EFieldIteratorFlags::ExcludeSuper); It; ++It)
                        {
                            if (It->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
                            {
                                Function = *It;
                                break;
                            }
                        }
                    }
                }
                if (Function)
                {
                    UK2Node_CallFunction* Node = FUnrealMCPCommonUtils::CreateFunctionCallNode(Graph, Function, Position);
                    if (!Node) { OutError = TEXT("failed to create external call_function node"); }
                    return Node;
                }
            }
        }

        // Fall back to the Blueprint's own class (self functions + inherited members).
        UClass* SearchClass = Blueprint->GeneratedClass ? Blueprint->GeneratedClass : Blueprint->ParentClass;
        for (UClass* C = SearchClass; C && !Function; C = C->GetSuperClass())
        {
            Function = C->FindFunctionByName(FName(*FunctionName));
            if (!Function)
            {
                for (TFieldIterator<UFunction> It(C, EFieldIteratorFlags::ExcludeSuper); It; ++It)
                {
                    if (It->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
                    {
                        Function = *It;
                        break;
                    }
                }
            }
        }

        if (!Function)
        {
            OutError = FString::Printf(TEXT("function not found: '%s' (target '%s')"),
                *FunctionName, Target.IsEmpty() ? TEXT("self") : *Target);
            return nullptr;
        }

        UK2Node_CallFunction* Node = FUnrealMCPCommonUtils::CreateFunctionCallNode(Graph, Function, Position);
        if (!Node) { OutError = TEXT("failed to create call_function node"); }
        return Node;
    }

    OutError = FString::Printf(TEXT("unsupported node type: '%s'"), *NodeType);
    return nullptr;
}

UEdGraphPin* FUnrealMCPGraphCommands::ResolvePin(UEdGraphNode* Node, const FString& PinPath,
                                                 EEdGraphPinDirection Direction, FString& OutError)
{
    if (!Node)
    {
        OutError = TEXT("null node");
        return nullptr;
    }

    // Dotted syntax "Parent.Sub" addresses a split-struct sub-pin.
    FString ParentName, SubName;
    const bool bHasSub = PinPath.Split(TEXT("."), &ParentName, &SubName);
    const FString& TopName = bHasSub ? ParentName : PinPath;

    auto MatchPin = [&](UEdGraphPin* Pin, const FString& Name) -> bool
    {
        if (!Pin) return false;
        if (Direction != EGPD_MAX && Pin->Direction != Direction) return false;
        return Pin->PinName.ToString().Equals(Name, ESearchCase::IgnoreCase);
    };

    UEdGraphPin* TopPin = nullptr;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (MatchPin(Pin, TopName)) { TopPin = Pin; break; }
    }

    if (!TopPin)
    {
        // Build a helpful "available pins" message — the #1 thing the old flow lacked.
        TArray<FString> Available;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Direction == EGPD_MAX || Pin->Direction == Direction)
            {
                Available.Add(Pin->PinName.ToString());
            }
        }
        OutError = FString::Printf(TEXT("pin '%s' not found on '%s'. Available %s pins: [%s]"),
            *TopName, *Node->GetName(),
            Direction == EGPD_Input ? TEXT("input") : (Direction == EGPD_Output ? TEXT("output") : TEXT("")),
            *FString::Join(Available, TEXT(", ")));
        return nullptr;
    }

    if (!bHasSub)
    {
        return TopPin;
    }

    // Find/auto-create the named sub-pin.
    const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
    if (TopPin->SubPins.Num() == 0 && K2Schema && K2Schema->CanSplitStructPin(*TopPin))
    {
        K2Schema->SplitPin(TopPin);
    }
    for (UEdGraphPin* Sub : TopPin->SubPins)
    {
        // Split sub-pin names look like "Parent_X" or contain the field name.
        const FString SubPinName = Sub->PinName.ToString();
        if (SubPinName.EndsWith(SubName, ESearchCase::IgnoreCase) ||
            SubPinName.Equals(SubName, ESearchCase::IgnoreCase))
        {
            return Sub;
        }
    }

    OutError = FString::Printf(TEXT("sub-pin '%s' not found under '%s'"), *SubName, *TopName);
    return nullptr;
}

bool FUnrealMCPGraphCommands::ConnectPins(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, FString& OutError)
{
    if (!FromPin || !ToPin)
    {
        OutError = TEXT("null pin");
        return false;
    }

    const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
    if (!K2Schema)
    {
        // Last-resort raw link.
        FromPin->MakeLinkTo(ToPin);
        return true;
    }

    // TryCreateConnection runs the same validation the editor UI does: direction,
    // type compatibility, autocast insertion, array/struct promotion. This is the
    // key difference from the old MakeLinkTo path that silently produced bad graphs.
    const FPinConnectionResponse Response = K2Schema->CanCreateConnection(FromPin, ToPin);
    if (Response.Response == CONNECT_RESPONSE_DISALLOW)
    {
        OutError = FString::Printf(TEXT("schema disallows connection %s -> %s: %s"),
            *FromPin->PinName.ToString(), *ToPin->PinName.ToString(), *Response.Message.ToString());
        return false;
    }

    const bool bModified = K2Schema->TryCreateConnection(FromPin, ToPin);
    if (!bModified)
    {
        OutError = FString::Printf(TEXT("TryCreateConnection failed %s -> %s"),
            *FromPin->PinName.ToString(), *ToPin->PinName.ToString());
        return false;
    }
    return true;
}

TSharedPtr<FJsonObject> FUnrealMCPGraphCommands::PinToJson(UEdGraphPin* Pin) const
{
    TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("name"), Pin->PinName.ToString());
    J->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    J->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
    if (Pin->PinType.PinSubCategoryObject.IsValid())
    {
        J->SetStringField(TEXT("sub_type"), Pin->PinType.PinSubCategoryObject->GetName());
    }
    J->SetBoolField(TEXT("is_array"), Pin->PinType.IsArray());
    J->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    J->SetNumberField(TEXT("link_count"), Pin->LinkedTo.Num());

    const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
    J->SetBoolField(TEXT("can_split"), K2Schema ? K2Schema->CanSplitStructPin(*Pin) : false);

    if (Pin->SubPins.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Subs;
        for (UEdGraphPin* Sub : Pin->SubPins)
        {
            Subs.Add(MakeShared<FJsonValueString>(Sub->PinName.ToString()));
        }
        J->SetArrayField(TEXT("sub_pins"), Subs);
    }
    return J;
}

TSharedPtr<FJsonObject> FUnrealMCPGraphCommands::NodeToJson(UEdGraphNode* Node) const
{
    TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
    J->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    J->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());

    TArray<TSharedPtr<FJsonValue>> Pins;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        Pins.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
    }
    J->SetArrayField(TEXT("pins"), Pins);
    return J;
}

// =============================================================================
// build_blueprint_graph
// =============================================================================
//
// Params:
//   "blueprint_name" (string, required)
//   "graph"          (string, optional, default "EventGraph")
//   "nodes"          (array, required) — each:
//        { "ref": "<alias>", "type": "<event|call_function|variable_get|
//          variable_set|self|branch|sequence>", ...type-specific fields...,
//          "position": [x,y] (optional),
//          "pin_defaults": { "<pin>": "<literal>", ... } (optional) }
//   "connections"    (array, optional) — each:
//        { "from": "<ref>", "from_pin": "<pin or Parent.Sub>",
//          "to": "<ref>",   "to_pin":   "<pin or Parent.Sub>" }
//
// Returns:
//   { "success": bool,
//     "node_ids": { "<ref>": "<guid>", ... },
//     "connections": [ { "from":..,"to":.., "ok": bool, "error": "..." } ],
//     "errors": [ "..." ] }
//
// Atomic-ish: every node is created first (so refs resolve regardless of order),
// then literals are set, then connections are wired. A bad connection is reported
// per-entry rather than aborting the whole build, and the report makes it obvious
// which wire failed and why (with the available-pin list).

TSharedPtr<FJsonObject> FUnrealMCPGraphCommands::HandleBuildGraph(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("build_blueprint_graph requires params"));
    }

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("missing 'blueprint_name'"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FString GraphName;
    Params->TryGetStringField(TEXT("graph"), GraphName);
    FString GraphError;
    UEdGraph* Graph = ResolveGraph(Blueprint, GraphName, GraphError);
    if (!Graph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(GraphError);
    }

    const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("nodes"), NodesArray) || NodesArray->Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'nodes' must be a non-empty array"));
    }

    TArray<TSharedPtr<FJsonValue>> ErrorList;
    TMap<FString, UEdGraphNode*> RefToNode;
    TSharedPtr<FJsonObject> NodeIds = MakeShared<FJsonObject>();

    // Pass 1 — create every node so refs resolve regardless of declaration order.
    int32 AnonCounter = 0;
    for (const TSharedPtr<FJsonValue>& NodeVal : *NodesArray)
    {
        const TSharedPtr<FJsonObject> NodeSpec = NodeVal->AsObject();
        if (!NodeSpec.IsValid()) { continue; }

        FString Ref;
        if (!NodeSpec->TryGetStringField(TEXT("ref"), Ref) || Ref.IsEmpty())
        {
            Ref = FString::Printf(TEXT("__node_%d"), AnonCounter++);
        }

        FString NodeError;
        UEdGraphNode* Node = CreateNodeFromSpec(Blueprint, Graph, NodeSpec, NodeError);
        if (!Node)
        {
            ErrorList.Add(MakeShared<FJsonValueString>(
                FString::Printf(TEXT("node '%s': %s"), *Ref, *NodeError)));
            continue;
        }
        RefToNode.Add(Ref, Node);
        NodeIds->SetStringField(Ref, Node->NodeGuid.ToString());
    }

    // Pass 2 — pin literal defaults.
    for (const TSharedPtr<FJsonValue>& NodeVal : *NodesArray)
    {
        const TSharedPtr<FJsonObject> NodeSpec = NodeVal->AsObject();
        if (!NodeSpec.IsValid()) { continue; }
        FString Ref;
        NodeSpec->TryGetStringField(TEXT("ref"), Ref);
        UEdGraphNode** Found = RefToNode.Find(Ref);
        if (!Found) { continue; }

        const TSharedPtr<FJsonObject>* Defaults = nullptr;
        if (NodeSpec->TryGetObjectField(TEXT("pin_defaults"), Defaults) && Defaults)
        {
            const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
            for (const auto& Pair : (*Defaults)->Values)
            {
                FString PinErr;
                UEdGraphPin* Pin = ResolvePin(*Found, Pair.Key, EGPD_Input, PinErr);
                if (!Pin)
                {
                    ErrorList.Add(MakeShared<FJsonValueString>(
                        FString::Printf(TEXT("pin_default '%s' on '%s': %s"), *Pair.Key, *Ref, *PinErr)));
                    continue;
                }
                const FString LiteralValue = Pair.Value->AsString();
                if (K2Schema)
                {
                    K2Schema->TrySetDefaultValue(*Pin, LiteralValue);
                }
                else
                {
                    Pin->DefaultValue = LiteralValue;
                }
            }
        }
    }

    // Pass 3 — connections.
    TArray<TSharedPtr<FJsonValue>> ConnReport;
    const TArray<TSharedPtr<FJsonValue>>* ConnArray = nullptr;
    if (Params->TryGetArrayField(TEXT("connections"), ConnArray))
    {
        for (const TSharedPtr<FJsonValue>& ConnVal : *ConnArray)
        {
            const TSharedPtr<FJsonObject> Conn = ConnVal->AsObject();
            if (!Conn.IsValid()) { continue; }

            FString FromRef, FromPin, ToRef, ToPin;
            Conn->TryGetStringField(TEXT("from"), FromRef);
            Conn->TryGetStringField(TEXT("from_pin"), FromPin);
            Conn->TryGetStringField(TEXT("to"), ToRef);
            Conn->TryGetStringField(TEXT("to_pin"), ToPin);

            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("from"), FString::Printf(TEXT("%s.%s"), *FromRef, *FromPin));
            Entry->SetStringField(TEXT("to"), FString::Printf(TEXT("%s.%s"), *ToRef, *ToPin));

            UEdGraphNode** FromNode = RefToNode.Find(FromRef);
            UEdGraphNode** ToNode = RefToNode.Find(ToRef);
            if (!FromNode || !ToNode)
            {
                Entry->SetBoolField(TEXT("ok"), false);
                Entry->SetStringField(TEXT("error"),
                    FString::Printf(TEXT("unknown node ref(s): %s%s"),
                        FromNode ? TEXT("") : *FString::Printf(TEXT("'%s' "), *FromRef),
                        ToNode ? TEXT("") : *FString::Printf(TEXT("'%s'"), *ToRef)));
                ConnReport.Add(MakeShared<FJsonValueObject>(Entry));
                continue;
            }

            FString PinErr;
            UEdGraphPin* SourcePin = ResolvePin(*FromNode, FromPin, EGPD_Output, PinErr);
            if (!SourcePin)
            {
                Entry->SetBoolField(TEXT("ok"), false);
                Entry->SetStringField(TEXT("error"), PinErr);
                ConnReport.Add(MakeShared<FJsonValueObject>(Entry));
                continue;
            }
            UEdGraphPin* DestPin = ResolvePin(*ToNode, ToPin, EGPD_Input, PinErr);
            if (!DestPin)
            {
                Entry->SetBoolField(TEXT("ok"), false);
                Entry->SetStringField(TEXT("error"), PinErr);
                ConnReport.Add(MakeShared<FJsonValueObject>(Entry));
                continue;
            }

            FString ConnErr;
            const bool bOk = ConnectPins(Graph, SourcePin, DestPin, ConnErr);
            Entry->SetBoolField(TEXT("ok"), bOk);
            if (!bOk) { Entry->SetStringField(TEXT("error"), ConnErr); }
            ConnReport.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    // Reconstruct + mark modified so the graph is consistent and saveable.
    for (const auto& Pair : RefToNode)
    {
        if (Pair.Value)
        {
            Pair.Value->ReconstructNode();
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    // Roll up overall success: any node error or any failed connection drops it.
    bool bAllConnsOk = true;
    for (const TSharedPtr<FJsonValue>& E : ConnReport)
    {
        bool bOk = false;
        E->AsObject()->TryGetBoolField(TEXT("ok"), bOk);
        bAllConnsOk &= bOk;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), ErrorList.Num() == 0 && bAllConnsOk);
    Result->SetObjectField(TEXT("node_ids"), NodeIds);
    Result->SetArrayField(TEXT("connections"), ConnReport);
    Result->SetArrayField(TEXT("errors"), ErrorList);
    if (ErrorList.Num() > 0 || !bAllConnsOk)
    {
        Result->SetStringField(TEXT("error"),
            TEXT("graph built with errors — see 'errors' and per-connection 'ok' flags"));
    }
    return Result;
}

// =============================================================================
// describe_blueprint_node
// =============================================================================
//
// Params:
//   "blueprint_name" (string, required)
//   "graph"          (string, optional, default "EventGraph")
//   "node_id"        (string, optional) — a node GUID, or "all" to dump every node
//
// Returns: { "success": true, "nodes": [ {node...}, ... ] }
//
// The introspection the old flow lacked: read the REAL pins of nodes so the caller
// wires by fact, not by guessing "exec"/"then"/"Target"/"ReturnValue".

TSharedPtr<FJsonObject> FUnrealMCPGraphCommands::HandleDescribeNode(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("describe_blueprint_node requires params"));
    }

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("missing 'blueprint_name'"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FString GraphName;
    Params->TryGetStringField(TEXT("graph"), GraphName);
    FString GraphError;
    UEdGraph* Graph = ResolveGraph(Blueprint, GraphName, GraphError);
    if (!Graph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(GraphError);
    }

    FString NodeId;
    Params->TryGetStringField(TEXT("node_id"), NodeId);
    const bool bAll = NodeId.IsEmpty() || NodeId.Equals(TEXT("all"), ESearchCase::IgnoreCase);

    TArray<TSharedPtr<FJsonValue>> NodesJson;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node) { continue; }
        if (bAll || Node->NodeGuid.ToString() == NodeId)
        {
            NodesJson.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
        }
    }

    if (!bAll && NodesJson.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("node '%s' not found in graph '%s'"), *NodeId, *Graph->GetName()));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("graph"), Graph->GetName());
    Result->SetArrayField(TEXT("nodes"), NodesJson);
    return Result;
}

// =============================================================================
// split_struct_pin
// =============================================================================
//
// Params:
//   "blueprint_name" (string, required)
//   "graph"          (string, optional, default "EventGraph")
//   "node_id"        (string, required) — node GUID
//   "pin"            (string, required) — name of the struct pin to split
//
// Returns: { "success": true, "sub_pins": [ "<name>", ... ] }
//
// The one operation the upstream README explicitly says needs manual editor work.

TSharedPtr<FJsonObject> FUnrealMCPGraphCommands::HandleSplitStructPin(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("split_struct_pin requires params"));
    }

    FString BlueprintName, NodeId, PinName;
    Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName);
    Params->TryGetStringField(TEXT("node_id"), NodeId);
    Params->TryGetStringField(TEXT("pin"), PinName);
    if (BlueprintName.IsEmpty() || NodeId.IsEmpty() || PinName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("split_struct_pin requires 'blueprint_name', 'node_id', and 'pin'"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FString GraphName;
    Params->TryGetStringField(TEXT("graph"), GraphName);
    FString GraphError;
    UEdGraph* Graph = ResolveGraph(Blueprint, GraphName, GraphError);
    if (!Graph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(GraphError);
    }

    UEdGraphNode* Node = nullptr;
    for (UEdGraphNode* N : Graph->Nodes)
    {
        if (N && N->NodeGuid.ToString() == NodeId) { Node = N; break; }
    }
    if (!Node)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("node '%s' not found"), *NodeId));
    }

    UEdGraphPin* Pin = nullptr;
    for (UEdGraphPin* P : Node->Pins)
    {
        if (P && P->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase)) { Pin = P; break; }
    }
    if (!Pin)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("pin '%s' not found on node"), *PinName));
    }

    const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
    if (!K2Schema || !K2Schema->CanSplitStructPin(*Pin))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("pin '%s' is not a splittable struct pin"), *PinName));
    }

    K2Schema->SplitPin(Pin);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TArray<TSharedPtr<FJsonValue>> Subs;
    for (UEdGraphPin* Sub : Pin->SubPins)
    {
        Subs.Add(MakeShared<FJsonValueString>(Sub->PinName.ToString()));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("sub_pins"), Subs);
    return Result;
}
