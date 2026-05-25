# UECommandForge Phase 6 — 맵 배치 구현 계획

> **에이전트 실행자 필독:** Phase 5 (`2026-05-25-uecommandforge-phase5.md`) 완료 후 실행하세요.
> REQUIRED SUB-SKILL: `superpowers:subagent-driven-development` (권장) 또는 `superpowers:executing-plans`

**목표:** 지정된 맵(없으면 자동 생성)을 열고, 주어진 Transform 위치에 Actor를 배치한 뒤 저장한다. 검증 단계에서 저장된 맵에 해당 Actor 클래스가 실제로 존재하는지 확인한다.

**아키텍처:** `FMapActorPlacer`가 `UEditorLoadingAndSavingUtils::LoadMap` → `GEditor->AddActor` → `UEditorLoadingAndSavingUtils::SaveCurrentLevel` 순서로 동작한다. 맵이 없으면 `UWorldFactory`로 신규 생성한다. `PlaceActorCommandlet`이 Placer를 호출하고 결과를 Result JSON에 기록한다.

**기술 스택:** UE 5.7, `UnrealEd`, `EditorScriptingUtilities`, `AssetTools`.

---

## 파일 구조

| 경로 | 책임 |
|---|---|
| `Plugins/.../Public/Builders/MapActorPlacer.h` | 맵 열기·Actor 배치·저장 인터페이스 |
| `Plugins/.../Private/Builders/MapActorPlacer.cpp` | 구현 |
| `Plugins/.../Private/Commandlets/PlaceActorCommandlet.h/.cpp` | 배치 Commandlet |
| `Plugins/.../Tests/MapActorPlacerTest.cpp` | 자동화 테스트 |
| `tools/ue/place_actor.sh` | Shell Wrapper |

---

## Task 1: MapActorPlacer (TDD)

- [ ] **Step 1: 실패하는 자동화 테스트 작성**

```cpp
// Tests/MapActorPlacerTest.cpp
#include "Misc/AutomationTest.h"
#include "Builders/MapActorPlacer.h"
#include "Specs/PlacementSpec.h"
#include "CommandForgeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMapActorPlacerTest,
    "UECommandForge.Builders.MapActorPlacer.PlacesActorInMap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapActorPlacerTest::RunTest(const FString& Parameters)
{
    FPlacementSpec Spec;
    Spec.MapPath        = TEXT("/Game/Tests/TestPlacementMap");
    Spec.BlueprintPath  = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.Location       = FVector(100.0f, 200.0f, 0.0f);
    Spec.Rotation       = FRotator::ZeroRotator;

    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FMapActorPlacer::Place(Spec, Errors);

    TestTrue(TEXT("배치 성공"), bOk);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());
    return true;
}
```

- [ ] **Step 2: 테스트 실행 — 실패 확인**

```bash
./tools/ue/run_automation_tests.sh
```

예상: `FMapActorPlacerTest` FAIL — `FMapActorPlacer` 미정의.

- [ ] **Step 3: `MapActorPlacer.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/PlacementSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FMapActorPlacer
    {
    public:
        static bool Place(const FPlacementSpec& Spec, TArray<FCommandForgeError>& OutErrors);

    private:
        static UWorld* LoadOrCreateMap(const FString& MapPath,
                                        TArray<FCommandForgeError>& OutErrors);
    };
}
```

- [ ] **Step 4: `MapActorPlacer.cpp` 작성**

```cpp
#include "Builders/MapActorPlacer.h"
#include "EditorLoadingAndSavingUtils.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "Factories/WorldFactory.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace UECommandForge
{
    UWorld* FMapActorPlacer::LoadOrCreateMap(const FString& MapPath,
                                               TArray<FCommandForgeError>& OutErrors)
    {
        // 맵 파일이 존재하면 로드
        const FString MapFilePath = FPackageName::LongPackageNameToFilename(
            MapPath, FPackageName::GetMapPackageExtension());

        if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*MapFilePath))
        {
            UEditorLoadingAndSavingUtils::LoadMap(MapPath);
            return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        }

        // 맵이 없으면 신규 생성
        UWorldFactory* Factory = NewObject<UWorldFactory>();
        Factory->WorldType = EWorldType::Editor;

        IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
        const FString PackagePath = FPackageName::GetLongPackagePath(MapPath);
        const FString AssetName   = FPackageName::GetLongPackageAssetName(MapPath);

        UObject* NewMap = AssetTools.CreateAsset(AssetName, PackagePath,
                                                   UWorld::StaticClass(), Factory);
        if (!NewMap)
        {
            OutErrors.Add({ TEXT("MAP_CREATE_FAILED"),
                FString::Printf(TEXT("맵 생성 실패: %s"), *MapPath),
                TEXT("Placement.MapPath") });
            return nullptr;
        }

        UEditorLoadingAndSavingUtils::LoadMap(MapPath);
        return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    }

    bool FMapActorPlacer::Place(const FPlacementSpec& Spec, TArray<FCommandForgeError>& OutErrors)
    {
        UWorld* World = LoadOrCreateMap(Spec.MapPath, OutErrors);
        if (!World)
        {
            if (OutErrors.IsEmpty())
            {
                OutErrors.Add({ TEXT("MAP_LOAD_FAILED"),
                    FString::Printf(TEXT("맵 로드 실패: %s"), *Spec.MapPath),
                    TEXT("Placement.MapPath") });
            }
            return false;
        }

        // Blueprint 클래스 로드
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Spec.BlueprintPath);
        if (!BP || !BP->GeneratedClass)
        {
            OutErrors.Add({ TEXT("BLUEPRINT_NOT_FOUND"),
                FString::Printf(TEXT("Blueprint 없음: %s"), *Spec.BlueprintPath),
                TEXT("Placement.BlueprintPath") });
            return false;
        }

        // Actor 배치
        const FTransform Transform(Spec.Rotation, Spec.Location);
        AActor* PlacedActor = GEditor->AddActor(
            World->GetCurrentLevel(),
            BP->GeneratedClass,
            Transform);

        if (!PlacedActor)
        {
            OutErrors.Add({ TEXT("PLACE_FAILED"),
                FString::Printf(TEXT("Actor 배치 실패: %s"), *Spec.BlueprintPath),
                TEXT("") });
            return false;
        }

        // 맵 저장
        UEditorLoadingAndSavingUtils::SaveCurrentLevel();
        return true;
    }
}
```

- [ ] **Step 5: 테스트 재실행 — 통과 확인**

```bash
./tools/ue/run_automation_tests.sh
```

예상: `FMapActorPlacerTest` PASS.

- [ ] **Step 6: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/MapActorPlacer.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/MapActorPlacer.cpp \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/MapActorPlacerTest.cpp
git commit -m "feat: MapActorPlacer — 맵 열기·Actor 배치·저장 + 자동화 테스트"
```

---

## Task 2: Actor 배치 검증

저장된 맵을 다시 로드해 해당 Actor 클래스가 존재하는지 리플렉션으로 확인한다.

- [ ] **Step 1: `MapActorPlacer.h`에 검증 메서드 추가**

```cpp
// MapActorPlacer.h에 추가
static bool ValidatePlacement(const FPlacementSpec& Spec,
                               TMap<FString, FString>& OutValidation,
                               TArray<FCommandForgeError>& OutErrors);
```

- [ ] **Step 2: `MapActorPlacer.cpp`에 구현 추가**

```cpp
bool FMapActorPlacer::ValidatePlacement(const FPlacementSpec& Spec,
                                         TMap<FString, FString>& OutValidation,
                                         TArray<FCommandForgeError>& OutErrors)
{
    UEditorLoadingAndSavingUtils::LoadMap(Spec.MapPath);
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    if (!World)
    {
        OutErrors.Add({ TEXT("MAP_LOAD_FAILED"),
            FString::Printf(TEXT("검증용 맵 로드 실패: %s"), *Spec.MapPath),
            TEXT("Placement.MapPath") });
        return false;
    }

    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Spec.BlueprintPath);
    if (!BP || !BP->GeneratedClass) { return false; }

    bool bFound = false;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if ((*It)->IsA(BP->GeneratedClass))
        {
            bFound = true;
            break;
        }
    }

    OutValidation.Add(TEXT("actor_placed"), bFound ? TEXT("ok") : TEXT("not_found"));
    return bFound;
}
```

- [ ] **Step 3: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/MapActorPlacer.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/MapActorPlacer.cpp
git commit -m "feat: MapActorPlacer — Actor 배치 검증 메서드 추가"
```

---

## Task 3: PlaceActorCommandlet + Shell Wrapper

- [ ] **Step 1: `PlaceActorCommandlet.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PlaceActorCommandlet.generated.h"

UCLASS()
class UPlaceActorCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UPlaceActorCommandlet();
    virtual int32 Main(const FString& Params) override;
};
```

- [ ] **Step 2: `PlaceActorCommandlet.cpp` 작성**

Phase 3 Commandlet 패턴. `FMapActorPlacer::Place` 후 `FMapActorPlacer::ValidatePlacement` 호출. 검증 결과를 `Report.Validation`에 병합. `Report.Commandlet = TEXT("PlaceActor")`.

```cpp
#include "PlaceActorCommandlet.h"
#include "Specs/AIFlowSpecParser.h"
#include "Specs/AIFlowSpecValidator.h"
#include "Builders/MapActorPlacer.h"
#include "Reports/JsonReportWriter.h"
#include "Misc/FileHelper.h"

UPlaceActorCommandlet::UPlaceActorCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UPlaceActorCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens; TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("SpecFile"));
    const FString OutPath  = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("PlaceActor");

    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *SpecFile))
    {
        Report.Errors.Add({ TEXT("SPEC_FILE_NOT_FOUND"),
            FString::Printf(TEXT("파일 없음: %s"), *SpecFile), TEXT("") });
        UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FAIFlowSpec Spec;
    if (!UECommandForge::FAIFlowSpecParser::Parse(JsonStr, Spec, Report.Errors) ||
        !UECommandForge::FAIFlowSpecValidator::Validate(Spec, Report.Errors))
    {
        UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    if (!UECommandForge::FMapActorPlacer::Place(Spec.Placement, Report.Errors))
    {
        UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
    }

    UECommandForge::FMapActorPlacer::ValidatePlacement(Spec.Placement, Report.Validation, Report.Errors);
    Report.bOk = Report.Errors.IsEmpty();
    Report.ModifiedAssets.Add(Spec.Placement.MapPath);

    UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
```

- [ ] **Step 3: `tools/ue/place_actor.sh` 작성**

```bash
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/run_commandlet.sh" PlaceActor -SpecFile="$1" "$@"
```

- [ ] **Step 4: 실행 권한 부여 및 커밋**

```bash
chmod +x tools/ue/place_actor.sh
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/PlaceActorCommandlet.* \
        tools/ue/place_actor.sh
git commit -m "feat: PlaceActorCommandlet + Shell Wrapper"
```

---

## Phase 6 인수 조건

```bash
# 1. 플러그인 빌드
./tools/ue/build_plugin.sh

# 2. 자동화 테스트
./tools/ue/run_automation_tests.sh

# 3. Actor 배치
./tools/ue/place_actor.sh specs/examples/guard_ai.json
jq -e '.ok and .validation.actor_placed == "ok"' \
  "$(ls -t Saved/CodexReports/PlaceActor_*.json | head -1)"
```

Phase 6 완료 조건: 지정 맵에 BP_Guard Actor가 배치되고, `actor_placed: "ok"`가 Result JSON에 기록됨.
