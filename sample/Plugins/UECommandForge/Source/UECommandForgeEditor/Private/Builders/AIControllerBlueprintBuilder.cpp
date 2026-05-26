#include "Builders/AIControllerBlueprintBuilder.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"

namespace UECommandForge
{
    bool FAIControllerBlueprintBuilder::Build(const FBlueprintSpec& Spec,
                                               TArray<FCommandForgeError>& OutErrors,
                                               TMap<FString, FString>& OutValidation)
    {
        UClass* ParentClass = LoadObject<UClass>(nullptr,
            *FString::Printf(TEXT("/Script/AIModule.%s"), *Spec.ParentClass));
        if (!ParentClass)
        {
            ParentClass = LoadObject<UClass>(nullptr,
                *FString::Printf(TEXT("/Script/Engine.%s"), *Spec.ParentClass));
        }
        if (!ParentClass)
        {
            OutErrors.Add({ TEXT("PARENT_CLASS_NOT_FOUND"),
                FString::Printf(TEXT("부모 클래스를 찾을 수 없습니다: %s"), *Spec.ParentClass),
                TEXT("AIController.ParentClass") });
            return false;
        }

        UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(
            ParentClass,
            CreatePackage(*Spec.AssetPath),
            *FPackageName::GetLongPackageAssetName(Spec.AssetPath),
            BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass());

        if (!BP)
        {
            OutErrors.Add({ TEXT("BP_CREATE_FAILED"),
                FString::Printf(TEXT("Blueprint 생성 실패: %s"), *Spec.AssetPath),
                TEXT("AIController.AssetPath") });
            return false;
        }

        FKismetEditorUtilities::CompileBlueprint(BP);

        if (BP->Status != BS_UpToDate)
        {
            OutValidation.Add(TEXT("compile_status"), TEXT("error"));
            OutErrors.Add({ TEXT("BP_COMPILE_FAILED"),
                FString::Printf(TEXT("Blueprint 컴파일 실패: %s"), *Spec.AssetPath),
                TEXT("AIController.AssetPath") });
            return false;
        }
        OutValidation.Add(TEXT("compile_status"), TEXT("ok"));

        UPackage* Package = BP->GetOutermost();
        Package->SetDirtyFlag(true);
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Spec.AssetPath, FPackageName::GetAssetPackageExtension());
        const bool bSavedResult = UPackage::SavePackage(Package, BP, *FileName, SaveArgs);

        const bool bSavedOk = bSavedResult &&
                               FPlatformFileManager::Get().GetPlatformFile().FileExists(*FileName);
        OutValidation.Add(TEXT("asset_on_disk"), bSavedOk ? TEXT("true") : TEXT("false"));
        if (!bSavedOk)
        {
            OutErrors.Add({ TEXT("BP_SAVE_FAILED"),
                FString::Printf(TEXT("Blueprint 저장 실패: %s"), *FileName),
                TEXT("AIController.AssetPath") });
            return false;
        }

        IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        AR.ScanModifiedAssetFiles({ FileName });
        TArray<FAssetData> Assets;
        AR.GetAssetsByPackageName(*Spec.AssetPath, Assets);
        OutValidation.Add(TEXT("asset_in_registry"), (Assets.Num() > 0) ? TEXT("true") : TEXT("false"));

        return true;
    }
}
