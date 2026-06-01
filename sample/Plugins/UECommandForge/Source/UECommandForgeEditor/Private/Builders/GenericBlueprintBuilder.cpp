#include "Builders/GenericBlueprintBuilder.h"

#include "Builders/BlueprintValidationRunner.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "UObject/GarbageCollection.h"

namespace UECommandForge
{
    UClass* FGenericBlueprintBuilder::ResolveParentClass(const FString& ParentClass,
                                                         const TArray<FString>& ParentClassModules)
    {
        if (ParentClass.IsEmpty())
        {
            return nullptr;
        }

        if (ParentClass.StartsWith(TEXT("/Script/")))
        {
            return LoadObject<UClass>(nullptr, *ParentClass);
        }

        for (const FString& Module : ParentClassModules)
        {
            const FString NormalizedModule = Module.StartsWith(TEXT("/Script/"))
                ? Module
                : FString::Printf(TEXT("/Script/%s"), *Module);
            if (UClass* Class = LoadObject<UClass>(nullptr,
                    *FString::Printf(TEXT("%s.%s"), *NormalizedModule, *ParentClass)))
            {
                return Class;
            }
        }

        return LoadObject<UClass>(nullptr, *ParentClass);
    }

    bool FGenericBlueprintBuilder::DeleteExistingBlueprint(const FString& AssetPath,
                                                           const FString& Field,
                                                           TArray<FCommandForgeError>& OutErrors)
    {
        const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
        if (UPackage* ExistingPackage = FindPackage(nullptr, *AssetPath))
        {
            ExistingPackage->FullyLoad();
            if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *AssetName))
            {
                TArray<UObject*> AssetsToDelete;
                AssetsToDelete.Add(ExistingAsset);
                if (ObjectTools::ForceDeleteObjects(AssetsToDelete, false) != AssetsToDelete.Num())
                {
                    OutErrors.Add({ TEXT("BP_DELETE_EXISTING_FAILED"),
                        FString::Printf(TEXT("기존 Blueprint 삭제 실패: %s"), *AssetPath),
                        Field });
                    return false;
                }
                CollectGarbage(RF_NoFlags);
            }
        }

        const FString FileName = FPaths::ConvertRelativePathToFull(
            FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension()));
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (PlatformFile.FileExists(*FileName) && !PlatformFile.DeleteFile(*FileName))
        {
            OutErrors.Add({ TEXT("BP_DELETE_EXISTING_FILE_FAILED"),
                FString::Printf(TEXT("기존 Blueprint 파일 삭제 실패: %s"), *FileName),
                Field });
            return false;
        }

        return true;
    }

    bool FGenericBlueprintBuilder::Build(const FBlueprintSpec& Spec,
                                         const FGenericBlueprintBuildOptions& Options,
                                         TArray<FCommandForgeError>& OutErrors,
                                         TMap<FString, FString>& OutValidation)
    {
        const FString Field = Options.FieldPrefix.IsEmpty()
            ? TEXT("Blueprint.AssetPath")
            : Options.FieldPrefix + TEXT(".AssetPath");
        const FString ParentField = Options.FieldPrefix.IsEmpty()
            ? TEXT("Blueprint.ParentClass")
            : Options.FieldPrefix + TEXT(".ParentClass");

        UClass* ParentClass = ResolveParentClass(Spec.ParentClass, Options.ParentClassModules);
        if (!ParentClass)
        {
            OutErrors.Add({ TEXT("PARENT_CLASS_NOT_FOUND"),
                FString::Printf(TEXT("부모 클래스를 찾을 수 없습니다: %s"), *Spec.ParentClass),
                ParentField });
            return false;
        }

        if (!DeleteExistingBlueprint(Spec.AssetPath, Field, OutErrors))
        {
            return false;
        }

        const FString AssetName = FPackageName::GetLongPackageAssetName(Spec.AssetPath);
        UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
            ParentClass,
            CreatePackage(*Spec.AssetPath),
            *AssetName,
            BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass());

        if (!Blueprint)
        {
            OutErrors.Add({ TEXT("BP_CREATE_FAILED"),
                FString::Printf(TEXT("Blueprint 생성 실패: %s"), *Spec.AssetPath),
                Field });
            return false;
        }

        OutValidation.Add(TEXT("parent_class"), ParentClass->GetPathName());
        return FBlueprintValidationRunner::CompileSaveAndValidate(
            Blueprint, Spec.AssetPath, Field, OutErrors, OutValidation);
    }
}
