# UECommandForge Phase 7 — 워크플로우 Commandlet 구현 계획

> **에이전트 실행자 필독:** Phase 6 (`2026-05-25-uecommandforge-phase6.md`) 완료 후 실행하세요.
> REQUIRED SUB-SKILL: `superpowers:subagent-driven-development` (권장) 또는 `superpowers:executing-plans`

**목표:** Phase 3~6의 개별 Commandlet을 단일 `UnrealEditor-Cmd` 부팅에서 선형 실행하는 `CreateAIFlowCommandlet`을 구현한다. 중간 실패 시 `rollback_available` 플래그를 Result JSON에 기록한다. `SetupNPCCharacter`와 `SetupPatrolAI`는 Spec 프로파일 래퍼로 제공한다.

**아키텍처:** `CreateAIFlowCommandlet`이 내부적으로 `CharacterBlueprintBuilder → AIControllerBlueprintBuilder → StateTreeBuilder → AIFlowBinder → MapActorPlacer`를 순서대로 호출한다. 각 단계 결과는 Result JSON의 하위 섹션에 기록된다. 어느 단계에서 실패해도 이미 생성된 에셋 목록을 `created_assets`에 유지해 `rollback_available: true`를 표시한다.

**기술 스택:** Phase 1~6 빌더·검증기 전부. 신규 모듈 의존성 없음.

---

## 파일 구조

| 경로 | 책임 |
|---|---|
| `Plugins/.../Private/Commandlets/CreateAIFlowCommandlet.h/.cpp` | 워크플로우 오케스트레이터 |
| `Plugins/.../Tests/CreateAIFlowCommandletTest.cpp` | 전체 플로우 자동화 테스트 |
| `tools/ue/create_ai_flow.sh` | 워크플로우 Shell Wrapper |
| `tools/ue/setup_npc_character.sh` | NPC Character Spec 프로파일 래퍼 |
| `tools/ue/setup_patrol_ai.sh` | Patrol AI Spec 프로파일 래퍼 |
| `tools/ue/*.bat` | Windows Command Prompt용 UE Commandlet 래퍼 |
| `tools/test/automation/run.bat` | Windows Command Prompt용 자동화 테스트 실행 래퍼 |
| `tools/test/smoke/create_ai_flow.bat` | Windows Command Prompt용 CreateAIFlow 스모크 테스트 |
| `tools/test/smoke/windows_command_wrappers.sh` | Windows 래퍼 정적 스모크 테스트 |
| `specs/profiles/npc_character.json` | NPC Character 기본 Spec 프로파일 |
| `specs/profiles/patrol_ai.json` | Patrol AI 기본 Spec 프로파일 |

---

## Task 1: 워크플로우 Result JSON 구조 확장

Result JSON에 `steps` 배열을 추가해 각 단계 결과를 기록한다. `FCommandForgeReport`를 확장한다.

- [x] **Step 1: `CommandForgeTypes.h`에 `FCommandForgeStepResult` 추가**

```cpp
// CommandForgeTypes.h에 추가
USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeStepResult
{
    GENERATED_BODY()

    UPROPERTY() FString Step;
    UPROPERTY() bool bOk = false;
    UPROPERTY() TArray<FString> CreatedAssets;
    UPROPERTY() TMap<FString, FString> Validation;
    UPROPERTY() TArray<FCommandForgeError> Errors;
};
```

- [x] **Step 2: `FCommandForgeReport`에 `Steps`와 `bRollbackAvailable` 추가**

```cpp
// JsonReportWriter.h의 FCommandForgeReport에 추가
TArray<FCommandForgeStepResult> Steps;
bool bRollbackAvailable = false;
```

- [x] **Step 3: `JsonReportWriter.cpp`에 `steps`와 `rollback_available` 직렬화 추가**

`Write` 메서드에 아래 코드 추가:

```cpp
Root->SetBoolField(TEXT("rollback_available"), Report.bRollbackAvailable);

TArray<TSharedPtr<FJsonValue>> StepValues;
for (const FCommandForgeStepResult& Step : Report.Steps)
{
    TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
    S->SetStringField(TEXT("step"), Step.Step);
    S->SetBoolField(TEXT("ok"),     Step.bOk);
    S->SetArrayField(TEXT("created_assets"), ToArray(Step.CreatedAssets));

    TSharedRef<FJsonObject> StepValidation = MakeShared<FJsonObject>();
    for (const auto& Kv : Step.Validation)
    {
        StepValidation->SetStringField(Kv.Key, Kv.Value);
    }
    S->SetObjectField(TEXT("validation"), StepValidation);

    TArray<TSharedPtr<FJsonValue>> StepErrors;
    for (const FCommandForgeError& E : Step.Errors)
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("code"),    E.Code);
        O->SetStringField(TEXT("message"), E.Message);
        O->SetStringField(TEXT("field"),   E.Field);
        StepErrors.Add(MakeShared<FJsonValueObject>(O));
    }
    S->SetArrayField(TEXT("errors"), StepErrors);
    StepValues.Add(MakeShared<FJsonValueObject>(S));
}
Root->SetArrayField(TEXT("steps"), StepValues);
```

- [x] **Step 4: 커밋**

완료 커밋: `52135e4 feat: add workflow result steps to JSON report`

```bash
git add Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/CommandForgeTypes.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Reports/JsonReportWriter.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Reports/JsonReportWriter.cpp
git commit -m "feat: Result JSON에 steps·rollback_available 필드 추가"
```

---

## Task 2: CreateAIFlowCommandlet (TDD)

- [x] **Step 1: 실패하는 자동화 테스트 작성**

```cpp
// Tests/CreateAIFlowCommandletTest.cpp
#include "Misc/AutomationTest.h"
#include "Commandlets/CreateAIFlowCommandlet.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateAIFlowCommandletTest,
    "UECommandForge.Commandlets.CreateAIFlow.ExecutesFullWorkflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCreateAIFlowCommandletTest::RunTest(const FString& Parameters)
{
    const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_create_ai_flow.json"));

    // 테스트용 Spec 직접 구성 (specs/examples/guard_ai.json 내용)
    FAIFlowSpec Spec;
    Spec.Character.AssetPath    = TEXT("/Game/Tests/Workflow/BP_Guard");
    Spec.Character.ParentClass  = TEXT("Character");
    Spec.Character.DisplayName  = TEXT("GuardCharacter");
    Spec.AIController.AssetPath = TEXT("/Game/Tests/Workflow/BP_GuardCtrl");
    Spec.AIController.ParentClass = TEXT("AIController");
    Spec.AIController.DisplayName = TEXT("GuardController");
    Spec.StateTree.AssetPath    = TEXT("/Game/Tests/Workflow/ST_Guard");
    Spec.StateTree.Tasks.Add({ TEXT("Wait"),   TEXT("Idle") });
    Spec.StateTree.Tasks.Add({ TEXT("MoveTo"), TEXT("Patrol") });
    Spec.Placement.MapPath       = TEXT("/Game/Tests/Workflow/TestMap");
    Spec.Placement.BlueprintPath = TEXT("/Game/Tests/Workflow/BP_Guard");

    FCommandForgeReport Report;
    const bool bOk = UECommandForge::FCreateAIFlowWorkflow::Execute(Spec, OutPath, Report);

    TestTrue(TEXT("전체 플로우 성공"), bOk);
    TestTrue(TEXT("steps 5개"), Report.Steps.Num() == 5);
    TestFalse(TEXT("rollback 불필요"), Report.bRollbackAvailable);

    FString Content;
    TestTrue(TEXT("파일 존재"), FFileHelper::LoadFileToString(Content, *OutPath));
    TestTrue(TEXT("actor_placed ok"), Content.Contains(TEXT("\"actor_placed\"")));
    return true;
}
```

- [x] **Step 2: `CreateAIFlowCommandlet.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"
#include "CreateAIFlowCommandlet.generated.h"

namespace UECommandForge
{
    class FCreateAIFlowWorkflow
    {
    public:
        static bool Execute(const FAIFlowSpec& Spec, const FString& OutPath,
                             FCommandForgeReport& OutReport);
    };
}

UCLASS()
class UCreateAIFlowCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCreateAIFlowCommandlet();
    virtual int32 Main(const FString& Params) override;
};
```

- [x] **Step 3: `CreateAIFlowCommandlet.cpp` 작성**

```cpp
#include "Commandlets/CreateAIFlowCommandlet.h"
#include "Specs/AIFlowSpecParser.h"
#include "Specs/AIFlowSpecValidator.h"
#include "Builders/CharacterBlueprintBuilder.h"
#include "Builders/AIControllerBlueprintBuilder.h"
#include "Builders/StateTreeBuilder.h"
#include "Builders/AIFlowBinder.h"
#include "Builders/MapActorPlacer.h"
#include "Reports/JsonReportWriter.h"
#include "Misc/FileHelper.h"

namespace UECommandForge
{
    // 단계 실행 헬퍼: 성공 시 Report.CreatedAssets에 추가, 실패 시 rollback_available 설정
    static bool RunStep(FCommandForgeReport& Report, const FString& StepName,
                        TFunctionRef<bool(FCommandForgeStepResult&)> Func)
    {
        FCommandForgeStepResult Step;
        Step.Step = StepName;
        Step.bOk  = Func(Step);

        if (!Step.bOk) { Report.bRollbackAvailable = !Report.CreatedAssets.IsEmpty(); }
        for (const FString& A : Step.CreatedAssets) { Report.CreatedAssets.Add(A); }
        Report.Steps.Add(MoveTemp(Step));
        return Step.bOk;
    }

    bool FCreateAIFlowWorkflow::Execute(const FAIFlowSpec& Spec, const FString& OutPath,
                                         FCommandForgeReport& OutReport)
    {
        OutReport.Commandlet = TEXT("CreateAIFlow");

        // Step 1: Character Blueprint
        if (!RunStep(OutReport, TEXT("CreateCharacterBlueprint"),
            [&](FCommandForgeStepResult& S)
            {
                if (!FCharacterBlueprintBuilder::Build(Spec.Character, S.Errors)) { return false; }
                S.CreatedAssets.Add(Spec.Character.AssetPath);
                S.Validation.Add(TEXT("blueprint_compile"), TEXT("passed"));
                return true;
            })) { UECommandForge::FJsonReportWriter::Write(OutPath, OutReport); return false; }

        // Step 2: AIController Blueprint
        if (!RunStep(OutReport, TEXT("CreateAIControllerBlueprint"),
            [&](FCommandForgeStepResult& S)
            {
                if (!FAIControllerBlueprintBuilder::Build(Spec.AIController, S.Errors)) { return false; }
                S.CreatedAssets.Add(Spec.AIController.AssetPath);
                S.Validation.Add(TEXT("blueprint_compile"), TEXT("passed"));
                return true;
            })) { UECommandForge::FJsonReportWriter::Write(OutPath, OutReport); return false; }

        // Step 3: StateTree
        if (!RunStep(OutReport, TEXT("CreateStateTree"),
            [&](FCommandForgeStepResult& S)
            {
                if (!FStateTreeBuilder::Build(Spec.StateTree, S.Errors)) { return false; }
                S.CreatedAssets.Add(Spec.StateTree.AssetPath);
                return true;
            })) { UECommandForge::FJsonReportWriter::Write(OutPath, OutReport); return false; }

        // Step 4: AI Flow Binding
        if (!RunStep(OutReport, TEXT("BindAIFlow"),
            [&](FCommandForgeStepResult& S)
            {
                return FAIFlowBinder::Bind(Spec, S.Errors);
            })) { UECommandForge::FJsonReportWriter::Write(OutPath, OutReport); return false; }

        // Step 5: Map Placement
        if (!RunStep(OutReport, TEXT("PlaceActor"),
            [&](FCommandForgeStepResult& S)
            {
                if (!FMapActorPlacer::Place(Spec.Placement, S.Errors)) { return false; }
                FMapActorPlacer::ValidatePlacement(Spec.Placement, S.Validation, S.Errors);
                OutReport.ModifiedAssets.Add(Spec.Placement.MapPath);
                return S.Errors.IsEmpty();
            })) { UECommandForge::FJsonReportWriter::Write(OutPath, OutReport); return false; }

        OutReport.bOk = true;
        UECommandForge::FJsonReportWriter::Write(OutPath, OutReport);
        return true;
    }
}

UCreateAIFlowCommandlet::UCreateAIFlowCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UCreateAIFlowCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens; TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("SpecFile"));
    const FString OutPath  = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("CreateAIFlow");

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

    const bool bOk = UECommandForge::FCreateAIFlowWorkflow::Execute(Spec, *OutPath, Report);
    return bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::BuildFailed);
}
```

- [x] **Step 4: 테스트 재실행 — 통과 확인**

```bash
./tools/test/automation/run.sh
```

예상: `FCreateAIFlowCommandletTest` PASS.

검증 결과:
- RED: `CreateAIFlowCommandlet.h` 미존재로 샘플 타깃 빌드 실패 확인
- GREEN: 샘플 타깃 빌드 성공
- 리뷰 수정 RED: 리포트 저장 실패가 플로우 성공으로 처리되는 결함을 실패 테스트로 재현
- 리뷰 수정 GREEN: 리포트 저장 실패 시 `REPORT_WRITE_FAILED` 기록 및 `EngineError` 반환 경로 추가
- 자동화: `./tools/test/automation/run.sh` PASS 15 / FAIL 0
- 리포트: `steps` 5개, `rollback_available=false`, `actor_placed=ok`

- [x] **Step 5: 커밋**

완료 커밋: `dedcd95 feat: add create ai flow commandlet`

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateAIFlowCommandlet.* \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/CreateAIFlowCommandletTest.cpp
git commit -m "feat: CreateAIFlowCommandlet — 워크플로우 오케스트레이터 + 자동화 테스트"
```

---

## Task 3: Shell Wrapper 3종 + Spec 프로파일

- [x] **Step 1: `tools/ue/create_ai_flow.sh` 작성**

```bash
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ "$#" -lt 1 ]; then
  echo "사용법: $0 <spec_file> [extra args...]" >&2
  exit 2
fi
SPEC_INPUT="$1"
shift
SPEC_FILE="$(cd "$(dirname "${SPEC_INPUT}")" && pwd)/$(basename "${SPEC_INPUT}")"
exec "${SCRIPT_DIR}/run_commandlet.sh" CreateAIFlow -SpecFile="${SPEC_FILE}" "$@"
```

- [x] **Step 2: `specs/profiles/npc_character.json` 작성**

```json
{
  "Version": "1",
  "Character": {
    "AssetPath": "/Game/Characters/NPC/BP_NPCCharacter",
    "ParentClass": "Character",
    "DisplayName": "NPCCharacter"
  },
  "AIController": {
    "AssetPath": "/Game/Characters/NPC/BP_NPCController",
    "ParentClass": "AIController",
    "DisplayName": "NPCController"
  },
  "StateTree": {
    "AssetPath": "/Game/Characters/NPC/ST_NPC",
    "Tasks": [
      { "TaskType": "Wait", "StateName": "Idle" }
    ]
  },
  "Placement": {
    "MapPath":       "/Game/Maps/MainLevel",
    "BlueprintPath": "/Game/Characters/NPC/BP_NPCCharacter",
    "Location":  { "X": 0.0, "Y": 0.0, "Z": 0.0 },
    "Rotation":  { "Pitch": 0.0, "Yaw": 0.0, "Roll": 0.0 }
  }
}
```

- [x] **Step 3: `specs/profiles/patrol_ai.json` 작성**

```json
{
  "Version": "1",
  "Character": {
    "AssetPath": "/Game/AI/Patrol/BP_PatrolGuard",
    "ParentClass": "Character",
    "DisplayName": "PatrolGuard"
  },
  "AIController": {
    "AssetPath": "/Game/AI/Patrol/BP_PatrolController",
    "ParentClass": "AIController",
    "DisplayName": "PatrolController"
  },
  "StateTree": {
    "AssetPath": "/Game/AI/Patrol/ST_Patrol",
    "Tasks": [
      { "TaskType": "Wait",   "StateName": "Idle" },
      { "TaskType": "MoveTo", "StateName": "Patrol" }
    ]
  },
  "Placement": {
    "MapPath":       "/Game/Maps/PatrolLevel",
    "BlueprintPath": "/Game/AI/Patrol/BP_PatrolGuard",
    "Location":  { "X": 500.0, "Y": 0.0, "Z": 0.0 },
    "Rotation":  { "Pitch": 0.0, "Yaw": 90.0, "Roll": 0.0 }
  }
}
```

- [x] **Step 4: `tools/ue/setup_npc_character.sh` 작성**

```bash
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
exec "${SCRIPT_DIR}/create_ai_flow.sh" "${REPO_ROOT}/specs/profiles/npc_character.json" "$@"
```

- [x] **Step 5: `tools/ue/setup_patrol_ai.sh` 작성**

```bash
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
exec "${SCRIPT_DIR}/create_ai_flow.sh" "${REPO_ROOT}/specs/profiles/patrol_ai.json" "$@"
```

- [x] **Step 6: 실행 권한 부여 및 커밋**

```bash
chmod +x tools/ue/create_ai_flow.sh tools/ue/setup_npc_character.sh tools/ue/setup_patrol_ai.sh
git add tools/ue/create_ai_flow.sh tools/ue/setup_npc_character.sh tools/ue/setup_patrol_ai.sh \
        specs/profiles/
git commit -m "feat: 워크플로우 Shell Wrapper 3종 + Spec 프로파일 (npc_character, patrol_ai)"
```

검증 결과:
- `jq empty specs/profiles/npc_character.json specs/profiles/patrol_ai.json` 통과
- `git diff --check` 통과
- `./tools/test/automation/run.sh` PASS 15 / FAIL 0
- direct wrapper smoke: `./tools/test/smoke/create_ai_flow.sh specs/examples/guard_ai.json` PASS 12 / FAIL 0, `CreateAIFlow_20260528T000727Z.json` 생성

리뷰 수정 결과:
- `tools/ue/run_commandlet.sh`에 `UE_COMMANDLET_TIMEOUT` 기반 무한 대기 방지 가드 추가
- `tools/test/smoke/create_ai_flow.sh` 추가로 `CreateAIFlow` Result JSON, 5단계 성공, 생성 `.uasset`, 수정 `.umap` 검증

---

## Task 3.5: Windows Command Prompt 래퍼

- [x] **Step 1: `tools/ue/*.bat` 커맨드 표면 추가**

Windows Command Prompt에서도 Phase 1~7 Commandlet 표면을 실행할 수 있도록 `.sh` 래퍼와 같은 역할의 `.bat` 파일을 추가했다.

추가 범위:
- `tools/ue/ue_env.bat`
- `tools/ue/run_commandlet.bat`
- `tools/ue/hello.bat`
- `tools/ue/create_character_bp.bat`
- `tools/ue/create_ai_controller.bat`
- `tools/ue/compile_blueprints.bat`
- `tools/ue/create_statetree.bat`
- `tools/ue/bind_ai_flow.bat`
- `tools/ue/validate_ai_flow.bat`
- `tools/ue/place_actor.bat`
- `tools/ue/create_ai_flow.bat`
- `tools/ue/setup_npc_character.bat`
- `tools/ue/setup_patrol_ai.bat`

- [x] **Step 2: Windows 테스트 래퍼 추가**

추가 범위:
- `tools/test/automation/run.bat`
- `tools/test/smoke/create_ai_flow.bat`

`tools/test/smoke/create_ai_flow.bat`은 Result JSON의 `ok`, `rollback_available`, `steps`, `created_assets`, `modified_assets`, `compile_status`, `actor_placed`를 검증하고, 생성된 `.uasset` 및 수정된 `.umap` 파일 존재도 확인한다.

- [x] **Step 3: 정적 스모크 테스트 추가**

`tools/test/smoke/windows_command_wrappers.sh`로 Windows 래퍼 파일 존재와 핵심 커맨드 문자열을 검증한다.

검증 결과:
- `./tools/test/smoke/windows_command_wrappers.sh` 통과
- `bash -n tools/test/smoke/windows_command_wrappers.sh` 통과
- `git diff --check` 통과

로컬 환경 제한:
- 현재 작업 환경은 macOS라 `.bat` 실행 검증은 수행하지 않았다.
- Windows 래퍼는 정적 스모크 테스트로 경로·핵심 문자열·스모크 검증 로직을 확인했다.

---

## Task 4: README 커맨드 표면 업데이트

- [x] **Step 1: `README.md` 커맨드 표면 테이블 업데이트**

```markdown
## Codex 명령 표면 (Phase 7)

| macOS/Linux/Git Bash | Windows Command Prompt | 목적 |
|---|---|---|
| `tools/ue/hello.sh` | `tools\ue\hello.bat` | 헬스체크 — `Hello` commandlet 실행 후 Result JSON 기록 |
| `tools/ue/create_character_bp.sh <spec.json>` | `tools\ue\create_character_bp.bat <spec.json>` | Character Blueprint 생성 |
| `tools/ue/create_ai_controller.sh <spec.json>` | `tools\ue\create_ai_controller.bat <spec.json>` | AIController Blueprint 생성 |
| `tools/ue/compile_blueprints.sh` | `tools\ue\compile_blueprints.bat` | Blueprint 일괄 컴파일 |
| `tools/ue/create_statetree.sh <spec.json>` | `tools\ue\create_statetree.bat <spec.json>` | StateTree 에셋 생성 |
| `tools/ue/bind_ai_flow.sh <spec.json>` | `tools\ue\bind_ai_flow.bat <spec.json>` | Character, AIController, StateTree 바인딩 |
| `tools/ue/validate_ai_flow.sh <spec.json>` | `tools\ue\validate_ai_flow.bat <spec.json>` | AI 플로우 바인딩 CDO 검증 |
| `tools/ue/place_actor.sh <spec.json>` | `tools\ue\place_actor.bat <spec.json>` | 맵에 Actor 배치 |
| `tools/ue/create_ai_flow.sh <spec.json>` | `tools\ue\create_ai_flow.bat <spec.json>` | Phase 3~6 전체 워크플로우 실행 |
| `tools/ue/setup_npc_character.sh` | `tools\ue\setup_npc_character.bat` | NPC Character 기본 프로파일로 워크플로우 실행 |
| `tools/ue/setup_patrol_ai.sh` | `tools\ue\setup_patrol_ai.bat` | Patrol AI 기본 프로파일로 워크플로우 실행 |

LLM은 이 표에 있는 명령만 호출해야 한다.
```

- [x] **Step 2: 커밋**

```bash
git add README.md
git commit -m "docs: README 커맨드 표면 업데이트 (Phase 7 전체 반영)"
```

완료 커밋:
- `4c78748 feat: add windows command wrappers`
- `3fd722a test: record phase 7 acceptance artifacts`

문서 업데이트 결과:
- `README.md`의 Codex 명령 표면을 Phase 7 기준으로 갱신
- macOS/Linux/Git Bash `.sh`와 Windows Command Prompt `.bat` 커맨드를 함께 표기
- 자동화 테스트, CreateAIFlow 스모크 테스트, Windows 래퍼 정적 스모크 테스트 명령 추가
- Windows Command Prompt 기본 `UE_ROOT` 경로 추가

---

## Phase 7 (마스터 플랜) 최종 인수 조건

```bash
# 1. 플러그인 빌드
./tools/ue/build_plugin.sh

# 2. 전체 자동화 테스트
./tools/test/automation/run.sh

# 3. 워크플로우 엔드-투-엔드
./tools/ue/create_ai_flow.sh specs/examples/guard_ai.json

# 4. 마스터 플랜 인수 조건 (두 조건 모두 true여야 함)
jq -e '.ok and .validation.actor_placed and .steps | length == 5' \
  "$(ls -t Saved/CodexReports/CreateAIFlow_*.json | head -1)"
```

Phase 7 완료 조건 = 마스터 플랜 완료 조건:
- `ok: true`
- `steps` 배열 5개 (각 단계 성공)
- `validation.actor_placed`가 truthy
- 모든 자동화 테스트 PASS

최종 검증 결과 (2026-05-28):
- `./tools/ue/build_plugin.sh` 통과
- `./tools/test/automation/run.sh` PASS 15 / FAIL 0 / SKIP 0
- `./tools/ue/create_ai_flow.sh specs/examples/guard_ai.json` 통과
- `sample/Saved/CodexReports/CreateAIFlow_20260528T003339Z.json` 인수 조건 통과
  - `ok: true`
  - `rollback_available: false`
  - `steps` 길이 5
  - `created_assets` 3개
  - `modified_assets` 1개
  - `validation.actor_placed: ok`
