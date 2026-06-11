# UECommandForge

LLM 기반 Unreal Engine 자동화 브리지. 전체 설계는 `ue_commandlet_based_llm_automation_plan.md` 참조.

## 디렉토리 구조

```
unreal-harness/
  sample/               UE 샘플 프로젝트 (UECommandForgeSample.uproject + Plugins/)
  tools/ue/             Shell/Batch 래퍼 — LLM이 호출하는 명령 표면
  docs/                 계획서, 메모리, 설계 문서
```

## Codex 명령 표면 (Phase 8)

| macOS/Linux/Git Bash | Windows Command Prompt | 목적 |
|---|---|---|
| `tools/ue/hello.sh` | `tools\ue\hello.bat` | 헬스체크 — `Hello` commandlet 실행 후 Result JSON 기록 |
| `tools/ue/create_character_bp.sh <spec.json>` | `tools\ue\create_character_bp.bat <spec.json>` | Character Blueprint 생성 |
| `tools/ue/create_ai_controller.sh <spec.json>` | `tools\ue\create_ai_controller.bat <spec.json>` | AIController Blueprint 생성 |
| `tools/ue/compile_blueprints.sh` | `tools\ue\compile_blueprints.bat` | Blueprint 일괄 컴파일 |
| `tools/ue/set_blueprint_defaults.sh <spec.json>` | `tools\ue\set_blueprint_defaults.bat <spec.json>` | Blueprint CDO 기본값/컴포넌트 적용 |
| `tools/ue/create_statetree.sh <spec.json>` | `tools\ue\create_statetree.bat <spec.json>` | StateTree 에셋 생성 |
| `tools/ue/bind_ai_flow.sh <spec.json>` | `tools\ue\bind_ai_flow.bat <spec.json>` | Character, AIController, StateTree 바인딩 |
| `tools/ue/validate_ai_flow.sh <spec.json>` | `tools\ue\validate_ai_flow.bat <spec.json>` | AI 플로우 바인딩 CDO 검증 |
| `tools/ue/place_actor.sh <spec.json>` | `tools\ue\place_actor.bat <spec.json>` | 맵에 Actor 배치 |
| `tools/ue/create_ai_flow.sh <spec.json>` | `tools\ue\create_ai_flow.bat <spec.json>` | Phase 3~6 전체 워크플로우 실행 |
| `tools/ue/setup_npc_character.sh` | `tools\ue\setup_npc_character.bat` | NPC Character 기본 프로파일로 워크플로우 실행 |
| `tools/ue/setup_patrol_ai.sh` | `tools\ue\setup_patrol_ai.bat` | Patrol AI 기본 프로파일로 워크플로우 실행 |
| `tools/ue/snapshot_assets.sh` | `tools\ue\snapshot_assets.bat` | AssetRegistry 기반 에셋 스냅샷 생성 |
| `tools/ue/validate_asset_rules.sh <policy.json>` | `tools\ue\validate_asset_rules.bat <policy.json>` | 에셋 경로/네이밍 정책 검증 |
| `tools/ue/plan_asset_changes.sh <plan.json>` | `tools\ue\plan_asset_changes.bat <plan.json>` | 에셋 변경 preflight 및 rollback plan 생성 |
| `tools/ue/apply_asset_changes.sh <plan.json>` | `tools\ue\apply_asset_changes.bat <plan.json>` | 승인된 에셋 변경 적용 |
| `tools/ue/rollback_asset_changes.sh <rollback.json>` | `tools\ue\rollback_asset_changes.bat <rollback.json>` | 에셋 변경 rollback |
| `tools/ue/create_project_folders.sh <folders.json>` | `tools\ue\create_project_folders.bat <folders.json>` | 표준 프로젝트 폴더 생성 |
| `tools/ue/generate_cpp_class.sh <spec.json>` | `tools\ue\generate_cpp_class.bat <spec.json>` | C++ 클래스 보일러플레이트 생성 |
| `tools/ue/validate_cpp_reflection.sh <policy.json>` | `tools\ue\validate_cpp_reflection.bat <policy.json>` | UPROPERTY/UFUNCTION 정책 검증 |
| `tools/ue/validate_buildcs.sh <policy.json>` | `tools\ue\validate_buildcs.bat <policy.json>` | Build.cs 의존성 검증 |
| `tools/ue/analyze_uht_log.sh <log>` | `tools\ue\analyze_uht_log.bat <log>` | UHT/build 로그 분석 |
| `tools/ue/validate_data_source.sh <schema.json> <source.csv>` | `tools\ue\validate_data_source.bat <schema.json> <source.csv>` | CSV/JSON 데이터 소스 검증 |
| `tools/ue/import_data_source.sh <schema.json> <source.csv>` | `tools\ue\import_data_source.bat <schema.json> <source.csv>` | 검증된 데이터 소스 import |
| `tools/ue/validate_datatable.sh <schema.json> [asset-path]` | `tools\ue\validate_datatable.bat <schema.json> [asset-path]` | UE DataTable Row 구조 검증 |
| `tools/ue/validate_config_rules.sh <schema.json> [config.ini]` | `tools\ue\validate_config_rules.bat <schema.json> [config.ini]` | Config 값 검증 |
| `tools/ue/validate_project_rules.sh -AssetPolicy=<policy.json> [-BuildCsPolicy=<policy.json>] [-DataSchema=<schema.json> -DataSource=<source.csv>] [-ConfigRules=<schema.json> [-Config=<config.ini>]]` | `tools\ue\validate_project_rules.bat -AssetPolicy=<policy.json> [-BuildCsPolicy=<policy.json>] [-DataSchema=<schema.json> -DataSource=<source.csv>] [-ConfigRules=<schema.json> [-Config=<config.ini>]]` | Phase 8 통합 품질 게이트 실행 및 JSON/Markdown 리포트 생성 |

LLM은 이 표에 있는 명령만 호출해야 한다.

## 테스트 명령

| macOS/Linux/Git Bash | Windows Command Prompt | 목적 |
|---|---|---|
| `tools/test/automation/run.sh` | `tools\test\automation\run.bat` | UECommandForge 자동화 테스트 실행 |
| `tools/test/smoke/create_ai_flow.sh specs/examples/guard_ai.json` | `tools\test\smoke\create_ai_flow.bat specs\examples\guard_ai.json` | CreateAIFlow Result JSON과 생성 에셋 검증 |
| `tools/test/smoke/prototype_automation.sh` | `tools\test\smoke\prototype_automation.bat` | Phase 8 통합 품질 게이트 Result JSON/Markdown 검증 |
| `tools/test/smoke/windows_command_wrappers.sh` | 해당 없음 | Windows `.bat` 래퍼 정적 스모크 테스트 |

## 환경 요구 사항

- Unreal Engine 5.7 설치 필요.
- 기본 경로가 아닌 경우 `UE_ROOT` 환경변수 설정.
- macOS 기본값: `/Users/Shared/Epic Games/UE_5.7`
- Windows 기본값 (Git Bash): `/c/Program Files/Epic Games/UE_5.7`
- Windows 기본값 (Command Prompt): `C:\Program Files\Epic Games\UE_5.7`
- Windows Command Prompt 릴리즈/설치 wrapper는 실행 시 `tools\windows\bootstrap_dependencies.bat`로 `Git.Git`, `jqlang.jq`를 확인한다. 순수 `.bat` 테스트 wrapper는 `jq`만 확인한다.
- `tools\lint\cpp_static_analysis.bat`은 실행 모드에 맞춰 `Cppcheck.Cppcheck`, `LLVM.LLVM`을 확인한다. `--check-tools`는 설치 없이 현재 탐지 결과만 출력한다.
- 누락 의존성은 기본적으로 `winget`으로 자동 설치한다. 자동 설치를 끄려면 해당 Command Prompt 세션에서 `set UECF_AUTO_INSTALL_DEPS=0`을 설정한다.
- 자동 설치는 `winget install --id ... -e --source winget --disable-interactivity`로 실행된다. 설치 확인만 하고 싶으면 `UECF_SKIP_DEP_INSTALL=1`을 설정한다.

## 설치 및 릴리즈 패키지

릴리즈 설치는 UE 플러그인과 Codex 도구를 분리한다.

| 대상 | 설치 위치 |
|---|---|
| UE plugin | `<Project>/Plugins/UECommandForge` |
| Codex tools/specs | `~/.codex/UECommandForge` |
| 프로젝트 연결 manifest | `<Project>/UECommandForge/uecommandforge-project.json` |
| Codex 설치 manifest | `~/.codex/UECommandForge/uecommandforge-installed.json` |

로컬 릴리즈 패키지 생성:

```bash
tools/release/package_plugin.sh --version 0.8.0 --channel local --out-dir sample/Saved/Release/Plugin
tools/release/package_tools.sh --version 0.8.0 --channel local --out-dir sample/Saved/Release/Tools
tools/release/package_source.sh --version 0.8.0 --channel local --out-dir sample/Saved/Release/Source
```

각 패키지 ZIP 옆에는 같은 디렉터리의 `checksums.txt`가 있어야 한다. 설치 전 검증:

```bash
tools/release/verify_release_package.sh \
  sample/Saved/Release/Tools/UECommandForge-0.8.0-Tools.zip \
  sample/Saved/Release/Tools/checksums.txt
```

프로젝트 설치:

```bash
./install-uecommandforge.sh --project /path/to/MyProject.uproject
```

패키지 ZIP 경로를 생략하면 `sample/Saved/Release/Installer` 아래에 로컬 plugin/tools 패키지를 만든 뒤 설치한다. 기존 `sample/Saved/PluginBuild` 산출물을 재사용하려면 `UECF_INSTALL_SKIP_PLUGIN_BUILD=1`을 설정한다.

이미 생성된 패키지를 명시해서 설치할 수도 있다.

```bash
./install-uecommandforge.sh \
  --project /path/to/MyProject.uproject \
  --plugin-package sample/Saved/Release/Plugin/UECommandForge-0.8.0-UE5.7-Mac.zip \
  --tools-package sample/Saved/Release/Tools/UECommandForge-0.8.0-Tools.zip
```

업데이트는 backup을 강제한다.

```bash
tools/release/update_install.sh \
  --project /path/to/MyProject.uproject \
  --plugin-package sample/Saved/Release/Plugin/UECommandForge-0.8.0-UE5.7-Mac.zip \
  --tools-package sample/Saved/Release/Tools/UECommandForge-0.8.0-Tools.zip
```

삭제:

```bash
tools/release/uninstall.sh --project /path/to/MyProject.uproject
```

기본 삭제는 프로젝트 플러그인과 project link만 제거하고 Codex tools/specs와 설치 manifest는 보존한다. Codex tools/specs까지 제거하려면 명시적으로 지정한다.

```bash
tools/release/uninstall.sh \
  --project /path/to/MyProject.uproject \
  --remove-codex-tools true \
  --remove-specs true
```

설치 검증 smoke:

```bash
tools/test/smoke/release_package_install.sh
tools/test/smoke/installer_install_update_uninstall.sh
```

Windows Command Prompt와 PowerShell wrapper는 포함되어 있지만, 실제 `.bat` 실행 검증은 Windows 호스트가 필요하다. macOS에서는 `tools/test/smoke/windows_release_validation_report.sh`가 `windows_host_required` 제한 리포트를 생성한다.

## 배포 게이트

릴리즈 전 macOS 로컬 게이트:

```bash
git diff --check
tools/test/smoke/release_version_policy.sh
tools/ue/build_plugin.sh
tools/test/automation/run.sh
tools/test/smoke/windows_command_wrappers.sh
tools/test/smoke/prototype_automation.sh
tools/test/smoke/release_package_source.sh
tools/test/smoke/release_package_tools.sh
tools/test/smoke/release_package_plugin.sh
tools/test/smoke/release_package_install.sh
tools/test/smoke/installer_install_update_uninstall.sh
```

Windows 실기 검증을 하지 못한 릴리즈는 `tools/test/smoke/windows_release_validation_report.sh`로 `windows_host_required` 제한 리포트를 남긴다. 배포 후에는 GitHub Release artifact를 새 디렉터리에 다운로드한 뒤 각 ZIP과 같은 디렉터리의 `checksums.txt`를 `tools/release/verify_release_package.sh`로 검증하고, 깨끗한 프로젝트에서 installer smoke를 재실행한다.

## 개발용 C++ 정적 분석

`clang-tidy`와 `cppcheck`는 UECommandForge 플러그인 배포물에 포함하지 않는다. 배포 패키지는 Unreal 플러그인 코드만 포함하고, 정적 분석기는 개발/CI 환경의 외부 prerequisite로 설치한다.

macOS:

```bash
brew install llvm cppcheck
```

Windows:

```powershell
winget install LLVM.LLVM
winget install Cppcheck.Cppcheck
```

또는 Chocolatey 사용 시:

```powershell
choco install llvm cppcheck -y
```

도구 확인:

```bash
tools/lint/cpp_static_analysis.sh --check-tools
```

Windows PowerShell:

```powershell
.\tools\lint\cpp_static_analysis.ps1 -CheckTools
```

Windows Command Prompt:

```bat
tools\lint\cpp_static_analysis.bat --check-tools
```

탐색 우선순위:

- `CLANG_TIDY`, `CPPCHECK` 환경변수
- `clang-tidy`/`cppcheck` 또는 Windows의 `clang-tidy.exe`/`cppcheck.exe` PATH
- macOS Homebrew LLVM: `/opt/homebrew/opt/llvm/bin/clang-tidy`, `/usr/local/opt/llvm/bin/clang-tidy`
- Windows LLVM/Cppcheck 기본 설치 경로와 Visual Studio LLVM 후보 경로

`tools/lint/cpp_static_analysis.sh`는 macOS/Linux/Windows Git Bash용이고, `tools/lint/cpp_static_analysis.ps1`은 Windows PowerShell용이며, `tools/lint/cpp_static_analysis.bat`은 Windows Command Prompt용이다. 기본 실행은 안정적인 `cppcheck`만 수행한다. `clang-tidy`는 `compile_commands.json` 생성 후 `--clang-tidy-only` 또는 `RUN_CLANG_TIDY=1`로 명시적으로 실행한다.

`compile_commands.json` 생성:

```bash
tools/lint/generate_compile_commands.sh
```

Windows PowerShell:

```powershell
.\tools\lint\generate_compile_commands.ps1
```

Windows Command Prompt:

```bat
tools\lint\generate_compile_commands.bat
```

기본 출력 위치는 repo root의 `compile_commands.json`이고 git에는 포함하지 않는다. 다른 출력 위치를 쓰려면 세 환경 모두 `COMPILE_COMMANDS_DIR`를 설정한다.

`clang-tidy` 기본 check set은 `clang-analyzer-*`이며 `CLANG_TIDY_CHECKS`로 재정의할 수 있다. Unreal/Xcode/LLVM 버전 차이로 모듈 관련 컴파일 오류가 날 수 있으므로 CI 기본 게이트에는 포함하지 않고, 로컬 정밀 분석용 opt-in 단계로 둔다.

GitHub Actions의 `C++ Static Analysis` workflow는 push/PR에서 `cppcheck`와 wrapper 동작만 검증한다. `workflow_dispatch`에서 `run_clang_tidy`를 `true`로 실행하면 macOS job이 `tools/lint/generate_compile_commands.sh`로 `compile_commands.json`를 생성한 뒤 `tools/lint/cpp_static_analysis.sh --clang-tidy-only`를 실행한다. 이 opt-in 단계는 Unreal Engine이 설치된 runner가 필요하며, 기본 경로가 아니면 `ue_root` input으로 `UE_ROOT`를 넘긴다.

## Result JSON

모든 commandlet은 `sample/Saved/UECommandForge/Reports/<Commandlet>_<UTC>.json`에 기록한다. `PrototypeAutomation`은 child commandlet report를 함께 남기고, 통합 JSON과 같은 basename의 Markdown 요약 리포트도 생성한다. 종료 코드:

| 코드 | 의미 |
|---|---|
| 0 | OK |
| 2 | Spec 파싱 실패 |
| 3 | 빌드/컴파일 실패 |
| 4 | 검증 실패 |
| 5 | UE/엔진 오류 |
