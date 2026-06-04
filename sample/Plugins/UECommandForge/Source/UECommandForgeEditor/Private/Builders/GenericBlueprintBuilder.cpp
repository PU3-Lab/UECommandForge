#include "Builders/GenericBlueprintBuilder.h"

#include "Builders/BlueprintValidationRunner.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"
#include "UObject/GarbageCollection.h"
#include "GameFramework/Actor.h"

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

        // 1. 부모 클래스 로드 및 검증
        UClass* ParentClass = ResolveParentClass(Spec.ParentClass, Options.ParentClassModules);
        if (!ParentClass)
        {
            OutErrors.Add({ TEXT("PARENT_CLASS_NOT_FOUND"),
                FString::Printf(TEXT("부모 클래스를 찾을 수 없습니다: %s"), *Spec.ParentClass),
                ParentField });
            return false;
        }

        OutValidation.Add(TEXT("parent_class_loaded"), TEXT("true"));

        if (Options.bRequireActorParent && !ParentClass->IsChildOf(AActor::StaticClass()))
        {
            OutErrors.Add({ TEXT("PARENT_CLASS_NOT_ACTOR"),
                FString::Printf(TEXT("부모 클래스가 Actor 계층이 아닙니다: %s"), *ParentClass->GetName()),
                ParentField });
            return false;
        }

        if (Options.bRequireBlueprintable && !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
        {
            OutErrors.Add({ TEXT("PARENT_CLASS_NOT_BLUEPRINTABLE"),
                FString::Printf(TEXT("부모 클래스로부터 Blueprint를 생성할 수 없습니다: %s"), *ParentClass->GetName()),
                ParentField });
            return false;
        }

        OutValidation.Add(TEXT("parent_class_blueprintable"), TEXT("true"));

        // 2. 에셋 존재 여부 및 allow_replace 검사
        const FString FileName = FPaths::ConvertRelativePathToFull(
            FPackageName::LongPackageNameToFilename(Spec.AssetPath, FPackageName::GetAssetPackageExtension()));
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        UPackage* ExistingPackage = FindPackage(nullptr, *Spec.AssetPath);
        const bool bAssetExists = ExistingPackage || PlatformFile.FileExists(*FileName);

        if (bAssetExists)
        {
            if (!Options.bAllowReplace)
            {
                OutErrors.Add({ TEXT("BP_ALREADY_EXISTS"),
                    FString::Printf(TEXT("에셋이 이미 존재하며 allow_replace가 false입니다: %s"), *Spec.AssetPath),
                    Field });
                return false;
            }
            else
            {
                if (!DeleteExistingBlueprint(Spec.AssetPath, Field, OutErrors))
                {
                    return false;
                }
            }
        }

        // 3. 블루프린트 생성
        UPackage* Package = CreatePackage(*Spec.AssetPath);
        if (!Package)
        {
            OutErrors.Add({ TEXT("PACKAGE_CREATE_FAILED"),
                FString::Printf(TEXT("패키지 생성에 실패했습니다: %s"), *Spec.AssetPath),
                Field });
            return false;
        }

        const FString AssetName = FPackageName::GetLongPackageAssetName(Spec.AssetPath);
        UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
            ParentClass,
            Package,
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

        // GC 보호 강제 등록 및 해제 가드
        Blueprint->AddToRoot();
        Package->AddToRoot();

        ON_SCOPE_EXIT
        {
            if (Blueprint)
            {
                Blueprint->RemoveFromRoot();
            }
            if (Package)
            {
                Package->RemoveFromRoot();
            }
        };

        OutValidation.Add(TEXT("asset_created"), TEXT("true"));
        OutValidation.Add(TEXT("parent_class"), ParentClass->GetPathName());

        // 4. 컴파일 및 저장 검증
        bool bSuccess = true;

        if (Options.bCompile)
        {
            if (!FBlueprintValidationRunner::Compile(Blueprint, Spec.AssetPath, Field, OutErrors, OutValidation))
            {
                bSuccess = false;
            }
        }
        else
        {
            OutValidation.Add(TEXT("compile_status"), TEXT("skipped"));
        }

        if (bSuccess && Options.bSave)
        {
            if (!FBlueprintValidationRunner::Save(Blueprint, Spec.AssetPath, Field, OutErrors, OutValidation))
            {
                bSuccess = false;
            }
        }
        else if (!Options.bSave)
        {
            OutValidation.Add(TEXT("asset_on_disk"), TEXT("skipped"));
            OutValidation.Add(TEXT("asset_in_registry"), TEXT("skipped"));
        }

        return bSuccess;
    }
}

