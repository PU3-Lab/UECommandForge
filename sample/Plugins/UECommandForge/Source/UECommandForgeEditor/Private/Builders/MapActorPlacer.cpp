#include "Builders/MapActorPlacer.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace UECommandForge
{
    namespace
    {
        void AddMapError(TArray<FCommandForgeError>& OutErrors,
                         const TCHAR* Code,
                         const FString& Message,
                         const TCHAR* Field)
        {
            OutErrors.Add({ Code, Message, Field });
        }

        FString MapFileName(const FString& MapPath)
        {
            return FPaths::ConvertRelativePathToFull(
                FPackageName::LongPackageNameToFilename(MapPath, FPackageName::GetMapPackageExtension()));
        }

        bool ValidateMapPath(const FString& MapPath,
                             TArray<FCommandForgeError>& OutErrors)
        {
            if (MapPath.IsEmpty() || !MapPath.StartsWith(TEXT("/Game/")))
            {
                AddMapError(OutErrors, TEXT("INVALID_MAP_PATH"),
                    FString::Printf(TEXT("맵 경로는 /Game/ 으로 시작해야 합니다: %s"), *MapPath),
                    TEXT("Placement.MapPath"));
                return false;
            }

            FText Reason;
            if (!FPackageName::IsValidLongPackageName(MapPath, false, &Reason))
            {
                AddMapError(OutErrors, TEXT("INVALID_MAP_PATH"),
                    FString::Printf(TEXT("유효한 Unreal 맵 패키지 경로가 아닙니다: %s (%s)"),
                        *MapPath,
                        *Reason.ToString()),
                    TEXT("Placement.MapPath"));
                return false;
            }

            return true;
        }

        UBlueprint* LoadPlacementBlueprint(const FString& BlueprintPath,
                                           TArray<FCommandForgeError>& OutErrors)
        {
            UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
            if (!Blueprint || !Blueprint->GeneratedClass)
            {
                AddMapError(OutErrors, TEXT("BLUEPRINT_NOT_FOUND"),
                    FString::Printf(TEXT("Blueprint 없음: %s"), *BlueprintPath),
                    TEXT("Placement.BlueprintPath"));
                return nullptr;
            }
            return Blueprint;
        }

        void RemoveExistingPlacedActors(UWorld* World, UClass* ActorClass)
        {
            TArray<AActor*> ActorsToRemove;
            for (const ULevel* Level : World->GetLevels())
            {
                if (!Level)
                {
                    continue;
                }
                for (AActor* Actor : Level->Actors)
                {
                    if (Actor && Actor->IsA(ActorClass))
                    {
                        ActorsToRemove.Add(Actor);
                    }
                }
            }

            for (AActor* Actor : ActorsToRemove)
            {
                World->EditorDestroyActor(Actor, true);
            }
        }
    }

    UWorld* FMapActorPlacer::LoadOrCreateMap(const FString& MapPath,
                                             TArray<FCommandForgeError>& OutErrors)
    {
        if (!ValidateMapPath(MapPath, OutErrors))
        {
            return nullptr;
        }

        if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*MapFileName(MapPath)))
        {
            UEditorLoadingAndSavingUtils::LoadMap(MapPath);
            return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        }

        UWorld* NewWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
        if (!NewWorld)
        {
            AddMapError(OutErrors, TEXT("MAP_CREATE_FAILED"),
                FString::Printf(TEXT("맵 생성 실패: %s"), *MapPath),
                TEXT("Placement.MapPath"));
            return nullptr;
        }
        if (!UEditorLoadingAndSavingUtils::SaveMap(NewWorld, MapPath))
        {
            AddMapError(OutErrors, TEXT("MAP_SAVE_FAILED"),
                FString::Printf(TEXT("신규 맵 저장 실패: %s"), *MapPath),
                TEXT("Placement.MapPath"));
            return nullptr;
        }
        return NewWorld;
    }

    bool FMapActorPlacer::Place(const FPlacementSpec& Spec, TArray<FCommandForgeError>& OutErrors)
    {
        UWorld* World = LoadOrCreateMap(Spec.MapPath, OutErrors);
        if (!World)
        {
            if (OutErrors.IsEmpty())
            {
                AddMapError(OutErrors, TEXT("MAP_LOAD_FAILED"),
                    FString::Printf(TEXT("맵 로드 실패: %s"), *Spec.MapPath),
                    TEXT("Placement.MapPath"));
            }
            return false;
        }

        UBlueprint* Blueprint = LoadPlacementBlueprint(Spec.BlueprintPath, OutErrors);
        if (!Blueprint)
        {
            return false;
        }

        RemoveExistingPlacedActors(World, Blueprint->GeneratedClass);
        const FTransform Transform(Spec.Rotation, Spec.Location);
        AActor* Actor = GEditor ? GEditor->AddActor(World->GetCurrentLevel(), Blueprint->GeneratedClass, Transform) : nullptr;
        if (!Actor)
        {
            AddMapError(OutErrors, TEXT("PLACE_ACTOR_FAILED"),
                FString::Printf(TEXT("Actor 배치 실패: %s"), *Spec.BlueprintPath),
                TEXT("Placement.BlueprintPath"));
            return false;
        }

        World->MarkPackageDirty();
        if (!UEditorLoadingAndSavingUtils::SaveMap(World, Spec.MapPath))
        {
            AddMapError(OutErrors, TEXT("MAP_SAVE_FAILED"),
                FString::Printf(TEXT("맵 저장 실패: %s"), *Spec.MapPath),
                TEXT("Placement.MapPath"));
            return false;
        }
        return true;
    }

    bool FMapActorPlacer::ValidatePlacement(const FPlacementSpec& Spec,
                                            TMap<FString, FString>& OutValidation,
                                            TArray<FCommandForgeError>& OutErrors)
    {
        if (!ValidateMapPath(Spec.MapPath, OutErrors))
        {
            OutValidation.Add(TEXT("actor_placed"), TEXT("not_found"));
            return false;
        }

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World || World->GetOutermost()->GetName() != Spec.MapPath)
        {
            World = UEditorLoadingAndSavingUtils::LoadMap(Spec.MapPath);
        }
        if (!World)
        {
            AddMapError(OutErrors, TEXT("MAP_LOAD_FAILED"),
                FString::Printf(TEXT("검증용 맵 로드 실패: %s"), *Spec.MapPath),
                TEXT("Placement.MapPath"));
            OutValidation.Add(TEXT("actor_placed"), TEXT("not_found"));
            return false;
        }

        UBlueprint* Blueprint = LoadPlacementBlueprint(Spec.BlueprintPath, OutErrors);
        if (!Blueprint)
        {
            OutValidation.Add(TEXT("actor_placed"), TEXT("not_found"));
            return false;
        }

        bool bFound = false;
        for (ULevel* Level : World->GetLevels())
        {
            if (!Level)
            {
                continue;
            }
            for (AActor* Actor : Level->Actors)
            {
                if (Actor && Actor->IsA(Blueprint->GeneratedClass))
                {
                    bFound = true;
                    break;
                }
            }
            if (bFound)
            {
                break;
            }
        }

        OutValidation.Add(TEXT("actor_placed"), bFound ? TEXT("ok") : TEXT("not_found"));
        if (!bFound)
        {
            AddMapError(OutErrors, TEXT("ACTOR_NOT_FOUND"),
                FString::Printf(TEXT("맵에서 Actor를 찾을 수 없습니다: %s"), *Spec.BlueprintPath),
                TEXT("Placement.BlueprintPath"));
        }
        return bFound;
    }
}
