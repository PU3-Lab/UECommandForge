#include "BuildCs/BuildCsDependencyUtils.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UECommandForge::BuildCs
{
    namespace
    {
        bool ContainsDependency(const FString& Content, const FString& ListName, const FString& Dependency)
        {
            const FString Search = ListName + TEXT(".AddRange");
            int32 SearchStart = 0;
            while (SearchStart < Content.Len())
            {
                const int32 Start = Content.Find(Search, ESearchCase::IgnoreCase,
                    ESearchDir::FromStart, SearchStart);
                if (Start == INDEX_NONE)
                {
                    return false;
                }

                const int32 End = Content.Find(TEXT(");"), ESearchCase::IgnoreCase,
                    ESearchDir::FromStart, Start);
                const FString Block = End == INDEX_NONE ? Content.Mid(Start) : Content.Mid(Start, End - Start);
                if (Block.Contains(FString::Printf(TEXT("\"%s\""), *Dependency), ESearchCase::CaseSensitive))
                {
                    return true;
                }

                SearchStart = End == INDEX_NONE ? Start + Search.Len() : End + 2;
            }
            return false;
        }

        void AddMissingDependenciesToList(FString& Content, const FString& ListName,
            const TArray<FString>& Dependencies)
        {
            if (Dependencies.IsEmpty())
            {
                return;
            }

            const FString Search = ListName + TEXT(".AddRange");
            const int32 Start = Content.Find(Search, ESearchCase::IgnoreCase);
            if (Start == INDEX_NONE)
            {
                const FString DependencyList = TEXT("\"") + FString::Join(Dependencies, TEXT("\", \"")) + TEXT("\"");
                const FString NewLine = FString::Printf(
                    TEXT("        %s.AddRange(new[] { %s });\n"), *ListName, *DependencyList);
                const int32 InsertAfterPch = Content.Find(TEXT("PCHUsage"), ESearchCase::IgnoreCase);
                if (InsertAfterPch != INDEX_NONE)
                {
                    const int32 LineEnd = Content.Find(TEXT("\n"), ESearchCase::CaseSensitive,
                        ESearchDir::FromStart, InsertAfterPch);
                    Content.InsertAt(LineEnd == INDEX_NONE ? Content.Len() : LineEnd + 1, NewLine);
                }
                else
                {
                    Content += TEXT("\n") + NewLine;
                }
                return;
            }

            const int32 InsertAt = Content.Find(TEXT("});"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
            if (InsertAt == INDEX_NONE)
            {
                return;
            }

            FString InsertText;
            for (const FString& Dependency : Dependencies)
            {
                InsertText += FString::Printf(TEXT(",\n            \"%s\""), *Dependency);
            }
            Content.InsertAt(InsertAt, InsertText + TEXT("\n        "));
        }
    }

    bool IsSafeModuleName(const FString& Value)
    {
        const FString Trimmed = Value.TrimStartAndEnd();
        if (Trimmed.IsEmpty() || !(FChar::IsAlpha(Trimmed[0]) || Trimmed[0] == TEXT('_')))
        {
            return false;
        }

        for (const TCHAR Character : Trimmed)
        {
            if (!(FChar::IsAlnum(Character) || Character == TEXT('_')))
            {
                return false;
            }
        }
        return true;
    }

    FString ResolveModuleRoot(const FString& ModuleName)
    {
        if (!IsSafeModuleName(ModuleName))
        {
            return TEXT("");
        }

        const TArray<FString> Candidates = {
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), ModuleName),
            FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UECommandForge"), TEXT("Source"), ModuleName)
        };

        for (const FString& Candidate : Candidates)
        {
            const FString FullPath = FPaths::ConvertRelativePathToFull(Candidate);
            if (FPaths::DirectoryExists(FullPath))
            {
                return FullPath;
            }
        }
        return TEXT("");
    }

    FString BuildCsPathForModuleRoot(const FString& ModuleRoot, const FString& ModuleName)
    {
        if (ModuleRoot.IsEmpty())
        {
            return TEXT("");
        }
        return FPaths::ConvertRelativePathToFull(
            FPaths::Combine(ModuleRoot, ModuleName + TEXT(".Build.cs")));
    }

    FBuildCsDependencyCheck CheckDependencies(const FString& ModuleName,
        const TArray<FString>& RequiredPublicDependencies,
        const TArray<FString>& RequiredPrivateDependencies)
    {
        FBuildCsDependencyCheck Check;
        Check.ModuleName = ModuleName.TrimStartAndEnd();
        Check.ModuleRoot = ResolveModuleRoot(Check.ModuleName);
        Check.BuildCsPath = BuildCsPathForModuleRoot(Check.ModuleRoot, Check.ModuleName);
        Check.bBuildCsExists = !Check.BuildCsPath.IsEmpty() && FPaths::FileExists(Check.BuildCsPath);

        FString Content;
        if (!Check.bBuildCsExists || !FFileHelper::LoadFileToString(Content, *Check.BuildCsPath))
        {
            return Check;
        }

        for (const FString& Dependency : RequiredPublicDependencies)
        {
            if (!ContainsDependency(Content, TEXT("PublicDependencyModuleNames"), Dependency))
            {
                Check.MissingPublicDependencies.Add(Dependency);
            }
        }
        for (const FString& Dependency : RequiredPrivateDependencies)
        {
            if (!ContainsDependency(Content, TEXT("PrivateDependencyModuleNames"), Dependency))
            {
                Check.MissingPrivateDependencies.Add(Dependency);
            }
        }

        Check.OriginalContent = Content;
        Check.UpdatedContent = Content;
        AddMissingDependenciesToList(Check.UpdatedContent, TEXT("PublicDependencyModuleNames"),
            Check.MissingPublicDependencies);
        AddMissingDependenciesToList(Check.UpdatedContent, TEXT("PrivateDependencyModuleNames"),
            Check.MissingPrivateDependencies);
        Check.bChanged = Check.UpdatedContent != Content;
        return Check;
    }

    bool WriteUpdatedBuildCs(const FBuildCsDependencyCheck& Check)
    {
        if (!Check.bBuildCsExists || !Check.bChanged || Check.UpdatedContent.IsEmpty())
        {
            return false;
        }
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Check.BuildCsPath));
        return FFileHelper::SaveStringToFile(Check.UpdatedContent, *Check.BuildCsPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}
