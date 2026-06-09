# UECommandForge 실행 속도 개선 구현 계획

> **에이전트 작업자 필수 지침:** 이 계획을 구현할 때는 `superpowers:subagent-driven-development` 또는 `superpowers:executing-plans`를 사용해 작업 단위로 진행한다. 각 단계는 체크박스(`- [ ]`)로 추적한다.

**목표:** Unreal Python 금지 원칙을 유지하면서 `UnrealEditor-Cmd.exe` 실행 횟수를 줄여 UECommandForge 자동화의 전체 실행 시간을 단축한다.

**아키텍처:** Unreal 접근은 계속 UECommandForge C++ commandlet과 `tools/ue/` wrapper 안에서만 수행한다. Blueprint 생성은 기존 `CreateBlueprintBatch`를 기본 fast path로 사용하고, C++ 클래스 여러 개 생성에는 새 `GenerateCppClassBatch` commandlet을 추가한다. 작업자는 단건 wrapper보다 batch wrapper를 먼저 검토하도록 운영 규칙을 갱신한다.

**기술 스택:** Unreal Engine 5.7, UECommandForge Editor commandlet, C++ automation test, Windows `.bat` wrapper, Git Bash smoke script, `Saved/CodexReports` Result JSON.

---

## 범위

이 계획은 실행 속도 개선만 다룬다. 다음 방식은 계속 금지한다.

- `import unreal`
- `-ExecutePythonScript`
- PythonScriptPlugin
- Editor Utility Python
- 임시 `.py` 자동화
- 실패한 commandlet을 우회하는 외부 launcher

새 C++ reflection 클래스와 Blueprint를 함께 다룰 때는 기존 규칙을 유지한다.

1. C++ 파일을 생성한다.
2. UHT/build 또는 reflection 검증을 수행한다.
3. 새 Unreal 프로세스에서 Blueprint를 생성한다.
4. `Saved/CodexReports`의 최신 Result JSON을 확인한다.

## 현재 문제

최근 `AWeapon` / `BP_Weapon` 생성은 다음 commandlet을 각각 별도 Unreal 프로세스로 실행했다.

- `GenerateCppClass`
- `ValidateCppReflection`
- `CreateBlueprint`
- `CompileBlueprints`

실제 commandlet 처리 시간은 짧았지만, 매번 Unreal 프로세스를 새로 띄우는 비용이 컸다. 설치된 bench 스크립트는 이미 이 문제를 측정하고 batch 방식의 이점을 보여준다.

- `C:\Users\Sam\.codex\UECommandForge\tools\ue\bench\baseline_npc_setup.sh`
- `C:\Users\Sam\.codex\UECommandForge\tools\ue\bench\batch_npc_setup.sh`
- `C:\Users\Sam\.codex\UECommandForge\tools\ue\bench\batch_only_setup.sh`
- `C:\Users\Sam\.codex\UECommandForge\tools\ue\bench\blueprint_only_setup.sh`

단, bench 스크립트는 성능 측정 fixture다. 실제 프로젝트 asset 생성에 직접 사용하지 않고, 그 안의 batch 패턴만 운영 workflow에 반영한다.

## 파일 구조

- 수정: `C:\Workspaces\projects\factory-space\frontend\AGENTS.md`
  - batch wrapper 우선 사용, 중복 `CompileBlueprints` 생략 규칙 추가
- 수정: `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge\Source\UECommandForgeEditor\Private\Commandlets\GenerateCppClassCommandlet.cpp`
  - 단일 C++ class 생성 로직을 batch commandlet에서 재사용할 수 있게 분리
- 생성: `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge\Source\UECommandForgeEditor\Private\Commandlets\GenerateCppClassBatchCommandlet.h`
  - batch C++ 생성 commandlet 선언
- 생성: `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge\Source\UECommandForgeEditor\Private\Commandlets\GenerateCppClassBatchCommandlet.cpp`
  - 여러 C++ class spec을 한 Unreal 프로세스에서 처리
- 생성: `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge\Source\UECommandForgeEditor\Public\Specs\CppClassBatchSpec.h`
  - `cpp_class_batch` spec 구조 정의
- 생성: `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge\Source\UECommandForgeEditor\Public\Specs\CppClassBatchSpecParser.h`
  - batch spec parser 인터페이스
- 생성: `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge\Source\UECommandForgeEditor\Private\Specs\CppClassBatchSpecParser.cpp`
  - batch spec JSON parsing 및 검증
- 생성: `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge\Source\UECommandForgeEditor\Tests\CppClassBatchSpecParserTest.cpp`
  - valid/invalid batch spec parser test
- 생성: `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge\Source\UECommandForgeEditor\Tests\GenerateCppClassBatchCommandletTest.cpp`
  - batch commandlet dry-run/apply/failure behavior test
- 생성: `C:\Workspaces\projects\factory-space\frontend\tools\ue\generate_cpp_class_batch.bat`
  - Windows wrapper
- 생성: `C:\Workspaces\projects\factory-space\frontend\tools\ue\generate_cpp_class_batch.sh`
  - Git Bash/macOS/Linux wrapper
- 생성: `C:\Workspaces\projects\factory-space\frontend\tools\test\smoke\cpp_generation_batch.sh`
  - 여러 C++ class 생성을 Unreal 기동 1회로 수행하는 smoke test

## 작업 1: 즉시 적용할 운영 규칙 추가

**파일:**
- 수정: `C:\Workspaces\projects\factory-space\frontend\AGENTS.md`

- [ ] **1단계: UECommandForge 실행 속도 규칙 추가**

Unreal 자동화 규칙 아래에 다음 섹션을 추가한다.

```markdown
### UECommandForge 실행 속도 규칙

- 반복 생성 작업은 단건 wrapper보다 batch wrapper를 우선 사용한다.
  - Blueprint 여러 개: `tools/ue/create_blueprint_batch.*`
  - Blueprint 생성과 기본 컴포넌트/기본값 적용: batch spec의 `components`와 defaults 기능을 우선 검토한다.
- `CreateBlueprint` 또는 `CreateBlueprintBatch` Result JSON에 `compile_status: ok`, `asset_on_disk: true`, `asset_in_registry: true`가 있으면 같은 턴에서 `CompileBlueprints`를 반복 실행하지 않는다.
- C++ 새 클래스 생성 후 Blueprint 생성은 반드시 UHT/build 또는 reflection 검증 뒤 새 Unreal 프로세스에서 실행한다.
- Result 확인은 전체 로그보다 최신 `Saved/CodexReports/*.json`의 `ok`, `errors`, `issues`, `validation`을 우선한다.
```

- [ ] **2단계: 문서 diff 확인**

실행:

```powershell
git diff -- AGENTS.md
```

예상 결과: 속도 규칙 섹션만 추가된다.

## 작업 2: 현재 bench fixture를 운영 기준으로 문서화

**파일:**
- 생성: `C:\Workspaces\projects\factory-space\frontend\tools\test\smoke\uecommandforge_speed_baseline.sh`

- [ ] **1단계: bench fixture 존재 확인 smoke script 작성**

```bash
#!/usr/bin/env bash
set -euo pipefail

CODEX_UECF="${CODEX_HOME:-$HOME/.codex}/UECommandForge"

echo "[speed-baseline] installed tools: ${CODEX_UECF}/tools/ue"
test -f "${CODEX_UECF}/tools/ue/bench/baseline_npc_setup.sh"
test -f "${CODEX_UECF}/tools/ue/bench/batch_npc_setup.sh"

echo "[speed-baseline] baseline fixture exists"
echo "[speed-baseline] batch fixture exists"
echo "[speed-baseline] bench scripts are timing fixtures, not production asset generators"
```

- [ ] **2단계: smoke script 실행**

실행:

```powershell
bash tools/test/smoke/uecommandforge_speed_baseline.sh
```

예상 결과:

```text
[speed-baseline] baseline fixture exists
[speed-baseline] batch fixture exists
```

## 작업 3: `cpp_class_batch` spec parser 추가

**파일:**
- 생성: `Plugins\UECommandForge\Source\UECommandForgeEditor\Public\Specs\CppClassBatchSpec.h`
- 생성: `Plugins\UECommandForge\Source\UECommandForgeEditor\Public\Specs\CppClassBatchSpecParser.h`
- 생성: `Plugins\UECommandForge\Source\UECommandForgeEditor\Private\Specs\CppClassBatchSpecParser.cpp`
- 생성: `Plugins\UECommandForge\Source\UECommandForgeEditor\Tests\CppClassBatchSpecParserTest.cpp`

- [ ] **1단계: 실패하는 parser test 먼저 작성**

`CppClassBatchSpecParserTest.cpp`는 두 개 class를 가진 valid spec과 빈 `classes` 배열을 가진 invalid spec을 검증한다.

핵심 assertion:

```cpp
TestTrue(TEXT("valid batch spec parses"), UECommandForge::FCppClassBatchSpecParser::Parse(ValidJson, Spec, Errors));
TestEqual(TEXT("class count"), Spec.Classes.Num(), 2);
TestFalse(TEXT("empty classes rejected"), UECommandForge::FCppClassBatchSpecParser::Parse(InvalidJson, InvalidSpec, InvalidErrors));
```

- [ ] **2단계: RED 확인**

실행:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' Wanted_FactoryEditor Win64 Development -Project='C:\Workspaces\projects\factory-space\frontend\Wanted_Factory.uproject' -WaitMutex -NoHotReload
```

예상 결과: `Specs/CppClassBatchSpecParser.h`가 없어서 빌드 실패.

- [ ] **3단계: batch spec struct 구현**

`CppClassBatchSpec.h`는 다음 필드를 가진다.

```cpp
USTRUCT()
struct UECOMMANDFORGEEDITOR_API FCppClassBatchSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Version;
    UPROPERTY() FString Kind;
    UPROPERTY() FString TransactionId;
    UPROPERTY() TArray<FCommandForgeCppClassInfoSpec> Classes;
    UPROPERTY() TArray<FString> Includes;
    UPROPERTY() TArray<FCommandForgeCppPropertySpec> Properties;
    UPROPERTY() TArray<FCommandForgeCppFunctionSpec> Functions;
    UPROPERTY() FCommandForgeCppBuildDependencySpec BuildDependencies;
};
```

- [ ] **4단계: parser 구현**

parser는 다음을 검증한다.

- `kind == "cpp_class_batch"`
- `classes` 배열이 최소 1개 이상
- 각 class 항목이 object
- `name`, `type`, `module`, `output_path`, `base_class`, `blueprint_type`, `tick` parsing
- `includes` 배열 parsing

- [ ] **5단계: GREEN 확인**

실행:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' Wanted_FactoryEditor Win64 Development -Project='C:\Workspaces\projects\factory-space\frontend\Wanted_Factory.uproject' -WaitMutex -NoHotReload
```

예상 결과: 빌드 성공.

## 작업 4: `GenerateCppClassBatch` commandlet 추가

**파일:**
- 생성: `Plugins\UECommandForge\Source\UECommandForgeEditor\Private\Commandlets\GenerateCppClassBatchCommandlet.h`
- 생성: `Plugins\UECommandForge\Source\UECommandForgeEditor\Private\Commandlets\GenerateCppClassBatchCommandlet.cpp`
- 수정: `Plugins\UECommandForge\Source\UECommandForgeEditor\Private\Commandlets\GenerateCppClassCommandlet.cpp`
- 생성: `Plugins\UECommandForge\Source\UECommandForgeEditor\Tests\GenerateCppClassBatchCommandletTest.cpp`

- [ ] **1단계: 실패하는 commandlet test 먼저 작성**

테스트는 commandlet 한 번으로 두 class 파일을 생성하고 Result JSON의 `changed_files`가 4개인지 확인한다.

핵심 assertion:

```cpp
const int32 ExitCode = Commandlet->Main(FString::Printf(TEXT("-Spec=\"%s\" -Output=\"%s\" -Apply"), *SpecPath, *ReportPath));
TestEqual(TEXT("batch exit code"), ExitCode, 0);
TestTrue(TEXT("report ok"), Root->GetBoolField(TEXT("ok")));
TestEqual(TEXT("changed files count"), Root->GetArrayField(TEXT("changed_files")).Num(), 4);
```

- [ ] **2단계: RED 확인**

실행:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' Wanted_FactoryEditor Win64 Development -Project='C:\Workspaces\projects\factory-space\frontend\Wanted_Factory.uproject' -WaitMutex -NoHotReload
```

예상 결과: `GenerateCppClassBatchCommandlet.h`가 없어서 빌드 실패.

- [ ] **3단계: 단일 class 생성 로직 추출**

`GenerateCppClassCommandlet.cpp`에서 다음 로직을 helper로 분리한다.

- class symbol 결정
- include/header/source preview 생성
- module root 검증
- Build.cs dependency 검증
- `-Apply` 시 파일 쓰기
- Result JSON validation/changed_files 기록

기존 `GenerateCppClass` Result JSON 형식은 유지한다.

- [ ] **4단계: batch commandlet 구현**

`GenerateCppClassBatchCommandlet`의 동작:

- `-Spec`, `-Output`, `-Apply`, `-ApplyBuildCs=true` parsing
- `cpp_class_batch` spec 읽기
- 각 class entry를 `FCommandForgeCppClassSpec`로 변환
- 추출한 단일 class 생성 helper를 loop에서 호출
- 첫 실패 시 class index를 `issues[].field`에 기록
- 하나의 Result JSON에 `commandlet: "GenerateCppClassBatch"` 기록

- [ ] **5단계: GREEN 확인**

실행:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' Wanted_FactoryEditor Win64 Development -Project='C:\Workspaces\projects\factory-space\frontend\Wanted_Factory.uproject' -WaitMutex -NoHotReload
```

예상 결과: 빌드 성공.

## 작업 5: batch wrapper 추가

**파일:**
- 생성: `tools\ue\generate_cpp_class_batch.bat`
- 생성: `tools\ue\generate_cpp_class_batch.sh`

- [ ] **1단계: Windows wrapper 작성**

`generate_cpp_class_batch.bat`는 다음 commandlet을 호출한다.

```bat
call "%SCRIPT_DIR%run_commandlet.bat" GenerateCppClassBatch -Spec="%SPEC_PATH%" %EXTRA_ARGS%
```

- [ ] **2단계: shell wrapper 작성**

`generate_cpp_class_batch.sh`는 다음 commandlet을 호출한다.

```bash
"${SCRIPT_DIR}/run_commandlet.sh" GenerateCppClassBatch -Spec="${SPEC_PATH}" "$@"
```

- [ ] **3단계: wrapper usage 확인**

실행:

```powershell
& .\tools\ue\generate_cpp_class_batch.bat
```

예상 결과: exit code `2`, usage 출력.

## 작업 6: 빠른 workflow smoke test 추가

**파일:**
- 생성: `tools\test\smoke\cpp_generation_batch.sh`

- [ ] **1단계: smoke test 작성**

smoke test는 `Saved/CodexReports/smoke_cpp_class_batch.json`에 spec을 만들고 `generate_cpp_class_batch.sh -Apply`를 실행한다.

검증 항목:

- 최신 `GenerateCppClassBatch_*.json` 존재
- `"ok": true`
- `SmokeBatchActorA.h` 기록
- `SmokeBatchActorB.h` 기록

- [ ] **2단계: smoke test 실행**

실행:

```powershell
bash tools/test/smoke/cpp_generation_batch.sh
```

예상 결과:

```text
[cpp-generation-batch] ok: ...
```

## 작업 7: end-to-end 속도 비교

**파일:**
- 사용: `C:\Users\Sam\.codex\UECommandForge\tools\ue\bench\*.sh`
- 사용: `tools\ue\generate_cpp_class_batch.*`

- [ ] **1단계: 기존 방식 측정**

두 class를 `generate_cpp_class.bat` 두 번으로 생성한다.

예상 결과: Unreal commandlet launch 2회.

- [ ] **2단계: batch 방식 측정**

두 class를 `generate_cpp_class_batch.bat` 한 번으로 생성한다.

예상 결과: Unreal commandlet launch 1회.

- [ ] **3단계: 빌드 검증**

실행:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' Wanted_FactoryEditor Win64 Development -Project='C:\Workspaces\projects\factory-space\frontend\Wanted_Factory.uproject' -WaitMutex -NoHotReload
```

예상 결과: UHT 및 C++ 빌드 성공.

## 작업 8: 향후 생성 작업 결정 트리 추가

**파일:**
- 수정: `AGENTS.md`

- [ ] **1단계: 결정 트리 추가**

```markdown
### UECommandForge 생성 작업 결정 트리

1. Blueprint만 여러 개 만들면 `create_blueprint_batch.*`를 사용한다.
2. C++ 클래스만 여러 개 만들면 `generate_cpp_class_batch.*`를 사용한다.
3. 새 C++ 부모 클래스 기반 Blueprint를 만들면 다음 순서를 지킨다.
   - `generate_cpp_class_batch.*` 또는 `generate_cpp_class.*`
   - Unreal build/UHT 또는 `validate_cpp_reflection.*`
   - `create_blueprint_batch.*` 또는 `create_blueprint.*`
4. `CreateBlueprint*` Result JSON이 compile/save/registry 검증을 통과하면 같은 턴의 `compile_blueprints.*`는 생략한다.
5. 실패하면 임시 Python 우회 없이 Result JSON, project log, crash report를 먼저 확인한다.
```

- [ ] **2단계: 문서 diff 확인**

실행:

```powershell
git diff -- AGENTS.md
```

예상 결과: 속도/결정 트리 규칙만 추가된다.

## 검증 체크리스트

- [ ] `import unreal`을 추가하지 않았다.
- [ ] `-ExecutePythonScript`를 추가하지 않았다.
- [ ] PythonScriptPlugin 자동화를 추가하지 않았다.
- [ ] 프로젝트 루트에 임시 `.py`, `.bat`, `.ps1`, `.json` 파일을 남기지 않았다.
- [ ] smoke test spec은 `Saved/CodexReports` 아래에만 생성한다.
- [ ] 기존 `GenerateCppClass` 테스트가 계속 통과한다.
- [ ] 새 `GenerateCppClassBatch` 테스트가 통과한다.
- [ ] Blueprint fast path는 기존 `create_blueprint_batch`를 우선 사용한다.
- [ ] 새 wrapper는 `run_commandlet.*`를 호출해 Python guard를 유지한다.
- [ ] batch 가능한 작업에서 Unreal 프로세스 실행 횟수가 줄어든 것을 시간 측정으로 확인한다.

## 권장 실행 순서

1. `AGENTS.md`에 batch 우선 운영 규칙을 먼저 추가한다.
2. `CreateBlueprintBatch`를 실제 생성 작업의 기본 경로로 사용한다.
3. C++ 여러 class 생성이 반복될 때 `GenerateCppClassBatch`를 구현한다.
4. smoke test와 end-to-end timing으로 실제 단축 효과를 확인한다.
