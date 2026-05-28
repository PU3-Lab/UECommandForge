# UECommandForge Phase 8 리뷰 및 Eval 리포트

작성일: 2026-05-28

## 범위

대상 범위는 Phase 8 제품화 진행 중 배포 및 릴리즈 패키징 구간이다.

- 기준 범위: `b98718d..HEAD`
- 주요 변경:
  - source release package 생성 및 검증
  - plugin/tools/source package manifest, checksum, validation report 검증 강화
  - local install/update/uninstall script 추가
  - release package install smoke와 installer update/uninstall smoke 추가
  - Windows wrapper 정적 검증과 Windows host 제한 리포트 추가
  - README 설치 문서와 Phase 8 계획서 상태 갱신
  - Phase 8 release version `0.8.0` 승격과 CHANGELOG 작성

## 코드 리뷰 결과

결론: 차단 이슈 없음.

서브에이전트 리뷰를 사용했다.

| 리뷰 | 결과 |
|---|---|
| `reviewer` 1차 | High 1건, Medium 2건 발견 |
| `security_reviewer` 1차 | Medium 1건 발견 |
| `reviewer` 재리뷰 | Critical/High/Medium 없음 |
| `security_reviewer` 재리뷰 | Critical/High/Medium 없음 |
| `gpt-5.5 high` pre-commit review | Critical/High/Medium 없음 |

처리한 주요 발견 사항:

- default uninstall이 Codex tools/specs를 보존하면서 metadata를 삭제해 재설치를 막던 문제 수정
- unmanaged project/Codex metadata만 남아 있는 fresh install overwrite 거부
- plugin package와 tools package의 version/channel mismatch 설치 거부
- Source.zip에서 내부 `docs/superpowers/` planning docs 제외
- same-version/different-channel mismatch 자동 regression smoke 추가

잔여 리스크:

- Windows `.bat` wrapper는 macOS에서 실제 실행하지 못했고, 정적 검증과 `windows_host_required` 제한 리포트로 기록했다.
- installer smoke는 기본적으로 `--run-commandlet-check false`를 사용한다. UE commandlet 기반 post-install 검증은 Codex sandbox 내부에서 timeout이 발생해, macOS 로컬 UE 실행 환경에서 sandbox 외부로 별도 검증했다.
- plugin/tools 호환성은 현재 `version + release_channel` 기준이다. 같은 값으로 독립 빌드된 package 조합은 허용된다.

## Eval 정의

### Release Packaging Eval

| Eval | 성공 기준 | 결과 |
|---|---|---|
| `tools-package` | Tools.zip이 tools/specs/root installer/manifest/checksum/notes/report를 포함하고 검증을 통과한다 | PASS |
| `plugin-package` | plugin zip이 `.uplugin`, platform binary, manifest/checksum/report를 포함하고 version/platform 검증을 통과한다 | PASS |
| `source-package` | Source.zip이 release-facing source/tools/specs만 포함하고 generated/internal docs를 제외한다 | PASS |
| `package-verifier` | traversal, symlink, file-list drift, checksum drift, external checksum mismatch를 거부한다 | PASS |

### Installer Eval

| Eval | 성공 기준 | 결과 |
|---|---|---|
| `local-install` | plugin은 project `Plugins/UECommandForge`, tools/specs는 Codex home에 설치된다 | PASS |
| `managed-overwrite-guard` | unmanaged plugin/tools/specs/project metadata/Codex metadata overwrite를 거부한다 | PASS |
| `update-backup` | update는 backup을 강제하고 plugin/tools/specs를 모두 백업한다 | PASS |
| `default-uninstall-reinstall` | default uninstall 후 Codex tools/specs metadata가 보존되어 재설치가 가능하다 | PASS |
| `full-uninstall` | 명시 옵션으로 plugin/tools/specs/project link/env/installed manifest를 제거한다 | PASS |
| `release-match` | plugin/tools version 또는 release channel mismatch 설치를 거부한다 | PASS |

### Documentation Eval

| Eval | 성공 기준 | 결과 |
|---|---|---|
| `readme-install` | README가 package 생성, 검증, install, update, uninstall, smoke, Windows 제한을 설명한다 | PASS |
| `phase-plan-status` | Phase 8 계획서가 Task 6 완료/제한 상태와 리뷰 수정 결과를 반영한다 | PASS |
| `source-install-note` | Source.zip 내부 install guide가 installer 미구현이라고 잘못 안내하지 않는다 | PASS |
| `release-version-policy` | uplugin `VersionName`, README artifact 예시, CHANGELOG가 Phase 8 release version `0.8.0`을 일관되게 사용한다 | PASS |

## Eval 실행 결과

최종 재검증:

| 명령 | 결과 |
|---|---|
| `./tools/test/smoke/release_package_install.sh` | PASS |
| `./tools/test/smoke/release_package_source.sh` | PASS |
| `./tools/test/smoke/installer_install_update_uninstall.sh` | PASS |
| `./tools/test/smoke/release_package_tools.sh` | PASS |
| `./tools/test/smoke/release_package_plugin.sh` | PASS, sandbox 외부 full build |
| `UECF_RELEASE_PLUGIN_SKIP_BUILD=1 ./tools/test/smoke/release_package_plugin.sh` | PASS |
| `./tools/test/smoke/release_version_policy.sh` | PASS |
| `./tools/test/smoke/windows_command_wrappers.sh` | PASS |
| `./tools/test/smoke/windows_release_validation_report.sh` | PASS |
| `tools/release/install_local.sh --run-commandlet-check true ...` | PASS, sandbox 외부 |
| `bash -n install-uecommandforge.sh tools/release/*.sh tools/test/smoke/release_package_install.sh tools/test/smoke/installer_install_update_uninstall.sh tools/test/smoke/release_package_source.sh tools/test/smoke/windows_release_validation_report.sh tools/test/smoke/windows_command_wrappers.sh` | PASS |
| `git diff --check` | PASS |
| secret pattern scan | PASS |

## 최종 판정

Phase 8의 배포 및 릴리즈 패키징 구간은 macOS 로컬 검증 기준으로 통과 상태다.

완료된 핵심 범위:

- plugin/tools/source release package 생성
- manifest/checksum/validation report 기반 검증
- local install/update/uninstall
- installer security guard와 regression smoke
- Windows wrapper 정적 검증 및 제한 리포트
- UE Hello commandlet 기반 macOS post-install 검증
- Phase 8 release version `0.8.0` 승격 및 CHANGELOG 작성
- README 설치 문서와 review/eval report

남은 범위:

- Windows 실제 호스트에서 `.bat` package/install wrapper 실행 검증
- Phase 8 DataAsset/DataTable/Config 제품화 Task 진행
