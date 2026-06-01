# 세션 요약 — 2026-06-01

## 수행 내용

### Windows 설치 실기 검증 및 후속 수정

`install-uecommandforge.bat` 중심으로 Windows 실제 환경 설치를 확인했다. Unreal Engine 5.7, Visual Studio BuildTools, Windows SDK 환경에서 plugin packaging과 프로젝트 설치를 진행했고, 설치 후 사용자 경로의 `.bat` wrapper로 commandlet 실행까지 검증했다.

| 항목 | 결과 |
|---|---|
| Windows installer 실행 | `install-uecommandforge.bat --project C:\Workspaces\projects\factory-space\frontend` 실행 성공 |
| 프로젝트 경로 자동 해석 | 디렉터리 인자를 `Wanted_Factory.uproject`로 해석 |
| plugin package build | UE 5.7 Win64 package build 성공 |
| 프로젝트 plugin 설치 | `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge` 설치 확인 |
| Codex tools 설치 | `C:\Users\Sam\.codex\UECommandForge` 설치 확인 |
| 기본 commandlet | `hello.bat` 실행 및 `.validation.engine_boot == "ok"` 확인 |
| read-only commandlet | `snapshot_assets.bat -RootPaths=/Game` 실행 성공 |
| 검증 commandlet | `validate_asset_rules.bat` 인자 전달은 성공, 대상 프로젝트 정책 위반 18건 확인 |
| 생성 계열 dry-run | `create_project_folders.bat ... -DryRun` 실행 성공 |
| 공백 포함 경로 | `-Spec="...Windows Bat Space\project folders.json"` 형태 실행 성공 |

### Windows `.bat` wrapper 인자 전달 수정

설치된 `validate_asset_rules.bat`가 `-Policy` 값을 commandlet에 전달하지 못해 `MISSING_ARG`를 반환하는 문제를 추적했다. 원인은 wrapper에서 `-Policy="%PATH%"` 형태가 `cmd` 인자 처리 과정에서 `-Policy`와 값으로 분리되고, `run_commandlet.bat`가 이를 UE commandlet이 기대하는 `-Policy=...` 형태로 복원하지 못한 것이었다.

수정 내용:

- `tools\ue\run_commandlet.bat`에서 알려진 값 옵션을 `-Key=Value` 형태로 복원
- 이미 `-Key=Value`로 들어온 인자는 따옴표를 유지한 채 전달
- `-Markdown`, `-AssetPaths`, `-AssetRootPaths`, `-Format`, `-ApplyBuildCs` 등 실제 commandlet 값 옵션 allowlist 보강
- Windows batch가 comma를 인자 구분자로 취급하는 경우를 고려해 `-AssetPaths`, `-AssetRootPaths`, `-RootPaths`의 분리된 조각을 comma list로 복원
- comma list tail 재결합은 `/Game`, `/Plugin`, `/Engine` 경로 조각으로 제한해 독립 positional argument 흡수를 방지
- 인자 수집 구간에서 delayed expansion을 끄고 `!` 포함 경로 손상을 방지
- `&`, `|`, `<`, `>` 포함 인자는 batch 재조립 구조에서 안전하게 전달할 수 없으므로 명시적으로 거부
- `CommandletName`의 `&`, `|`, `<`, `>` 포함 값도 명시적으로 거부
- 사용자-facing UE wrapper들의 delayed expansion 기반 인자 수집/forwarding을 줄여 wrapper 내부에서 `!` 포함 인자가 손상되지 않도록 보강
- commandlet 전용 추가 인자를 UE 공통 플래그보다 앞에 배치
- 디버깅용 `UECF_DEBUG_ARGS` 출력 추가
- 수정본을 설치 경로 `C:\Users\Sam\.codex\UECommandForge\tools\ue\run_commandlet.bat`에도 반영

### Superpowers plugin 상태 확인

사용자가 Superpowers plugin을 재설치한 뒤 활성화 여부를 확인했다.

| 항목 | 결과 |
|---|---|
| plugin cache | `C:\Users\Sam\.codex\plugins\cache\openai-curated\superpowers\fef63ecf` 존재 |
| plugin manifest | `.codex-plugin\plugin.json` 존재, `skills: ./skills/` 확인 |
| Codex config | `[plugins."superpowers@openai-curated"] enabled = true` 확인 |
| 현재 세션 tool 검색 | `tool_search superpowers` 결과 0개 |
| 현재 세션 활성 skill 목록 | `superpowers:*` 노출 |
| 서브에이전트 도구 | 부모 세션에서 `multi_agent_v1.spawn_agent`/`wait_agent` 사용 가능, 전용 `reviewer` role은 미노출 |

판정:

- Superpowers는 디스크에 설치되어 있고 config에서도 enabled 상태다.
- deferred tool 검색에는 노출되지 않지만, 현재 Codex 세션의 skill 목록에는 `superpowers:*`가 로드되어 있다.
- `config.toml`에 `[marketplaces.openai-curated]` 정의는 보이지 않는다.
- 전용 reviewer role은 없으므로, 코드 리뷰 단계에서는 `default` 서브에이전트에 reviewer 역할을 명시해 진행한다.

## 변경 파일

| 파일 | 내용 |
|---|---|
| `tools/ue/run_commandlet.bat` | Windows commandlet wrapper의 `-Key Value`, `-Key=Value`, 공백 포함 값 전달 보강 |
| `tools/ue/*.bat` 일부 | 사용자-facing wrapper의 delayed expansion 기반 extra arg 수집 제거 |
| `tools/test/smoke/windows_command_wrappers.sh` | PowerShell 내부 batch exit code 전파를 위해 bootstrap 음성 테스트에 `call` 사용 |
| `docs/superpowers/plans/2026-06-01-windows-install-followup-validation.md` | Windows 설치 후속 검증 기록 문서 추가 |
| `SESSION_SUMMARY.md` | 이번 세션 요약 추가 |

## 코드 리뷰

저장소 지침에 따라 코드 변경 후 서브에이전트 리뷰를 수행했다. 현재 세션에는 전용 `reviewer` role이 없어 `default` 서브에이전트에 reviewer/security reviewer 역할을 명시해 진행했다.

| 지적 | 처리 |
|---|---|
| `!ARG!=!NEXT_ARG!` 형태가 공백 포함 경로를 깨뜨릴 수 있음 | 전체 `"-Key=Value"` quoting으로 수정하고 공백 포함 경로 테스트 통과 |
| 모든 `-Flag value`를 결합하면 boolean flag와 positional argument를 오해할 수 있음 | 알려진 값 옵션 목록에 대해서만 결합하도록 수정 |
| delayed expansion으로 `!` 포함 값이 손상될 수 있음 | 인자 수집 구간에서 `DisableDelayedExpansion` 적용 |
| 값 옵션 allowlist 누락으로 일부 `-Key value` 인자가 깨질 수 있음 | 실제 commandlet 값 옵션 allowlist 보강 |
| comma list 값이 batch 인자 분리로 깨질 수 있음 | `-AssetPaths`, `-AssetRootPaths`, `-RootPaths` 조각 재결합 |
| comma list 재결합이 독립 positional arg를 흡수할 수 있음 | tail 재결합 대상을 `/Game`, `/Plugin`, `/Engine` 경로 조각으로 제한 |
| `&`, `|`, `<`, `>` 포함 값이 batch 재조립 중 명령 주입으로 이어질 수 있음 | 해당 cmd metacharacter 포함 인자 명시 거부 |
| `CommandletName` 메타문자 값이 직접 호출 경로에서 명령 주입으로 이어질 수 있음 | commandlet 이름에도 unsafe cmd metacharacter 거부 적용 |
| 사용자-facing wrapper가 `run_commandlet.bat` 전에 `!` 포함 값을 손상시킬 수 있음 | `!EXTRA_ARGS!` 수집 패턴 제거 및 `%*` forwarding wrapper에 `DisableDelayedExpansion` 적용 |
| 문서의 PASS 기준이 policy fail과 충돌함 | PASS 기준과 섹션별 기대 결과 정리 |

## 검증 결과

| 검증 | 결과 |
|---|---|
| 설치 산출물 존재 확인 | PASS |
| 설치된 `.bat` wrapper 목록 비교 | PASS |
| `cmd /c "C:\Users\Sam\.codex\UECommandForge\tools\ue\hello.bat"` | PASS |
| `cmd /c "C:\Users\Sam\.codex\UECommandForge\tools\ue\snapshot_assets.bat -RootPaths=/Game"` | PASS |
| `cmd /c "C:\Users\Sam\.codex\UECommandForge\tools\ue\validate_asset_rules.bat ... -RootPaths=/Game"` | COMMANDLET PASS / POLICY FAIL |
| `cmd /c "C:\Users\Sam\.codex\UECommandForge\tools\ue\create_project_folders.bat ... -DryRun"` | PASS |
| 공백 포함 `-Spec` 경로 dry-run | PASS |
| `git diff --check` | PASS |
| `tools/test/smoke/windows_command_wrappers.sh` | PASS |
| `UECF_DEBUG_ARGS` 기반 인자 재조립 확인 | PASS, `-Markdown`, `-AssetPaths`, `-RootPaths`, `-ApplyBuildCs`, `!` 포함 `-Spec` 확인 |
| unsafe cmd metacharacter 음성 테스트 | PASS, `&` 포함 값은 exit `2`로 거부 |
| commandlet name metacharacter 음성 테스트 | PASS, `Hello&echo ...` 형태는 exit `2`로 거부 |
| comma list tail 회귀 테스트 | PASS, `LooseArg`는 `-RootPaths`에 흡수되지 않음 |
| 설치본 `hello.bat` 재확인 | PASS, `Hello_257384142.json`, `.ok == true`, `.validation.engine_boot == "ok"` |
| 설치본 `validate_asset_rules.bat` 재확인 | COMMANDLET PASS / POLICY FAIL, `ValidateAssetRules_2580322502.json`, `.errors == []`, `.validation.issue_count == "18"` |
| 설치본 `create_project_folders.bat ... -DryRun` 재확인 | PASS, `CreateProjectFolders_2580322502.json`, `.dry_run == true`, `.created_folder_count == "0"` |

정책 위반 요약:

- `MISSING_DEPENDENCY`: 16건
- `REDIRECTOR_FOUND`: 1건
- `ASSET_PREFIX_MISMATCH`: 1건

## 현재 상태

- **브랜치:** `main`
- **워킹 트리:** 변경 있음
- **커밋/푸시:** 이번 후속 수정은 아직 커밋하지 않음
- **이전 Windows installer packaging 수정 커밋:** `e46e301 fix: support Windows installer packaging`

현재 변경:

```text
 M SESSION_SUMMARY.md
 M tools/test/smoke/windows_command_wrappers.sh
 M tools/ue/*.bat
?? docs/superpowers/plans/2026-06-01-windows-install-followup-validation.md
```

## 다음 작업

1. Unreal Editor GUI에서 `UECommandForge` plugin 로드 수동 확인
2. 대상 프로젝트의 asset policy 위반 18건을 별도 이슈로 처리
3. 이번 변경을 커밋하고 푸시

---

# 세션 요약 — 2026-05-28

## 수행 내용

### Phase 8 Task 6 — release packaging gate 보강

릴리즈 패키징 스크립트와 smoke를 독립 리뷰 기반으로 반복 수정했다. 이번 세션의 핵심은 `Tools.zip`, plugin zip, source zip의 검증 신뢰도를 높이고, 아직 구현되지 않은 installer 명령이 manifest에 노출되지 않도록 정리한 것이다.

| 항목 | 결과 |
|---|---|
| plugin package build | full build 전 `sample/Saved/PluginBuild`를 비우고 실제 platform output 기준으로 패키징 |
| version 검증 | package version과 packaged `.uplugin` `VersionName` 불일치 시 실패 |
| platform 검증 | `--skip-build`는 platform output이 정확히 1개일 때만 허용 |
| symlink 방지 | zip 생성 전 staging tree symlink 차단 |
| manifest checksum | plugin/tool/spec 파일과 `install.md`, `release-notes.md` 모두 checksum 필수 |
| validation report | plugin/tools/source package 모두 `validation-report.json` 생성 및 checksum 필수 |
| source package | tracked-file allowlist 기반으로 README, release-facing docs, sample plugin source, tools, specs 포함, `.git`, `docs/memory`, generated output 제외 |
| source version 검증 | package version과 packaged `.uplugin` `VersionName` 불일치 시 실패 |
| installer | plugin package는 대상 프로젝트 `Plugins/UECommandForge`, tools/specs는 Codex home `UECommandForge`에 설치 |
| installer hardening | symlink target 거부, unmanaged target overwrite/delete 거부, external checksum fail-closed |
| update | plugin/tools/specs 모두 backup, `update_install.sh`는 backup 강제 |
| uninstall | plugin/project link 제거, Codex tools/specs는 명시 옵션으로 제거, env/installed manifest 제거 |
| Windows validation | macOS에서는 실기 blocked report 생성, `.bat` wrapper 정적 검증 강화 |
| zip content 검증 | 실제 ZIP 파일 목록, manifest 파일 목록, checksum key 목록이 정확히 일치해야 통과 |
| external checksum | `checksums.txt`는 현재 ZIP basename 1개만 허용하고 SHA256을 직접 비교 |
| installer command | installer 구현 전까지 `install_commands: []`만 허용 |
| 문서 | `docs/superpowers/plans/ucf-phase8-prototype-automation-plan.md`에 리뷰-수정-검증 로그 업데이트 |

### 세션 마무리 및 에이전트 지침 정리

작업 종료 절차를 프로젝트 지침에 추가하고, 기존 `CLAUDE.md`의 에이전트 공통 규칙을 Codex가 기본 인식하는 `AGENTS.md`로 옮겼다.

| 항목 | 결과 |
|---|---|
| 작업 종료 규칙 | 사용자가 `작업 종료`라고 말하면 `SESSION_SUMMARY.md` 갱신, 커밋, 푸시를 수행하도록 명시 |
| 지침 파일 | `CLAUDE.md`를 `AGENTS.md`로 rename하고 제목/첫 문장을 AGENTS 기준으로 조정 |
| 원격 반영 | `main` 브랜치에 커밋과 푸시 완료 |

## 변경 파일

| 파일 | 내용 |
|---|---|
| `AGENTS.md` | 한글 문서 작성, 문서 저장 위치, 서브에이전트 리뷰, 작업 종료 규칙을 담은 프로젝트 에이전트 지침 |
| `CLAUDE.md` | `AGENTS.md`로 이동됨 |
| `CHANGELOG.md` | `0.8.0 - 2026-05-28` release entry 추가 |
| `tools/release/package_plugin.sh` | stale build output 제거, platform 감지/검증, `.uplugin` version 일치 검증, symlink 차단, checksum 기반 verify 호출 |
| `tools/release/package_source.sh` | source package 생성, generated output 제외, validation report 생성, checksum 기반 verify 호출 |
| `tools/release/package_tools.sh` | symlink 차단, installer 미구현 안내 정리, checksum 기반 verify 호출 |
| `tools/release/install_local.sh` | plugin/tools package 검증 후 프로젝트와 Codex home에 설치, manifest/env/project link/log 생성 |
| `tools/release/uninstall.sh` | plugin/project link 제거, Codex tools/specs 선택 제거 |
| `tools/release/update_install.sh` | backup을 켠 install flow로 update 수행 |
| `tools/release/write_windows_validation_report.sh` | Windows 실기 검증 제한 리포트 생성 |
| `install-uecommandforge.sh`, `.bat`, `.ps1` | root installer entrypoint |
| `tools/release/write_manifest.sh` | package 전체 payload checksum 생성, package type 기록, installer command 빈 배열 유지 |
| `tools/release/verify_release_package.sh` | zip-slip/path traversal/symlink/checksum/file-list 검증 강화 |
| `tools/test/smoke/release_package_plugin.sh` | full build 기본 smoke, skip-build는 opt-in, version/platform 음성 테스트 |
| `tools/test/smoke/release_package_source.sh` | source package 정상/변조 검증 |
| `tools/test/smoke/release_package_tools.sh` | 변조 ZIP/checksum 음성 테스트 확장 |
| `tools/test/smoke/release_package_install.sh` | package install/uninstall smoke |
| `tools/test/smoke/installer_install_update_uninstall.sh` | install/update/uninstall smoke |
| `tools/test/smoke/release_version_policy.sh` | `0.8.0` release version, CHANGELOG, README artifact 예시 regression 검증 |
| `tools/test/smoke/windows_release_validation_report.sh` | Windows 실기 제한 리포트 smoke |
| `docs/superpowers/plans/ucf-phase8-review-eval-report.md` | 배포/릴리즈 패키징 구간 리뷰 및 eval 리포트 |
| `README.md` | 릴리즈 패키지 생성, 검증, install/update/uninstall, Windows 제한 안내 |
| `docs/superpowers/plans/ucf-phase8-prototype-automation-plan.md` | Task 6 진행 로그, 독립 리뷰 findings, 수정/검증 결과 기록 |
| `SESSION_SUMMARY.md` | 이번 세션 요약 갱신 |

## 독립 리뷰 루프

독립 에이전트 리뷰를 반복 적용했다.

| 리뷰 라운드 | 주요 지적 | 처리 |
|---|---|---|
| 1차 | platform mislabel, plugin VersionName mismatch, 미구현 installer command 광고, checksum/zip traversal 부족 | 수정 완료 |
| 2차 | stale `PluginBuild`, checksum coverage 부족, exact `..` 경로, symlink dereference, `install_commands` 허용 범위 | 수정 완료 |
| 3차 | manifest 밖 추가 ZIP 파일 허용, 외부 `checksums.txt`가 현재 ZIP을 검증하는지 불명확 | 수정 완료 |
| 4차 | verifier 임시 파일명이 package 파일명과 충돌하는 reserved filename 우회 | 수정 완료 |
| 5차 | nested `uecommandforge-manifest.json` basename 제외 우회 | 수정 완료 |
| 최종 | `code-reviewer`, `security-reviewer` 모두 CRITICAL/HIGH/MEDIUM findings 없음 | 승인 |
| source package 서브에이전트 리뷰 | source package version mismatch, `docs/memory` 포함, generated output 제외 smoke 부족 | 수정 완료 |
| installer 서브에이전트 리뷰 | default uninstall 후 재설치 실패, plugin/tools version mismatch 허용, source zip 내부 계획 문서 포함, unmanaged metadata overwrite | 수정 완료 |
| release version pre-commit 리뷰 | `gpt-5.5 high` 리뷰에서 CRITICAL/HIGH/MEDIUM findings 없음 | 승인 |

## 검증 결과

| 검증 | 결과 |
|---|---|
| `bash -n tools/release/package_plugin.sh tools/release/package_tools.sh tools/release/write_manifest.sh tools/release/verify_release_package.sh tools/test/smoke/release_package_plugin.sh tools/test/smoke/release_package_tools.sh` | 통과 |
| `./tools/test/smoke/release_package_tools.sh` | 통과 |
| `./tools/test/smoke/release_package_source.sh` | 통과 |
| `UECF_RELEASE_PLUGIN_SKIP_BUILD=1 ./tools/test/smoke/release_package_plugin.sh` | 통과 |
| `./tools/test/smoke/release_package_plugin.sh` | full build 통과, `UECommandForge-0.8.0-UE5.7-Mac.zip` 생성 |
| `./tools/test/smoke/windows_command_wrappers.sh` | 통과 |
| `./tools/test/smoke/release_package_install.sh` | 통과 |
| `./tools/test/smoke/installer_install_update_uninstall.sh` | 통과 |
| `./tools/test/smoke/windows_release_validation_report.sh` | 통과 |
| `./tools/test/smoke/release_package_source.sh` | 통과 |
| `./tools/test/smoke/release_package_install.sh` 리뷰 regression 보강 후 | 통과 |
| `./tools/test/smoke/installer_install_update_uninstall.sh` 리뷰 수정 후 | 통과 |
| `./tools/test/smoke/release_package_tools.sh` 리뷰 수정 후 | 통과 |
| `./tools/test/smoke/windows_command_wrappers.sh` 리뷰 수정 후 | 통과 |
| `./tools/test/smoke/windows_release_validation_report.sh` 리뷰 수정 후 | 통과 |
| `UECF_RELEASE_PLUGIN_SKIP_BUILD=1 ./tools/test/smoke/release_package_plugin.sh` | 통과 |
| source package install guide regression | `installer scripts are not shipped yet` 문구 제거 및 `tools/release/install_local.sh` 안내 확인 |
| `./tools/test/smoke/release_version_policy.sh` | `0.8.0` release version, CHANGELOG, README artifact 예시 검증 |
| `tools/release/install_local.sh --run-commandlet-check true ...` | 샌드박스 외부 통과, UE Hello commandlet 기반 post-install 검증 |
| source zip internal/generated output scan | `docs/memory`, plugin `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache` 매치 없음 |
| `git diff --check` | 통과 |
| secret pattern scan | 이상 없음 |
| UE 관련 잔여 프로세스 확인 | 없음 |

## 현재 상태

- **브랜치:** `main`
- **워킹 트리:** clean
- **최근 커밋 흐름:** release version promotion, 작업 종료 규칙 추가, `AGENTS.md` 지침 이동
- **origin 대비:** `main`과 `origin/main` 동기화
- **푸시:** 완료

## 다음 작업

1. Phase 8 Task 6 남은 항목 진행
   - Windows 실제 호스트에서 `.bat` package/install wrapper 실기 검증
2. Phase 8 DataAsset/DataTable/Config 제품화 Task 진행
