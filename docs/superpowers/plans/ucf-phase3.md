# UECommandForge Phase 3 — 블루프린트 생성 구현 계획

> **에이전트 실행자 필독:** Phase 2 (`2026-05-25-uecommandforge-phase2.md`) 완료 후 실행하세요.
> REQUIRED SUB-SKILL: `superpowers:subagent-driven-development` (권장) 또는 `superpowers:executing-plans`

**목표:** `BlueprintSpec`으로부터 Character 및 AIController Blueprint 에셋을 생성하고, 컴파일·저장·검증한다. 검증은 3단계(컴파일 상태, 디스크 저장, AssetRegistry 등록)로 수행하며, 각 결과를 Result JSON `validation` 맵에 기록한다. CDO 설정 및 노드 편집은 이번 Phase 범위에서 제외한다.

**아키텍처:** `CharacterBlueprintBuilder`와 `AIControllerBlueprintBuilder`는 `FKismetEditorUtilities::CreateBlueprint` + `FKismetEditorUtilities::CompileBlueprint`를 사용한다. Builder는 `OutValidation` 맵을 통해 3단계 검증 결과를 반환하고, Commandlet은 이를 `FCommandForgeReport.Validation`에 병합한다. Commandlet은 얇게 유지하고, 실제 작업은 Builder에 위임한다.

**기술 스택:** UE 5.7, `BlueprintGraph`, `KismetCompiler`, `Kismet`, `AssetTools`, `AssetRegistry`, `EditorScriptingUtilities`.

---

## 리스크

- **컴파일 상태 감지:** `CompileBlueprint` 후 반드시 `BP->Status == BS_UpToDate`를 확인한다. 이를 누락하면 컴파일 실패가 `ok: true`로 잘못 보고된다. 실패 시 즉시 `ok: false`로 처리하고 중단한다.
- **SavePackage 실패 감지:** `UPackage::SavePackage` 반환값(`ESavePackageResult`)만으로는 충분하지 않다. 반드시 `IFileManager::Get().FileExists(*FileName)`로 이중 확인한다.
- **AssetRegistry 지연:** `SavePackage` 직후 AssetRegistry에 즉시 반영되지 않을 수 있다. `IAssetRegistry::ScanModifiedAssetFiles({FileName})`를 호출해 강제 갱신 후 `GetAssetsByPackageName`으로 확인한다. Registry 미등록은 `ok: false`가 아닌 경고만 기록한다.
- **컴파일 경고 정책:** 경고는 Result JSON `validation` 맵에 기록하되 실행을 실패시키지 않는다. 오직 에러만 `ok: false`로 처리한다.

---

## 파일 구조

| 경로 | 책임 |
|---|---|
| `Plugins/.../Public/Builders/CharacterBlueprintBuilder.h` | Character BP 생성·컴파일·저장·3단계 검증 인터페이스 |
| `Plugins/.../Private/Builders/CharacterBlueprintBuilder.cpp` | 구현 |
| `Plugins/.../Public/Builders/AIControllerBlueprintBuilder.h` | AIController BP 생성·컴파일·저장·3단계 검증 인터페이스 |
| `Plugins/.../Private/Builders/AIControllerBlueprintBuilder.cpp` | 구현 |
| `Plugins/.../Private/Commandlets/CreateCharacterBlueprintCommandlet.h/.cpp` | Character BP Commandlet |
| `Plugins/.../Private/Commandlets/CreateAIControllerBlueprintCommandlet.h/.cpp` | AIController BP Commandlet |
| `Plugins/.../Private/Commandlets/CompileBlueprintsCommandlet.h/.cpp` | 기존 BP 일괄 컴파일 Commandlet |
| `Plugins/.../Tests/CharacterBlueprintBuilderTest.cpp` | 빌더 자동화 테스트 |
| `tools/ue/create_character_bp.sh` | Character BP Shell Wrapper |
| `tools/ue/create_ai_controller.sh` | AIController BP Shell Wrapper |
| `tools/ue/compile_blueprints.sh` | 일괄 컴파일 Shell Wrapper |
| `tools/test/smoke/create_blueprint.sh` | Phase 3 end-to-end Smoke Test |

---

## Task 1: CharacterBlueprintBuilder (TDD)

- [ ] **Step 1: 실패하는 자동화 테스트 작성**

```cpp
// Tests/CharacterBlueprintBuilderTest.cpp
#include "Misc/AutomationTest.h"
#include "Builders/CharacterBlueprintBuilder.h"
#include "Specs/BlueprintSpec.h"
#include "CommandForgeTypes.h"
#include "HAL/PlatformFileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"

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
    TMap<FString, FString> Validation;
    const bool bOk = UECommandForge::FCharacterBlueprintBuilder::Build(Spec, Errors, Validation);

    TestTrue(TEXT("빌드 성공"), bOk);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());

    // 3단계 검증 결과 확인
    TestEqual(TEXT("컴파일 상태"), Validation.FindRef(TEXT("compile_status")), FString(TEXT("ok")));
    TestEqual(TEXT("디스크 저장"), Validation.FindRef(TEXT("asset_on_disk")), FString(TEXT("true")));
    TestTrue(TEXT("Registry 키 존재"), Validation.Contains(TEXT("asset_in_registry")));

    // 실제 .uasset 파일 존재 확인
    const FString FileName = FPackageName::LongPackageNameToFilename(
        Spec.AssetPath, FPackageName::GetAssetPackageExtension());
    TestTrue(TEXT(".uasset 파일 존재"), FPlatformFileManager::Get().GetPlatformFile().FileExists(*FileName));

    return true;
}
```

- [ ] **Step 2: 테스트 실행 — 실패 확인**

```bash
./tools/test/automation/run.sh
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
        static bool Build(const FBlueprintSpec& Spec,
                          TArray<FCommandForgeError>& OutErrors,
                          TMap<FString, FString>& OutValidation);
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
#include "HAL/PlatformFileManager.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"

namespace UECommandForge
{
    bool FCharacterBlueprintBuilder::Build(const FBlueprintSpec& Spec,
                                            TArray<FCommandForgeError>& OutErrors,
                                            TMap<FString, FString>& OutValidation)
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

        // Blueprint 생성
        const FString AssetName = FPackageName::GetLongPackageAssetName(Spec.AssetPath);
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

        // 단계 1: 컴파일 상태 검증
        if (BP->Status != BS_UpToDate)
        {
            OutValidation.Add(TEXT("compile_status"), TEXT("error"));
            OutErrors.Add({ TEXT("BP_COMPILE_FAILED"),
                FString::Printf(TEXT("Blueprint 컴파일 실패: %s"), *Spec.AssetPath),
                TEXT("Character.AssetPath") });
            return false;
        }
        OutValidation.Add(TEXT("compile_status"), TEXT("ok"));

        // 저장
        UPackage* Package = BP->GetOutermost();
        Package->SetDirtyFlag(true);
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Spec.AssetPath, FPackageName::GetAssetPackageExtension());
        const ESavePackageResult SaveResult = UPackage::SavePackage(Package, BP, *FileName, SaveArgs);

        // 단계 2: 디스크 저장 검증 (SavePackage 반환값 + FileExists 이중 확인)
        const bool bSavedOk = (SaveResult == ESavePackageResult::Success) &&
                               FPlatformFileManager::Get().GetPlatformFile().FileExists(*FileName);
        OutValidation.Add(TEXT("asset_on_disk"), bSavedOk ? TEXT("true") : TEXT("false"));
        if (!bSavedOk)
        {
            OutErrors.Add({ TEXT("BP_SAVE_FAILED"),
                FString::Printf(TEXT("Blueprint 저장 실패: %s"), *FileName),
                TEXT("Character.AssetPath") });
            return false;
        }

        // 단계 3: AssetRegistry 등록 확인 (강제 갱신 후 조회)
        IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        AR.ScanModifiedAssetFiles({ FileName });

        TArray<FAssetData> Assets;
        AR.GetAssetsByPackageName(*Spec.AssetPath, Assets);
        const bool bInRegistry = (Assets.Num() > 0);
        OutValidation.Add(TEXT("asset_in_registry"), bInRegistry ? TEXT("true") : TEXT("false"));
        // Registry 미등록은 경고만 기록하고 ok 처리 (비동기 갱신 지연 허용)

        return true;
    }
}
```

- [ ] **Step 5: 테스트 재실행 — 통과 확인**

```bash
./tools/test/automation/run.sh
```

예상: `FCharacterBlueprintBuilderTest` PASS.

- [ ] **Step 6: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/CharacterBlueprintBuilder.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/CharacterBlueprintBuilder.cpp \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/CharacterBlueprintBuilderTest.cpp
git commit -m "feat: CharacterBlueprintBuilder — BP 생성·컴파일·저장·3단계 검증 + 자동화 테스트"
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
        static bool Build(const FBlueprintSpec& Spec,
                          TArray<FCommandForgeError>& OutErrors,
                          TMap<FString, FString>& OutValidation);
    };
}
```

- [ ] **Step 2: `AIControllerBlueprintBuilder.cpp` 작성**

`CharacterBlueprintBuilder.cpp`와 동일한 패턴 (3단계 검증 포함). 부모 클래스 로드 시 `AIController`를 `"/Script/AIModule.AIController"`에서 찾는다.

```cpp
#include "Builders/AIControllerBlueprintBuilder.h"
#include "KismetEditorUtilities.h"
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

        // 단계 1: 컴파일 상태 검증
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
        const ESavePackageResult SaveResult = UPackage::SavePackage(Package, BP, *FileName, SaveArgs);

        // 단계 2: 디스크 저장 검증
        const bool bSavedOk = (SaveResult == ESavePackageResult::Success) &&
                               FPlatformFileManager::Get().GetPlatformFile().FileExists(*FileName);
        OutValidation.Add(TEXT("asset_on_disk"), bSavedOk ? TEXT("true") : TEXT("false"));
        if (!bSavedOk)
        {
            OutErrors.Add({ TEXT("BP_SAVE_FAILED"),
                FString::Printf(TEXT("Blueprint 저장 실패: %s"), *FileName),
                TEXT("AIController.AssetPath") });
            return false;
        }

        // 단계 3: AssetRegistry 등록 확인
        IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        AR.ScanModifiedAssetFiles({ FileName });
        TArray<FAssetData> Assets;
        AR.GetAssetsByPackageName(*Spec.AssetPath, Assets);
        OutValidation.Add(TEXT("asset_in_registry"), (Assets.Num() > 0) ? TEXT("true") : TEXT("false"));

        return true;
    }
}
```

- [ ] **Step 3: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/AIControllerBlueprintBuilder.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/AIControllerBlueprintBuilder.cpp
git commit -m "feat: AIControllerBlueprintBuilder — 3단계 검증 포함"
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
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FAIFlowSpecParser::Parse(JsonStr, Spec, ParseErrors))
    {
        Report.Errors = ParseErrors;
        UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TArray<FCommandForgeError> ValidateErrors;
    UECommandForge::FAIFlowSpecValidator::Validate(Spec, ValidateErrors);
    if (!ValidateErrors.IsEmpty())
    {
        Report.Errors = ValidateErrors;
        UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    TArray<FCommandForgeError> BuildErrors;
    TMap<FString, FString> Validation;
    const bool bBuilt = UECommandForge::FCharacterBlueprintBuilder::Build(Spec.Character, BuildErrors, Validation);

    Report.Errors = BuildErrors;
    Report.Validation = Validation;

    if (!bBuilt)
    {
        UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::BuildFailed);
    }

    Report.bOk = true;
    Report.CreatedAssets.Add(Spec.Character.AssetPath);
    UECommandForge::FJsonReportWriter::Write(*OutPath, Report);
    return 0;
}
```

- [ ] **Step 3: `CreateAIControllerBlueprintCommandlet` 작성**

`CreateCharacterBlueprintCommandlet`과 동일한 패턴. `FCharacterBlueprintBuilder` 대신 `FAIControllerBlueprintBuilder` 사용. Report.Commandlet = `"CreateAIControllerBlueprint"`.

- [ ] **Step 4: `CompileBlueprintsCommandlet.h/.cpp` 작성**

`-AssetPaths` 파라미터로 쉼표 구분된 에셋 경로 목록을 받아 `FKismetEditorUtilities::CompileBlueprint`로 일괄 컴파일. 각 에셋 컴파일 결과(`BS_UpToDate` 여부)를 `validation` 맵에 기록.

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

## Task 4: Smoke Test 스크립트

- [ ] **Step 1: `tools/test/smoke/create_blueprint.sh` 작성**

```bash
#!/usr/bin/env bash
# Blueprint 생성 end-to-end smoke test (Character + AIController)
# 사용법: ./tools/test/smoke/create_blueprint.sh <spec_file>
#   예시: ./tools/test/smoke/create_blueprint.sh specs/examples/guard_ai.json
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAMPLE_DIR="$(cd "${SCRIPT_DIR}/../../../sample" && pwd)"
UE_TOOLS="${SCRIPT_DIR}/../../ue"
SPEC_FILE="${1:-${SCRIPT_DIR}/../../../specs/examples/guard_ai.json}"

PASS=0
FAIL=0

check() {
    local desc="$1"
    local result="$2"
    if [ "$result" = "true" ]; then
        echo "  PASS: ${desc}"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: ${desc}"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== Blueprint Create Smoke Test ==="
echo "Spec: ${SPEC_FILE}"
echo ""

# --- Character Blueprint ---
echo "[1] CreateCharacterBlueprint"
"${UE_TOOLS}/create_character_bp.sh" "${SPEC_FILE}"
CHAR_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/CreateCharacterBlueprint_"*.json | head -1)"

CHAR_OK=$(jq -r '.ok' "${CHAR_REPORT}")
CHAR_COMPILE=$(jq -r '.validation.compile_status // "missing"' "${CHAR_REPORT}")
CHAR_ON_DISK=$(jq -r '.validation.asset_on_disk // "missing"' "${CHAR_REPORT}")
CHAR_IN_REG=$(jq -r 'if .validation | has("asset_in_registry") then "true" else "false" end' "${CHAR_REPORT}")
CHAR_ASSET_PATH=$(jq -r '.created_assets[0]' "${CHAR_REPORT}" | sed 's|/Game/|Content/|')
CHAR_UASSET="${SAMPLE_DIR}/${CHAR_ASSET_PATH}.uasset"

check "ok == true" "${CHAR_OK}"
check "compile_status == ok" "$([ "${CHAR_COMPILE}" = "ok" ] && echo true || echo false)"
check "asset_on_disk == true" "${CHAR_ON_DISK}"
check "asset_in_registry 키 존재" "${CHAR_IN_REG}"
check ".uasset 파일 존재" "$([ -f "${CHAR_UASSET}" ] && echo true || echo false)"

echo ""

# --- AIController Blueprint ---
echo "[2] CreateAIControllerBlueprint"
"${UE_TOOLS}/create_ai_controller.sh" "${SPEC_FILE}"
AI_REPORT="$(ls -t "${SAMPLE_DIR}/Saved/UECommandForge/Reports/CreateAIControllerBlueprint_"*.json | head -1)"

AI_OK=$(jq -r '.ok' "${AI_REPORT}")
AI_COMPILE=$(jq -r '.validation.compile_status // "missing"' "${AI_REPORT}")
AI_ON_DISK=$(jq -r '.validation.asset_on_disk // "missing"' "${AI_REPORT}")
AI_IN_REG=$(jq -r 'if .validation | has("asset_in_registry") then "true" else "false" end' "${AI_REPORT}")
AI_ASSET_PATH=$(jq -r '.created_assets[0]' "${AI_REPORT}" | sed 's|/Game/|Content/|')
AI_UASSET="${SAMPLE_DIR}/${AI_ASSET_PATH}.uasset"

check "ok == true" "${AI_OK}"
check "compile_status == ok" "$([ "${AI_COMPILE}" = "ok" ] && echo true || echo false)"
check "asset_on_disk == true" "${AI_ON_DISK}"
check "asset_in_registry 키 존재" "${AI_IN_REG}"
check ".uasset 파일 존재" "$([ -f "${AI_UASSET}" ] && echo true || echo false)"

echo ""
echo "=== 결과: PASS ${PASS} / FAIL ${FAIL} ==="

if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi
```

- [ ] **Step 2: 실행 권한 부여**

```bash
chmod +x tools/test/smoke/create_blueprint.sh
```

- [ ] **Step 3: 커밋**

```bash
git add tools/test/smoke/create_blueprint.sh
git commit -m "test: Phase 3 smoke test 스크립트 추가 (tools/smoke/)"
```

---

## Phase 3 인수 조건

```bash
# 1. 플러그인 빌드
./tools/ue/build_plugin.sh

# 2. 자동화 테스트 (compile_status, asset_on_disk, asset_in_registry 포함)
./tools/test/automation/run.sh

# 3. Character BP 생성
./tools/ue/create_character_bp.sh specs/examples/guard_ai.json

CHAR_REPORT="$(ls -t Saved/UECommandForge/Reports/CreateCharacterBlueprint_*.json | head -1)"
jq -e '.ok
  and (.validation.compile_status == "ok")
  and (.validation.asset_on_disk == "true")' "$CHAR_REPORT"

# 실제 .uasset 파일 존재 확인 (AssetPath를 경로로 변환)
CHAR_ASSET_PATH=$(jq -r '.created_assets[0]' "$CHAR_REPORT" | sed 's|/Game/|Content/|')
test -f "sample/${CHAR_ASSET_PATH}.uasset" && echo "Character .uasset 존재"

# 4. AIController BP 생성
./tools/ue/create_ai_controller.sh specs/examples/guard_ai.json

AI_REPORT="$(ls -t Saved/UECommandForge/Reports/CreateAIControllerBlueprint_*.json | head -1)"
jq -e '.ok
  and (.validation.compile_status == "ok")
  and (.validation.asset_on_disk == "true")' "$AI_REPORT"

AI_ASSET_PATH=$(jq -r '.created_assets[0]' "$AI_REPORT" | sed 's|/Game/|Content/|')
test -f "sample/${AI_ASSET_PATH}.uasset" && echo "AIController .uasset 존재"
```

Phase 3 완료 조건: 두 Blueprint 에셋이 `Content/` 디렉터리에 `.uasset`으로 생성되고, `compile_status == "ok"`, `asset_on_disk == "true"`가 Result JSON에 기록됨. `asset_in_registry`는 기록되어야 하지만 값은 무관(비동기 허용).

---

## [후순위] Task 4: CDO 프로퍼티 설정 (Phase 3 완료 후 별도 세부 기획 필요)

> **주의:** 이 Task는 현재 Phase 3 인수 조건에 포함되지 않는다. Task 1~3 완료 후 세부 기획을 거쳐 구현한다.

**목적:** 생성된 Blueprint의 CDO(Class Default Object)에 `AutoPossessAI` 등 프로퍼티를 설정한다.

**알려진 리스크 (세부 기획 시 반드시 검토):**
- **CDO 2-pass 필수:** `AutoPossessAI` 등 일부 프로퍼티는 첫 번째 `CompileBlueprint` 후 기본값으로 리셋될 수 있다. 반드시 컴파일 후 CDO를 재검증하고, 값이 틀리면 재설정 후 재컴파일한다.
- **프로퍼티 접근 방법:** `BP->GeneratedClass->GetDefaultObject()`로 CDO를 가져온 뒤 `FindFProperty`로 프로퍼티를 찾아 값을 설정한다. `FProperty::CopyCompleteValue` 또는 `ImportText_Direct` 사용.
- **컴파일 순서 의존성:** CDO 설정은 반드시 첫 번째 `CompileBlueprint` 완료(`BS_UpToDate`) 이후에 수행해야 한다. 생성 직후 CDO 접근 시 GeneratedClass가 null일 수 있다.

**예상 Builder 인터페이스 확장 (미확정):**

```cpp
// FCharacterBlueprintBuilder에 추가 예정
static bool SetCDOProperties(UBlueprint* BP,
                              const FCharacterCDOSpec& CDOSpec,
                              TArray<FCommandForgeError>& OutErrors,
                              TMap<FString, FString>& OutValidation);
```

**예상 검증 항목 (미확정):**

| validation 키 | 의미 |
|---|---|
| `cdo_auto_possess_ai` | AutoPossessAI 설정값 (`"PlacedInWorldOrSpawned"` 등) |
| `cdo_compile_status` | 2-pass 컴파일 후 상태 (`"ok"` / `"error"`) |
