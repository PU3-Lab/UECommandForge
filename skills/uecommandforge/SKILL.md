---
name: uecommandforge
description: Use when Codex needs to automate Unreal Engine work through UECommandForge, including Blueprint, asset, C++ reflection, DataTable, config, project-rule validation, or Result JSON inspection tasks. Use before running Unreal automation, choosing UECommandForge wrappers, checking installation manifests, or handling requests that might otherwise tempt direct Unreal Python access.
---

# UECommandForge

## 핵심 원칙

Unreal Editor, asset, Blueprint, C++ reflection 작업은 직접 Unreal Python으로 처리하지 않는다. 설치된 UECommandForge commandlet wrapper를 사용하고, 실행 후 대상 프로젝트의 `Saved/UECommandForge/Reports`에서 Result JSON을 확인한다.

AGENTS의 안전 규칙을 따른다. `import unreal`, `-ExecutePythonScript`, PythonScriptPlugin, Editor Utility, 원격 Python 실행, 임시 `.py` 우회는 사용하지 않는다.

## 상세 안전 규칙

자동화 실행 전 다음 상세 규칙을 읽고 적용한다.

```text
~/.codex/UECommandForge/specs/codex/unreal-automation-agents.md
```

저장소에서 개발 중이면 repo의 `specs/codex/unreal-automation-agents.md`를 사용한다. 이 skill은 실행 절차의 요약이며, 임시 산출물 위치와 새 automation 구현 요구사항은 상세 규칙을 기준으로 한다.

## 설치 상태 확인

1. Codex home을 정한다. 기본은 `$CODEX_HOME`, 없으면 `~/.codex`다.
2. 다음 파일을 확인한다.
   - `~/.codex/UECommandForge/uecommandforge-installed.json`
   - `~/.codex/UECommandForge/uecommandforge.env`
   - `~/.codex/UECommandForge/uecommandforge.env.sh`
3. 대상 프로젝트는 우선순위대로 결정한다.
   - 사용자 요청의 명시적 `.uproject`
   - `UECF_PROJECT_FILE`
   - `~/.codex/UECommandForge/uecommandforge.env`
   - installed manifest의 `project_file`
4. 프로젝트 manifest가 있으면 함께 확인한다.
   - `<Project>/UECommandForge/uecommandforge-project.json`

설치 파일이 없거나 manifest가 깨져 있으면 wrapper 실행을 추측하지 말고 설치 또는 update를 먼저 안내한다.

## Wrapper 선택

Windows Command Prompt 또는 PowerShell에서 작업하면 `.bat` wrapper를 우선 사용한다. Git Bash, macOS, Linux에서는 `.sh` wrapper를 사용한다.

기본 위치:

```text
~/.codex/UECommandForge/tools/ue
```

자주 쓰는 wrapper:

```text
hello
snapshot_assets
validate_asset_rules
plan_asset_changes
apply_asset_changes
rollback_asset_changes
create_project_folders
generate_cpp_class
generate_cpp_class_batch
validate_cpp_reflection
validate_buildcs
analyze_uht_log
validate_data_source
import_data_source
validate_datatable
validate_config_rules
validate_project_rules
create_character_bp
create_ai_controller
create_statetree
bind_ai_flow
validate_ai_flow
set_blueprint_defaults
compile_blueprints
create_ai_flow
create_blueprint
create_blueprint_batch
```

작업에 맞는 wrapper가 없으면 임시 스크립트로 우회하지 않는다. 새 automation이 필요한 경우 repo에 commandlet과 `tools/ue/` wrapper를 추가하고 smoke test를 작성한다.

## 실행 절차

1. wrapper 파일 존재와 실행 권한을 확인한다.
2. spec이 필요한 wrapper는 repo 또는 설치된 `specs` 아래 JSON을 사용한다.
3. 반복 생성 작업은 단건 wrapper보다 batch wrapper를 우선 사용한다.
   - Blueprint 여러 개: `create_blueprint_batch`
   - C++ 클래스 여러 개: `generate_cpp_class_batch`
   - Blueprint 생성과 기본 컴포넌트 또는 기본값 적용: batch spec의 `components`와 defaults 기능
4. 새 C++ 부모 클래스 기반 Blueprint 생성은 프로세스를 분리한다.
   - `generate_cpp_class` 또는 `generate_cpp_class_batch`
   - Unreal build/UHT 또는 `validate_cpp_reflection`, `validate_buildcs`
   - 새 Unreal 프로세스에서 `create_blueprint` 또는 `create_blueprint_batch` 실행
5. 실행 결과의 종료 코드를 확인한다.
6. 최신 Result JSON의 `ok`, `errors`, `issues`, `validation`과 생성/변경 경로를 확인한다.
7. `CreateBlueprint` 결과의 `.validation`에 `compile_status: ok`, `asset_on_disk: true`, `asset_in_registry: true`가 있으면 같은 턴에 `compile_blueprints`를 반복 실행하지 않는다.
8. `CreateBlueprintBatch`는 spec의 `compile: true`, Result JSON의 `ok: true`, 빈 `errors`를 확인하면 같은 턴에 `compile_blueprints`를 반복 실행하지 않는다.

Result JSON 위치:

```text
<Project>/Saved/UECommandForge/Reports
```

## 실패 처리

wrapper 실패 시 먼저 다음을 수집한다.

- `<Project>/Saved/UECommandForge/Reports`
- `<Project>/Saved/Logs`
- `<Project>/Saved/Crashes`
- `%LOCALAPPDATA%\CrashReportClient\Saved\Crashes`

실패한 commandlet을 Unreal Python, 임시 launcher, 직접 Editor 조작으로 우회하지 않는다. 실패 원인이 wrapper 부재나 commandlet 기능 부족이면 UECommandForge repo의 commandlet/wrapper/test를 확장한다.
