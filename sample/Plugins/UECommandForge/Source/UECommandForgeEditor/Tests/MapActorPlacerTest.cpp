#include "Misc/AutomationTest.h"
#include "Builders/MapActorPlacer.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Specs/PlacementSpec.h"
#include "UObject/GarbageCollection.h"

namespace
{
    const TCHAR* TestPlacementMapPath = TEXT("/Game/Tests/TestPlacementMap");

    FString TestPlacementMapFileName()
    {
        return FPaths::ConvertRelativePathToFull(
            FPackageName::LongPackageNameToFilename(TestPlacementMapPath, FPackageName::GetMapPackageExtension()));
    }

    void DeleteTestPlacementMap()
    {
        if (GEditor)
        {
            UEditorLoadingAndSavingUtils::NewBlankMap(false);
        }
        CollectGarbage(RF_NoFlags);

        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        const FString FileName = TestPlacementMapFileName();
        if (PlatformFile.FileExists(*FileName))
        {
            PlatformFile.DeleteFile(*FileName);
        }
    }

    int32 CountPlacedActors(const FPlacementSpec& Spec)
    {
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Spec.BlueprintPath);
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!Blueprint || !Blueprint->GeneratedClass || !World)
        {
            return 0;
        }

        int32 Count = 0;
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
                    ++Count;
                }
            }
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMapActorPlacerTest,
    "UECommandForge.Builders.MapActorPlacer.PlacesActorInMap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapActorPlacerTest::RunTest(const FString& Parameters)
{
    DeleteTestPlacementMap();

    FPlacementSpec Spec;
    Spec.MapPath = TestPlacementMapPath;
    Spec.BlueprintPath = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.Location = FVector(100.0f, 200.0f, 0.0f);
    Spec.Rotation = FRotator::ZeroRotator;

    TArray<FCommandForgeError> Errors;
    TMap<FString, FString> Validation;
    const bool bOk = UECommandForge::FMapActorPlacer::Place(Spec, Errors);
    const bool bValid = UECommandForge::FMapActorPlacer::ValidatePlacement(Spec, Validation, Errors);
    const bool bSecondPlaceOk = UECommandForge::FMapActorPlacer::Place(Spec, Errors);
    const int32 PlacedActorCount = CountPlacedActors(Spec);

    TestTrue(TEXT("배치 성공"), bOk);
    TestTrue(TEXT("검증 성공"), bValid);
    TestTrue(TEXT("반복 배치 성공"), bSecondPlaceOk);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());
    TestEqual(TEXT("actor_placed"), Validation.FindRef(TEXT("actor_placed")), FString(TEXT("ok")));
    TestEqual(TEXT("반복 실행 후 Actor 1개"), PlacedActorCount, 1);

    DeleteTestPlacementMap();
    return true;
}
