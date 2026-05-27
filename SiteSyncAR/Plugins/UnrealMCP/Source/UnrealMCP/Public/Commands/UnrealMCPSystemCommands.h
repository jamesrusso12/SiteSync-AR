#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for System-level MCP commands.
 *
 * Adds the architectural pieces missing from the original chongdashu plugin:
 *   - execute_python              : run arbitrary editor Python without a C++ rebuild
 *   - save_all_dirty_packages     : persist MCP edits to .uasset / .umap on disk
 *   - save_package                : save a single asset by package path
 *   - reparent_actor_root         : swap which component is an actor's root
 *   - list_commands               : dispatcher introspection (returns every wired command)
 *
 * The execute_python tool is the highest-leverage addition — it lets future
 * Claude sessions reach the full `unreal` Python API surface (Datasmith import,
 * Merge Actors, DataAsset edits, etc.) without needing a new C++ dispatcher
 * entry + plugin DLL rebuild for every new operation.
 */
class UNREALMCP_API FUnrealMCPSystemCommands
{
public:
    FUnrealMCPSystemCommands();

    /** Returns the list of command names this handler dispatches. */
    static const TArray<FString>& GetSupportedCommands();

    /** Entry point from the bridge dispatcher. */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleExecutePython(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSaveAllDirtyPackages(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSavePackage(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReparentActorRoot(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListCommands(const TSharedPtr<FJsonObject>& Params);
};
