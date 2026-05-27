#include "Builders/CharacterBlueprintBuilder.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "UObject/SavePackage.h"
#include "UObject/GarbageCollection.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"

namespace UECommandForge
{
    bool FCharacterBlueprintBuilder::Build(const FBlueprintSpec& Spec,
                                            TArray<FCommandForgeError>& OutErrors,
                                            TMap<FString, FString>& OutValidation)
    {
        UClass* ParentClass = LoadObject<UClass>(nullptr,
            *FString::Printf(TEXT("/Script/Engine.%s"), *Spec.ParentClass));
        if (!ParentClass)
        {
            OutErrors.Add({ TEXT("PARENT_CLASS_NOT_FOUND"),
                FString::Printf(TEXT("부모 클래스를 찾을 수 없습니다: %s"), *Spec.ParentClass),
                TEXT("Character.ParentClass") });
            return false;
        }

        const FString AssetName = FPackageName::GetLongPackageAssetName(Spec.AssetPath);
        if (UPackage* ExistingPackage = FindPackage(nullptr, *Spec.AssetPath))
        {
            ExistingPackage->FullyLoad();
            if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *AssetName))
            {
                TArray<UObject*> AssetsToDelete;
                AssetsToDelete.Add(ExistingAsset);
                if (ObjectTools::ForceDeleteObjects(AssetsToDelete, false) != AssetsToDelete.Num())
                {
                    OutErrors.Add({ TEXT("BP_DELETE_EXISTING_FAILED"),
                        FString::Printf(TEXT("기존 Blueprint 삭제 실패: %s"), *Spec.AssetPath),
                        TEXT("Character.AssetPath") });
                    return false;
                }
                CollectGarbage(RF_NoFlags);
            }
        }

        const FString FileName = FPaths::ConvertRelativePathToFull(
            FPackageName::LongPackageNameToFilename(Spec.AssetPath, FPackageName::GetAssetPackageExtension()));
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (PlatformFile.FileExists(*FileName) && !PlatformFile.DeleteFile(*FileName))
        {
            OutErrors.Add({ TEXT("BP_DELETE_EXISTING_FILE_FAILED"),
                FString::Printf(TEXT("기존 Blueprint 파일 삭제 실패: %s"), *FileName),
                TEXT("Character.AssetPath") });
            return false;
        }

        UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(
            ParentClass,
            CreatePackage(*Spec.AssetPath),
            *AssetName,
            BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass());

        if (!BP)
        {
            OutErrors.Add({ TEXT("BP_CREATE_FAILED"),
                FString::Printf(TEXT("Blueprint 생성 실패: %s"), *Spec.AssetPath),
                TEXT("Character.AssetPath") });
            return false;
        }

        FKismetEditorUtilities::CompileBlueprint(BP);

        if (BP->Status != BS_UpToDate)
        {
            OutValidation.Add(TEXT("compile_status"), TEXT("error"));
            OutErrors.Add({ TEXT("BP_COMPILE_FAILED"),
                FString::Printf(TEXT("Blueprint 컴파일 실패: %s"), *Spec.AssetPath),
                TEXT("Character.AssetPath") });
            return false;
        }
        OutValidation.Add(TEXT("compile_status"), TEXT("ok"));

        UPackage* Package = BP->GetOutermost();
        Package->SetDirtyFlag(true);
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const bool bSavedOk = UPackage::SavePackage(Package, BP, *FileName, SaveArgs) &&
                               PlatformFile.FileExists(*FileName);
        OutValidation.Add(TEXT("asset_on_disk"), bSavedOk ? TEXT("true") : TEXT("false"));
        if (!bSavedOk)
        {
            OutErrors.Add({ TEXT("BP_SAVE_FAILED"),
                FString::Printf(TEXT("Blueprint 저장 실패: %s"), *FileName),
                TEXT("Character.AssetPath") });
            return false;
        }

        IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        AR.ScanModifiedAssetFiles({ FileName });

        TArray<FAssetData> Assets;
        AR.GetAssetsByPackageName(*Spec.AssetPath, Assets);
        const bool bInRegistry = (Assets.Num() > 0);
        OutValidation.Add(TEXT("asset_in_registry"), bInRegistry ? TEXT("true") : TEXT("false"));

        return true;
    }
}
