#pragma once

#include "CoreMinimal.h"

namespace UECommandForge::BuildCs
{
    struct FBuildCsDependencyCheck
    {
        FString ModuleName;
        FString ModuleRoot;
        FString BuildCsPath;
        bool bBuildCsExists = false;
        bool bChanged = false;
        TArray<FString> MissingPublicDependencies;
        TArray<FString> MissingPrivateDependencies;
        FString OriginalContent;
        FString UpdatedContent;
    };

    bool IsSafeModuleName(const FString& Value);
    FString ResolveModuleRoot(const FString& ModuleName);
    FString BuildCsPathForModuleRoot(const FString& ModuleRoot, const FString& ModuleName);
    FBuildCsDependencyCheck CheckDependencies(const FString& ModuleName,
        const TArray<FString>& RequiredPublicDependencies,
        const TArray<FString>& RequiredPrivateDependencies);
    bool WriteUpdatedBuildCs(const FBuildCsDependencyCheck& Check);
}
