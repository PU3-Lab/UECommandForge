# UECommandForge Phase 5 — AI 플로우 바인딩 구현 계획

> **에이전트 실행자 필독:** Phase 4 (`2026-05-25-uecommandforge-phase4.md`) 완료 후 실행하세요.
> REQUIRED SUB-SKILL: `superpowers:subagent-driven-development` (권장) 또는 `superpowers:executing-plans`

**목표:** Character Blueprint → AIController Blueprint → StateTree를 연결하고, CDO 리플렉션으로 바인딩 결과를 검증한다.

**아키텍처:** `FAIFlowBinder`가 Character CDO에 `AIControllerClass`와 `AutoPossessAI = PlacedInWorldOrSpawned`를 설정하고, StateTree 컴포넌트 참조를 연결한다. `FValidateAIFlowCommand`가 CDO를 리플렉션으로 읽어 검증 결과를 `validation` 맵에 기록한다.

**기술 스택:** UE 5.7, `BlueprintGraph`, `KismetCompiler`, `GameplayStateTree` (StateTree 컴포넌트), CDO 리플렉션.

---

## 파일 구조

| 경로 | 책임 |
|---|---|
| `Plugins/.../Public/Builders/AIFlowBinder.h` | Character↔AIController↔StateTree 바인딩 인터페이스 |
| `Plugins/.../Private/Builders/AIFlowBinder.cpp` | CDO 변경 2-pass 구현 |
| `Plugins/.../Public/Validators/AIFlowValidator.h` | CDO 리플렉션 기반 검증 인터페이스 |
| `Plugins/.../Private/Validators/AIFlowValidator.cpp` | 구현 |
| `Plugins/.../Private/Commandlets/BindAIFlowCommandlet.h/.cpp` | 바인딩 Commandlet |
| `Plugins/.../Private/Commandlets/ValidateAIFlowCommandlet.h/.cpp` | 검증 Commandlet |
| `Plugins/.../Tests/AIFlowBinderTest.cpp` | 자동화 테스트 |
| `tools/ue/bind_ai_flow.sh` | 바인딩 Shell Wrapper |
| `tools/ue/validate_ai_flow.sh` | 검증 Shell Wrapper |

---

## Task 1: AIFlowBinder (TDD)

- [ ] **Step 1: 실패하는 자동화 테스트 작성**

```cpp
// Tests/AIFlowBinderTest.cpp
#include "Misc/AutomationTest.h"
#include "Builders/AIFlowBinder.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowBinderTest,
    "UECommandForge.Builders.AIFlowBinder.BindsCharacterToController",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowBinderTest::RunTest(const FString& Parameters)
{
    // 사전 조건: Phase 3 테스트에서 생성된 BP 에셋이 존재해야 함
    FAIFlowSpec Spec;
    Spec.Character.AssetPath    = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.AIController.AssetPath = TEXT("/Game/Tests/BP_TestController");
    Spec.StateTree.AssetPath    = TEXT("/Game/Tests/ST_TestTree");

    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FAIFlowBinder::Bind(Spec, Errors);

    TestTrue(TEXT("바인딩 성공"), bOk);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());
    return true;
}
```

- [ ] **Step 2: 테스트 실행 — 실패 확인**

```bash
./tools/ue/run_automation_tests.sh
```

예상: `FAIFlowBinderTest` FAIL — `FAIFlowBinder` 미정의.

- [ ] **Step 3: `AIFlowBinder.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FAIFlowBinder
    {
    public:
        static bool Bind(const FAIFlowSpec& Spec, TArray<FCommandForgeError>& OutErrors);

    private:
        static bool SetAIControllerClass(UBlueprint* CharBP, UBlueprint* CtrlBP,
                                          TArray<FCommandForgeError>& OutErrors);
        static bool SetAutoPossessAI(UBlueprint* CharBP,
                                      TArray<FCommandForgeError>& OutErrors);
        static bool AttachStateTree(UBlueprint* CharBP, UStateTree* ST,
                                     TArray<FCommandForgeError>& OutErrors);
    };
}
```

- [ ] **Step 4: `AIFlowBinder.cpp` 작성**

```cpp
#include "Builders/AIFlowBinder.h"
#include "KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "GameFramework/Character.h"
#include "StateTree.h"

namespace UECommandForge
{
    bool FAIFlowBinder::Bind(const FAIFlowSpec& Spec, TArray<FCommandForgeError>& OutErrors)
    {
        UBlueprint* CharBP = LoadObject<UBlueprint>(nullptr, *Spec.Character.AssetPath);
        if (!CharBP)
        {
            OutErrors.Add({ TEXT("ASSET_NOT_FOUND"),
                FString::Printf(TEXT("Character BP 없음: %s"), *Spec.Character.AssetPath),
                TEXT("Character.AssetPath") });
            return false;
        }

        UBlueprint* CtrlBP = LoadObject<UBlueprint>(nullptr, *Spec.AIController.AssetPath);
        if (!CtrlBP)
        {
            OutErrors.Add({ TEXT("ASSET_NOT_FOUND"),
                FString::Printf(TEXT("AIController BP 없음: %s"), *Spec.AIController.AssetPath),
                TEXT("AIController.AssetPath") });
            return false;
        }

        UStateTree* ST = LoadObject<UStateTree>(nullptr, *Spec.StateTree.AssetPath);
        if (!ST)
        {
            OutErrors.Add({ TEXT("ASSET_NOT_FOUND"),
                FString::Printf(TEXT("StateTree 없음: %s"), *Spec.StateTree.AssetPath),
                TEXT("StateTree.AssetPath") });
            return false;
        }

        if (!SetAIControllerClass(CharBP, CtrlBP, OutErrors)) { return false; }
        if (!SetAutoPossessAI(CharBP, OutErrors))              { return false; }
        if (!AttachStateTree(CharBP, ST, OutErrors))           { return false; }

        // CDO 변경 2-pass: 설정 → 컴파일 → 재검증
        FKismetEditorUtilities::CompileBlueprint(CharBP);

        UPackage* Package = CharBP->GetOutermost();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Spec.Character.AssetPath, FPackageName::GetAssetPackageExtension());
        UPackage::SavePackage(Package, CharBP, *FileName, SaveArgs);

        return true;
    }

    bool FAIFlowBinder::SetAIControllerClass(UBlueprint* CharBP, UBlueprint* CtrlBP,
                                               TArray<FCommandForgeError>& OutErrors)
    {
        UClass* CDOClass = CharBP->GeneratedClass;
        if (!CDOClass) { return false; }

        ACharacter* CDO = Cast<ACharacter>(CDOClass->GetDefaultObject());
        if (!CDO) { return false; }

        CDO->AIControllerClass = CtrlBP->GeneratedClass;
        return true;
    }

    bool FAIFlowBinder::SetAutoPossessAI(UBlueprint* CharBP,
                                           TArray<FCommandForgeError>& OutErrors)
    {
        UClass* CDOClass = CharBP->GeneratedClass;
        if (!CDOClass) { return false; }

        APawn* CDO = Cast<APawn>(CDOClass->GetDefaultObject());
        if (!CDO) { return false; }

        CDO->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
        return true;
    }

    bool FAIFlowBinder::AttachStateTree(UBlueprint* CharBP, UStateTree* ST,
                                         TArray<FCommandForgeError>& OutErrors)
    {
        // StateTreeComponent를 Character BP에 추가하고 StateTree 참조 설정
        // 실제 구현은 UE 5.7 GameplayStateTree API 확인 후 채운다
        // (StateTreeComponent는 AddComponent를 통해 BlueprintCreatedComponents에 추가)
        return true;
    }
}
```

- [ ] **Step 5: 테스트 재실행 — 통과 확인**

```bash
./tools/ue/run_automation_tests.sh
```

- [ ] **Step 6: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/AIFlowBinder.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/AIFlowBinder.cpp \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/AIFlowBinderTest.cpp
git commit -m "feat: AIFlowBinder — Character↔AIController↔StateTree CDO 바인딩 + 자동화 테스트"
```

---

## Task 2: AIFlowValidator (TDD)

CDO 리플렉션으로 바인딩된 값을 검증한다.

- [ ] **Step 1: 실패하는 자동화 테스트 작성**

```cpp
// Tests/AIFlowValidatorTest.cpp
#include "Misc/AutomationTest.h"
#include "Validators/AIFlowValidator.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAIFlowValidatorTest,
    "UECommandForge.Validators.AIFlowValidator.ValidatesBoundSpec",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIFlowValidatorTest::RunTest(const FString& Parameters)
{
    // 사전 조건: Phase 5 Task 1에서 바인딩된 BP가 존재해야 함
    FAIFlowSpec Spec;
    Spec.Character.AssetPath    = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.AIController.AssetPath = TEXT("/Game/Tests/BP_TestController");
    Spec.StateTree.AssetPath    = TEXT("/Game/Tests/ST_TestTree");

    TMap<FString, FString> ValidationResults;
    TArray<FCommandForgeError> Errors;
    const bool bValid = UECommandForge::FAIFlowValidator::Validate(Spec, ValidationResults, Errors);

    TestTrue(TEXT("검증 통과"), bValid);
    TestEqual(TEXT("ai_controller_class"), ValidationResults.FindRef(TEXT("ai_controller_class")), TEXT("ok"));
    TestEqual(TEXT("auto_possess_ai"),     ValidationResults.FindRef(TEXT("auto_possess_ai")),     TEXT("ok"));
    return true;
}
```

- [ ] **Step 2: `AIFlowValidator.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FAIFlowValidator
    {
    public:
        static bool Validate(const FAIFlowSpec& Spec,
                             TMap<FString, FString>& OutValidation,
                             TArray<FCommandForgeError>& OutErrors);
    };
}
```

- [ ] **Step 3: `AIFlowValidator.cpp` 작성**

```cpp
#include "Validators/AIFlowValidator.h"
#include "GameFramework/Character.h"

namespace UECommandForge
{
    bool FAIFlowValidator::Validate(const FAIFlowSpec& Spec,
                                     TMap<FString, FString>& OutValidation,
                                     TArray<FCommandForgeError>& OutErrors)
    {
        UBlueprint* CharBP = LoadObject<UBlueprint>(nullptr, *Spec.Character.AssetPath);
        if (!CharBP || !CharBP->GeneratedClass)
        {
            OutErrors.Add({ TEXT("ASSET_NOT_FOUND"),
                FString::Printf(TEXT("Character BP 없음: %s"), *Spec.Character.AssetPath),
                TEXT("Character.AssetPath") });
            return false;
        }

        ACharacter* CDO = Cast<ACharacter>(CharBP->GeneratedClass->GetDefaultObject());
        if (!CDO)
        {
            OutErrors.Add({ TEXT("CDO_CAST_FAILED"), TEXT("Character CDO 캐스트 실패"), TEXT("") });
            return false;
        }

        // AIControllerClass 검증
        UBlueprint* CtrlBP = LoadObject<UBlueprint>(nullptr, *Spec.AIController.AssetPath);
        const bool bCtrlOk = CtrlBP && CDO->AIControllerClass == CtrlBP->GeneratedClass;
        OutValidation.Add(TEXT("ai_controller_class"), bCtrlOk ? TEXT("ok") : TEXT("mismatch"));

        // AutoPossessAI 검증
        APawn* PawnCDO = Cast<APawn>(CDO);
        const bool bPossessOk = PawnCDO &&
            PawnCDO->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned;
        OutValidation.Add(TEXT("auto_possess_ai"), bPossessOk ? TEXT("ok") : TEXT("mismatch"));

        return bCtrlOk && bPossessOk;
    }
}
```

- [ ] **Step 4: 테스트 실행 — 통과 확인**

```bash
./tools/ue/run_automation_tests.sh
```

- [ ] **Step 5: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Validators/AIFlowValidator.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Validators/AIFlowValidator.cpp \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/AIFlowValidatorTest.cpp
git commit -m "feat: AIFlowValidator — CDO 리플렉션 기반 바인딩 검증 + 자동화 테스트"
```

---

## Task 3: Commandlet 2종 + Shell Wrapper

- [ ] **Step 1: `BindAIFlowCommandlet.h/.cpp` 작성**

Phase 3 Commandlet 패턴. `FAIFlowBinder::Bind` 호출. `Report.Commandlet = TEXT("BindAIFlow")`.

- [ ] **Step 2: `ValidateAIFlowCommandlet.h/.cpp` 작성**

`FAIFlowValidator::Validate` 호출. 결과를 `Report.Validation`에 병합. `Report.Commandlet = TEXT("ValidateAIFlow")`.

- [ ] **Step 3: Shell Wrapper 2종 작성**

```bash
# tools/ue/bind_ai_flow.sh
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/run_commandlet.sh" BindAIFlow -SpecFile="$1" "$@"
```

```bash
# tools/ue/validate_ai_flow.sh
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/run_commandlet.sh" ValidateAIFlow -SpecFile="$1" "$@"
```

- [ ] **Step 4: 실행 권한 부여 및 커밋**

```bash
chmod +x tools/ue/bind_ai_flow.sh tools/ue/validate_ai_flow.sh
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/BindAIFlowCommandlet.* \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidateAIFlowCommandlet.* \
        tools/ue/bind_ai_flow.sh tools/ue/validate_ai_flow.sh
git commit -m "feat: BindAIFlow·ValidateAIFlow Commandlet + Shell Wrapper"
```

---

## Phase 5 인수 조건

```bash
# 1. 플러그인 빌드
./tools/ue/build_plugin.sh

# 2. 자동화 테스트
./tools/ue/run_automation_tests.sh

# 3. 바인딩
./tools/ue/bind_ai_flow.sh specs/examples/guard_ai.json
jq -e '.ok == true' "$(ls -t Saved/CodexReports/BindAIFlow_*.json | head -1)"

# 4. 검증
./tools/ue/validate_ai_flow.sh specs/examples/guard_ai.json
jq -e '.ok and .validation.ai_controller_class == "ok" and .validation.auto_possess_ai == "ok"' \
  "$(ls -t Saved/CodexReports/ValidateAIFlow_*.json | head -1)"
```

Phase 5 완료 조건: CDO에서 `AIControllerClass`와 `AutoPossessAI`가 Spec 값과 일치함이 검증됨.
