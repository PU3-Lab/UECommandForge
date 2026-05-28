#include "Commandlets/AssetSnapshotCommandlet.h"
#include "Reports/JsonReportWriter.h"
#include "CommandForgeTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"

UAssetSnapshotCommandlet::UAssetSnapshotCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace
{
    TArray<FString> ParseRootPaths(const FString& RootPathsArg)
    {
        TArray<FString> RootPaths;
        if (RootPathsArg.IsEmpty())
        {
            RootPaths.Add(TEXT("/Game"));
            RootPaths.Add(TEXT("/UECommandForge"));
            return RootPaths;
        }

        RootPathsArg.ParseIntoArray(RootPaths, TEXT(","), true);
        for (FString& RootPath : RootPaths)
        {
            RootPath = RootPath.TrimStartAndEnd();
        }
        return RootPaths;
    }

    TArray<FString> NamesToStrings(const TArray<FName>& Names)
    {
        TArray<FString> Out;
        for (const FName& Name : Names)
        {
            Out.Add(Name.ToString());
        }
        return Out;
    }
}

int32 UAssetSnapshotCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("AssetSnapshot");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        FCommandForgeError Err;
        Err.Code = TEXT("MISSING_ARG");
        Err.Message = TEXT("-Output 인자가 없습니다.");
        Report.Errors.Add(Err);
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    const TArray<FString> RootPaths = ParseRootPaths(ParamsMap.FindRef(TEXT("RootPaths")));

    TArray<FAssetData> AllAssets;
    for (const FString& RootPath : RootPaths)
    {
        if (!RootPath.IsEmpty())
        {
            AssetRegistry.GetAssetsByPath(FName(*RootPath), AllAssets, true);
        }
    }

    AllAssets.Sort([](const FAssetData& A, const FAssetData& B)
    {
        return A.PackageName.LexicalLess(B.PackageName);
    });

    for (const FAssetData& AssetData : AllAssets)
    {
        FCommandForgeAssetSnapshotRecord Record;
        Record.AssetName = AssetData.AssetName.ToString();
        Record.PackagePath = AssetData.PackagePath.ToString();
        Record.ObjectPath = AssetData.GetObjectPathString();
        Record.AssetClass = AssetData.AssetClassPath.ToString();
        Record.PackageName = AssetData.PackageName.ToString();
        Record.DiskPath = FPackageName::LongPackageNameToFilename(
            Record.PackageName, FPackageName::GetAssetPackageExtension());
        Record.bIsRedirector = Record.AssetClass.Contains(TEXT("ObjectRedirector"));

        UPackage* Package = FindPackage(nullptr, *Record.PackageName);
        Record.bPackageDirty = Package != nullptr && Package->IsDirty();

        TArray<FName> Dependencies;
        AssetRegistry.GetDependencies(
            AssetData.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
        Record.Dependencies = NamesToStrings(Dependencies);

        TArray<FName> Referencers;
        AssetRegistry.GetReferencers(
            AssetData.PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package);
        Record.Referencers = NamesToStrings(Referencers);

        Report.Assets.Add(Record);
    }

    Report.Validation.Add(TEXT("root_paths"), ParamsMap.FindRef(TEXT("RootPaths")).IsEmpty()
        ? TEXT("/Game,/UECommandForge")
        : ParamsMap.FindRef(TEXT("RootPaths")));
    Report.Validation.Add(TEXT("asset_count"), FString::FromInt(Report.Assets.Num()));
    Report.bOk = true;

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return bWrote ? 0 : static_cast<int32>(ECommandForgeExitCode::EngineError);
}
