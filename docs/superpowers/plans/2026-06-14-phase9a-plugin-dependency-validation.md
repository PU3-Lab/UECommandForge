# Phase 9A — ValidatePluginDependencies Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 출시 전 `.uproject`/`.uplugin` 플러그인 의존성을 JSON 정책으로 검증하는 `ValidatePluginDependencies` 커맨드렛을 추가한다.

**Architecture:** 기존 `ValidateBuildCs` 커맨드렛 패턴을 그대로 따른다 — `UCommandlet::Main`에서 인자 파싱 → 정책 JSON 파싱(`FCommandForgePolicyParser`) → `FProjectDescriptor`/`FPluginDescriptor`(Projects 모듈)로 대상 프로젝트 분석 → `FCommandForgeReport`에 `ValidationIssues` 축적 → `FJsonReportWriter`로 기록 → `ECommandForgeExitCode` 반환. 읽기 전용.

**Tech Stack:** UE 5.7 C++20, `UnrealEd`, `Json`, **`Projects`**(신규 의존성), 기존 `CommandForgePolicyParser`/`JsonReportWriter`/`CommandForgeTypes`.

> **범위:** 본 계획은 9A의 두 커맨드렛 중 **ValidatePluginDependencies만** 다룬다. `DiffPlatformConfig`는 스펙 H1(`FConfigContext` 크로스플랫폼 로드) 스파이크 결과에 설계가 좌우되므로 **별도 계획**으로 분리한다. 스펙: [2026-06-14-phase9a-pre-release-validation-design.md](../specs/2026-06-14-phase9a-pre-release-validation-design.md)

---

## 진행 상태 (2026-06-15 업데이트)

**Task 1–7 구현·검증 완료** (branch `docs/harness-scope-and-release-roadmap`, worktree `worktree-phase9a-plugin-deps` 병합).

| Task | 내용 | 커밋 | 상태 |
|---|---|---|---|
| 1 | 스켈레톤 + `Projects` 의존성 | `8adc993` | ✅ 커밋 및 실기 검증 완료 |
| 2 | 정책 JSON 파싱 | `23f8a8b` | ✅ 커밋 및 실기 검증 완료 |
| 3 | 대상 `.uproject` 플러그인 활성 상태 읽기 | `fbd5e0a` | ✅ 커밋 및 실기 검증 완료 |
| 4 | required 누락/비활성 규칙 | `0ce52e2` | ✅ 커밋 및 실기 검증 완료 |
| 5 | forbiddenInShipping + `-Configuration` 게이트 | `3c2809f` | ✅ 커밋 및 실기 검증 완료 |
| 6 | `.uplugin` 모듈 타입으로 금지 정밀화 | `9affb42` | ✅ 커밋 및 실기 검증 완료 |
| 7 | 래퍼 + 예시 정책 + README | `bf1e85d` | ✅ 커밋 및 실기 검증 완료 |

> **검증 상태:** UE 5.7 빌드 및 자동화 테스트 59개 전체 통과(`PASS: 59 / FAIL: 0`), 그리고 `tools/ue/validate_plugin_deps.sh` 래퍼 스모크 테스트를 완료하고 리포트 생성 및 정상 판정(`ok: true`, `issue_count: 0`)을 확인 완료함.

> **스펙 대비 잔여(별도 작업):** 스펙 §3.4 M3 심화(`EnabledByDefault`·`PlatformAllowList/DenyList`/`TargetAllowList` 반영), §3.5–3.6 Editor 모듈 warning(`PLUGIN_EDITOR_MODULE_IN_RUNTIME_PLUGIN`, `PLUGIN_MODULE_TYPE_SUSPECT`), §2.3 `summary` 필드, §7.3 커밋된 smoke test 파일(`tools/test/smoke/validate_plugin_deps.sh`)은 본 계획 범위 밖이며 후속 작업으로 남는다.

---

## File Structure

| 파일 | 책임 |
|---|---|
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidatePluginDependenciesCommandlet.h` | 커맨드렛 선언 |
| `.../Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp` | 인자 파싱, 정책/프로젝트 분석, 규칙 적용, 리포트 기록 |
| `.../UECommandForgeEditor.Build.cs` | `Projects` 모듈 의존성 추가 |
| `.../Tests/ValidatePluginDependenciesCommandletTest.cpp` | 자동화 테스트 (temp 프로젝트/플러그인 합성) |
| `tools/ue/validate_plugin_deps.sh` / `.bat` | 에이전트 명령 표면 래퍼 |
| `specs/policies/plugin.policy.example.json` | 예시 정책 |
| `README.md` | 명령 표면 표에 등재 |

경로 표기상 `EDITOR=sample/Plugins/UECommandForge/Source/UECommandForgeEditor` 로 줄여 쓴다.

---

## Task 1: 커맨드렛 스켈레톤 + Projects 의존성

**Files:**
- Modify: `EDITOR/UECommandForgeEditor.Build.cs`
- Create: `EDITOR/Private/Commandlets/ValidatePluginDependenciesCommandlet.h`
- Create: `EDITOR/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp`
- Test: `EDITOR/Tests/ValidatePluginDependenciesCommandletTest.cpp`

- [x] **Step 1: Build.cs에 `Projects` 의존성 추가**

`UECommandForgeEditor.Build.cs`의 `PrivateDependencyModuleNames` 배열 끝에 `"Projects"`를 추가한다 (`FProjectDescriptor`/`FPluginDescriptor` 사용 위함):

```csharp
PrivateDependencyModuleNames.AddRange(new[]
{
    "UnrealEd", "Json", "JsonUtilities",
    "AssetTools", "AssetRegistry",
    "BlueprintGraph", "KismetCompiler", "Kismet",
    "EditorScriptingUtilities",
    "GameplayTags",
    "AIModule", "GameplayTasks", "NavigationSystem",
    "StateTreeModule", "StateTreeEditorModule", "GameplayStateTreeModule",
    "PropertyBindingUtils",
    "Projects"
});
```

- [x] **Step 2: 헤더 생성**

`ValidatePluginDependenciesCommandlet.h`:

```cpp
#pragma once

#include "Commandlets/Commandlet.h"
#include "ValidatePluginDependenciesCommandlet.generated.h"

UCLASS()
class UValidatePluginDependenciesCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UValidatePluginDependenciesCommandlet();
    virtual int32 Main(const FString& Params) override;
};
```

- [x] **Step 3: 실패 테스트 작성 — `-Policy` 누락 시 SpecParseFailed**

`ValidatePluginDependenciesCommandletTest.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "Commandlets/ValidatePluginDependenciesCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsMissingPolicyArgTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.FailsWithoutPolicyArg",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    FString PluginDepsTempDir()
    {
        return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tmp"), TEXT("PluginDepsTest"));
    }

    bool LoadPluginDepsReport(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path)) { return false; }
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }
}

bool FValidatePluginDepsMissingPolicyArgTest::RunTest(const FString& Parameters)
{
    const FString OutPath = FPaths::Combine(PluginDepsTempDir(), TEXT("missing_policy.json"));
    IFileManager::Get().Delete(*OutPath);

    UValidatePluginDependenciesCommandlet* Commandlet =
        NewObject<UValidatePluginDependenciesCommandlet>();
    const FString Params = FString::Printf(TEXT("-Output=\"%s\""), *OutPath);
    const int32 ExitCode = Commandlet->Main(Params);

    TestEqual(TEXT("exit code is SpecParseFailed"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::SpecParseFailed));

    TSharedPtr<FJsonObject> Report;
    TestTrue(TEXT("report written"), LoadPluginDepsReport(OutPath, Report));
    if (Report.IsValid())
    {
        TestFalse(TEXT("ok is false"), Report->GetBoolField(TEXT("ok")));
    }
    return true;
}
```

- [x] **Step 4: 테스트 실패 확인**

Run: `tools/ue/build_plugin.sh && tools/test/automation/run.sh`
Expected: 컴파일 실패 (`Main` 미구현) 또는 링크 실패 — 테스트가 RED.

- [x] **Step 5: 최소 구현 — 인자 파싱 + 빈 리포트 기록**

`ValidatePluginDependenciesCommandlet.cpp`:

```cpp
#include "Commandlets/ValidatePluginDependenciesCommandlet.h"

#include "CommandForgeTypes.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"

UValidatePluginDependenciesCommandlet::UValidatePluginDependenciesCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::ValidatePluginDepsPrivate
{
    void AddError(FCommandForgeReport& Report, const FString& Code, const FString& Message,
        const FString& Field)
    {
        FCommandForgeError Error;
        Error.Code = Code;
        Error.Message = Message;
        Error.Field = Field;
        Report.Errors.Add(Error);
    }
}

int32 UValidatePluginDependenciesCommandlet::Main(const FString& Params)
{
    namespace Private = UECommandForge::ValidatePluginDepsPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString PolicyPath = ParamsMap.FindRef(TEXT("Policy"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidatePluginDependencies");
    Report.bDryRun = true;

    if (OutPath.IsEmpty())
    {
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (PolicyPath.IsEmpty())
    {
        Private::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Policy 인자가 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    // 후속 Task에서 정책/프로젝트 분석을 채운다.
    Report.bOk = true;
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return 0;
}
```

- [x] **Step 6: 테스트 통과 확인**

Run: `tools/ue/build_plugin.sh && tools/test/automation/run.sh`
Expected: `FValidatePluginDepsMissingPolicyArgTest` PASS.

- [x] **Step 7: 커밋**

```bash
git add sample/Plugins/UECommandForge/Source/UECommandForgeEditor/UECommandForgeEditor.Build.cs \
        sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidatePluginDependenciesCommandlet.h \
        sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp \
        sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/ValidatePluginDependenciesCommandletTest.cpp
git commit -m "feat: scaffold ValidatePluginDependencies commandlet"
```

---

## Task 2: 정책 JSON 파싱

**Files:**
- Modify: `EDITOR/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp`
- Test: `EDITOR/Tests/ValidatePluginDependenciesCommandletTest.cpp`

정책 스키마: `kind: "plugin_dependency_policy"`, 배열 `required`/`forbiddenInShipping`/`optional`/`allowedEditorOnly`.

- [x] **Step 1: 실패 테스트 — 잘못된 kind는 SpecParseFailed**

테스트 파일에 추가:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsRejectsWrongKindTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.RejectsWrongPolicyKind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool SavePluginDepsPolicy(const FString& Path, const FString& Kind,
        const FString& RequiredJsonArray, const FString& ForbiddenJsonArray)
    {
        const FString Json = FString::Printf(TEXT(R"({
  "version": "1",
  "kind": "%s",
  "required": %s,
  "forbiddenInShipping": %s,
  "optional": [],
  "allowedEditorOnly": []
})"), *Kind, *RequiredJsonArray, *ForbiddenJsonArray);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Json, *Path,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool FValidatePluginDepsRejectsWrongKindTest::RunTest(const FString& Parameters)
{
    const FString PolicyPath = FPaths::Combine(PluginDepsTempDir(), TEXT("wrong_kind.policy.json"));
    const FString OutPath = FPaths::Combine(PluginDepsTempDir(), TEXT("wrong_kind.json"));
    TestTrue(TEXT("policy saved"),
        SavePluginDepsPolicy(PolicyPath, TEXT("buildcs_policy"), TEXT("[]"), TEXT("[]")));

    UValidatePluginDependenciesCommandlet* Commandlet =
        NewObject<UValidatePluginDependenciesCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Policy=\"%s\""), *OutPath, *PolicyPath);
    const int32 ExitCode = Commandlet->Main(Params);

    TestEqual(TEXT("exit code SpecParseFailed"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::SpecParseFailed));
    return true;
}
```

- [x] **Step 2: 테스트 실패 확인**

Run: `tools/test/automation/run.sh`
Expected: FAIL — 현재 구현은 kind를 검사하지 않아 0을 반환.

- [x] **Step 3: 정책 파싱 구현**

`.cpp` 상단 include 추가: `#include "Dom/JsonObject.h"`, `#include "Specs/CommandForgePolicyParser.h"`.

`Private` 네임스페이스에 추가:

```cpp
    struct FPluginPolicy
    {
        TArray<FString> Required;
        TArray<FString> ForbiddenInShipping;
        TArray<FString> Optional;
        TArray<FString> AllowedEditorOnly;
    };

    bool TryGetStringArray(const TSharedPtr<FJsonObject>& Object, const FString& FieldName,
        TArray<FString>& OutValues)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
        {
            return true; // 선택 필드
        }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString S;
            if (!Value->TryGetString(S)) { return false; }
            OutValues.Add(S.TrimStartAndEnd());
        }
        return true;
    }

    bool ParsePolicy(const FCommandForgePolicyDocument& Document, FPluginPolicy& Out,
        FCommandForgeReport& Report)
    {
        if (!Document.Kind.Equals(TEXT("plugin_dependency_policy"), ESearchCase::IgnoreCase))
        {
            AddError(Report, TEXT("POLICY_KIND_MISMATCH"),
                TEXT("정책 kind는 'plugin_dependency_policy'여야 합니다."), TEXT("kind"));
            return false;
        }
        bool bOk = true;
        bOk &= TryGetStringArray(Document.RootObject, TEXT("required"), Out.Required);
        bOk &= TryGetStringArray(Document.RootObject, TEXT("forbiddenInShipping"), Out.ForbiddenInShipping);
        bOk &= TryGetStringArray(Document.RootObject, TEXT("optional"), Out.Optional);
        bOk &= TryGetStringArray(Document.RootObject, TEXT("allowedEditorOnly"), Out.AllowedEditorOnly);
        if (!bOk)
        {
            AddError(Report, TEXT("PLUGIN_POLICY_INVALID"),
                TEXT("정책 배열은 string 배열이어야 합니다."), TEXT("policy"));
        }
        return bOk;
    }
```

`Main`의 "후속 Task에서..." 자리에 정책 로드/파싱을 삽입(ValidateBuildCs와 동일 형태):

```cpp
    FString PolicyJson;
    if (!FFileHelper::LoadFileToString(PolicyJson, *PolicyPath))
    {
        Private::AddError(Report, TEXT("POLICY_READ_FAILED"),
            TEXT("정책 파일을 읽을 수 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgePolicyDocument Document;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FCommandForgePolicyParser::ParseJson(PolicyJson, Document, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    Private::FPluginPolicy Policy;
    if (!Private::ParsePolicy(Document, Policy, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    Report.bOk = true; // 규칙은 후속 Task에서
    UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return 0;
```

- [x] **Step 4: 테스트 통과 확인**

Run: `tools/ue/build_plugin.sh && tools/test/automation/run.sh`
Expected: `FValidatePluginDepsRejectsWrongKindTest` PASS, 기존 테스트 유지.

- [x] **Step 5: 커밋**

```bash
git add -A && git commit -m "feat: parse plugin dependency policy JSON"
```

---

## Task 3: 대상 프로젝트 플러그인 활성 상태 계산

**Files:**
- Modify: `EDITOR/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp`
- Test: `EDITOR/Tests/ValidatePluginDependenciesCommandletTest.cpp`

`-Project=<.uproject>`를 `FProjectDescriptor::Load`로 읽어, 플러그인 이름 → 활성 여부 맵을 만든다. 활성 = `.uproject` `Plugins[]`에 `bEnabled:true`.

- [x] **Step 1: 실패 테스트 — 활성 플러그인 집합 노출**

테스트에 합성 .uproject 헬퍼 + 케이스 추가:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsReadsEnabledPluginsTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.ReadsEnabledPlugins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    // bEnabled가 다른 두 플러그인을 가진 최소 .uproject 생성
    bool SaveUProject(const FString& Path)
    {
        const FString Json = TEXT(R"({
  "FileVersion": 3,
  "EngineAssociation": "5.7",
  "Plugins": [
    { "Name": "Niagara", "Enabled": true },
    { "Name": "OnlineSubsystem", "Enabled": false }
  ]
})");
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Json, *Path,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool FValidatePluginDepsReadsEnabledPluginsTest::RunTest(const FString& Parameters)
{
    const FString ProjPath = FPaths::Combine(PluginDepsTempDir(), TEXT("Read"), TEXT("T.uproject"));
    const FString PolicyPath = FPaths::Combine(PluginDepsTempDir(), TEXT("read.policy.json"));
    const FString OutPath = FPaths::Combine(PluginDepsTempDir(), TEXT("read.json"));
    TestTrue(TEXT("uproject saved"), SaveUProject(ProjPath));
    TestTrue(TEXT("policy saved"),
        SavePluginDepsPolicy(PolicyPath, TEXT("plugin_dependency_policy"), TEXT("[]"), TEXT("[]")));

    UValidatePluginDependenciesCommandlet* Commandlet =
        NewObject<UValidatePluginDependenciesCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\""), *OutPath, *PolicyPath, *ProjPath);
    Commandlet->Main(Params);

    TSharedPtr<FJsonObject> Report;
    TestTrue(TEXT("report"), LoadPluginDepsReport(OutPath, Report));
    if (Report.IsValid())
    {
        const TSharedPtr<FJsonObject>* Summary = nullptr;
        if (Report->TryGetObjectField(TEXT("validation"), Summary) && Summary)
        {
            FString Enabled;
            (*Summary)->TryGetStringField(TEXT("enabled_plugin_count"), Enabled);
            TestEqual(TEXT("one enabled plugin"), Enabled, TEXT("1"));
        }
    }
    return true;
}
```

> 참고: 리포트 JSON의 `validation` 객체는 `FCommandForgeReport.Validation`(TMap) 직렬화 결과다.

- [x] **Step 2: 테스트 실패 확인**

Run: `tools/test/automation/run.sh`
Expected: FAIL — `enabled_plugin_count` 미기록.

- [x] **Step 3: 프로젝트 로드 구현**

`.cpp` include 추가: `#include "ProjectDescriptor.h"`, `#include "Misc/Paths.h"`.

`Private` 네임스페이스에 추가:

```cpp
    // .uproject를 읽어 이름->활성 여부 맵 생성 (Plugins[].bEnabled 기준)
    bool LoadEnabledPlugins(const FString& ProjectPath, TMap<FString, bool>& OutEnabled,
        FCommandForgeReport& Report)
    {
        FProjectDescriptor Descriptor;
        FText FailReason;
        if (!Descriptor.Load(ProjectPath, FailReason))
        {
            AddError(Report, TEXT("PROJECT_READ_FAILED"),
                FString::Printf(TEXT(".uproject 로드 실패: %s"), *FailReason.ToString()),
                TEXT("Project"));
            return false;
        }
        for (const FPluginReferenceDescriptor& Ref : Descriptor.Plugins)
        {
            OutEnabled.Add(Ref.Name, Ref.bEnabled);
        }
        return true;
    }
```

`Main`에서 정책 파싱 직후, `-Project` 처리:

```cpp
    const FString ProjectPath = ParamsMap.FindRef(TEXT("Project"));
    TMap<FString, bool> EnabledPlugins;
    if (!ProjectPath.IsEmpty())
    {
        if (!Private::LoadEnabledPlugins(ProjectPath, EnabledPlugins, Report))
        {
            Report.bOk = false;
            UECommandForge::FJsonReportWriter::Write(OutPath, Report);
            return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
        }
    }

    int32 EnabledCount = 0;
    for (const TPair<FString, bool>& Pair : EnabledPlugins)
    {
        if (Pair.Value) { ++EnabledCount; }
    }
    Report.Validation.Add(TEXT("enabled_plugin_count"), FString::FromInt(EnabledCount));
```

- [x] **Step 4: 테스트 통과 확인**

Run: `tools/ue/build_plugin.sh && tools/test/automation/run.sh`
Expected: `FValidatePluginDepsReadsEnabledPluginsTest` PASS.

- [x] **Step 5: 커밋**

```bash
git add -A && git commit -m "feat: read enabled plugins from target .uproject"
```

---

## Task 4: required 플러그인 누락/비활성 규칙

**Files:**
- Modify: `EDITOR/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp`
- Test: `EDITOR/Tests/ValidatePluginDependenciesCommandletTest.cpp`

- [x] **Step 1: 실패 테스트 — 누락/비활성 각각 error**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsRequiredRulesTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.FlagsMissingAndDisabledRequired",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    // 리포트의 issues[] 에서 code 존재 여부
    bool ReportHasIssueCode(const TSharedPtr<FJsonObject>& Report, const FString& Code)
    {
        const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
        if (!Report->TryGetArrayField(TEXT("issues"), Issues) || !Issues) { return false; }
        for (const TSharedPtr<FJsonValue>& V : *Issues)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (V->TryGetObject(Obj) && Obj)
            {
                FString C;
                if ((*Obj)->TryGetStringField(TEXT("code"), C) && C == Code) { return true; }
            }
        }
        return false;
    }
}

bool FValidatePluginDepsRequiredRulesTest::RunTest(const FString& Parameters)
{
    // Niagara=enabled, OnlineSubsystem=disabled (SaveUProject 재사용)
    const FString ProjPath = FPaths::Combine(PluginDepsTempDir(), TEXT("Req"), TEXT("T.uproject"));
    const FString PolicyPath = FPaths::Combine(PluginDepsTempDir(), TEXT("req.policy.json"));
    const FString OutPath = FPaths::Combine(PluginDepsTempDir(), TEXT("req.json"));
    TestTrue(TEXT("uproject"), SaveUProject(ProjPath));
    // required: OnlineSubsystem(비활성), Paper2D(아예 없음)
    TestTrue(TEXT("policy"), SavePluginDepsPolicy(PolicyPath, TEXT("plugin_dependency_policy"),
        TEXT(R"(["OnlineSubsystem","Paper2D"])"), TEXT("[]")));

    UValidatePluginDependenciesCommandlet* Commandlet =
        NewObject<UValidatePluginDependenciesCommandlet>();
    const FString Params = FString::Printf(
        TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\""), *OutPath, *PolicyPath, *ProjPath);
    const int32 ExitCode = Commandlet->Main(Params);

    TestEqual(TEXT("exit ValidationFailed"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> Report;
    TestTrue(TEXT("report"), LoadPluginDepsReport(OutPath, Report));
    if (Report.IsValid())
    {
        TestFalse(TEXT("ok false"), Report->GetBoolField(TEXT("ok")));
        TestTrue(TEXT("disabled flagged"),
            ReportHasIssueCode(Report, TEXT("PLUGIN_REQUIRED_DISABLED")));
        TestTrue(TEXT("missing flagged"),
            ReportHasIssueCode(Report, TEXT("PLUGIN_REQUIRED_MISSING")));
    }
    return true;
}
```

- [x] **Step 2: 테스트 실패 확인**

Run: `tools/test/automation/run.sh`
Expected: FAIL — 규칙 미구현.

- [x] **Step 3: 규칙 구현**

`Private`에 `AddIssue` 헬퍼 추가(ValidateBuildCs와 동일하나 severity 인자화):

```cpp
    void AddIssue(FCommandForgeReport& Report, const FString& Severity, const FString& Code,
        const FString& Message, const FString& Field, const FString& FilePath,
        const FString& SuggestedFix)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = Severity;
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Field = Field;
        Issue.FilePath = FilePath;
        Issue.SuggestedFix = SuggestedFix;
        Report.ValidationIssues.Add(Issue);
    }

    bool HasErrorIssue(const TArray<FCommandForgeValidationIssue>& Issues)
    {
        for (const FCommandForgeValidationIssue& I : Issues)
        {
            if (I.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase)) { return true; }
        }
        return false;
    }
```

`Main`의 `enabled_plugin_count` 기록 다음에 required 검사 추가:

```cpp
    for (const FString& Name : Policy.Required)
    {
        const bool* Enabled = EnabledPlugins.Find(Name);
        if (Enabled == nullptr)
        {
            Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_REQUIRED_MISSING"),
                TEXT("Required plugin is missing."),
                FString::Printf(TEXT("required[%s]"), *Name), ProjectPath,
                TEXT("Enable the plugin in the project or remove it from the required policy."));
        }
        else if (!*Enabled)
        {
            Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_REQUIRED_DISABLED"),
                TEXT("Required plugin is present but disabled."),
                FString::Printf(TEXT("required[%s]"), *Name), ProjectPath,
                TEXT("Set Enabled=true for this plugin in the .uproject."));
        }
    }
```

`Main` 말미의 `Report.bOk = true; ...return 0;` 블록을 최종 판정으로 교체:

```cpp
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() && !Private::HasErrorIssue(Report.ValidationIssues);
    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote) { return static_cast<int32>(ECommandForgeExitCode::EngineError); }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
```

(앞 Task들에서 임시로 둔 `Report.bOk = true; Write; return 0;` 라인은 제거.)

- [x] **Step 4: 테스트 통과 확인**

Run: `tools/ue/build_plugin.sh && tools/test/automation/run.sh`
Expected: `FValidatePluginDepsRequiredRulesTest` PASS, 기존 PASS 유지.

- [x] **Step 5: 커밋**

```bash
git add -A && git commit -m "feat: flag missing/disabled required plugins"
```

---

## Task 5: forbiddenInShipping 규칙 (-Configuration 게이트)

**Files:**
- Modify: `EDITOR/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp`
- Test: `EDITOR/Tests/ValidatePluginDependenciesCommandletTest.cpp`

`-Configuration=Shipping`일 때만 `forbiddenInShipping` 위반을 error로 본다. (정밀화: 런타임 모듈 보유 여부는 Task 6에서 .uplugin 분석과 함께 보강. 본 Task는 "활성 + Shipping 구성"까지.)

- [x] **Step 1: 실패 테스트 — Shipping에서만 금지 위반**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsForbiddenShippingTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.FlagsForbiddenOnlyInShipping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FValidatePluginDepsForbiddenShippingTest::RunTest(const FString& Parameters)
{
    const FString ProjPath = FPaths::Combine(PluginDepsTempDir(), TEXT("Forb"), TEXT("T.uproject"));
    const FString PolicyPath = FPaths::Combine(PluginDepsTempDir(), TEXT("forb.policy.json"));
    TestTrue(TEXT("uproject"), SaveUProject(ProjPath)); // Niagara enabled
    // forbiddenInShipping: Niagara (활성)
    TestTrue(TEXT("policy"), SavePluginDepsPolicy(PolicyPath, TEXT("plugin_dependency_policy"),
        TEXT("[]"), TEXT(R"(["Niagara"])")));

    // Development -> ok
    {
        const FString OutDev = FPaths::Combine(PluginDepsTempDir(), TEXT("forb_dev.json"));
        UValidatePluginDependenciesCommandlet* C = NewObject<UValidatePluginDependenciesCommandlet>();
        const int32 Exit = C->Main(FString::Printf(
            TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\" -Configuration=Development"),
            *OutDev, *PolicyPath, *ProjPath));
        TestEqual(TEXT("dev ok"), Exit, 0);
    }
    // Shipping -> ValidationFailed
    {
        const FString OutShip = FPaths::Combine(PluginDepsTempDir(), TEXT("forb_ship.json"));
        UValidatePluginDependenciesCommandlet* C = NewObject<UValidatePluginDependenciesCommandlet>();
        const int32 Exit = C->Main(FString::Printf(
            TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\" -Configuration=Shipping"),
            *OutShip, *PolicyPath, *ProjPath));
        TestEqual(TEXT("ship fail"), Exit,
            static_cast<int32>(ECommandForgeExitCode::ValidationFailed));
        TSharedPtr<FJsonObject> Report;
        if (LoadPluginDepsReport(OutShip, Report) && Report.IsValid())
        {
            TestTrue(TEXT("forbidden flagged"),
                ReportHasIssueCode(Report, TEXT("PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED")));
        }
    }
    return true;
}
```

- [x] **Step 2: 테스트 실패 확인**

Run: `tools/test/automation/run.sh`
Expected: FAIL.

- [x] **Step 3: 규칙 구현**

`Main`의 required 검사 다음에 추가:

```cpp
    const FString Configuration = ParamsMap.FindRef(TEXT("Configuration"));
    const bool bShipping = Configuration.Equals(TEXT("Shipping"), ESearchCase::IgnoreCase);
    Report.Validation.Add(TEXT("configuration"), Configuration.IsEmpty() ? TEXT("Development") : Configuration);

    if (bShipping)
    {
        for (const FString& Name : Policy.ForbiddenInShipping)
        {
            const bool* Enabled = EnabledPlugins.Find(Name);
            if (Enabled != nullptr && *Enabled)
            {
                Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED"),
                    TEXT("Plugin is forbidden in Shipping configuration but currently enabled."),
                    FString::Printf(TEXT("Plugins.%s.Enabled"), *Name), ProjectPath,
                    FString::Printf(TEXT("Disable %s for Shipping builds."), *Name));
            }
        }
    }
```

- [x] **Step 4: 테스트 통과 확인**

Run: `tools/ue/build_plugin.sh && tools/test/automation/run.sh`
Expected: `FValidatePluginDepsForbiddenShippingTest` PASS.

- [x] **Step 5: 커밋**

```bash
git add -A && git commit -m "feat: flag forbidden-in-shipping plugins under Shipping config"
```

---

## Task 6: Editor 전용 모듈 경고 + 금지 정밀화 (.uplugin 분석)

**Files:**
- Modify: `EDITOR/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp`
- Test: `EDITOR/Tests/ValidatePluginDependenciesCommandletTest.cpp`

대상 프로젝트의 `Plugins/**.uplugin`을 스캔해 모듈 타입을 본다. (M2/M3) 활성 플러그인에 Editor/DeveloperTool 전용 모듈만 있고 Runtime 모듈이 없으면 `forbiddenInShipping` 위반에서 제외하고, runtime 플러그인에 Editor 모듈이 섞여 있으면 warning.

- [x] **Step 1: 실패 테스트 — Editor 전용 모듈 플러그인은 Shipping 금지 오탐 안 됨**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidatePluginDepsEditorOnlyNotForbiddenTest,
    "UECommandForge.Commandlets.ValidatePluginDeps.EditorOnlyModuleNotForbiddenInShipping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    // 프로젝트 Plugins/<Name>/<Name>.uplugin 합성 (단일 Editor 모듈)
    bool SaveEditorOnlyPlugin(const FString& ProjectDir, const FString& PluginName)
    {
        const FString PluginPath = FPaths::Combine(ProjectDir, TEXT("Plugins"), PluginName,
            PluginName + TEXT(".uplugin"));
        const FString Json = FString::Printf(TEXT(R"({
  "FileVersion": 3,
  "FriendlyName": "%s",
  "Modules": [ { "Name": "%sEditor", "Type": "Editor", "LoadingPhase": "Default" } ]
})"), *PluginName, *PluginName);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(PluginPath));
        return FFileHelper::SaveStringToFile(Json, *PluginPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool SaveUProjectWithPlugin(const FString& Path, const FString& PluginName)
    {
        const FString Json = FString::Printf(TEXT(R"({
  "FileVersion": 3, "EngineAssociation": "5.7",
  "Plugins": [ { "Name": "%s", "Enabled": true } ]
})"), *PluginName);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Json, *Path,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

bool FValidatePluginDepsEditorOnlyNotForbiddenTest::RunTest(const FString& Parameters)
{
    const FString ProjDir = FPaths::Combine(PluginDepsTempDir(), TEXT("EdOnly"));
    const FString ProjPath = FPaths::Combine(ProjDir, TEXT("T.uproject"));
    const FString PolicyPath = FPaths::Combine(PluginDepsTempDir(), TEXT("edonly.policy.json"));
    const FString OutPath = FPaths::Combine(PluginDepsTempDir(), TEXT("edonly.json"));
    TestTrue(TEXT("uproject"), SaveUProjectWithPlugin(ProjPath, TEXT("MyEditorTool")));
    TestTrue(TEXT("uplugin"), SaveEditorOnlyPlugin(ProjDir, TEXT("MyEditorTool")));
    TestTrue(TEXT("policy"), SavePluginDepsPolicy(PolicyPath, TEXT("plugin_dependency_policy"),
        TEXT("[]"), TEXT(R"(["MyEditorTool"])")));

    UValidatePluginDependenciesCommandlet* C = NewObject<UValidatePluginDependenciesCommandlet>();
    const int32 Exit = C->Main(FString::Printf(
        TEXT("-Output=\"%s\" -Policy=\"%s\" -Project=\"%s\" -Configuration=Shipping"),
        *OutPath, *PolicyPath, *ProjPath));

    // Editor 전용 모듈만 → Shipping 코드 미포함 → 위반 아님 → ok
    TestEqual(TEXT("ok"), Exit, 0);
    TSharedPtr<FJsonObject> Report;
    if (LoadPluginDepsReport(OutPath, Report) && Report.IsValid())
    {
        TestFalse(TEXT("forbidden NOT flagged"),
            ReportHasIssueCode(Report, TEXT("PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED")));
    }
    return true;
}
```

- [x] **Step 2: 테스트 실패 확인**

Run: `tools/test/automation/run.sh`
Expected: FAIL — 현재 구현은 모듈 타입 무시, Niagara 외 케이스에서 금지 오탐.

- [x] **Step 3: .uplugin 모듈 분석 구현**

`.cpp` include 추가: `#include "PluginDescriptor.h"`, `#include "HAL/FileManager.h"`.

`Private`에 추가:

```cpp
    // 활성 플러그인이 Runtime 코드(=Shipping에 들어가는 모듈)를 가지는지 판정.
    // 대상 프로젝트 Plugins 디렉터리에서 <Name>.uplugin을 찾아 모듈 타입을 본다.
    bool PluginHasRuntimeModule(const FString& ProjectPath, const FString& PluginName)
    {
        const FString PluginsDir = FPaths::Combine(FPaths::GetPath(ProjectPath), TEXT("Plugins"));
        TArray<FString> Found;
        IFileManager::Get().FindFilesRecursive(Found, *PluginsDir,
            *(PluginName + TEXT(".uplugin")), true, false);
        if (Found.Num() == 0)
        {
            // descriptor를 못 찾으면 보수적으로 runtime 보유로 간주(오탐<누락 방지)
            return true;
        }
        FPluginDescriptor Desc;
        FText FailReason;
        if (!Desc.Load(Found[0], FailReason))
        {
            return true;
        }
        for (const FModuleDescriptor& Module : Desc.Modules)
        {
            switch (Module.Type)
            {
                case EHostType::Runtime:
                case EHostType::RuntimeNoCommandlet:
                case EHostType::RuntimeAndProgram:
                case EHostType::ClientOnly:
                case EHostType::ServerOnly:
                    return true;
                default:
                    break;
            }
        }
        return false;
    }
```

Task 5의 forbiddenInShipping 루프 내부 조건을 정밀화:

```cpp
            if (Enabled != nullptr && *Enabled &&
                Private::PluginHasRuntimeModule(ProjectPath, Name))
            {
                Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED"),
                    /* ...동일... */);
            }
```

- [x] **Step 4: 테스트 통과 확인**

Run: `tools/ue/build_plugin.sh && tools/test/automation/run.sh`
Expected: `FValidatePluginDepsEditorOnlyNotForbiddenTest` PASS, Task 5의 Niagara 케이스도 PASS 유지(Niagara는 runtime 모듈 보유 — 단, 합성 .uproject엔 .uplugin이 없어 보수적 true로 처리되어 유지됨).

- [x] **Step 5: 커밋**

```bash
git add -A && git commit -m "feat: refine forbidden-in-shipping using .uplugin module types"
```

---

## Task 7: 래퍼 스크립트 + 예시 정책 + README + 스모크

**Files:**
- Create: `tools/ue/validate_plugin_deps.sh`
- Create: `tools/ue/validate_plugin_deps.bat`
- Create: `specs/policies/plugin.policy.example.json`
- Modify: `README.md`

- [x] **Step 1: 예시 정책 작성**

`specs/policies/plugin.policy.example.json`:

```json
{
  "version": "1",
  "kind": "plugin_dependency_policy",
  "required": ["EnhancedInput"],
  "forbiddenInShipping": ["EditorScriptingUtilities", "PythonScriptPlugin"],
  "optional": ["GameplayInsights"],
  "allowedEditorOnly": []
}
```

- [x] **Step 2: 셸 래퍼 작성 (validate_buildcs.sh 패턴)**

`tools/ue/validate_plugin_deps.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <plugin.policy.json> [-Project=...] [-Configuration=Shipping] [extra args...]" >&2
  exit 2
fi

POLICY_PATH="$1"
shift

if [ -f "${POLICY_PATH}" ]; then
  POLICY_PATH="$(cd "$(dirname "${POLICY_PATH}")" && pwd)/$(basename "${POLICY_PATH}")"
fi

exec "${SCRIPT_DIR}/run_commandlet.sh" ValidatePluginDependencies -Policy="${POLICY_PATH}" "$@"
```

실행 권한: `chmod +x tools/ue/validate_plugin_deps.sh`

- [x] **Step 3: 배치 래퍼 작성**

`tools/ue/validate_plugin_deps.bat` — 기존 `tools/ue/validate_buildcs.bat`를 복사해 commandlet 이름만 `ValidatePluginDependencies`로 바꾸고 인자 안내 문구를 수정한다. (기존 .bat 구조 그대로 따른다.)

- [x] **Step 4: README 명령 표면 표에 등재**

`README.md`의 "AI 에이전트 명령 표면" 표에 행 추가:

```markdown
| `tools/ue/validate_plugin_deps.sh <policy.json>` | `tools\ue\validate_plugin_deps.bat <policy.json>` | 플러그인 의존성 정책 검증 |
```

- [x] **Step 5: 스모크 검증 (수동/CI)**

Run:
```bash
tools/ue/validate_plugin_deps.sh specs/policies/plugin.policy.example.json \
  -Project="$(pwd)/sample/UECommandForgeSample.uproject" -Configuration=Shipping
jq -e '.ok != null and (.command == "ValidatePluginDependencies")' \
  "$(ls -t sample/Saved/UECommandForge/Reports/ValidatePluginDependencies_*.json | head -1)"
```
Expected: Result JSON 생성, `command` 일치, `ok` 필드 존재.

- [x] **Step 6: 커밋**

```bash
git add tools/ue/validate_plugin_deps.sh tools/ue/validate_plugin_deps.bat \
        specs/policies/plugin.policy.example.json README.md
git commit -m "feat: add validate_plugin_deps wrappers, example policy, README entry"
```

---

## 후속 (별도 계획)

- **DiffPlatformConfig** — 스펙 H1 스파이크(`FConfigContext` 크로스플랫폼 effective value 로드) 선행 후 별도 plan 작성.
- **9D 통합** — `validate_project_rules`에 `-CheckPluginDeps` 플래그로 합류.
- **M3 심화** — `EnabledByDefault` 엔진 플러그인까지 required 판정 확장(현재는 .uproject `Plugins[]` 기준). 필요 시 IPluginManager 또는 엔진 Plugins 디렉터리 스캔 추가.

---

## Self-Review 결과

- **스펙 커버리지:** required 누락/비활성(Task 4), forbiddenInShipping+Configuration(Task 5), Editor 모듈 정밀화/오탐 방지(Task 6), 정책 스키마(Task 2), 리포트/exit code 상속(Task 1·4), 래퍼/예시/README(Task 7) — 스펙 §3 전 항목 매핑됨. ConfigDiff(§4)는 범위 분리 명시.
- **플레이스홀더:** 없음(모든 코드 스텝에 실제 코드). Task 7 Step 3 .bat만 "기존 파일 복사+이름 변경" 지시 — 기존 산출물 재사용이라 허용.
- **타입 일관성:** `FCommandForgeReport`, `FCommandForgeValidationIssue`(Severity/Code/Message/Field/FilePath/SuggestedFix), `ECommandForgeExitCode`, `FCommandForgePolicyParser::ParseJson`, `FProjectDescriptor::Load`/`FPluginReferenceDescriptor.bEnabled`, `FPluginDescriptor::Load`/`FModuleDescriptor.Type`(`EHostType`) — 실제 코드베이스/엔진 시그니처와 일치.
- **알려진 가정:** Task 6에서 .uplugin 미발견 시 "보수적 runtime=true". 합성 .uproject 기반 테스트에서 Niagara 케이스(Task 5)가 유지되는 근거이기도 함.
