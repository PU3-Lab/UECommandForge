# Prototype UE 기능 테스트 시나리오

작성일: 2026-06-01

## 목적

`docs/superpowers/plans/references/prototype.md`에 정의된 prototype 기능군을 실제 Unreal Engine 프로젝트에서 Commandlet 실행 기준으로 검증한다. 이 문서는 Windows 호스트에서 UECommandForge가 설치된 Unreal 프로젝트 루트로 이동한 뒤 Codex가 `tools\ue\*.bat` wrapper를 실행해 에셋 생성, C++ 보일러플레이트 생성, 데이터/설정 검증 기능이 기대한 report와 산출물을 남기는지 확인하기 위한 기능 테스트 시나리오다.

## 검증 범위

| 기능군 | prototype 목적 | 테스트 관점 |
|---|---|---|
| 에셋 생성 / 경로 / 네이밍 관리 | 반복적인 에셋 생성과 정리 자동화 | 폴더 생성, 에셋 snapshot, 네이밍/경로 정책 검증, 변경 계획/rollback 계획 생성 |
| C++ 클래스 생성 반복 | Unreal C++ 보일러플레이트 자동화 | `UActorComponent` 생성, `UPROPERTY`/`UFUNCTION` 메타데이터 출력, `.Build.cs` 의존성 검증, reflection 정책 검증 |
| DataAsset / DataTable / Config 관리 | 데이터 정의와 설정 파일 안정화 | CSV schema 검증, DataTable import/검증/rollback, Config 값 검증 |

## 사전 조건

- Windows 10/11
- Unreal Engine 설치
- 테스트 대상 Unreal 프로젝트에 `Plugins\UECommandForge` 설치
- Codex tools/specs 설치 경로 존재: `%USERPROFILE%\.codex\UECommandForge`
- `UE_ROOT`가 Unreal Engine 설치 경로를 가리킴
- Command Prompt에서 `git`, `bash`, `jq` 실행 가능
- 테스트 대상 Unreal 프로젝트 루트에서 실행
- `UECF_PROJECT_FILE`이 테스트 대상 `.uproject`를 가리킴

기본 UE 경로를 사용하는 경우:

```bat
set UE_ROOT=C:\Program Files\Epic Games\UE_5.7
```

다른 경로를 사용하는 경우:

```bat
set UE_ROOT=D:\Epic Games\UE_5.7
```

Commandlet timeout이 부족하면 늘린다.

```bat
set UE_COMMANDLET_TIMEOUT=180
```

## 공통 준비

테스트 대상 Unreal 프로젝트 루트로 이동한다.

```bat
cd /d C:\Path\To\YourUnrealProject
```

사람이 Command Prompt에서 직접 실행할 때만 아래 환경 변수를 수동 설정한다. Codex로 테스트할 때는 다음 “Codex 자동 설정” 절차를 사용하므로 이 블록을 직접 입력할 필요가 없다.

```bat
set UECF_PROJECT_FILE=%CD%\YourUnrealProject.uproject
set UECF_HOME=%USERPROFILE%\.codex\UECommandForge
set UECF_TOOLS=%UECF_HOME%\tools\ue
set UECF_SPECS=%UECF_HOME%\specs
set UECF_REPORT_DIR=%CD%\Saved\UECommandForge/Reports
```

### Codex 자동 설정

Codex에서 실행할 때는 현재 Unreal 프로젝트 루트에서 아래 PowerShell 블록을 먼저 실행한다. 이 블록은 `.uproject`를 자동 탐색하고 `%USERPROFILE%\.codex\UECommandForge\uecommandforge.env`에 테스트 대상 프로젝트를 기록한다. 이후 wrapper는 셸 환경 변수가 유지되지 않아도 같은 프로젝트를 바라본다.

```powershell
$project = Get-ChildItem *.uproject | Select-Object -First 1
if (-not $project) { throw "현재 폴더에서 .uproject를 찾지 못했습니다." }
$uecfHome = "$env:USERPROFILE\.codex\UECommandForge"
$ueRoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.7" }
@"
UECF_PROJECT_FILE=$($project.FullName)
UE_ROOT=$ueRoot
"@ | Set-Content -Encoding ASCII "$uecfHome\uecommandforge.env"
$env:UECF_PROJECT_FILE = $project.FullName
$env:UECF_HOME = $uecfHome
$env:UECF_TOOLS = "$uecfHome\tools\ue"
$env:UECF_SPECS = "$uecfHome\specs"
$env:UECF_REPORT_DIR = "$PWD\Saved\UECommandForge/Reports"
New-Item -ItemType Directory -Force $env:UECF_REPORT_DIR | Out-Null
```

프로젝트 파일명을 모르면 PowerShell로 먼저 확인한다.

```powershell
Get-ChildItem *.uproject
```

경로가 유효한지 확인한다.

```powershell
$env:UECF_PROJECT_FILE
$env:UECF_HOME
$env:UECF_TOOLS
$env:UECF_SPECS
Test-Path $env:UECF_PROJECT_FILE
Test-Path "$env:UECF_TOOLS\hello.bat"
Test-Path "$env:UECF_SPECS\policies\project_folders.json"
```

Commandlet 기본 실행을 확인한다.

```bat
%UECF_TOOLS%\hello.bat
```

기대 결과:

- exit code `0`
- `Saved\UECommandForge/Reports\Hello_*.json` 생성
- 최신 report의 `.ok == true`

## Codex 실행 기준

이 시나리오를 Codex로 실행할 때는 테스트 대상 Unreal 프로젝트 루트를 워킹 폴더로 사용한다. `C:\Workspaces\projects\UECommandForge`는 문서와 개발 레포이며, 실제 기능 테스트의 기준 경로가 아니다. Codex는 이 문서의 명령을 Unreal 프로젝트 루트에서 실행하고, 실제 Unreal Engine 실행 결과를 기준으로 PASS/FAIL을 판단한다.

Codex에게 줄 기본 지시문:

```text
현재 Unreal 프로젝트 워킹 폴더에서 UECommandForge prototype UE 기능 테스트를 진행해줘.

조건:
- 현재 폴더의 .uproject를 찾아 UECF_PROJECT_FILE로 설정
- %USERPROFILE%\.codex\UECommandForge 아래의 tools/specs를 사용
- UE_ROOT, UECF_PROJECT_FILE, UECF_TOOLS, UECF_SPECS, UECF_REPORT_DIR을 먼저 확인
- 테스트 중 생성되는 report는 현재 Unreal 프로젝트의 Saved\UECommandForge/Reports 기준으로 최신 파일을 확인
- 실패하면 해당 명령, exit code, 최신 report JSON, Saved\Logs 최신 로그를 수집
- 테스트 산출물로 생긴 소스/에셋 변경은 임의로 삭제하거나 되돌리지 말고 먼저 보고
- 커밋/푸시는 하지 말고 결과만 보고
```

Codex가 처음 확인할 항목:

```powershell
git status --short --branch
$project = Get-ChildItem *.uproject | Select-Object -First 1
$env:UECF_PROJECT_FILE = $project.FullName
$env:UECF_HOME = "$env:USERPROFILE\.codex\UECommandForge"
$env:UECF_TOOLS = "$env:UECF_HOME\tools\ue"
$env:UECF_SPECS = "$env:UECF_HOME\specs"
$env:UECF_REPORT_DIR = "$PWD\Saved\UECommandForge/Reports"
$env:UE_ROOT
$env:UECF_PROJECT_FILE
$env:UECF_TOOLS
$env:UECF_SPECS
$env:UECF_REPORT_DIR
$env:UE_COMMANDLET_TIMEOUT
Get-ChildItem Saved\UECommandForge/Reports -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 5
```

Codex 실행 순서:

1. 공통 준비의 환경 변수와 `hello.bat`를 먼저 실행한다.
2. 시나리오 1부터 9까지 순서대로 실행한다.
3. 각 시나리오마다 exit code와 최신 report의 핵심 필드를 확인한다.
4. 실패가 발생해도 다음 시나리오를 계속 진행할 수 있으면 계속 진행하고, 마지막에 실패 목록을 묶어 보고한다.
5. C++ 생성 시나리오처럼 워킹 트리를 변경할 수 있는 항목은 생성 파일 경로와 `git status --short`를 함께 보고한다.

Codex 결과 보고는 이 문서 하단의 “결과 보고 형식”을 사용한다.

확인 예시:

```powershell
$r = Get-ChildItem Saved\UECommandForge/Reports\Hello_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
jq ".ok" $r.FullName
```

## 시나리오 1: 프로젝트 폴더 구조 자동 생성

### 목적

prototype의 “폴더 구조 자동 생성” 기능을 검증한다. `%UECF_SPECS%\policies\project_folders.json`에 정의된 `/Game` 하위 폴더 구조가 Commandlet으로 생성되는지 확인한다.

### 실행

```bat
%UECF_TOOLS%\create_project_folders.bat %UECF_SPECS%\policies\project_folders.json
```

### 기대 결과

- exit code `0`
- `Saved\UECommandForge/Reports\CreateProjectFolders_*.json` 생성
- 최신 report의 `.ok == true`
- report의 commandlet 이름이 `CreateProjectFolders`
- 지정된 폴더가 Content Browser 기준 `/Game/Characters`, `/Game/AI`, `/Game/UI`, `/Game/Data`, `/Game/Maps`, `/Game/Systems` 아래에 생성됨

### 확인

```powershell
$r = Get-ChildItem Saved\UECommandForge/Reports\CreateProjectFolders_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
jq ".ok, .commandlet" $r.FullName
```

## 시나리오 2: 에셋 snapshot 및 네이밍/경로 정책 검증

### 목적

prototype의 “에셋 네이밍 규칙 검사”, “잘못된 경로의 에셋 정리”, “깨진 참조 검사”의 기초 검증 경로를 확인한다. 실제 에셋 목록을 snapshot으로 수집한 뒤 `assets.policy.json` 기준으로 Blueprint, StateTree, World 에셋의 prefix와 허용 경로를 검사한다.

주의: 이 시나리오는 실제 프로젝트의 현재 자산 품질을 audit한다. commandlet이 정상 실행되어 report를 만들었더라도 프로젝트에 redirector, 누락 의존성, prefix 위반이 있으면 report의 `.ok`는 `false`가 될 수 있다. 이 경우 “도구 실행”은 PASS이고 “프로젝트 정책 준수”는 FAIL로 분리해 보고한다.

### 실행

```bat
%UECF_TOOLS%\snapshot_assets.bat -RootPaths=/Game,/UECommandForge
%UECF_TOOLS%\validate_asset_rules.bat %UECF_SPECS%\policies\assets.policy.json -RootPaths=/Game,/UECommandForge
```

### 기대 결과

- `snapshot_assets.bat` exit code `0`
- `validate_asset_rules.bat`는 정책 위반이 있으면 non-zero일 수 있음
- `AssetSnapshot_*.json`, `ValidateAssetRules_*.json` 생성
- `AssetSnapshot_*.json`의 `.ok == true`
- `ValidateAssetRules_*.json`에 `.commandlet == "ValidateAssetRules"` 기록
- `ValidateAssetRules_*.json`의 `.ok == true`이면 프로젝트 정책 준수 PASS
- `.ok == false`이면 `.issues[]`의 `code`, `asset`, `message`를 프로젝트 수정 후보로 기록

### 확인

```powershell
$snapshot = Get-ChildItem Saved\UECommandForge/Reports\AssetSnapshot_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$policy = Get-ChildItem Saved\UECommandForge/Reports\ValidateAssetRules_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
jq ".ok, .commandlet" $snapshot.FullName
jq ".ok, .commandlet" $policy.FullName
```

## 시나리오 3: 에셋 변경 계획과 rollback 계획 생성

### 목적

prototype의 “잘못된 경로의 에셋 정리”를 실제 적용 전 계획 단계에서 검증한다. 설치된 sample spec의 `asset_changes.plan.json`은 샘플 프로젝트의 `/Game/Tests/BP_TestCharacter`, `/Game/Tests/BP_TestController`를 전제하므로 일반 Unreal 프로젝트에서는 실패할 수 있다. 실제 프로젝트 테스트에서는 아래처럼 프로젝트 독립적인 계획 fixture를 생성해 `PlanAssetChanges`와 rollback plan writer가 동작하는지 확인한다.

### 준비

```powershell
$d = "Saved\UECommandForge/Reports\asset_change_feature_fixtures"
New-Item -ItemType Directory -Force $d | Out-Null
@'
{
  "version": "1",
  "kind": "asset_change_plan",
  "transaction_id": "tx-codex-project-asset-change-plan",
  "allow_delete": false,
  "delete_assets": [],
  "operations": [
    {
      "operation": "create_folder",
      "path": "/Game/Tests/PlannedCodex"
    }
  ]
}
'@ | Set-Content -Encoding UTF8 "$d\asset_changes.plan.json"
```

### 실행

상대 경로는 설치된 wrapper 버전에 따라 `PLAN_READ_FAILED`가 날 수 있다. 최신 wrapper는 파일 인자를 절대 경로로 정규화하지만, 테스트 시나리오는 결과 재현성을 위해 절대 경로로 실행한다.

```powershell
$plan = Resolve-Path "Saved\UECommandForge/Reports\asset_change_feature_fixtures\asset_changes.plan.json"
& "$env:UECF_TOOLS\plan_asset_changes.bat" $plan.Path
```

### 기대 결과

- exit code `0`
- `PlanAssetChanges_*.json` 생성
- 최신 report의 `.ok == true`
- `.rollback_available == true`
- `.rollback_plan_path`가 비어 있지 않음
- rollback plan 파일이 실제로 존재함

### 확인

```powershell
$r = Get-ChildItem Saved\UECommandForge/Reports\PlanAssetChanges_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
jq ".ok, .rollback_available, .rollback_plan_path" $r.FullName
```

## 시나리오 4: C++ ActorComponent 생성

### 목적

prototype의 “UObject / UActorComponent 생성”, “UPROPERTY 메타데이터 추가”, “UFUNCTION 메타데이터 추가” 기능을 검증한다. `GenerateCppClass`는 기본이 dry-run이므로 파일 생성까지 확인하려면 반드시 `-Apply`를 붙인다.

### 준비

기존 파일과 충돌하지 않도록 테스트마다 고유 class spec을 만든다.

```powershell
$d = "Saved\UECommandForge/Reports\cpp_generation_feature_fixtures"
New-Item -ItemType Directory -Force $d | Out-Null
$stamp = Get-Date -Format "yyyyMMddHHmmss"
$className = "UCodexFeatureHealthComponent$stamp"
@"
{
  "version": "1",
  "kind": "cpp_class",
  "transaction_id": "tx-codex-cpp-$stamp",
  "class": {
    "name": "$className",
    "type": "ActorComponent",
    "module": "UECommandForgeRuntime",
    "output_path": "Generated/CodexFeature",
    "base_class": "UActorComponent",
    "blueprint_type": true,
    "tick": false
  },
  "includes": [
    "CoreMinimal.h",
    "Components/ActorComponent.h"
  ],
  "build_dependencies": {
    "public": [
      "Core",
      "CoreUObject",
      "Engine"
    ],
    "private": []
  },
  "properties": [
    {
      "name": "MaxHealth",
      "type": "float",
      "default": "100.0f",
      "metadata": {
        "EditAnywhere": true,
        "BlueprintReadOnly": true,
        "Category": "Health"
      }
    }
  ],
  "functions": [
    {
      "name": "ApplyDamage",
      "return_type": "void",
      "metadata": {
        "BlueprintCallable": true,
        "Category": "Health"
      },
      "params": [
        {
          "name": "DamageAmount",
          "type": "float"
        }
      ]
    }
  ]
}
"@ | Set-Content -Encoding UTF8 "$d\cpp_health_component.json"
Set-Content -Encoding ASCII "$d\class_name.txt" $className
```

### 실행

dry-run만 확인하려면 `-Apply` 없이 실행한다. 이 경우 `.ok == true`, `.dry_run == true`, `.applied == false`이고 파일은 생성되지 않는다.

```bat
%UECF_TOOLS%\generate_cpp_class.bat Saved\UECommandForge/Reports\cpp_generation_feature_fixtures\cpp_health_component.json
```

파일 생성까지 확인하려면 `-Apply`를 붙여 실행한다.

```bat
%UECF_TOOLS%\generate_cpp_class.bat Saved\UECommandForge/Reports\cpp_generation_feature_fixtures\cpp_health_component.json -Apply
```

### 기대 결과

- exit code `0`
- `GenerateCppClass_*.json` 생성
- 최신 report의 `.ok == true`
- dry-run 실행이면 `.dry_run == true`, `.applied == false`, preview/checksum이 생성됨
- `-Apply` 실행이면 `.dry_run == false`, `.applied == true`
- `Plugins\UECommandForge\Source\UECommandForgeRuntime\Public\Generated\CodexFeature\<ClassName>.h` 생성
- `Plugins\UECommandForge\Source\UECommandForgeRuntime\Private\Generated\CodexFeature\<ClassName>.cpp` 생성
- 헤더에 `UPROPERTY`와 `UFUNCTION` 선언이 포함됨
- 소스에 `<ClassName>::ApplyDamage` 구현 stub이 포함됨

### 확인

```powershell
$className = Get-Content Saved\UECommandForge/Reports\cpp_generation_feature_fixtures\class_name.txt
$r = Get-ChildItem Saved\UECommandForge/Reports\GenerateCppClass_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
jq ".ok, .commandlet, .dry_run, .applied, .validation.class_symbol" $r.FullName
findstr /c:"UPROPERTY" "Plugins\UECommandForge\Source\UECommandForgeRuntime\Public\Generated\CodexFeature\$className.h"
findstr /c:"UFUNCTION" "Plugins\UECommandForge\Source\UECommandForgeRuntime\Public\Generated\CodexFeature\$className.h"
findstr /c:"$className::ApplyDamage" "Plugins\UECommandForge\Source\UECommandForgeRuntime\Private\Generated\CodexFeature\$className.cpp"
```

### 정리

`GenerateCppClass -Apply`는 실제 프로젝트 플러그인 `Source` 아래에 C++ 파일을 생성하고, 현재 report에는 rollback plan이 제공되지 않는다. 검증 후 테스트 산출물이 빌드 대상에 섞이지 않도록 생성 파일을 제거한다.

```powershell
$className = Get-Content Saved\UECommandForge/Reports\cpp_generation_feature_fixtures\class_name.txt
$header = "Plugins\UECommandForge\Source\UECommandForgeRuntime\Public\Generated\CodexFeature\$className.h"
$source = "Plugins\UECommandForge\Source\UECommandForgeRuntime\Private\Generated\CodexFeature\$className.cpp"
Remove-Item -LiteralPath $header,$source -Force -ErrorAction Stop
if ((Test-Path -LiteralPath $header) -or (Test-Path -LiteralPath $source)) {
  throw "C++ 생성 테스트 파일 정리에 실패했습니다."
}
```

## 시나리오 5: C++ reflection 및 Build.cs 정책 검증

### 목적

prototype의 “.Build.cs 의존성 추가”, “UPROPERTY 메타데이터 추가”, “UFUNCTION 메타데이터 추가” 검증 경로를 확인한다. 생성된 C++ 파일과 현재 module 설정이 정책을 만족하는지 검사한다.

### 실행

```bat
%UECF_TOOLS%\validate_buildcs.bat %UECF_SPECS%\policies\buildcs.policy.json
```

reflection 정책은 테스트 프로젝트의 임시 fixture로 실패 감지를 확인한다.

```powershell
$d = "Saved\UECommandForge/Reports\ReflectionSmoke"
New-Item -ItemType Directory -Force $d | Out-Null
@'
#pragma once

#include "Components/ActorComponent.h"
#include "BrokenReflectionComponent.generated.h"

UCLASS(BlueprintType)
class UBrokenReflectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RuntimeHealth = 100.0f;

    UFUNCTION(BlueprintCallable)
    void ApplyDamage(float Amount);
};
'@ | Set-Content -Encoding UTF8 "$d\BrokenReflectionComponent.h"
@"
{
  "version": "1",
  "kind": "cpp_reflection_policy",
  "files": [
    {
      "path": "$((Resolve-Path "$d\BrokenReflectionComponent.h").Path.Replace('\', '/'))",
      "role": "ActorComponent"
    }
  ]
}
"@ | Set-Content -Encoding UTF8 "$d\cpp_reflection_policy.json"
& "$env:UECF_TOOLS\validate_cpp_reflection.bat" "$d\cpp_reflection_policy.json"
```

### 기대 결과

- `validate_buildcs.bat` exit code `0`
- `ValidateBuildCs_*.json` 생성
- 최신 report의 `.ok == true`
- 누락된 module dependency가 없어야 함
- reflection fixture는 의도적으로 깨진 header를 검사하므로 commandlet exit code가 non-zero일 수 있음
- `ValidateCppReflection_*.json`의 `.ok == false`
- report에 reflection 정책 위반 issue가 포함됨

### 확인

```powershell
$build = Get-ChildItem Saved\UECommandForge/Reports\ValidateBuildCs_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$reflection = Get-ChildItem Saved\UECommandForge/Reports\ValidateCppReflection_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
jq ".ok, .commandlet" $build.FullName
jq ".ok, .commandlet, [.issues[].code]" $reflection.FullName
```

## 시나리오 6: CSV 데이터 schema 검증

### 목적

prototype의 “필수 필드 누락 검사”, “중복 ID 검사”, “잘못된 타입 검사”를 검증한다. 테스트 fixture를 생성하고 정상 CSV와 실패 CSV를 각각 실행해 expected pass/fail을 확인한다.

### 준비

```powershell
$d = "Saved\UECommandForge/Reports\prototype_feature_fixtures"
New-Item -ItemType Directory -Force $d | Out-Null
@'
{
  "version": "1",
  "kind": "data_schema",
  "key_field": "id",
  "source_type": "csv",
  "fields": [
    { "name": "id", "type": "string", "required": true, "unique": true },
    { "name": "level", "type": "int", "required": true, "min": 1, "max": 10 },
    { "name": "rarity", "type": "string", "required": true, "allowed_values": ["common", "rare"] }
  ]
}
'@ | Set-Content -Encoding UTF8 "$d\data_schema.json"
"id,level,rarity", "row_1,5,common", "row_2,7,rare" | Set-Content -Encoding UTF8 "$d\ok.csv"
"id,level,rarity", "row_1,5,common", "row_1,7,rare" | Set-Content -Encoding UTF8 "$d\duplicate_id.csv"
"id,rarity", "row_1,common" | Set-Content -Encoding UTF8 "$d\missing_required.csv"
"id,level,rarity", "row_1,42,common" | Set-Content -Encoding UTF8 "$d\range_violation.csv"
```

### 실행

정상 데이터:

```bat
%UECF_TOOLS%\validate_data_source.bat Saved\UECommandForge/Reports\prototype_feature_fixtures\data_schema.json Saved\UECommandForge/Reports\prototype_feature_fixtures\ok.csv
```

중복 ID:

```bat
%UECF_TOOLS%\validate_data_source.bat Saved\UECommandForge/Reports\prototype_feature_fixtures\data_schema.json Saved\UECommandForge/Reports\prototype_feature_fixtures\duplicate_id.csv
```

필수 필드 누락:

```bat
%UECF_TOOLS%\validate_data_source.bat Saved\UECommandForge/Reports\prototype_feature_fixtures\data_schema.json Saved\UECommandForge/Reports\prototype_feature_fixtures\missing_required.csv
```

범위 위반:

```bat
%UECF_TOOLS%\validate_data_source.bat Saved\UECommandForge/Reports\prototype_feature_fixtures\data_schema.json Saved\UECommandForge/Reports\prototype_feature_fixtures\range_violation.csv
```

### 기대 결과

- 정상 데이터는 exit code `0`, `.ok == true`
- 중복 ID는 exit code non-zero, issue code `DATA_SOURCE_DUPLICATE_VALUE`
- 필수 필드 누락은 exit code non-zero, issue code `DATA_SOURCE_REQUIRED_FIELD_MISSING`
- 범위 위반은 exit code non-zero, issue code `DATA_SOURCE_RANGE_VIOLATION`
- 각 실행마다 `ValidateDataSource_*.json` 생성

## 시나리오 7: DataTable import, 검증, rollback

### 목적

prototype의 “DataTable Row 구조 관리”와 데이터 변경 rollback 가능성을 검증한다. CSV를 DataTable asset으로 import하고, row count를 검증한 뒤 rollback plan으로 생성된 asset을 제거한다.

### 준비

```powershell
$d = "Saved\UECommandForge/Reports\data_import_feature_fixtures"
$asset = "/Game/Tests/DataImportCodex/DT_CodexImport"
New-Item -ItemType Directory -Force $d | Out-Null
@"
{
  "version": "1",
  "kind": "data_schema",
  "source_type": "csv",
  "target_asset_path": "$asset",
  "row_struct_path": "/Script/GameplayTags.GameplayTagTableRow",
  "fields": [
    { "name": "Tag", "type": "string", "required": true },
    { "name": "DevComment", "type": "string" }
  ]
}
"@ | Set-Content -Encoding UTF8 "$d\import_schema.json"
",Tag,DevComment", "0,Codex.Feature.Row0,first row", "1,Codex.Feature.Row1,second row" | Set-Content -Encoding UTF8 "$d\import_source.csv"
```

### 실행

```bat
%UECF_TOOLS%\import_data_source.bat Saved\UECommandForge/Reports\data_import_feature_fixtures\import_schema.json Saved\UECommandForge/Reports\data_import_feature_fixtures\import_source.csv
%UECF_TOOLS%\validate_datatable.bat Saved\UECommandForge/Reports\data_import_feature_fixtures\import_schema.json /Game/Tests/DataImportCodex/DT_CodexImport
```

rollback은 최신 `ImportDataSource_*.json`의 `.rollback_plan_path`를 읽어서 실행한다.

```powershell
$import = Get-ChildItem Saved\UECommandForge/Reports\ImportDataSource_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$rollback = jq -r ".rollback_plan_path" $import.FullName
& "$env:UECF_TOOLS\rollback_asset_changes.bat" $rollback
```

### 기대 결과

- exit code `0`
- `ImportDataSource_*.json`의 `.ok == true`
- `ValidateDataTable_*.json`의 `.ok == true`
- `RollbackAssetChanges_*.json`의 `.ok == true`
- rollback 후 테스트용 DataTable `.uasset`이 제거됨

### 수동 확인

```powershell
$r = Get-ChildItem Saved\UECommandForge/Reports\ImportDataSource_*.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
jq ".ok, .applied, .rollback_available, .post_validation" $r.FullName
```

## 시나리오 8: Config 값 유효성 검증

### 목적

prototype의 “Config 값 유효성 검사”를 검증한다. 정상 config와 범위 위반 config를 실행해 expected pass/fail을 확인한다.

### 준비

```powershell
$d = "Saved\UECommandForge/Reports\prototype_feature_fixtures"
New-Item -ItemType Directory -Force $d | Out-Null
@'
{
  "version": "1",
  "kind": "config_rules",
  "config_section": "/Script/UECommandForgeSmoke.Settings",
  "fields": [
    { "name": "bEnabled", "type": "bool", "required": true },
    { "name": "MaxCount", "type": "int", "required": true, "min": 1, "max": 10 }
  ]
}
'@ | Set-Content -Encoding UTF8 "$d\config_rules.json"
"[/Script/UECommandForgeSmoke.Settings]", "bEnabled=true", "MaxCount=5" | Set-Content -Encoding UTF8 "$d\config.ini"
"[/Script/UECommandForgeSmoke.Settings]", "bEnabled=true", "MaxCount=42" | Set-Content -Encoding UTF8 "$d\config_invalid.ini"
```

### 실행

정상 config:

```bat
%UECF_TOOLS%\validate_config_rules.bat Saved\UECommandForge/Reports\prototype_feature_fixtures\config_rules.json Saved\UECommandForge/Reports\prototype_feature_fixtures\config.ini
```

범위 위반 config:

```bat
%UECF_TOOLS%\validate_config_rules.bat Saved\UECommandForge/Reports\prototype_feature_fixtures\config_rules.json Saved\UECommandForge/Reports\prototype_feature_fixtures\config_invalid.ini
```

### 기대 결과

- 정상 config는 exit code `0`, `.ok == true`
- 범위 위반 config는 exit code non-zero, issue code `CONFIG_RANGE_VIOLATION`
- 각 실행마다 `ValidateConfigRules_*.json` 생성

## 시나리오 9: prototype 통합 품질 게이트

### 목적

prototype 기능군의 핵심 자동화가 하나의 통합 Commandlet 경로로 묶여 실행되는지 확인한다.

### 준비

```powershell
$d = "Saved\UECommandForge/Reports\prototype_automation_feature_fixtures"
New-Item -ItemType Directory -Force $d | Out-Null
@'
{
  "version": "1",
  "kind": "asset_policy",
  "rules": [
    { "asset_class": "*", "allowed_path": "/Game/Tests" }
  ]
}
'@ | Set-Content -Encoding UTF8 "$d\asset_policy.json"
@'
{
  "version": "1",
  "kind": "buildcs_policy",
  "modules": [
    {
      "name": "UECommandForgeRuntime",
      "required_public": ["Core"],
      "required_private": []
    }
  ]
}
'@ | Set-Content -Encoding UTF8 "$d\buildcs_policy.json"
@'
{
  "version": "1",
  "kind": "data_schema",
  "key_field": "id",
  "source_type": "csv",
  "fields": [
    { "name": "id", "type": "string", "required": true, "unique": true },
    { "name": "level", "type": "int", "required": true, "min": 1, "max": 10 }
  ]
}
'@ | Set-Content -Encoding UTF8 "$d\data_schema.json"
"id,level", "row_1,5", "row_2,7" | Set-Content -Encoding UTF8 "$d\data.csv"
@'
{
  "version": "1",
  "kind": "config_rules",
  "config_section": "/Script/UECommandForgeSmoke.ProjectRules",
  "fields": [
    { "name": "bEnabled", "type": "bool", "required": true },
    { "name": "MaxCount", "type": "int", "required": true, "min": 1, "max": 10 }
  ]
}
'@ | Set-Content -Encoding UTF8 "$d\config_rules.json"
"[/Script/UECommandForgeSmoke.ProjectRules]", "bEnabled=true", "MaxCount=5" | Set-Content -Encoding UTF8 "$d\config.ini"
```

### 실행

상대 경로는 child commandlet이 읽는 과정에서 프로젝트/프로세스 기준 차이로 실패할 수 있으므로 절대 경로로 실행한다.

```powershell
$d = Resolve-Path "Saved\UECommandForge/Reports\prototype_automation_feature_fixtures"
& "$env:UECF_TOOLS\validate_project_rules.bat" `
  "-AssetPolicy=$d\asset_policy.json" `
  "-AssetRootPaths=/Game/Tests" `
  "-BuildCsPolicy=$d\buildcs_policy.json" `
  "-DataSchema=$d\data_schema.json" `
  "-DataSource=$d\data.csv" `
  "-ConfigRules=$d\config_rules.json" `
  "-Config=$d\config.ini"
```

### 기대 결과

- exit code `0`
- `PrototypeAutomation_*.json` 통합 report의 `.ok == true`
- `.validation.suite_count == "4"`
- `.validation.failed_suite_count == "0"`
- Markdown report 생성

## 최종 PASS 기준

아래 항목이 모두 충족되면 prototype UE 기능 테스트 PASS로 본다.

- `hello.bat` 기본 Commandlet 실행 PASS
- 프로젝트 폴더 생성 PASS
- 에셋 snapshot 및 정책 검증 PASS
- 에셋 변경 계획과 rollback 계획 생성 PASS
- C++ class dry-run PASS
- C++ class `-Apply` 파일 생성 PASS 또는 사용자가 dry-run만 요청한 경우 명시적 SKIP
- Build.cs 정책 검증 PASS
- CSV schema 정상/실패 케이스가 기대한 exit code와 issue code를 반환
- DataTable import/검증/rollback PASS
- Config 정상/실패 케이스가 기대한 exit code와 issue code를 반환
- `validate_project_rules.bat` 통합 품질 게이트 PASS

## 실패 시 수집할 정보

실패한 시나리오마다 아래 정보를 남긴다.

- 실행한 명령 전체
- exit code
- 콘솔 로그 마지막 80줄
- 생성된 최신 report JSON 경로
- report의 `.ok`, `.commandlet`, `.issues`
- `UE_ROOT`
- `UE_COMMANDLET_TIMEOUT`
- Unreal Engine 설치 경로
- `Saved\Logs` 최신 로그

수집 예시:

```bat
echo UE_ROOT=%UE_ROOT%
echo UE_COMMANDLET_TIMEOUT=%UE_COMMANDLET_TIMEOUT%
dir /b /o-d Saved\UECommandForge/Reports\*.json
dir /b /o-d Saved\Logs\*.log
```

## 결과 보고 형식

```text
Prototype UE 기능 테스트 결과
- OS:
- UE_ROOT:
- Unreal Engine 버전:
- hello.bat: PASS/FAIL
- 프로젝트 폴더 생성: PASS/FAIL
- 에셋 snapshot: PASS/FAIL
- 에셋 정책 검증 기능: PASS/FAIL
- 프로젝트 에셋 정책 준수: PASS/FAIL
- 에셋 변경 계획/rollback 계획: PASS/FAIL
- C++ class dry-run: PASS/FAIL
- C++ class apply 파일 생성: PASS/FAIL/SKIP
- Build.cs/reflection 검증: PASS/FAIL
- CSV schema 검증: PASS/FAIL
- DataTable import/검증/rollback: PASS/FAIL
- Config 검증: PASS/FAIL
- validate_project_rules.bat: PASS/FAIL
- 실패 report 경로:
- 메모:
```
