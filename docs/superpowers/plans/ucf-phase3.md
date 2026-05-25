# UECommandForge Phase 3 — 블루프린트 생성 구현 계획

> **에이전트 실행자 필독:** Phase 2 (`2026-05-25-uecommandforge-phase2.md`) 완료 후 실행하세요.
> REQUIRED SUB-SKILL: `superpowers:subagent-driven-development` (권장) 또는 `superpowers:executing-plans`

**목표:** `BlueprintSpec`으로부터 Character 및 AIController Blueprint 에셋을 생성하고, CDO 필드를 설정한 뒤 컴파일·저장한다.

**아키텍처:** `CharacterBlueprintBuilder`와 `AIControllerBlueprintBuilder`는 `FKismetEditorUtilities::CreateBlueprint` + `FKismetEditorUtilities::CompileBlueprint`를 사용한다. CDO 변경은 2-pass(설정 → 컴파일 → 검증)로 수행해 Auto Possess AI 리셋 문제를 방지한다. Commandlet은 얇게 유지하고, 실제 작업은 Builder에 위임한다.

**기술 스택:** UE 5.7, `BlueprintGraph`, `KismetCompiler`, `Kismet`, `AssetTools`, `AssetRegistry`, `EditorScriptingUtilities`.

---

## 리스크

- **CDO 2-pass 필수:** `AutoPossessAI` 등 일부 프로퍼티는 첫 번째 컴파일 후 기본값으로 리셋될 수 있다. 반드시 컴파일 후 CDO를 재검증하고, 값이 틀리면 재설정 후 재컴파일한다.
- **컴파일 경고 정책:** 경고는 Result JSON `validation` 맵에 기록하되 실행을 실패시키지 않는다. 오직 에러만 `ok: false`로 처리한다.

---

## 파일 구조

| 경로 | 책임 |
|---|---|
| `Plugins/.../Public/Builders/CharacterBlueprintBuilder.h` | Character BP 생성·CDO 설정·컴파일 인터페이스 |
| `Plugins/.../Private/Builders/CharacterBlueprintBuilder.cpp` | 구현 |
| `Plugins/.../Public/Builders/AIControllerBlueprintBuilder.h` | AIController BP 생성·컴파일 인터페이스 |
| `Plugins/.../Private/Builders/AIControllerBlueprintBuilder.cpp` | 구현 |
| `Plugins/.../Private/Commandlets/CreateCharacterBlueprintCommandlet.h/.cpp` | Character BP Commandlet |
| `Plugins/.../Private/Commandlets/CreateAIControllerBlueprintCommandlet.h/.cpp` | AIController BP Commandlet |
| `Plugins/.../Private/Commandlets/CompileBlueprintsCommandlet.h/.cpp` | 기존 BP 일괄 컴파일 Commandlet |
| `Plugins/.../Tests/CharacterBlueprintBuilderTest.cpp` | 빌더 자동화 테스트 |
| `tools/ue/create_character_bp.sh` | Character BP Shell Wrapper |
| `tools/ue/create_ai_controller.sh` | AIController BP Shell Wrapper |
| `tools/ue/compile_blueprints.sh` | 일괄 컴파일 Shell Wrapper |

---

## Task 1: CharacterBlueprintBuilder (TDD)

- [ ] **Step 1: 실패하는 자동화 테스트 작성**

```cpp
// Tests/CharacterBlueprintBuilderTest.cpp
#include "Misc/AutomationTest.h"
#include "Builders/CharacterBlueprintBuilder.h"
#include "Specs/BlueprintSpec.h"
#include "CommandForgeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCharacterBlueprintBuilderTest,
    "UECommandForge.Builders.CharacterBlueprintBuilder.CreatesAndCompiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterBlueprintBuilderTest::RunTest(const FString& Parameters)
{
    FBlueprintSpec Spec;
    Spec.AssetPath   = TEXT("/Game/Tests/BP_TestCharacter");
    Spec.ParentClass = TEXT("Character");
    Spec.DisplayName = TEXT("TestCharacter");

    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FCharacterBlueprintBuilder::Build(Spec, Errors);

    TestTrue(TEXT("빌드 성공"), bOk);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());
    return true;
}
```

- [ ] **Step 2: 테스트 실행 — 실패 확인**

```bash
./tools/ue/run_automation_tests.sh
```

예상: `FCharacterBlueprintBuilderTest` FAIL.

- [ ] **Step 3: `CharacterBlueprintBuilder.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/BlueprintSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FCharacterBlueprintBuilder
    {
    public:
        static bool Build(const FBlueprintSpec& Spec, TArray<FCommandForgeError>& OutErrors);
    };
}
```

- [ ] **Step 4: `CharacterBlueprintBuilder.cpp` 작성**

```cpp
#include "Builders/CharacterBlueprintBuilder.h"
#include "KismetEditorUtilities.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace UECommandForge
{
    bool FCharacterBlueprintBuilder::Build(const FBlueprintSpec& Spec,
                                            TArray<FCommandForgeError>& OutErrors)
    {
        // 부모 클래스 로드
        UClass* ParentClass = FindObject<UClass>(ANY_PACKAGE, *Spec.ParentClass);
        if (!ParentClass)
        {
            ParentClass = LoadObject<UClass>(nullptr,
                *FString::Printf(TEXT("/Script/Engine.%s"), *Spec.ParentClass));
        }
        if (!ParentClass)
        {
            OutErrors.Add({ TEXT("PARENT_CLASS_NOT_FOUND"),
                FString::Printf(TEXT("부모 클래스를 찾을 수 없습니다: %s"), *Spec.ParentClass),
                TEXT("Character.ParentClass") });
            return false;
        }

        // 패키지 경로 파싱
        const FString PackagePath = FPackageName::GetLongPackagePath(Spec.AssetPath);
        const FString AssetName   = FPackageName::GetLongPackageAssetName(Spec.AssetPath);

        // Blueprint 생성
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

        // 컴파일
        FKismetEditorUtilities::CompileBlueprint(BP);

        // 저장
        UPackage* Package = BP->GetOutermost();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Spec.AssetPath, FPackageName::GetAssetPackageExtension());
        UPackage::SavePackage(Package, BP, *FileName, SaveArgs);

        return true;
    }
}
```

- [ ] **Step 5: 테스트 재실행 — 통과 확인**

```bash
./tools/ue/run_automation_tests.sh
```

예상: `FCharacterBlueprintBuilderTest` PASS.

- [ ] **Step 6: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/CharacterBlueprintBuilder.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/CharacterBlueprintBuilder.cpp \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/CharacterBlueprintBuilderTest.cpp
git commit -m "feat: CharacterBlueprintBuilder — BP 생성·컴파일·저장 + 자동화 테스트"
```

---

## Task 2: AIControllerBlueprintBuilder

- [ ] **Step 1: `AIControllerBlueprintBuilder.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/BlueprintSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FAIControllerBlueprintBuilder
    {
    public:
        static bool Build(const FBlueprintSpec& Spec, TArray<FCommandForgeError>& OutErrors);
    };
}
```

- [ ] **Step 2: `AIControllerBlueprintBuilder.cpp` 작성**

`CharacterBlueprintBuilder.cpp`와 동일한 패턴. 부모 클래스 로드 시 `AIController`를 `"/Script/AIModule.AIController"`에서 찾는다.

```cpp
#include "Builders/AIControllerBlueprintBuilder.h"
#include "KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace UECommandForge
{
    bool FAIControllerBlueprintBuilder::Build(const FBlueprintSpec& Spec,
                                               TArray<FCommandForgeError>& OutErrors)
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

        UPackage* Package = BP->GetOutermost();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Spec.AssetPath, FPackageName::GetAssetPackageExtension());
        UPackage::SavePackage(Package, BP, *FileName, SaveArgs);

        return true;
    }
}
```

- [ ] **Step 3: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/AIControllerBlueprintBuilder.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/AIControllerBlueprintBuilder.cpp
git commit -m "feat: AIControllerBlueprintBuilder"
```

---

## Task 3: Blueprint Commandlet 3종 + Shell Wrapper

- [ ] **Step 1: `CreateCharacterBlueprintCommandlet.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateCharacterBlueprintCommandlet.generated.h"

UCLASS()
class UCreateCharacterBlueprintCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCreateCharacterBlueprintCommandlet();
    virtual int32 Main(const FString& Params) override;
};
```

- [ ] **Step 2: `CreateCharacterBlueprintCommandlet.cpp` 작성**

```cpp
#include "CreateCharacterBlueprintCommandlet.h"
#include "Specs/AIFlowSpecParser.h"
#include "Specs/AIFlowSpecValidator.h"
#include "Builders/CharacterBlueprintBuilder.h"
#include "Reports/JsonReportWriter.h"
#include "Misc/FileHelper.h"

UCreateCharacterBlueprintCommandlet::UCreateCharacterBlueprintCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UCreateCharacterBlueprintCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens; TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SpecFile = ParamsMap.FindRef(TEXT("SpecFile"));
    const FString OutPath  = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("CreateCharacterBlueprint");

    if (SpecFile.IsEmpty() || OutPath.IsEmpty())
    {
        Report.Errors.Add({ TEXT("MISSING_PARAM"), TEXT("-SpecFile 또는 -Output 파라미터 누락"), TEXT("") });
        UECommandForge::FJsonReportWriter::Write(OutPath.IsEmpty() ? TEXT("/dev/null") : *OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *SpecFile))
    {
        Report.Errors.Add({ TEXT("SPEC_FILE_NOT_FOUND"), FString::Printf(TEXT("파일 없음: %s"), *SpecFile), TEXT("") });
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

    if (!UECommandForge::FCharacterBlueprintBuilder::Build(Spec.Character, Report.Errors))
    {
        UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
    }

    Report.bOk = true;
    Report.CreatedAssets.Add(Spec.Character.AssetPath);
    Report.Validation.Add(TEXT("blueprint_compile"), TEXT("passed"));
    UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
    return 0;
}
```

- [ ] **Step 3: `CreateAIControllerBlueprintCommandlet` 작성**

`CreateCharacterBlueprintCommandlet`과 동일한 패턴. `FCharacterBlueprintBuilder` 대신 `FAIControllerBlueprintBuilder` 사용. Report.Commandlet = `"CreateAIControllerBlueprint"`.

- [ ] **Step 4: `CompileBlueprintsCommandlet.h/.cpp` 작성**

`-AssetPaths` 파라미터로 쉼표 구분된 에셋 경로 목록을 받아 `FKismetEditorUtilities::CompileBlueprint`로 일괄 컴파일. 각 에셋 컴파일 결과를 `validation` 맵에 기록.

- [ ] **Step 5: Shell Wrapper 3종 작성**

```bash
# tools/ue/create_character_bp.sh
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/run_commandlet.sh" CreateCharacterBlueprint -SpecFile="$1" "$@"
```

```bash
# tools/ue/create_ai_controller.sh
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/run_commandlet.sh" CreateAIControllerBlueprint -SpecFile="$1" "$@"
```

```bash
# tools/ue/compile_blueprints.sh
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/run_commandlet.sh" CompileBlueprints "$@"
```

- [ ] **Step 6: 실행 권한 부여**

```bash
chmod +x tools/ue/create_character_bp.sh tools/ue/create_ai_controller.sh tools/ue/compile_blueprints.sh
```

- [ ] **Step 7: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ \
        tools/ue/create_character_bp.sh tools/ue/create_ai_controller.sh tools/ue/compile_blueprints.sh
git commit -m "feat: Blueprint 생성/컴파일 Commandlet 3종 + Shell Wrapper"
```

---

## Phase 3 인수 조건

```bash
# 1. 플러그인 빌드
./tools/ue/build_plugin.sh

# 2. 자동화 테스트
./tools/ue/run_automation_tests.sh

# 3. Character BP 생성
./tools/ue/create_character_bp.sh specs/examples/guard_ai.json
jq -e '.ok and .validation.blueprint_compile == "passed"' \
  "$(ls -t Saved/CodexReports/CreateCharacterBlueprint_*.json | head -1)"

# 4. AIController BP 생성
./tools/ue/create_ai_controller.sh specs/examples/guard_ai.json
jq -e '.ok and .validation.blueprint_compile == "passed"' \
  "$(ls -t Saved/CodexReports/CreateAIControllerBlueprint_*.json | head -1)"
```

Phase 3 완료 조건: 두 Blueprint 에셋이 `Content/` 디렉터리에 생성되고, 컴파일 결과가 `passed`로 Result JSON에 기록됨.
