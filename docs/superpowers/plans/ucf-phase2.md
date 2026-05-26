# UECommandForge Phase 2 — Spec JSON 파서 구현 계획

> **에이전트 실행자 필독:** 이 계획을 실행하기 전에 반드시 Phase 1 (`2026-05-25-uecommandforge-master.md`) 이 완료되어야 합니다.
> REQUIRED SUB-SKILL: `superpowers:subagent-driven-development` (권장) 또는 `superpowers:executing-plans`

**목표:** Spec JSON 문자열을 UE `USTRUCT`로 변환하고, 유효성 오류는 기존 Result JSON 계약을 통해 반환한다.

**아키텍처:** `FJsonObjectConverter::JsonObjectStringToUStruct` 기반 파서. Spec 구조체는 Runtime 모듈이 아닌 Editor 모듈에 둔다(파싱은 에디터 전용 작업). 유효성 검사는 파서와 분리된 `Validator` 클래스로 구현.

**기술 스택:** UE 5.7 C++20, `Json`, `JsonUtilities` (이미 Editor Build.cs에 추가됨).

---

## 파일 구조

| 경로 | 책임 |
|---|---|
| `Plugins/.../Public/Specs/AIFlowSpec.h` | AI 플로우 전체 Spec USTRUCT |
| `Plugins/.../Public/Specs/BlueprintSpec.h` | Blueprint 생성 Spec |
| `Plugins/.../Public/Specs/StateTreeSpec.h` | StateTree 생성 Spec |
| `Plugins/.../Public/Specs/PlacementSpec.h` | 맵 배치 Spec |
| `Plugins/.../Private/Specs/AIFlowSpecParser.cpp` | JSON → USTRUCT 변환 |
| `Plugins/.../Private/Specs/AIFlowSpecValidator.cpp` | 필수 필드·경로 접두사·enum 화이트리스트 검사 |
| `Plugins/.../Tests/AIFlowSpecParserTest.cpp` | 정상 JSON pass + 오류 JSON rejection 자동화 테스트 |
| `specs/examples/guard_ai.json` | 테스트용 황금 Spec 예제 |

---

## Task 1: Spec USTRUCT 정의

- [ ] **Step 1: `BlueprintSpec.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "BlueprintSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGE_API FBlueprintSpec
{
    GENERATED_BODY()
    UPROPERTY() FString AssetPath;      // e.g. "/Game/AI/BP_Guard"
    UPROPERTY() FString ParentClass;    // e.g. "Character"
    UPROPERTY() FString DisplayName;
};
```

- [ ] **Step 2: `StateTreeSpec.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "StateTreeSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGE_API FStateTreeTaskSpec
{
    GENERATED_BODY()
    UPROPERTY() FString TaskType;   // "Wait" | "MoveTo" | "PlayMontage"
    UPROPERTY() FString StateName;
};

USTRUCT()
struct UECOMMANDFORGE_API FStateTreeSpec
{
    GENERATED_BODY()
    UPROPERTY() FString AssetPath;
    UPROPERTY() TArray<FStateTreeTaskSpec> Tasks;
};
```

- [ ] **Step 3: `PlacementSpec.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "PlacementSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGE_API FPlacementSpec
{
    GENERATED_BODY()
    UPROPERTY() FString MapPath;        // "/Game/Maps/TestMap"
    UPROPERTY() FString BlueprintPath;
    UPROPERTY() FVector Location  = FVector::ZeroVector;
    UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
};
```

- [ ] **Step 4: `AIFlowSpec.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/BlueprintSpec.h"
#include "Specs/StateTreeSpec.h"
#include "Specs/PlacementSpec.h"
#include "AIFlowSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGE_API FAIFlowSpec
{
    GENERATED_BODY()
    UPROPERTY() FString Version = TEXT("1");
    UPROPERTY() FBlueprintSpec Character;
    UPROPERTY() FBlueprintSpec AIController;
    UPROPERTY() FStateTreeSpec StateTree;
    UPROPERTY() FPlacementSpec Placement;
};
```

- [ ] **Step 5: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Specs/
git commit -m "feat: AIFlowSpec USTRUCT 정의 (Blueprint/StateTree/Placement)"
```

---

## Task 2: 예제 Spec 파일 작성 (TDD 입력 데이터)

- [ ] **Step 1: `specs/examples/guard_ai.json` 작성**

```json
{
  "Version": "1",
  "Character": {
    "AssetPath": "/Game/AI/Characters/BP_Guard",
    "ParentClass": "Character",
    "DisplayName": "GuardCharacter"
  },
  "AIController": {
    "AssetPath": "/Game/AI/Controllers/BP_GuardController",
    "ParentClass": "AIController",
    "DisplayName": "GuardController"
  },
  "StateTree": {
    "AssetPath": "/Game/AI/StateTrees/ST_Guard",
    "Tasks": [
      { "TaskType": "Wait",   "StateName": "Idle" },
      { "TaskType": "MoveTo", "StateName": "Patrol" }
    ]
  },
  "Placement": {
    "MapPath":       "/Game/Maps/TestMap",
    "BlueprintPath": "/Game/AI/Characters/BP_Guard",
    "Location":  { "X": 0.0, "Y": 0.0, "Z": 0.0 },
    "Rotation":  { "Pitch": 0.0, "Yaw": 0.0, "Roll": 0.0 }
  }
}
```

- [ ] **Step 2: `specs/examples/guard_ai_broken.json` 작성 (의도적 오류 JSON)**

```json
{
  "Version": "1",
  "Character": {
    "AssetPath": "not_starting_with_game",
    "ParentClass": "",
    "DisplayName": ""
  }
}
```

- [ ] **Step 3: 커밋**

```bash
git add specs/
git commit -m "feat: Spec 예제 JSON 추가 (정상 guard_ai + 오류 broken)"
```

---

## Task 3: AIFlowSpecParser 구현 (TDD)

- [ ] **Step 1: 실패하는 자동화 테스트 작성**

```cpp
// Tests/AIFlowSpecParserTest.cpp
#include "Misc/AutomationTest.h"
#include "Specs/AIFlowSpecParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowSpecParserTest,
    "UECommandForge.Specs.AIFlowSpecParser.ParsesGuardAiSpec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowSpecParserTest::RunTest(const FString& Parameters)
{
    const FString Json = TEXT(R"({
        "Version": "1",
        "Character": { "AssetPath": "/Game/AI/BP_Guard", "ParentClass": "Character", "DisplayName": "Guard" },
        "AIController": { "AssetPath": "/Game/AI/BP_Ctrl", "ParentClass": "AIController", "DisplayName": "Ctrl" },
        "StateTree": { "AssetPath": "/Game/AI/ST_Guard", "Tasks": [] },
        "Placement": { "MapPath": "/Game/Maps/Test", "BlueprintPath": "/Game/AI/BP_Guard" }
    })");

    FAIFlowSpec Spec;
    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FAIFlowSpecParser::Parse(Json, Spec, Errors);

    TestTrue(TEXT("parse succeeded"), bOk);
    TestEqual(TEXT("character path"), Spec.Character.AssetPath, TEXT("/Game/AI/BP_Guard"));
    TestEqual(TEXT("statetree path"), Spec.StateTree.AssetPath, TEXT("/Game/AI/ST_Guard"));
    return true;
}
```

- [ ] **Step 2: 테스트 실행 — 실패 확인**

```bash
./tools/test/run_automation_tests.sh
```

예상: `FAIFlowSpecParserTest` FAIL — `FAIFlowSpecParser` 미정의.

- [ ] **Step 3: `AIFlowSpecParser.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FAIFlowSpecParser
    {
    public:
        static bool Parse(const FString& Json, FAIFlowSpec& OutSpec,
                          TArray<FCommandForgeError>& OutErrors);
    };
}
```

- [ ] **Step 4: `AIFlowSpecParser.cpp` 작성**

```cpp
#include "Specs/AIFlowSpecParser.h"
#include "JsonObjectConverter.h"

namespace UECommandForge
{
    bool FAIFlowSpecParser::Parse(const FString& Json, FAIFlowSpec& OutSpec,
                                   TArray<FCommandForgeError>& OutErrors)
    {
        if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutSpec, 0, 0))
        {
            FCommandForgeError Err;
            Err.Code    = TEXT("PARSE_FAILED");
            Err.Message = TEXT("JSON을 FAIFlowSpec으로 변환하지 못했습니다.");
            OutErrors.Add(Err);
            return false;
        }
        return true;
    }
}
```

- [ ] **Step 5: 테스트 재실행 — 통과 확인**

```bash
./tools/test/run_automation_tests.sh
```

예상: `FAIFlowSpecParserTest` PASS.

- [ ] **Step 6: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Specs/AIFlowSpecParser.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Specs/AIFlowSpecParser.cpp \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/AIFlowSpecParserTest.cpp
git commit -m "feat: AIFlowSpecParser — JSON→USTRUCT 변환 + 자동화 테스트"
```

---

## Task 4: AIFlowSpecValidator 구현 (TDD)

검사 규칙:
- 필수 필드: `Character.AssetPath`, `Character.ParentClass`, `AIController.AssetPath`, `StateTree.AssetPath`, `Placement.MapPath`, `Placement.BlueprintPath`
- 경로 접두사: `/Game/` 로 시작해야 함
- `StateTree.Tasks[].TaskType` 화이트리스트: `Wait`, `MoveTo`, `PlayMontage`

- [ ] **Step 1: 실패하는 검사 테스트 작성**

```cpp
// Tests/AIFlowSpecValidatorTest.cpp
#include "Misc/AutomationTest.h"
#include "Specs/AIFlowSpecValidator.h"
#include "Specs/AIFlowSpec.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowSpecValidatorTest,
    "UECommandForge.Specs.AIFlowSpecValidator.RejectsBrokenSpec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowSpecValidatorTest::RunTest(const FString& Parameters)
{
    FAIFlowSpec Spec;
    Spec.Character.AssetPath  = TEXT("not_game_path");
    Spec.Character.ParentClass = TEXT("");

    TArray<FCommandForgeError> Errors;
    const bool bValid = UECommandForge::FAIFlowSpecValidator::Validate(Spec, Errors);

    TestFalse(TEXT("validation should fail"), bValid);
    TestTrue(TEXT("at least one error"), Errors.Num() > 0);
    return true;
}
```

- [ ] **Step 2: 테스트 실행 — 실패 확인**

```bash
./tools/test/run_automation_tests.sh
```

예상: `FAIFlowSpecValidatorTest` FAIL — `FAIFlowSpecValidator` 미정의.

- [ ] **Step 3: `AIFlowSpecValidator.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FAIFlowSpecValidator
    {
    public:
        static bool Validate(const FAIFlowSpec& Spec, TArray<FCommandForgeError>& OutErrors);

    private:
        static void CheckRequiredPath(const FString& Value, const FString& Field,
                                      TArray<FCommandForgeError>& OutErrors);
        static void CheckGamePrefix(const FString& Value, const FString& Field,
                                    TArray<FCommandForgeError>& OutErrors);
    };
}
```

- [ ] **Step 4: `AIFlowSpecValidator.cpp` 작성**

```cpp
#include "Specs/AIFlowSpecValidator.h"

namespace UECommandForge
{
    static const TSet<FString> AllowedTaskTypes = { TEXT("Wait"), TEXT("MoveTo"), TEXT("PlayMontage") };

    void FAIFlowSpecValidator::CheckRequiredPath(const FString& Value, const FString& Field,
                                                  TArray<FCommandForgeError>& OutErrors)
    {
        if (Value.IsEmpty())
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), FString::Printf(TEXT("'%s' 필드는 필수입니다."), *Field), Field });
        }
    }

    void FAIFlowSpecValidator::CheckGamePrefix(const FString& Value, const FString& Field,
                                                TArray<FCommandForgeError>& OutErrors)
    {
        if (!Value.IsEmpty() && !Value.StartsWith(TEXT("/Game/")))
        {
            OutErrors.Add({ TEXT("INVALID_PATH"), FString::Printf(TEXT("'%s'는 /Game/ 으로 시작해야 합니다."), *Field), Field });
        }
    }

    bool FAIFlowSpecValidator::Validate(const FAIFlowSpec& Spec, TArray<FCommandForgeError>& OutErrors)
    {
        CheckRequiredPath(Spec.Character.AssetPath,    TEXT("Character.AssetPath"),    OutErrors);
        CheckRequiredPath(Spec.Character.ParentClass,  TEXT("Character.ParentClass"),  OutErrors);
        CheckRequiredPath(Spec.AIController.AssetPath, TEXT("AIController.AssetPath"), OutErrors);
        CheckRequiredPath(Spec.StateTree.AssetPath,    TEXT("StateTree.AssetPath"),    OutErrors);
        CheckRequiredPath(Spec.Placement.MapPath,      TEXT("Placement.MapPath"),      OutErrors);
        CheckRequiredPath(Spec.Placement.BlueprintPath,TEXT("Placement.BlueprintPath"),OutErrors);

        CheckGamePrefix(Spec.Character.AssetPath,    TEXT("Character.AssetPath"),    OutErrors);
        CheckGamePrefix(Spec.AIController.AssetPath, TEXT("AIController.AssetPath"), OutErrors);
        CheckGamePrefix(Spec.StateTree.AssetPath,    TEXT("StateTree.AssetPath"),    OutErrors);
        CheckGamePrefix(Spec.Placement.MapPath,      TEXT("Placement.MapPath"),      OutErrors);
        CheckGamePrefix(Spec.Placement.BlueprintPath,TEXT("Placement.BlueprintPath"),OutErrors);

        for (const FStateTreeTaskSpec& Task : Spec.StateTree.Tasks)
        {
            if (!AllowedTaskTypes.Contains(Task.TaskType))
            {
                OutErrors.Add({ TEXT("INVALID_TASK_TYPE"),
                    FString::Printf(TEXT("허용되지 않는 TaskType: '%s'. 허용 목록: Wait, MoveTo, PlayMontage"), *Task.TaskType),
                    TEXT("StateTree.Tasks[].TaskType") });
            }
        }
        return OutErrors.IsEmpty();
    }
}
```

- [ ] **Step 5: 테스트 재실행 — 통과 확인**

```bash
./tools/test/run_automation_tests.sh
```

예상: `FAIFlowSpecValidatorTest` PASS.

- [ ] **Step 6: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Specs/AIFlowSpecValidator.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Specs/AIFlowSpecValidator.cpp \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/AIFlowSpecValidatorTest.cpp
git commit -m "feat: AIFlowSpecValidator — 필수 필드·경로 접두사·TaskType 화이트리스트 검사"
```

---

## Phase 2 인수 조건

```bash
# 1. 빌드
./tools/ue/build_plugin.sh

# 2. 자동화 테스트 (Parser + Validator 모두 통과)
./tools/test/run_automation_tests.sh

# 3. guard_ai.json 파서 통합 확인
./tools/ue/run_commandlet.sh ParseSpecCheck -SpecFile="${PWD}/specs/examples/guard_ai.json"
jq -e '.ok == true' "$(ls -t Saved/CodexReports/ParseSpecCheck_*.json | head -1)"

# 4. broken JSON 거부 확인
./tools/ue/run_commandlet.sh ParseSpecCheck -SpecFile="${PWD}/specs/examples/guard_ai_broken.json"
jq -e '.ok == false and (.errors | length) > 0' "$(ls -t Saved/CodexReports/ParseSpecCheck_*.json | head -1)"
```

Phase 2 완료 조건: `guard_ai.json` 파싱 성공 + `guard_ai_broken.json` 거부 + 구조화 오류 Result JSON 반환.
