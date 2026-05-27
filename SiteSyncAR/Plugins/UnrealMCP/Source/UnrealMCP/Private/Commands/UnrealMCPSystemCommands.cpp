#include "Commands/UnrealMCPSystemCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Subsystems/EditorActorSubsystem.h"

#include "FileHelpers.h"
#include "UObject/Package.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"

FUnrealMCPSystemCommands::FUnrealMCPSystemCommands()
{
}

const TArray<FString>& FUnrealMCPSystemCommands::GetSupportedCommands()
{
    static const TArray<FString> Commands = {
        TEXT("execute_python"),
        TEXT("save_all_dirty_packages"),
        TEXT("save_package"),
        TEXT("reparent_actor_root"),
        TEXT("list_commands"),
    };
    return Commands;
}

TSharedPtr<FJsonObject> FUnrealMCPSystemCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("execute_python"))           return HandleExecutePython(Params);
    if (CommandType == TEXT("save_all_dirty_packages"))  return HandleSaveAllDirtyPackages(Params);
    if (CommandType == TEXT("save_package"))             return HandleSavePackage(Params);
    if (CommandType == TEXT("reparent_actor_root"))      return HandleReparentActorRoot(Params);
    if (CommandType == TEXT("list_commands"))            return HandleListCommands(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown system command: %s"), *CommandType));
}

// -----------------------------------------------------------------------------
// execute_python
// -----------------------------------------------------------------------------
//
// Params:
//   "script"      (string, optional)  : Python source to run inline.
//   "script_file" (string, optional)  : Absolute or project-relative path to a .py file.
//   "mode"        (string, optional)  : "statement" (default) | "file" | "eval".
//                                        statement = ExecuteStatement (multi-line OK, no return value)
//                                        file      = ExecuteFile      (same as -run=pythonscript -script=)
//                                        eval      = EvaluateStatement (returns expression result string)
//
// Exactly one of `script` or `script_file` must be provided.
//
// Returns:
//   { "success": bool, "result": string, "log_output": string, "errors": [string,...] }
//
// Why this exists: every new "I need MCP to do X" used to require a C++ command
// handler, dispatcher entry, plugin rebuild, and editor restart. Python execution
// collapses that to one tool call.

TSharedPtr<FJsonObject> FUnrealMCPSystemCommands::HandleExecutePython(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("execute_python requires params"));
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("PythonScriptPlugin is not loaded or Python is unavailable. ")
            TEXT("Enable 'Python Editor Script Plugin' in the project's Plugins list."));
    }

    FString ScriptText;
    FString ScriptFile;
    FString ModeStr = TEXT("statement");

    Params->TryGetStringField(TEXT("script"), ScriptText);
    Params->TryGetStringField(TEXT("script_file"), ScriptFile);
    Params->TryGetStringField(TEXT("mode"), ModeStr);

    if (ScriptText.IsEmpty() && ScriptFile.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("execute_python requires either 'script' (inline source) or 'script_file' (path)."));
    }

    // Resolve script_file path: accept absolute, project-relative, or plugin-relative.
    if (!ScriptFile.IsEmpty())
    {
        if (FPaths::IsRelative(ScriptFile))
        {
            const FString ProjectRelative = FPaths::Combine(FPaths::ProjectDir(), ScriptFile);
            if (FPaths::FileExists(ProjectRelative))
            {
                ScriptFile = ProjectRelative;
            }
        }
        if (!FPaths::FileExists(ScriptFile))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("script_file not found: %s"), *ScriptFile));
        }
    }

    FPythonCommandEx PyCmd;

    if (ModeStr.Equals(TEXT("file"), ESearchCase::IgnoreCase))
    {
        PyCmd.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
        // ExecuteFile takes the script path (optionally followed by args) in Command.
        PyCmd.Command = ScriptFile.IsEmpty() ? ScriptText : ScriptFile;
    }
    else if (ModeStr.Equals(TEXT("eval"), ESearchCase::IgnoreCase))
    {
        PyCmd.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
        if (!ScriptFile.IsEmpty())
        {
            FString FileContent;
            FFileHelper::LoadFileToString(FileContent, *ScriptFile);
            PyCmd.Command = FileContent;
        }
        else
        {
            PyCmd.Command = ScriptText;
        }
    }
    else
    {
        // "statement" — default. Multi-line OK, return value ignored.
        PyCmd.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
        if (!ScriptFile.IsEmpty())
        {
            FString FileContent;
            FFileHelper::LoadFileToString(FileContent, *ScriptFile);
            PyCmd.Command = FileContent;
        }
        else
        {
            PyCmd.Command = ScriptText;
        }
    }

    PyCmd.Flags = EPythonCommandFlags::None;

    const bool bSuccess = PythonPlugin->ExecPythonCommandEx(PyCmd);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("result"), PyCmd.CommandResult);

    // Errors come back as an array of FPythonLogOutputEntry inside PyCmd.LogOutput;
    // surface anything tagged as an error so the caller can see Python tracebacks.
    TArray<TSharedPtr<FJsonValue>> ErrorArray;
    FString CombinedLog;
    for (const FPythonLogOutputEntry& Entry : PyCmd.LogOutput)
    {
        CombinedLog += Entry.Output;
        if (Entry.Type == EPythonLogOutputType::Error)
        {
            ErrorArray.Add(MakeShared<FJsonValueString>(Entry.Output));
        }
    }
    Result->SetStringField(TEXT("log_output"), CombinedLog);
    Result->SetArrayField(TEXT("errors"), ErrorArray);

    if (!bSuccess)
    {
        // Bubble a top-level error message too, so the wire-level error path lights up.
        const FString ErrMsg = ErrorArray.Num() > 0
            ? FString::Printf(TEXT("Python execution failed: %s"), *ErrorArray[0]->AsString())
            : TEXT("Python execution failed (no error detail returned)");
        Result->SetStringField(TEXT("error"), ErrMsg);
    }

    return Result;
}

// -----------------------------------------------------------------------------
// save_all_dirty_packages
// -----------------------------------------------------------------------------
//
// Params:
//   "save_content_packages" (bool, optional, default true)
//   "save_map_packages"     (bool, optional, default true)
//   "prompt_user"           (bool, optional, default false)  — usually false for headless MCP use
//
// Returns: { "success": bool, "saved": bool }
//
// Why this exists: CLAUDE.md explicitly notes that MCP edits do NOT trigger Save All,
// so structural changes lived in editor memory only until James hit Ctrl+S. This lets
// Claude finalize its own changes.

TSharedPtr<FJsonObject> FUnrealMCPSystemCommands::HandleSaveAllDirtyPackages(const TSharedPtr<FJsonObject>& Params)
{
    bool bSaveContentPackages = true;
    bool bSaveMapPackages = true;
    bool bPromptUser = false;

    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("save_content_packages"), bSaveContentPackages);
        Params->TryGetBoolField(TEXT("save_map_packages"), bSaveMapPackages);
        Params->TryGetBoolField(TEXT("prompt_user"), bPromptUser);
    }

    const bool bSaved = FEditorFileUtils::SaveDirtyPackages(
        bPromptUser,
        bSaveMapPackages,
        bSaveContentPackages,
        /*bFastSave*/ false,
        /*bNotifyNoPackagesSaved*/ false,
        /*bCanBeDeclined*/ false);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("saved"), bSaved);
    return Result;
}

// -----------------------------------------------------------------------------
// save_package
// -----------------------------------------------------------------------------
//
// Params:
//   "package_path" (string, required)  : e.g. "/Game/Blueprints/BP_ARPlayerController"
//
// Returns: { "success": bool, "saved": bool, "package_path": string }

TSharedPtr<FJsonObject> FUnrealMCPSystemCommands::HandleSavePackage(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("save_package requires params"));
    }

    FString PackagePath;
    if (!Params->TryGetStringField(TEXT("package_path"), PackagePath) || PackagePath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("save_package requires 'package_path' string"));
    }

    UPackage* Package = FindPackage(nullptr, *PackagePath);
    if (!Package)
    {
        Package = LoadPackage(nullptr, *PackagePath, LOAD_None);
    }

    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Package not found: %s"), *PackagePath));
    }

    TArray<UPackage*> PackagesToSave;
    PackagesToSave.Add(Package);
    const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, /*bOnlyDirty*/ false);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("package_path"), PackagePath);
    return Result;
}

// -----------------------------------------------------------------------------
// reparent_actor_root
// -----------------------------------------------------------------------------
//
// Params:
//   "actor_name"     (string, required) : actor label or unique name in current level
//   "new_root"       (string, required) : component name to promote to RootComponent
//
// Returns: { "success": bool, "actor": string, "new_root": string }
//
// Solves the 2026-05-20 "BIMMesh is root, SetActorScale3D clobbers component scale"
// class of bugs without requiring a custom Blueprint refactor.

TSharedPtr<FJsonObject> FUnrealMCPSystemCommands::HandleReparentActorRoot(const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("reparent_actor_root requires params"));
    }

    FString ActorName;
    FString NewRootName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) ||
        !Params->TryGetStringField(TEXT("new_root"), NewRootName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("reparent_actor_root requires 'actor_name' and 'new_root' strings"));
    }

    UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
    if (!ActorSubsystem)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("EditorActorSubsystem unavailable"));
    }

    AActor* TargetActor = nullptr;
    for (AActor* A : ActorSubsystem->GetAllLevelActors())
    {
        if (A && (A->GetActorLabel() == ActorName || A->GetName() == ActorName))
        {
            TargetActor = A;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    USceneComponent* NewRoot = nullptr;
    TArray<USceneComponent*> SceneComponents;
    TargetActor->GetComponents<USceneComponent>(SceneComponents);
    for (USceneComponent* Comp : SceneComponents)
    {
        if (Comp && Comp->GetName() == NewRootName)
        {
            NewRoot = Comp;
            break;
        }
    }

    if (!NewRoot)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("SceneComponent '%s' not found on '%s'"), *NewRootName, *ActorName));
    }

    TargetActor->SetRootComponent(NewRoot);
    TargetActor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actor"), ActorName);
    Result->SetStringField(TEXT("new_root"), NewRootName);
    return Result;
}

// -----------------------------------------------------------------------------
// list_commands
// -----------------------------------------------------------------------------
//
// Returns the static command-name list this handler dispatches. Future work:
// extend each handler with the same static accessor and have the bridge
// aggregate all of them — that turns the "Unknown command" silent-fail into
// a queryable introspection surface.

TSharedPtr<FJsonObject> FUnrealMCPSystemCommands::HandleListCommands(const TSharedPtr<FJsonObject>& Params)
{
    TArray<TSharedPtr<FJsonValue>> CommandList;
    for (const FString& Cmd : GetSupportedCommands())
    {
        CommandList.Add(MakeShared<FJsonValueString>(Cmd));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("system_commands"), CommandList);
    return Result;
}
