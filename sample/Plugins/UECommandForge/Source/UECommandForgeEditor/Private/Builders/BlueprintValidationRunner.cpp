#include "Builders/BlueprintValidationRunner.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

namespace UECommandForge
{
    bool FBlueprintValidationRunner::Compile(UBlueprint* Blueprint,
                                             const FString& AssetPath,
                                             const FString& Field,
                                             TArray<FCommandForgeError>& OutErrors,
                                             TMap<FString, FString>& OutValidation)
    {
        if (!Blueprint)
        {
            OutErrors.Add({ TEXT("BLUEPRINT_NULL"),
                FString::Printf(TEXT("Blueprint 객체가 없습니다: %s"), *AssetPath),
                Field });
            return false;
        }

        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (Blueprint->Status != BS_UpToDate)
        {
            OutValidation.Add(TEXT("compile_status"), TEXT("error"));
            OutErrors.Add({ TEXT("BP_COMPILE_FAILED"),
                FString::Printf(TEXT("Blueprint 컴파일 실패: %s"), *AssetPath),
                Field });
            return false;
        }

        OutValidation.Add(TEXT("compile_status"), TEXT("ok"));
        return true;
    }

    bool FBlueprintValidationRunner::Save(UBlueprint* Blueprint,
                                          const FString& AssetPath,
                                          const FString& Field,
                                          TArray<FCommandForgeError>& OutErrors,
                                          TMap<FString, FString>& OutValidation)
    {
        if (!Blueprint)
        {
            OutErrors.Add({ TEXT("BLUEPRINT_NULL"),
                FString::Printf(TEXT("Blueprint 객체가 없습니다: %s"), *AssetPath),
                Field });
            return false;
        }

        const FString FileName = FPaths::ConvertRelativePathToFull(
            FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension()));
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

        UPackage* Package = Blueprint->GetOutermost();
        Package->SetDirtyFlag(true);

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const bool bSavedOk = UPackage::SavePackage(Package, Blueprint, *FileName, SaveArgs) &&
            PlatformFile.FileExists(*FileName);
        OutValidation.Add(TEXT("asset_on_disk"), bSavedOk ? TEXT("true") : TEXT("false"));
        if (!bSavedOk)
        {
            OutErrors.Add({ TEXT("BP_SAVE_FAILED"),
                FString::Printf(TEXT("Blueprint 저장 실패: %s"), *FileName),
                Field });
            return false;
        }

        ValidateRegistry(AssetPath, FileName, OutValidation);
        return true;
    }

    bool FBlueprintValidationRunner::ValidateRegistry(const FString& AssetPath,
                                                      const FString& FileName,
                                                      TMap<FString, FString>& OutValidation)
    {
        IAssetRegistry& AssetRegistry =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        AssetRegistry.ScanModifiedAssetFiles({ FileName });

        TArray<FAssetData> Assets;
        AssetRegistry.GetAssetsByPackageName(*AssetPath, Assets);
        const bool bInRegistry = Assets.Num() > 0;
        OutValidation.Add(TEXT("asset_in_registry"), bInRegistry ? TEXT("true") : TEXT("false"));
        return bInRegistry;
    }

    bool FBlueprintValidationRunner::CompileSaveAndValidate(UBlueprint* Blueprint,
                                                            const FString& AssetPath,
                                                            const FString& Field,
                                                            TArray<FCommandForgeError>& OutErrors,
                                                            TMap<FString, FString>& OutValidation)
    {
        if (!Compile(Blueprint, AssetPath, Field, OutErrors, OutValidation))
        {
            return false;
        }
        return Save(Blueprint, AssetPath, Field, OutErrors, OutValidation);
    }
}
