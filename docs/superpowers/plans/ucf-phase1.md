# UECommandForge Phase 1 — Commandlet 실행 하네스 구현 계획

> **에이전트 작업자:** REQUIRED SUB-SKILL: `superpowers:subagent-driven-development` (권장) 또는 `superpowers:executing-plans`로 태스크 단위 실행. 각 스텝은 체크박스(`- [ ]`) 구문으로 진행 상태를 추적한다.

**목표:** `UnrealEditor-Cmd`를 헤드리스로 부팅하여 `Hello` Commandlet을 실행하고, 계약 준수 Result JSON을 `Saved/CodexReports/`에 기록하는 완전한 실행 하네스를 구축한다.

**아키텍처:** Shell 래퍼(`tools/ue/`) → `UnrealEditor-Cmd` → `UHelloCommandlet` → `FJsonReportWriter::Write` → Result JSON. 런타임 모듈은 POD 타입만 보유; 에디터 모듈이 Commandlet·리포트 작성기·테스트를 담당한다.

**기술 스택:** Unreal Engine 5.7 (C++20), `UnrealEd`, `Json`, `JsonUtilities`. POSIX bash (macOS) + Git Bash (Windows).

---

## Phase 1 인수 조건

```bash
# 1. 플러그인 빌드 성공
./tools/ue/build_plugin.sh

# 2. 자동화 테스트 통과
./tools/ue/run_automation_tests.sh

# 3. Hello commandlet 엔드투엔드
./tools/ue/hello.sh

# 4. Result JSON 계약 확인
jq -e '.ok and .commandlet == "Hello"' "$(ls -t sample/Saved/CodexReports/Hello_*.json | head -1)"
```

---

## Phase 1 파일 구조

| 경로 | 역할 | 태스크 |
|---|---|---|
| `UECommandForgeSample.uproject` | 샘플 프로젝트 디스크립터; StateTree 플러그인 활성화 | 1 |
| `Config/DefaultEngine.ini` | 최소 엔진 설정 | 1 |
| `Config/DefaultGame.ini` | 최소 게임 설정 (플레이스홀더) | 1 |
| `.gitignore` | UE 빌드 아티팩트 제외 | 1 |
| `README.md` | LLM 대상 명령 표면 + Spec/Result JSON 계약 (스켈레톤) | 1 |
| `Plugins/UECommandForge/UECommandForge.uplugin` | 플러그인 디스크립터 (두 모듈) | 3 |
| `Plugins/UECommandForge/Source/UECommandForgeRuntime/UECommandForgeRuntime.Build.cs` | 런타임 모듈 빌드 규칙 | 3 |
| `Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/CommandForgeTypes.h` | 공유 POD 타입 (`FCommandForgeError`, `ECommandForgeExitCode`) | 3 |
| `Plugins/UECommandForge/Source/UECommandForgeRuntime/Private/CommandForgeTypes.cpp` | 모듈 구현 + IMPLEMENT_MODULE | 3 |
| `Plugins/UECommandForge/Source/UECommandForgeEditor/UECommandForgeEditor.Build.cs` | 에디터 모듈 빌드 규칙 | 4 |
| `Plugins/UECommandForge/Source/UECommandForgeEditor/Public/UECommandForgeEditorModule.h` | 에디터 모듈 헤더 | 4 |
| `Plugins/UECommandForge/Source/UECommandForgeEditor/Private/UECommandForgeEditorModule.cpp` | 에디터 모듈 IMPLEMENT_MODULE | 4 |
| `Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Reports/JsonReportWriter.h` | Result JSON 작성기 인터페이스 | 5 |
| `Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Reports/JsonReportWriter.cpp` | Result JSON 원자 쓰기 구현 | 5 |
| `Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/JsonReportWriterTest.cpp` | 리포트 작성기 자동화 테스트 | 5 |
| `Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/HelloCommandlet.h` | Hello commandlet 헤더 | 6 |
| `Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/HelloCommandlet.cpp` | Hello commandlet 구현 | 6 |
| `tools/ue/ue_env.sh` | OS 감지 + `UNREAL_EDITOR_CMD` 경로 결정 | 2 |
| `tools/ue/run_commandlet.sh` | 공통 래퍼: 프로젝트 경로·플래그·출력 경로 | 2 |
| `tools/ue/hello.sh` | `Hello` commandlet 호출 | 2 |
| `tools/ue/build_plugin.sh` | CI 게이트용 `RunUAT BuildPlugin` | 4 |
| `tools/ue/run_automation_tests.sh` | `UECommandForge` 자동화 테스트 실행 | 5 |

**경계 결정:**
- **런타임 모듈 = 데이터 전용.** `UnrealEd`/`AssetTools`를 건드리는 모든 코드는 에디터 모듈에 위치.
- **`JsonReportWriter`는 독립 인터페이스, Commandlet 기반 클래스 아님.** Commandlet은 얇게, Writer는 독립적으로 단위 테스트 가능.
- **래퍼는 멍청하다.** Spec JSON을 절대 파싱하지 않으며 파일 경로만 전달. 모든 로직은 C++에 위치.

---

## 태스크 1: 저장소 스캐폴딩 (uproject, gitignore, README 스켈레톤)

**파일:**
- 생성: `sample/UECommandForgeSample.uproject`
- 생성: `sample/Config/DefaultEngine.ini`
- 생성: `sample/Config/DefaultGame.ini`
- 생성: `.gitignore`
- 생성: `README.md`

- [ ] **스텝 1: `.gitignore` 작성**

```gitignore
# Unreal generated
Binaries/
Build/
DerivedDataCache/
Intermediate/
Saved/
*.VC.db
*.opensdf
*.sln
*.xcodeproj/
*.xcworkspace/

# OS
.DS_Store
Thumbs.db

# But keep CodexReports examples
!Saved/.gitkeep
```

- [ ] **스텝 2: `UECommandForgeSample.uproject` 작성**

```json
{
  "FileVersion": 3,
  "EngineAssociation": "5.7",
  "Category": "",
  "Description": "Sample project for UECommandForge LLM automation bridge.",
  "Modules": [],
  "Plugins": [
    { "Name": "UECommandForge", "Enabled": true },
    { "Name": "StateTree", "Enabled": true },
    { "Name": "GameplayStateTree", "Enabled": true }
  ]
}
```

- [ ] **스텝 3: `Config/DefaultEngine.ini` 작성 (최소)**

```ini
[/Script/Engine.Engine]
+ActiveGameNameRedirects=(OldGameName="TP_Blank",NewGameName="/Script/UECommandForgeSample")
```

- [ ] **스텝 4: `Config/DefaultGame.ini` 작성 (빈 플레이스홀더)**

```ini
[/Script/EngineSettings.GeneralProjectSettings]
ProjectID=00000000000000000000000000000000
ProjectName=UECommandForgeSample
```

- [ ] **스텝 5: `README.md` 스켈레톤 작성**

```markdown
# UECommandForge

LLM 기반 Unreal Engine 자동화 브리지. 전체 설계는 `ue_commandlet_based_llm_automation_plan.md` 참조.

## Codex 명령 표면 (Phase 1)

| 명령 | 목적 |
|---|---|
| `tools/ue/hello.sh` | 헬스체크 — `Hello` commandlet 실행 후 Result JSON 기록. |

이후 Phase가 추가될수록 명령이 늘어난다. LLM은 이 표 외의 명령을 호출해서는 안 된다.

## 환경 요구 사항

- Unreal Engine 5.7 설치 필요.
- 기본 경로가 아닌 경우 `UE_ROOT` 환경변수 설정.
- macOS 기본값: `/Users/Shared/Epic Games/UE_5.7`
- Windows 기본값 (Git Bash): `/c/Program Files/Epic Games/UE_5.7`

## Result JSON

모든 commandlet은 `Saved/CodexReports/<Commandlet>_<UTC>.json`에 기록한다. 종료 코드:

| 코드 | 의미 |
|---|---|
| 0 | OK |
| 2 | Spec 파싱 실패 |
| 3 | 빌드/컴파일 실패 |
| 4 | 검증 실패 |
| 5 | UE/엔진 오류 |
```

- [ ] **스텝 6: 커밋 준비 — 사용자에게 아래 내용을 보여주고 승인 요청**

```
커밋 메시지: feat: scaffold UECommandForgeSample uproject and Codex contract README
대상 파일: UECommandForgeSample.uproject Config/ .gitignore README.md
```

승인 후 실행:
```bash
git add UECommandForgeSample.uproject Config/ .gitignore README.md
git commit -m "feat: scaffold UECommandForgeSample uproject and Codex contract README"
```

---

## 태스크 2: Shell 래퍼 기반 (`ue_env.sh`, `run_commandlet.sh`, `hello.sh`)

**파일:**
- 생성: `tools/ue/ue_env.sh`
- 생성: `tools/ue/run_commandlet.sh`
- 생성: `tools/ue/hello.sh`

모든 래퍼는 순수 POSIX bash (zsh 전용 기능 없음)로 macOS와 Windows Git Bash 양쪽에서 동일하게 실행된다.

- [ ] **스텝 1: `tools/ue/ue_env.sh` 작성**

```bash
#!/usr/bin/env bash
# 현재 OS에 맞는 UNREAL_EDITOR_CMD와 PROJECT_FILE을 결정한다.
# 이 파일은 source로 불러올 것. 직접 실행 금지.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PROJECT_FILE="${REPO_ROOT}/sample/UECommandForgeSample.uproject"

resolve_ue_cmd() {
  if [ -n "${UNREAL_EDITOR_CMD:-}" ]; then
    return 0
  fi

  local ue_root="${UE_ROOT:-}"
  local platform
  case "$(uname -s)" in
    Darwin)
      ue_root="${ue_root:-/Users/Shared/Epic Games/UE_5.7}"
      UNREAL_EDITOR_CMD="${ue_root}/Engine/Binaries/Mac/UnrealEditor-Cmd"
      platform="Mac"
      ;;
    MINGW*|MSYS*|CYGWIN*)
      ue_root="${ue_root:-/c/Program Files/Epic Games/UE_5.7}"
      UNREAL_EDITOR_CMD="${ue_root}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      platform="Win64"
      ;;
    Linux)
      ue_root="${ue_root:-/opt/UnrealEngine/UE_5.7}"
      UNREAL_EDITOR_CMD="${ue_root}/Engine/Binaries/Linux/UnrealEditor-Cmd"
      platform="Linux"
      ;;
    *)
      echo "[ue_env] 지원하지 않는 OS: $(uname -s)" >&2
      return 5
      ;;
  esac

  if [ ! -x "${UNREAL_EDITOR_CMD}" ]; then
    echo "[ue_env] UnrealEditor-Cmd를 찾을 수 없습니다: ${UNREAL_EDITOR_CMD}" >&2
    echo "[ue_env] UE_ROOT 또는 UNREAL_EDITOR_CMD 환경변수로 재정의하세요." >&2
    return 5
  fi

  export UNREAL_EDITOR_CMD PROJECT_FILE
  export UE_PLATFORM="${platform}"
}

resolve_ue_cmd
```

- [ ] **스텝 2: `tools/ue/run_commandlet.sh` 작성**

```bash
#!/usr/bin/env bash
# 사용법: run_commandlet.sh <CommandletName> [extra-args...]
# Result JSON은 항상 Saved/CodexReports/<Commandlet>_<UTC>.json에 기록된다.

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/ue_env.sh"

if [ $# -lt 1 ]; then
  echo "사용법: $0 <CommandletName> [extra args...]" >&2
  exit 2
fi

COMMANDLET="$1"
shift

REPORT_DIR="${REPO_ROOT}/sample/Saved/CodexReports"
mkdir -p "${REPORT_DIR}"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
OUTPUT_JSON="${REPORT_DIR}/${COMMANDLET}_${STAMP}.json"

"${UNREAL_EDITOR_CMD}" "${PROJECT_FILE}" \
  -run="${COMMANDLET}" \
  -Output="${OUTPUT_JSON}" \
  -unattended -nop4 -nosplash -log -stdout -FullStdOutLogOutput \
  "$@"

UE_EXIT=$?

if [ ! -f "${OUTPUT_JSON}" ]; then
  echo "[run_commandlet] Result JSON이 없습니다: ${OUTPUT_JSON}" >&2
  exit 5
fi

echo "${OUTPUT_JSON}"
exit ${UE_EXIT}
```

- [ ] **스텝 3: `tools/ue/hello.sh` 작성**

```bash
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/run_commandlet.sh" Hello "$@"
```

- [ ] **스텝 4: 스크립트 실행 권한 부여**

```bash
chmod +x tools/ue/ue_env.sh tools/ue/run_commandlet.sh tools/ue/hello.sh
```

- [ ] **스텝 5: 스모크 테스트 (실패 예상 — Commandlet 미존재)**

```bash
./tools/ue/hello.sh || echo "예상된 실패 exit=$?"
```

예상 결과: 0이 아닌 종료 코드. 이 단계는 래퍼가 `UnrealEditor-Cmd`를 부팅한다는 것을 증명한다. UE 실행 전에 실패한다면 (`UnrealEditor-Cmd not found` 등) `UE_ROOT`를 먼저 수정한다.

- [ ] **스텝 6: 커밋 준비 — 사용자에게 아래 내용을 보여주고 승인 요청**

```
커밋 메시지: feat: cross-platform Shell wrappers for UnrealEditor-Cmd
대상 파일: tools/ue/ue_env.sh tools/ue/run_commandlet.sh tools/ue/hello.sh
```

승인 후 실행:
```bash
git add tools/ue/ue_env.sh tools/ue/run_commandlet.sh tools/ue/hello.sh
git commit -m "feat: cross-platform Shell wrappers for UnrealEditor-Cmd"
```

---

## 태스크 3: 플러그인 디스크립터 및 런타임 모듈

**파일:**
- 생성: `Plugins/UECommandForge/UECommandForge.uplugin`
- 생성: `Plugins/UECommandForge/Source/UECommandForgeRuntime/UECommandForgeRuntime.Build.cs`
- 생성: `Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/CommandForgeTypes.h`
- 생성: `Plugins/UECommandForge/Source/UECommandForgeRuntime/Private/CommandForgeTypes.cpp`

- [ ] **스텝 1: `UECommandForge.uplugin` 작성**

```json
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "0.1.0",
  "FriendlyName": "UECommandForge",
  "Description": "LLM-driven Unreal automation bridge: Commandlets + Builders + Validators.",
  "Category": "Editor",
  "CreatedBy": "UECommandForge",
  "CreatedByURL": "",
  "DocsURL": "",
  "MarketplaceURL": "",
  "SupportURL": "",
  "EnabledByDefault": true,
  "CanContainContent": false,
  "IsBetaVersion": true,
  "Installed": false,
  "Modules": [
    {
      "Name": "UECommandForgeRuntime",
      "Type": "Runtime",
      "LoadingPhase": "Default"
    },
    {
      "Name": "UECommandForgeEditor",
      "Type": "Editor",
      "LoadingPhase": "PostEngineInit"
    }
  ]
}
```

- [ ] **스텝 2: `UECommandForgeRuntime.Build.cs` 작성**

```csharp
using UnrealBuildTool;

public class UECommandForgeRuntime : ModuleRules
{
    public UECommandForgeRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
```

- [ ] **스텝 3: `CommandForgeTypes.h` 작성**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.generated.h"

UENUM()
enum class ECommandForgeExitCode : uint8
{
    Ok = 0,
    SpecParseFailed = 2,
    BuildFailed = 3,
    ValidationFailed = 4,
    EngineError = 5
};

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeError
{
    GENERATED_BODY()

    UPROPERTY() FString Code;
    UPROPERTY() FString Message;
    UPROPERTY() FString Field;
};
```

- [ ] **스텝 4: `CommandForgeTypes.cpp` 작성**

```cpp
#include "CommandForgeTypes.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, UECommandForgeRuntime);
```

- [ ] **스텝 5: 커밋 준비 — 사용자에게 아래 내용을 보여주고 승인 요청**

```
커밋 메시지: feat: UECommandForge plugin scaffold + Runtime types
대상 파일: Plugins/UECommandForge/UECommandForge.uplugin
          Plugins/UECommandForge/Source/UECommandForgeRuntime/
```

승인 후 실행:
```bash
git add Plugins/UECommandForge/UECommandForge.uplugin \
        Plugins/UECommandForge/Source/UECommandForgeRuntime
git commit -m "feat: UECommandForge plugin scaffold + Runtime types"
```

---

## 태스크 4: 에디터 모듈 스켈레톤 + `build_plugin.sh`

**파일:**
- 생성: `Plugins/UECommandForge/Source/UECommandForgeEditor/UECommandForgeEditor.Build.cs`
- 생성: `Plugins/UECommandForge/Source/UECommandForgeEditor/Public/UECommandForgeEditorModule.h`
- 생성: `Plugins/UECommandForge/Source/UECommandForgeEditor/Private/UECommandForgeEditorModule.cpp`
- 생성: `tools/ue/build_plugin.sh`

- [ ] **스텝 1: `UECommandForgeEditor.Build.cs` 작성**

```csharp
using UnrealBuildTool;

public class UECommandForgeEditor : ModuleRules
{
    public UECommandForgeEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine",
            "UECommandForgeRuntime"
        });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "UnrealEd", "Json", "JsonUtilities",
            "AssetTools", "AssetRegistry",
            "BlueprintGraph", "KismetCompiler", "Kismet",
            "EditorScriptingUtilities",
            "AIModule"
            // StateTree 모듈은 Phase 4에서 추가
        });
    }
}
```

- [ ] **스텝 2: `UECommandForgeEditorModule.h` 작성**

```cpp
#pragma once

#include "Modules/ModuleInterface.h"

class FUECommandForgeEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};
```

- [ ] **스텝 3: `UECommandForgeEditorModule.cpp` 작성**

```cpp
#include "UECommandForgeEditorModule.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FUECommandForgeEditorModule, UECommandForgeEditor);
```

- [ ] **스텝 4: `tools/ue/build_plugin.sh` 작성**

```bash
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/ue_env.sh"

ENGINE_DIR="$(dirname "$(dirname "$(dirname "${UNREAL_EDITOR_CMD}")")")"
RUN_UAT="${ENGINE_DIR}/Build/BatchFiles/RunUAT.sh"
case "${UE_PLATFORM}" in
  Win64) RUN_UAT="${ENGINE_DIR}/Build/BatchFiles/RunUAT.bat" ;;
esac

PLUGIN="${REPO_ROOT}/Plugins/UECommandForge/UECommandForge.uplugin"
OUT="${REPO_ROOT}/Saved/PluginBuild"
mkdir -p "${OUT}"

exec "${RUN_UAT}" BuildPlugin \
  -Plugin="${PLUGIN}" \
  -Package="${OUT}" \
  -TargetPlatforms="${UE_PLATFORM}" \
  -Rocket
```

- [ ] **스텝 5: 스크립트 실행 권한 부여**

```bash
chmod +x tools/ue/build_plugin.sh
```

- [ ] **스텝 6: 빌드로 스캐폴딩 컴파일 검증**

```bash
./tools/ue/build_plugin.sh
```

예상 결과: BuildPlugin 성공 (아직 Commandlet 없음, 두 모듈이 깨끗하게 링크). 실패 시 모듈 의존성을 수정한 뒤 진행.

- [ ] **스텝 7: 커밋 준비 — 사용자에게 아래 내용을 보여주고 승인 요청**

```
커밋 메시지: feat: Editor module skeleton + RunUAT BuildPlugin wrapper
대상 파일: Plugins/UECommandForge/Source/UECommandForgeEditor/
          tools/ue/build_plugin.sh
```

승인 후 실행:
```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor tools/ue/build_plugin.sh
git commit -m "feat: Editor module skeleton + RunUAT BuildPlugin wrapper"
```

---

## 태스크 5: JsonReportWriter (TDD — 테스트 먼저)

**파일:**
- 생성: `Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Reports/JsonReportWriter.h`
- 생성: `Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Reports/JsonReportWriter.cpp`
- 생성: `Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/JsonReportWriterTest.cpp`
- 생성: `tools/ue/run_automation_tests.sh`

Result JSON 봉투 형식은 `ue_commandlet_based_llm_automation_plan.md` §11에 정의되어 있다.

- [ ] **스텝 1: 실패 자동화 테스트 작성 (RED)**

```cpp
// Tests/JsonReportWriterTest.cpp
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUECommandForgeJsonReportWriterTest,
    "UECommandForge.Reports.JsonReportWriter.WritesOkEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUECommandForgeJsonReportWriterTest::RunTest(const FString& Parameters)
{
    const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_writer.json"));
    IFileManager::Get().Delete(*OutPath);

    FCommandForgeReport Report;
    Report.bOk = true;
    Report.Commandlet = TEXT("Hello");

    UECommandForge::FJsonReportWriter::Write(OutPath, Report);

    FString Content;
    TestTrue(TEXT("파일 존재"), FFileHelper::LoadFileToString(Content, *OutPath));
    TestTrue(TEXT("ok=true 포함"), Content.Contains(TEXT("\"ok\": true")));
    TestTrue(TEXT("commandlet=Hello 포함"),
        Content.Contains(TEXT("\"commandlet\": \"Hello\"")));
    return true;
}
```

- [ ] **스텝 2: `JsonReportWriter.h` 작성**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"

struct FCommandForgeReport
{
    bool bOk = false;
    FString Commandlet;
    TArray<FString> CreatedAssets;
    TArray<FString> ModifiedAssets;
    TMap<FString, FString> Validation;
    TArray<FCommandForgeError> Errors;
    TArray<FString> NextSuggestions;
};

namespace UECommandForge
{
    class FJsonReportWriter
    {
    public:
        static bool Write(const FString& OutPath, const FCommandForgeReport& Report);
    };
}
```

- [ ] **스텝 3: `JsonReportWriter.cpp` 작성**

```cpp
#include "Reports/JsonReportWriter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace UECommandForge
{
    bool FJsonReportWriter::Write(const FString& OutPath, const FCommandForgeReport& Report)
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetBoolField(TEXT("ok"), Report.bOk);
        Root->SetStringField(TEXT("commandlet"), Report.Commandlet);

        auto ToArray = [](const TArray<FString>& In)
        {
            TArray<TSharedPtr<FJsonValue>> Out;
            for (const FString& S : In) { Out.Add(MakeShared<FJsonValueString>(S)); }
            return Out;
        };
        Root->SetArrayField(TEXT("created_assets"),  ToArray(Report.CreatedAssets));
        Root->SetArrayField(TEXT("modified_assets"), ToArray(Report.ModifiedAssets));
        Root->SetArrayField(TEXT("next_suggestions"), ToArray(Report.NextSuggestions));

        TSharedRef<FJsonObject> Validation = MakeShared<FJsonObject>();
        for (const auto& Kv : Report.Validation)
        {
            Validation->SetStringField(Kv.Key, Kv.Value);
        }
        Root->SetObjectField(TEXT("validation"), Validation);

        TArray<TSharedPtr<FJsonValue>> ErrorValues;
        for (const FCommandForgeError& E : Report.Errors)
        {
            TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("code"),    E.Code);
            O->SetStringField(TEXT("message"), E.Message);
            O->SetStringField(TEXT("field"),   E.Field);
            ErrorValues.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("errors"), ErrorValues);

        FString Serialized;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(Root, Writer);

        const FString Dir = FPaths::GetPath(OutPath);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
        return FFileHelper::SaveStringToFile(Serialized, *OutPath);
    }
}
```

- [ ] **스텝 4: `tools/ue/run_automation_tests.sh` 작성**

```bash
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/ue_env.sh"

REPORT_DIR="${REPO_ROOT}/sample/Saved/AutomationReports"
mkdir -p "${REPORT_DIR}"

"${UNREAL_EDITOR_CMD}" "${PROJECT_FILE}" \
  -unattended -nop4 -nosplash -nullrhi \
  -ExecCmds="Automation RunTests UECommandForge; Quit" \
  -ReportOutputPath="${REPORT_DIR}" \
  -log -stdout -FullStdOutLogOutput
```

- [ ] **스텝 5: 실행 권한 부여 및 테스트 실행 (GREEN)**

```bash
chmod +x tools/ue/run_automation_tests.sh
./tools/ue/run_automation_tests.sh
```

예상 결과: `FUECommandForgeJsonReportWriterTest` 통과. JSON 직렬화 형식(공백·들여쓰기)이 맞지 않으면 Writer가 아닌 테스트의 assertion 문자열을 UE 5.7 실제 출력에 맞춰 조정한다.

- [ ] **스텝 6: 커밋 준비 — 사용자에게 아래 내용을 보여주고 승인 요청**

```
커밋 메시지: feat: JsonReportWriter with automation test
대상 파일: Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Reports/
          Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Reports/
          Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/
          tools/ue/run_automation_tests.sh
```

승인 후 실행:
```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Reports \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Reports \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests \
        tools/ue/run_automation_tests.sh
git commit -m "feat: JsonReportWriter with automation test"
```

---

## 태스크 6: HelloCommandlet — 엔드투엔드 스모크

**파일:**
- 생성: `Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/HelloCommandlet.h`
- 생성: `Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/HelloCommandlet.cpp`

- [ ] **스텝 1: `HelloCommandlet.h` 작성**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "HelloCommandlet.generated.h"

UCLASS()
class UHelloCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UHelloCommandlet();
    virtual int32 Main(const FString& Params) override;
};
```

- [ ] **스텝 2: `HelloCommandlet.cpp` 작성**

```cpp
#include "HelloCommandlet.h"
#include "Reports/JsonReportWriter.h"
#include "Misc/CommandLine.h"

UHelloCommandlet::UHelloCommandlet()
{
    IsClient   = false;
    IsServer   = false;
    IsEditor   = true;
    LogToConsole = true;
}

int32 UHelloCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens; TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    if (OutPath.IsEmpty())
    {
        UE_LOG(LogInit, Error, TEXT("Hello: -Output=<path> 인자가 없습니다"));
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgeReport Report;
    Report.bOk = true;
    Report.Commandlet = TEXT("Hello");
    Report.Validation.Add(TEXT("engine_boot"), TEXT("ok"));

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    return bWrote ? 0 : static_cast<int32>(ECommandForgeExitCode::EngineError);
}
```

- [ ] **스텝 3: 플러그인 재빌드**

```bash
./tools/ue/build_plugin.sh
```

예상 결과: 성공.

- [ ] **스텝 4: 엔드투엔드 스모크 실행**

```bash
./tools/ue/hello.sh
```

예상 결과: `Saved/CodexReports/Hello_<UTC>.json` 경로를 출력하고 종료 코드 0으로 종료.

- [ ] **스텝 5: Result JSON 계약 검증**

```bash
jq -e '.ok == true and .commandlet == "Hello" and .validation.engine_boot == "ok"' \
  "$(ls -t sample/Saved/CodexReports/Hello_*.json | head -1)"
```

예상 결과: `true` 출력 후 종료 코드 0.

- [ ] **스텝 6: 커밋 준비 — 사용자에게 아래 내용을 보여주고 승인 요청**

```
커밋 메시지: feat: HelloCommandlet — end-to-end Result JSON smoke
대상 파일: Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/HelloCommandlet.h
          Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/HelloCommandlet.cpp
```

승인 후 실행:
```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/HelloCommandlet.*
git commit -m "feat: HelloCommandlet — end-to-end Result JSON smoke"
```

---

## Phase 1 인수 확인

모든 4개 체크를 통과해야 Phase 2로 진행 가능하다.

```bash
# 1. 플러그인 빌드 성공
./tools/ue/build_plugin.sh

# 2. 자동화 테스트 통과
./tools/ue/run_automation_tests.sh

# 3. Hello commandlet 엔드투엔드
./tools/ue/hello.sh

# 4. Result JSON 계약 확인
jq -e '.ok and .commandlet == "Hello"' "$(ls -t sample/Saved/CodexReports/Hello_*.json | head -1)"
```

**Phase 1 인도물:** 이후 모든 Phase는 "동작하는 Commandlet이 있어서 계약 준수 Result JSON을 기록할 수 있다; 내가 할 일은 그것을 확장하는 것뿐이다"라는 전제 아래 시작할 수 있다.
