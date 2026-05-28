# UECommandForge Phase 8 — Prototype 자동화 제품화 구현 계획

> 기준 문서: [references/prototype.md](references/prototype.md)
> 참고 상세안: [01_asset_path_naming_plan.md](references/01_asset_path_naming_plan.md), [05_cpp_class_generation_plan.md](references/05_cpp_class_generation_plan.md), [06_dataasset_datatable_config_plan.md](references/06_dataasset_datatable_config_plan.md)

## 목표

Phase 8은 prototype 문서의 3개 영역을 **뼈대나 MVP가 아니라 실제 프로젝트 운영에 투입 가능한 자동화 기능**으로 구현한다.

1. 에셋 생성 / 경로 / 네이밍 관리
2. C++ 클래스 생성 반복
3. DataAsset / DataTable / Config 관리

완료 기준은 단순 리포트 생성이 아니다. 각 영역은 아래 수준까지 포함해야 한다.

- 읽기 전용 분석
- 정책 기반 검증
- 사람이 검토 가능한 Markdown/JSON 리포트
- 안전장치가 있는 실제 적용 명령
- rollback plan 생성
- 적용 후 재검증
- 자동화 테스트와 스모크 테스트
- `.sh`와 Windows Command Prompt `.bat` 명령 표면
- README, 마스터 플랜, 리뷰/eval 리포트 업데이트

## 제품화 원칙

- 기존 Phase 1~7 패턴을 유지한다: Spec JSON 입력, UE Commandlet 실행, Result JSON 출력, Shell/Batch wrapper 제공.
- 위험 작업은 반드시 `-DryRun=true`가 기본값이고, 실제 적용은 `-Apply=true`와 명시적 spec이 필요하다.
- 에셋 move/rename/delete 같은 참조 영향 작업은 금지하지 않는다. 대신 preflight, rollback plan, post-validate를 필수로 한다.
- 모든 적용 명령은 `transaction_id`, `rollback_plan_path`, `changed_assets`, `changed_files`, `issues`를 Result JSON에 기록한다.
- 자동화는 실패 시 중간 상태를 숨기지 않고, 부분 성공 여부와 후속 복구 명령을 보고한다.
- Windows `.bat`는 정적 테스트만으로 완료하지 않는다. Phase 8 완료 전 Windows 실기 검증 항목을 별도 체크해야 한다.
- 작업 후 문서 업데이트와 리뷰/eval 리포트 작성은 완료 조건에 포함한다.

## 공통 Result JSON 계약

Phase 8부터 모든 신규 Commandlet은 기존 Result JSON 필드에 아래 필드를 추가로 지원한다.

| 필드 | 의미 |
|---|---|
| `issues[]` | 검증/적용 중 발견한 warning/error/info |
| `transaction_id` | 적용 작업 추적 ID |
| `dry_run` | dry-run 실행 여부 |
| `applied` | 실제 적용 여부 |
| `changed_assets[]` | 변경된 UE asset path |
| `changed_files[]` | 변경된 로컬 파일 path |
| `rollback_available` | rollback plan 사용 가능 여부 |
| `rollback_plan_path` | rollback plan JSON 경로 |
| `post_validation` | 적용 후 재검증 요약 |

`issues[]` 필드:

- `severity`: `error`, `warning`, `info`
- `code`
- `message`
- `field`
- `asset_path`
- `file_path`
- `suggested_fix`

## 명령 표면

| macOS/Linux/Git Bash | Windows Command Prompt | 목적 |
|---|---|---|
| `tools/ue/snapshot_assets.sh` | `tools\ue\snapshot_assets.bat` | AssetRegistry 기반 에셋 스냅샷 생성 |
| `tools/ue/validate_asset_rules.sh <policy.json>` | `tools\ue\validate_asset_rules.bat <policy.json>` | 에셋 경로/네이밍 정책 검증 |
| `tools/ue/plan_asset_changes.sh <plan.json>` | `tools\ue\plan_asset_changes.bat <plan.json>` | rename/move/delete/fixup 적용 전 preflight 및 rollback plan 생성 |
| `tools/ue/apply_asset_changes.sh <plan.json>` | `tools\ue\apply_asset_changes.bat <plan.json>` | 승인된 에셋 변경 적용 |
| `tools/ue/rollback_asset_changes.sh <rollback.json>` | `tools\ue\rollback_asset_changes.bat <rollback.json>` | 에셋 변경 rollback |
| `tools/ue/create_project_folders.sh <folders.json>` | `tools\ue\create_project_folders.bat <folders.json>` | 표준 폴더 구조 생성 |
| `tools/ue/generate_cpp_class.sh <spec.json>` | `tools\ue\generate_cpp_class.bat <spec.json>` | C++ 클래스 보일러플레이트 생성 |
| `tools/ue/validate_cpp_reflection.sh <policy.json>` | `tools\ue\validate_cpp_reflection.bat <policy.json>` | UPROPERTY/UFUNCTION 정책 검증 |
| `tools/ue/validate_buildcs.sh <policy.json>` | `tools\ue\validate_buildcs.bat <policy.json>` | Build.cs 의존성 검증 |
| `tools/ue/analyze_uht_log.sh <log>` | `tools\ue\analyze_uht_log.bat <log>` | UHT/build 로그 분석 |
| `tools/ue/validate_data_source.sh <schema.json> <source.csv>` | `tools\ue\validate_data_source.bat <schema.json> <source.csv>` | CSV/JSON 데이터 소스 검증 |
| `tools/ue/import_data_source.sh <schema.json> <source.csv>` | `tools\ue\import_data_source.bat <schema.json> <source.csv>` | 검증된 데이터 소스 import |
| `tools/ue/validate_datatable.sh <schema.json> <asset-path>` | `tools\ue\validate_datatable.bat <schema.json> <asset-path>` | UE DataTable Row 구조 검증 |
| `tools/ue/validate_config_rules.sh <schema.json>` | `tools\ue\validate_config_rules.bat <schema.json>` | Config 값 검증 |

## 파일 구조

| 경로 | 책임 |
|---|---|
| `sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Specs/AssetPolicySpec.h` | 에셋 정책 Spec 타입 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Specs/AssetChangePlanSpec.h` | 에셋 변경 계획 Spec 타입 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Specs/CppClassSpec.h` | C++ 클래스 생성 Spec 타입 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Specs/DataSchemaSpec.h` | 데이터 스키마 Spec 타입 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/AssetSnapshotCommandlet.*` | 에셋 스냅샷 Commandlet |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidateAssetRulesCommandlet.*` | 에셋 정책 검증 Commandlet |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/PlanAssetChangesCommandlet.*` | 에셋 변경 preflight 및 rollback plan 생성 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ApplyAssetChangesCommandlet.*` | 에셋 변경 적용 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/RollbackAssetChangesCommandlet.*` | 에셋 변경 rollback |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateProjectFoldersCommandlet.*` | 표준 폴더 생성 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/GenerateCppClassCommandlet.*` | C++ 클래스 생성 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidateCppReflectionCommandlet.*` | Reflection 정책 검증 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidateBuildCsCommandlet.*` | Build.cs 의존성 검증 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/AnalyzeUhtLogCommandlet.*` | UHT 로그 분석 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidateDataSourceCommandlet.*` | CSV/JSON 데이터 소스 검증 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ImportDataSourceCommandlet.*` | DataTable/DataAsset import |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidateDataTableCommandlet.*` | UE DataTable 검증 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidateConfigRulesCommandlet.*` | Config 검증 |
| `specs/policies/assets.policy.json` | 기본 에셋 경로/네이밍 정책 |
| `specs/policies/project_folders.json` | 표준 프로젝트 폴더 생성 Spec |
| `specs/policies/cpp_reflection.policy.json` | 기본 Reflection 정책 |
| `specs/policies/buildcs.policy.json` | 기본 Build.cs 정책 |
| `specs/schemas/items.schema.json` | 샘플 데이터 스키마 |
| `specs/examples/asset_change_plan.json` | 에셋 변경 계획 샘플 |
| `specs/examples/cpp_health_component.json` | C++ 생성 샘플 Spec |
| `specs/data/items.csv` | 데이터 검증 샘플 |
| `tools/test/smoke/prototype_automation.sh` | Phase 8 전체 스모크 검증 |

## Task 0: 공통 리포트, transaction, rollback 기반

진행 상태:

- 2026-05-28: `FCommandForgeValidationIssue`와 `FCommandForgeReport` 확장 필드 구현 완료.
- 2026-05-28: `JsonReportWriter`가 `issues`, `transaction_id`, `dry_run`, `applied`, `changed_assets`, `changed_files`, `rollback_plan_path`, `post_validation`을 직렬화하도록 확장 완료.
- 2026-05-28: `JsonReportWriter`가 `assets[]`를 직렬화하도록 확장되어 asset snapshot 리포트의 공통 출력 기반으로 재사용된다.
- 2026-05-28: `JsonReportWriterTest`에 공통 Result JSON 계약 검증 추가.
- 2026-05-28: `FCommandForgeRollbackPlan`, `FCommandForgeRollbackOperation`, `RollbackPlanWriter` 추가로 rollback plan JSON 생성 기반 구현 완료.
- 2026-05-28: `CommandForgePolicyParser` 추가로 Phase 8 정책 문서의 canonical JSON 파서 기반 구현 완료. YAML-like 입력은 현재 JSON 파서에서 명시적으로 실패 처리한다.
- 남은 항목: Task 1 이후 실제 asset/cpp/data commandlet에서 rollback plan 생성/소비 연결.
- 검증: RED 확인 시 `./tools/ue/build_plugin.sh`가 신규 테스트의 미구현 타입/필드 및 누락 header 컴파일 오류로 실패했고, 구현 후 `./tools/ue/build_plugin.sh`, sample `UnrealEditor` 타깃 빌드, `./tools/test/automation/run.sh`가 통과했다. 최종 자동화 결과는 PASS 18 / FAIL 0 / SKIP 0.

- [x] **Step 1: `FCommandForgeValidationIssue` 추가**

Result JSON `issues[]`를 지원한다.

- [x] **Step 2: transaction 필드 추가**

`FCommandForgeReport`에 `TransactionId`, `bDryRun`, `bApplied`, `ChangedAssets`, `ChangedFiles`, `RollbackPlanPath`, `PostValidation`을 추가한다.

- [x] **Step 3: rollback plan JSON 포맷 정의**

rollback plan은 최소 아래 정보를 가진다.

- transaction id
- commandlet
- timestamp
- planned operation
- before asset path/file path
- after asset path/file path
- dependency snapshot
- validation summary

- [x] **Step 4: 정책 파서 구현**

1차 구현은 JSON을 canonical format으로 삼는다. YAML은 optional import로만 허용하고, 내부에서는 JSON 구조로 정규화한다.

검증:
- `JsonReportWriterTest`에 `issues`, `transaction`, `rollback_plan_path` 직렬화 테스트 추가
- `./tools/test/automation/run.sh` 통과

## Task 1: 에셋 생성 / 경로 / 네이밍 관리 제품화

- [x] **Step 1: `AssetSnapshotCommandlet` 작성**

AssetRegistry에서 `/Game`과 project plugin content를 수집한다.

수집 필드:
- asset name
- package path
- object path
- asset class
- package name
- dependencies
- referencers
- redirector 여부
- disk path
- package dirty 여부

진행 상태:

- 2026-05-28: `AssetSnapshotCommandlet` 구현 완료. 기본 root path는 `/Game,/UECommandForge`이며, `-RootPaths=/Game/Tests`처럼 쉼표 구분 목록으로 좁혀 실행할 수 있다.
- 2026-05-28: Result JSON에 `assets[]`를 추가하고 `asset_name`, `package_path`, `object_path`, `asset_class`, `package_name`, `disk_path`, `is_redirector`, `package_dirty`, `dependencies`, `referencers`를 출력한다.
- 2026-05-28: macOS/Linux/Git Bash wrapper `tools/ue/snapshot_assets.sh`, Windows wrapper `tools/ue/snapshot_assets.bat` 추가.
- 2026-05-28: `run_commandlet.sh`에 `-nullrhi`를 추가해 wrapper commandlet을 headless 실행으로 고정했다. Codex 샌드박스 내부에서는 UE가 사용자 `~/Library/Application Support/Epic/...` 로그/Zen/DDC 초기화 접근에서 멈출 수 있어, 실제 UE commandlet smoke는 승인된 외부 실행으로 검증한다.
- 2026-05-28: 리뷰 후 `AssetSnapshotCommandlet`을 package 단위로 dedupe하도록 수정했다. Blueprint와 GeneratedClass가 같은 `package_name`으로 중복 출력되는 문제를 막고, `World` asset의 `disk_path`는 `.uasset`이 아니라 `.umap`으로 출력한다.
- 2026-05-28: `AssetSnapshotCommandletTest`가 Result JSON을 실제 파싱해 `assets[]` 존재, `package_name` 중복 없음, map asset `.umap` 경로를 검증하도록 보강했다.

검증:
- RED: `AssetSnapshotCommandletTest`를 먼저 추가했고, 구현 전 `./tools/ue/build_plugin.sh`가 누락된 `AssetSnapshotCommandlet.h`로 실패했다.
- GREEN: `./tools/ue/build_plugin.sh` 통과.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 18 / FAIL 0 / SKIP 0.
- GREEN: `./tools/test/smoke/windows_command_wrappers.sh` 통과.
- GREEN: `UE_COMMANDLET_TIMEOUT=90 ./tools/ue/snapshot_assets.sh -RootPaths=/Game/Tests` 외부 실행 통과. 생성 리포트 `sample/Saved/CodexReports/AssetSnapshot_20260528T023445Z.json`의 `asset_count`는 package dedupe 후 `11`, map asset 2개는 `.umap`, duplicate package count는 `0`.

- [x] **Step 2: `ValidateAssetRulesCommandlet` 작성**

정책 기반으로 prefix/path/redirector/broken reference/unused candidate를 검증한다.

검증 항목:
- class별 prefix/suffix
- class별 allowed path
- plugin content와 game content 분리 정책
- redirector 존재 여부
- dependency 누락
- 참조 없는 에셋 후보
- policy exception 적용

진행 상태:

- 2026-05-28: `FCommandForgeAssetPolicySpec`와 asset policy rule/exception/redirector/unused candidate 타입 추가.
- 2026-05-28: `ValidateAssetRulesCommandlet` 구현 완료. `-Policy=<asset.policy.json>`와 `-RootPaths=/Game/Tests`를 받아 AssetRegistry 기반으로 prefix, suffix, allowed path, redirector, missing dependency, unused candidate를 `issues[]`에 기록한다.
- 2026-05-28: 기본 정책 `specs/policies/assets.policy.json` 추가. `/Script/Engine.Blueprint`는 `BP_`, `/Script/StateTreeModule.StateTree`는 `ST_`, `/Script/Engine.World`는 `/Game` 경로를 기본 허용한다.
- 2026-05-28: macOS/Linux/Git Bash wrapper `tools/ue/validate_asset_rules.sh`, Windows wrapper `tools/ue/validate_asset_rules.bat`, smoke `tools/test/smoke/asset_policy.sh` 추가.
- 2026-05-28: `ValidateAssetRulesCommandletTest`가 의도적 위반 policy로 `ASSET_PREFIX_MISMATCH`, `ASSET_PATH_NOT_ALLOWED`, `UNUSED_ASSET_CANDIDATE` 이슈와 `ValidationFailed` exit code를 검증한다.
- 2026-05-28 리뷰 반영: policy exception 적용 전에 package dedupe/count를 확정해 `asset_count` 의미를 유지하고, Windows wrapper의 추가 인자 수집 방식을 기존 wrapper 패턴과 맞췄다.

검증:
- RED: `ValidateAssetRulesCommandletTest`를 먼저 추가했고, 구현 전 `./tools/ue/build_plugin.sh`가 누락된 `ValidateAssetRulesCommandlet.h`로 실패했다.
- GREEN: `./tools/ue/build_plugin.sh` 통과.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 19 / FAIL 0 / SKIP 0.
- GREEN: `./tools/test/smoke/windows_command_wrappers.sh` 통과.
- GREEN: `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/asset_policy.sh` 외부 실행 통과. 생성 리포트 `ValidateAssetRules_20260528T030803Z.json`의 `ok=true`, `asset_count=11`, `issue_count=0`.

- [x] **Step 3: `PlanAssetChangesCommandlet` 작성**

rename/move/delete/fixup 작업을 실제 적용 전 preflight한다.

지원 작업:
- create folder
- rename asset
- move asset
- fix redirector
- delete unused asset candidate

제약:
- delete는 `allow_delete: true`와 explicit asset list가 없으면 실패
- wildcard delete 금지
- referencer가 있는 delete는 실패
- move/rename은 redirector fixup plan을 함께 생성

진행 상태:

- 2026-05-28: `FCommandForgeAssetChangePlanSpec`와 operation 타입 추가.
- 2026-05-28: `PlanAssetChangesCommandlet` 구현 완료. `-Plan=<asset_change_plan.json>`를 받아 create folder, rename asset, move asset, fix redirector, delete unused asset candidate 작업을 실제 적용 없이 preflight한다.
- 2026-05-28: move/rename 요청은 source 존재, target 충돌, package path 유효성을 검증하고 rollback plan에 원 작업과 redirector fixup 작업을 함께 기록한다.
- 2026-05-28: delete 요청은 wildcard 금지, `allow_delete=true`, `delete_assets` 명시 목록, referencer 없음 조건을 검증한다.
- 2026-05-28: sample plan `specs/policies/asset_changes.plan.json`, macOS/Linux/Git Bash wrapper `tools/ue/plan_asset_changes.sh`, Windows wrapper `tools/ue/plan_asset_changes.bat`, smoke `tools/test/smoke/asset_change_plan.sh` 추가.
- 2026-05-28 리뷰 반영: Unity build에서 익명 namespace helper명이 충돌하지 않도록 `ValidateAssetRules` root path helper명을 고유화하고, `PlanAssetChanges` helper를 고유 namespace로 분리했다.
- 2026-05-28 리뷰 반영: sample plan의 rollback path 상대 경로 의존을 제거하고, 기본 `Saved/CodexReports/rollback_<transaction>.json` 경로를 smoke가 Result JSON에서 읽도록 변경했다.
- 2026-05-28 리뷰 반영: plan JSON의 `rollback_plan`이 임의 파일 경로로 쓰이지 않도록 상대 경로는 `Saved/CodexReports` 아래로 해석하고, 절대 경로나 `..`로 report 디렉터리를 벗어나는 경로는 `ROLLBACK_PATH_OUTSIDE_REPORT_DIR`로 거부한다.

검증:
- RED: `PlanAssetChangesCommandletTest`를 먼저 추가했고, 구현 전 `./tools/ue/build_plugin.sh`가 누락된 `PlanAssetChangesCommandlet.h`로 실패했다.
- GREEN: `./tools/ue/build_plugin.sh` 통과. 리뷰 수정 후 재실행해 `PlanAssetChangesCommandlet.cpp`와 `PlanAssetChangesCommandletTest.cpp` 재컴파일 포함 확인.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과. 이 과정에서 Unity build helper명 충돌을 발견해 수정 후 재통과했다.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 22 / FAIL 0 / SKIP 0. `RejectsRollbackPathOutsideReports`가 `rollback_plan` 경로 탈출 방지를 검증한다.
- GREEN: `./tools/test/smoke/windows_command_wrappers.sh` 통과.
- GREEN: `git diff --check` 통과.
- GREEN: secret pattern scan 통과. 기존 문서의 설명성 `secret/API key` 문자열만 매칭됐다.
- BLOCKED: `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/asset_change_plan.sh`는 `UnrealEditor-Cmd`가 macOS LaunchServices 경고 직후 UE 로그 없이 멈춰 timeout 124로 실패했다. 동일 시점 `UE_COMMANDLET_TIMEOUT=0 ./tools/ue/hello.sh`와 직접 `UnrealEditor-Cmd -run=Hello`도 같은 증상으로 멈춰 코드 경로가 아닌 로컬 UE commandlet launch 환경 문제로 분리했고, 리뷰 수정 후 재실행해도 동일 timeout 124가 재현됐다.
- RESOLVED: 2026-05-28 재검증에서 원인은 Codex 샌드박스 내부 실행으로 분리됐다. 승인된 샌드박스 외부 실행에서는 `UE_COMMANDLET_TIMEOUT=90 ./tools/ue/hello.sh`와 `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/asset_change_plan.sh`가 정상 통과했다.
- GREEN: `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/asset_change_plan.sh` 승인된 외부 실행 통과. `PlanAssetChanges_20260528T075210Z.json` 기준 `ok=true`, `operation_count=3`, `planned_change_count=3`, `rollback_available=true`.

- [x] **Step 4: `ApplyAssetChangesCommandlet` 작성**

승인된 plan을 실제 적용한다.

필수 동작:
- 작업 전 snapshot 저장
- UE AssetTools API 사용
- 작업 후 AssetRegistry 재스캔
- post validation 실행
- rollback plan 저장
- Result JSON에 changed assets 기록

진행 상태:

- 2026-05-28: `ApplyAssetChangesCommandlet` 추가. `-Plan=<asset_change_plan.json>`와 `-Output=<report.json>`를 받아 승인된 plan을 실제 적용한다.
- 2026-05-28: apply 전 `Saved/CodexReports/snapshot_<transaction_id>.json`에 작업 대상 package의 존재 여부와 disk path snapshot을 저장한다.
- 2026-05-28: create folder는 long package name을 검증한 뒤 Content 디렉터리를 생성하고, rename/move는 `AssetTools.RenameAssets`를 사용한다.
- 2026-05-28: delete는 `allow_delete=true`, `delete_assets` 명시 목록, wildcard 금지, referencer 없음 조건을 만족할 때만 `ObjectTools::ForceDeleteObjects`로 적용한다.
- 2026-05-28: `fix_redirector`는 AssetRegistry에서 `ObjectRedirector`를 수집해 `AssetTools.FixupReferencers`를 실행한다.
- 2026-05-28: 작업 후 AssetRegistry를 재스캔하고, 생성 폴더 존재 여부, rename/move target 존재 여부, delete 대상 제거 여부를 post validation으로 확인한다.
- 2026-05-28: rollback plan을 저장하고 Result JSON에 `dry_run=false`, `applied`, `rollback_available`, `changed_assets`, `changed_files`, `post_validation`을 기록한다.
- 2026-05-28 리뷰 반영: Unity build에서 테스트 helper명이 충돌하지 않도록 `PlanAssetChangesCommandletTest`와 `ValidateAssetRulesCommandletTest`의 JSON loader명을 고유화했다.
- 2026-05-28 리뷰 반영: 부분 적용 실패 시에도 `applied=true`를 유지하고, 실제 적용된 변경이 있으면 실패 report에서도 rollback plan을 저장하도록 수정했다.

검증:
- RED: `ApplyAssetChangesCommandletTest`를 먼저 추가했고, 구현 전 `./tools/ue/build_plugin.sh`가 누락된 `ApplyAssetChangesCommandlet.h`로 실패했다.
- GREEN: `./tools/ue/build_plugin.sh` 통과.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과. 테스트 helper명 Unity build 충돌 수정 후 재통과했다.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 24 / FAIL 0 / SKIP 0. `CreatesFolderAndRollbackPlan`, `RejectsUnsafeDelete`가 새로 포함됐다.
- GREEN: `git diff --check` 통과.
- GREEN: secret pattern scan 통과. 기존 문서의 설명성 `secret/API key` 문자열만 매칭됐다.

- [x] **Step 5: `RollbackAssetChangesCommandlet` 작성**

rollback plan을 읽어 가능한 작업을 되돌린다.

필수 동작:
- rollback 전 현재 상태 검증
- path collision 검사
- rollback 후 post validation
- rollback 실패 시 수동 복구 지침 기록

진행 상태:

- 2026-05-28: `RollbackAssetChangesCommandlet` 추가. `-RollbackPlan=<rollback.json>`와 `-Output=<report.json>`를 받아 rollback plan을 JSON으로 파싱한다.
- 2026-05-28: plan operations를 역순으로 실행해 apply 순서와 반대로 되돌리도록 구현했다.
- 2026-05-28: `rename_asset`/`move_asset` rollback은 현재 asset 존재 여부와 복구 경로 충돌을 검사한 뒤 `AssetTools.RenameAssets`로 원래 path로 복구한다.
- 2026-05-28: `delete_folder` rollback은 생성된 빈 폴더만 삭제한다. 리뷰 중 비어 있지 않은 폴더 재귀 삭제 위험을 발견해 `ROLLBACK_FOLDER_NOT_EMPTY`로 중단하고 수동 복구 지침을 남기도록 보강했다.
- 2026-05-28: `fix_redirector` rollback 항목은 `ObjectRedirector`를 수집해 `AssetTools.FixupReferencers`를 실행한다.
- 2026-05-28: 삭제된 asset 자동 복원은 source control/backup 의존 작업이므로 `MANUAL_RESTORE_REQUIRED` issue와 수동 복구 지침을 남긴다.
- 2026-05-28: 허용 mount path 판정이 `/GameFoo` 같은 유사 prefix를 통과시키지 않도록 `/Game/` 및 `/UECommandForge/` 기준으로 엄격화했다.
- 2026-05-28: rollback 후 AssetRegistry를 재스캔하고, 복구 asset 존재 여부와 삭제 폴더 제거 여부를 post validation으로 확인한다.

검증:
- RED: `RollbackAssetChangesCommandletTest`를 먼저 추가했고, 구현 전 `./tools/ue/build_plugin.sh`가 누락된 `RollbackAssetChangesCommandlet.h`로 실패했다.
- GREEN: `./tools/ue/build_plugin.sh` 통과. 리뷰 수정 후 재실행해 `RollbackAssetChangesCommandlet.cpp`와 `RollbackAssetChangesCommandletTest.cpp` 재컴파일 포함 확인.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 27 / FAIL 0 / SKIP 0. `DeletesCreatedFolder`, `RejectsPathCollision`, `RejectsNonEmptyFolderDelete`가 새로 포함됐다.
- GREEN: `git diff --check` 통과.
- GREEN: secret pattern scan 통과. 기존 문서의 설명성 `secret/API key` 문자열과 코드의 `Tokens` 변수명만 매칭됐다.

- [x] **Step 6: wrapper와 테스트 추가**

추가 파일:
- `tools/ue/snapshot_assets.sh/.bat`
- `tools/ue/validate_asset_rules.sh/.bat`
- `tools/ue/plan_asset_changes.sh/.bat`
- `tools/ue/apply_asset_changes.sh/.bat`
- `tools/ue/rollback_asset_changes.sh/.bat`
- `tools/ue/create_project_folders.sh/.bat`
- `tools/test/smoke/asset_policy.sh`
- `tools/test/smoke/asset_change_apply_rollback.sh`

인수 조건:
- snapshot이 변경 없이 성공
- policy 위반 샘플이 error issue를 생성
- 폴더 생성 apply가 실제 폴더를 생성
- rename/move apply 후 참조가 유지됨
- rollback 후 원래 path로 복구됨
- delete는 안전 조건 없으면 실패
- 자동화 테스트와 스모크 테스트 통과

진행 상태:

- 2026-05-28: macOS/Linux/Git Bash wrapper `tools/ue/apply_asset_changes.sh`, `tools/ue/rollback_asset_changes.sh` 추가.
- 2026-05-28: Windows wrapper `tools/ue/apply_asset_changes.bat`, `tools/ue/rollback_asset_changes.bat` 추가.
- 2026-05-28: apply/rollback 전체 흐름 smoke `tools/test/smoke/asset_change_apply_rollback.sh` 추가. plan 생성, apply 적용, rollback 실행, report JSON과 `/Game/Tests/Planned` 폴더 생성/삭제를 검증한다.
- 2026-05-28: `windows_command_wrappers.sh`에 apply/rollback Windows wrapper 정적 검증을 추가했다.
- 2026-05-28: `CreateProjectFoldersCommandlet` 추가. 기본은 dry-run이며 `-Apply`가 있을 때만 `/Game` 또는 `/UECommandForge` 아래 표준 폴더를 생성한다.
- 2026-05-28: folder spec `specs/policies/project_folders.json`, macOS/Linux/Git Bash wrapper `tools/ue/create_project_folders.sh`, Windows wrapper `tools/ue/create_project_folders.bat` 추가.
- 2026-05-28: `CreateProjectFoldersCommandlet`는 요청 폴더의 부모 계층까지 안전하게 확장하고, 실제 새로 생성한 폴더만 rollback plan에 기록한다. rollback은 역순 삭제와 빈 폴더 삭제 guard를 사용하므로 기존 폴더는 rollback 대상에 포함하지 않는다.

검증:
- RED: wrapper 구현 전 `./tools/test/smoke/windows_command_wrappers.sh`가 신규 `apply_asset_changes.bat` 누락으로 실패했다.
- RED: wrapper 구현 전 `tools/test/smoke/asset_change_apply_rollback.sh`가 실행 권한 보정 후 신규 `apply_asset_changes.sh` 부재를 검출하는 경로까지 진행될 예정이었으나, 선행 `PlanAssetChanges` commandlet 실행이 macOS LaunchServices/hiservices 경고 후 정지하는 기존 로컬 UE commandlet launch 이슈에 먼저 걸렸다.
- RED: `CreateProjectFoldersCommandletTest`를 먼저 추가했고, 구현 전 `./tools/ue/build_plugin.sh`가 누락된 `CreateProjectFoldersCommandlet.h`로 실패했다.
- GREEN: `./tools/ue/build_plugin.sh` 통과.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 29 / FAIL 0 / SKIP 0. `DryRunDoesNotCreateFolders`, `ApplyCreatesFoldersAndRollbackPlan`, `RejectsUnsafeFolderPath`가 새로 포함됐다.
- GREEN: `./tools/test/smoke/windows_command_wrappers.sh` 통과.
- GREEN: `tools/ue/apply_asset_changes.sh` 인자 누락 시 사용법과 exit 2 반환 확인.
- GREEN: `tools/ue/rollback_asset_changes.sh` 인자 누락 시 사용법과 exit 2 반환 확인.
- GREEN: `tools/ue/create_project_folders.sh` 인자 누락 시 사용법과 exit 2 반환 확인.
- BLOCKED: `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/asset_change_apply_rollback.sh`는 `PlanAssetChanges` 시작 직후 macOS LaunchServices/hiservices 경고 뒤 UE 로그 없이 멈춰 timeout 124로 실패했다. 이는 Step 3에서 기록한 직접 `UnrealEditor-Cmd -run=Hello` hang과 같은 로컬 commandlet launch 문제이며, apply/rollback wrapper 로직 전 단계에서 재현된다.
- RESOLVED: 2026-05-28 재검증에서 원인은 Codex 샌드박스 내부 실행으로 분리됐다. 승인된 샌드박스 외부 실행에서는 `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/asset_change_apply_rollback.sh`가 정상 통과했다.
- GREEN: `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/asset_change_apply_rollback.sh` 승인된 외부 실행 통과. `ApplyAssetChanges_20260528T075217Z.json` 기준 `ok=true`, `applied=true`, `applied_change_count=3`, `rollback_available=true`. `RollbackAssetChanges_20260528T075224Z.json` 기준 `ok=true`, `applied=true`, `applied_rollback_count=5`.
- GREEN: `./tools/test/smoke/windows_command_wrappers.sh` 재실행 통과.
- 참고: apply/rollback smoke는 rollback 후 원래 path와 폴더 상태를 복구했지만, UE가 `sample/Content/Tests/BP_TestCharacter.uasset`, `sample/Content/Tests/BP_TestController.uasset`를 다시 저장해 binary dirty 상태를 남겼다. `.uasset` 변경은 커밋 지시가 있을 때 함께 포함한다.

## Task 2: C++ 클래스 생성 반복 제품화

- [x] **Step 1: `CppClassSpec` 정의**

지원 타입:
- `Actor`
- `ActorComponent`
- `UObject`
- `DataAsset`
- `Interface`
- `GameInstanceSubsystem`
- `WorldSubsystem`
- `DeveloperSettings`

Spec 필드:
- class name
- module
- output path
- base class
- generated header include
- properties
- functions
- delegates
- includes
- Build.cs dependencies
- reflection metadata

진행 상태:

- 2026-05-28: `FCommandForgeCppClassSpec`, class info, property/function/param metadata, Build.cs dependency spec 타입 추가.
- 2026-05-28: canonical JSON 입력은 `kind=cpp_class`로 받으며, `class`, `includes`, `properties`, `functions`, `build_dependencies`를 파싱한다.
- 2026-05-28: 샘플 spec `specs/examples/cpp_health_component.json` 추가. `UCFHealthComponent` ActorComponent dry-run 생성 preview를 검증하는 기준 spec이다.

- [x] **Step 2: `GenerateCppClassCommandlet` 작성**

스펙 기반으로 `.h`/`.cpp`를 생성한다.

필수 동작:
- 기존 파일 존재 시 실패
- module root 밖 경로 실패
- dry-run preview 생성
- apply 시 파일 생성
- 생성 파일 목록과 checksum 기록
- Build.cs dependency plan 생성

진행 상태:

- 2026-05-28: `GenerateCppClassCommandlet` 추가. `-Spec=<cpp-class.json>`와 `-Output=<report.json>`를 받아 dry-run preview를 만들고, `-Apply`가 있을 때만 `.h/.cpp`를 생성한다.
- 2026-05-28: module root는 sample `Source/<Module>` 또는 plugin `Source/<Module>` 아래에서 찾고, 생성 대상은 `Public/<output_path>/<Class>.h`, `Private/<output_path>/<Class>.cpp`로 제한한다.
- 2026-05-28: `output_path`가 절대 경로, `..`, wildcard를 포함하거나 module root 밖으로 나가면 `OUTPUT_PATH_OUTSIDE_MODULE` issue로 실패한다.
- 2026-05-28: 기존 파일이 있으면 `CPP_CLASS_FILE_EXISTS`로 실패한다.
- 2026-05-28: header/source preview, 생성 경로, class symbol, SHA1 checksum, dependency plan을 Result JSON `validation`에 기록한다.
- 2026-05-28: ActorComponent 템플릿은 `UCLASS(BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))`, `UPROPERTY`, `UFUNCTION`, constructor tick 설정, property default 값을 생성한다.
- 2026-05-28 리뷰 반영: include와 metadata macro 출력 순서를 결정적으로 고정해 같은 spec이 항상 같은 preview/checksum을 만들도록 수정했다.
- 2026-05-28 리뷰 반영: include path, module/build dependency, base/property/function/param type, metadata key/value, property default의 안전 토큰 검증을 추가했다. 따옴표/개행/상위 경로 등 C++ 코드 생성 경계에서 위험한 입력은 `INVALID_INCLUDE` 등 validation issue로 거부한다.
- 2026-05-28 리뷰 반영: automation apply 테스트는 class별 `GeneratedTests/<Class>` 하위만 생성/삭제하고, 테스트 후 빈 `GeneratedTests` 루트 폴더도 정리하도록 보강했다.

검증:
- RED: `GenerateCppClassCommandletTest`를 먼저 추가했고, 구현 전 `./tools/ue/build_plugin.sh`가 누락된 `GenerateCppClassCommandlet.h`로 실패했다.
- GREEN: `./tools/ue/build_plugin.sh` 통과.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과. `build_plugin.sh`는 패키징용 HostProject를 빌드하므로 automation 실행 바이너리에 새 테스트를 링크하려면 sample 프로젝트의 `UnrealEditor` 타깃 빌드가 별도로 필요했다.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 33 / FAIL 0 / SKIP 0. `DryRunReportsPreview`, `ApplyWritesFilesAndChecksums`, `RejectsOutputOutsideModule`가 새로 포함됐다.
- GREEN: 리뷰 수정 후 `./tools/ue/build_plugin.sh` 재실행 통과.
- GREEN: 리뷰 수정 후 sample `UnrealEditor` 타깃 빌드 통과.
- GREEN: 리뷰 수정 후 `./tools/test/automation/run.sh` 통과, PASS 34 / FAIL 0 / SKIP 0. `RejectsInvalidCppTokens`가 추가되어 code-generation token validation을 검증한다.
- GREEN: `git diff --check` 통과.
- GREEN: secret pattern scan 통과. 기존 문서의 설명성 `secret/API key` 문자열과 테스트명의 `invalid token` 문자열만 매칭됐다.
- GREEN: `find sample/Plugins/UECommandForge/Source/UECommandForgeRuntime -path '*GeneratedTests*' -print` 결과 없음. GenerateCppClass automation이 임시 생성 파일/폴더를 남기지 않는다.

- [x] **Step 3: Build.cs 적용/검증 구현**

`ValidateBuildCsCommandlet`은 아래를 검증한다.

- Public/Private dependency 누락
- 사용하지 않는 dependency 후보
- module path 정책
- include path 정책

`GenerateCppClassCommandlet`은 `-ApplyBuildCs=true`일 때만 Build.cs를 수정한다.

진행 상태:

- 2026-05-28: `BuildCsDependencyUtils`를 추가해 module root 탐색, `*.Build.cs` 위치 확인, Public/Private dependency 누락 검출, dependency 삽입을 공통화했다.
- 2026-05-28: `ValidateBuildCsCommandlet` 추가. `kind=buildcs_policy` JSON의 `modules[].required_public`, `modules[].required_private`를 검증하고 module별 Build.cs path, missing dependency, issue count를 Result JSON에 기록한다.
- 2026-05-28: `GenerateCppClassCommandlet`에 `-ApplyBuildCs=true`를 추가했다. `-Apply`와 함께 요청된 경우에만 생성 spec의 Build.cs dependency plan을 실제 Build.cs에 적용하고, `buildcs_changed`, `buildcs_missing_public`, `buildcs_missing_private`, `changed_files`를 리포트한다.
- 2026-05-28: 정책 예제 `specs/policies/buildcs.policy.json` 추가. Runtime/Editor module의 필수 dependency를 commandlet smoke 기준으로 검증한다.
- 2026-05-28: `tools/ue/validate_buildcs.sh`, `tools/ue/validate_buildcs.bat` 추가. macOS/Linux wrapper는 파일 인자를 절대 경로로 정규화해 UE commandlet cwd 차이와 무관하게 정책 파일을 읽는다.
- 2026-05-28 리뷰 반영: 처음 추가한 helper 경로 `Private/Build/`가 `.gitignore`의 `Build/` 규칙에 걸려 커밋 누락 위험이 있어 `Private/BuildCs/`로 이동했다. `git check-ignore`로 추적 가능 상태를 확인했다.
- 2026-05-28 리뷰 반영: `required_public`/`required_private`가 문자열 배열이 아니거나 dependency token이 C++/Build.cs 식별자로 안전하지 않으면 `INVALID_DEPENDENCY_ARRAY`, `INVALID_BUILD_DEPENDENCY`로 실패하도록 보강했다.
- 2026-05-28 리뷰 반영: `ValidateBuildCsCommandletTest`에 `RejectsInvalidDependencyName`를 추가했고, sample 타깃을 다시 빌드해 automation discovery에 새 테스트가 포함되는 것을 확인했다.
- 2026-05-28 수정 리뷰 반복 반영: `BuildCsDependencyUtils`의 dependency 존재 검사를 첫 `AddRange` 블록에만 한정하지 않고 동일 list의 모든 `AddRange` 블록을 순회하도록 수정했다.
- 2026-05-28 수정 리뷰 반복 반영: `GenerateCppClassCommandlet`의 `build_dependencies.public/private` 파싱에서 JSON number가 문자열로 변환 통과되지 않도록 `EJson::String` 타입을 명시 검증한다.
- 2026-05-28 수정 리뷰 반복 반영: `GenerateCppClassCommandlet -ApplyBuildCs=true`는 Build.cs 누락을 C++ 파일 쓰기 전에 실패 처리하고, Build.cs 적용 후 class 파일 저장 실패 시 생성 파일 삭제와 Build.cs 원복을 수행한다.
- 2026-05-28 테스트 보강: `ValidateBuildCs.AcceptsDependencyInLaterAddRangeBlock`, `GenerateCppClass.RejectsInvalidBuildDependencyArray`, `GenerateCppClass.MissingBuildCsDoesNotWriteClassFiles` automation test를 추가했다.

검증:
- RED: 수정 전 `./tools/test/automation/run.sh`에서 41개 테스트 중 `ValidateBuildCs.AcceptsDependencyInLaterAddRangeBlock`가 실패했다. 첫 `AddRange`만 검색해 두 번째 `PublicDependencyModuleNames.AddRange`의 `AIModule`을 누락으로 오판한 것을 확인했다.
- RED: 1차 수정 후 `./tools/test/automation/run.sh`에서 `GenerateCppClass.RejectsInvalidBuildDependencyArray`가 실패했다. Unreal JSON의 `TryGetString()` 경로가 number를 문자열로 변환해 `SpecParseFailed` 대신 `ValidationFailed`가 반환되는 것을 확인했다.
- GREEN: `./tools/ue/build_plugin.sh` 통과.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 38 / FAIL 0 / SKIP 0. `ValidateBuildCs.PassesSatisfiedPolicy`, `ValidateBuildCs.ReportsMissingDependency`, `ValidateBuildCs.RejectsInvalidDependencyName`, `GenerateCppClass.ApplyBuildCsDependencies`가 포함됐다.
- GREEN: 수정 리뷰 반복 후 `./tools/ue/build_plugin.sh` 재통과.
- GREEN: 수정 리뷰 반복 후 sample `UnrealEditor` 타깃 빌드 재통과.
- GREEN: 수정 리뷰 반복 후 `./tools/test/automation/run.sh` 재통과, PASS 41 / FAIL 0 / SKIP 0.
- GREEN: `UE_COMMANDLET_TIMEOUT=90 ./tools/ue/validate_buildcs.sh specs/policies/buildcs.policy.json` 승인된 샌드박스 외부 실행 통과. `ValidateBuildCs_20260528T083437Z.json` 기준 `ok=true`, `checked_module_count=2`, `issue_count=0`.
- GREEN: 수정 리뷰 반복 후 `UE_COMMANDLET_TIMEOUT=90 ./tools/ue/validate_buildcs.sh specs/policies/buildcs.policy.json` 재통과.
- GREEN: `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/cpp_generation.sh` 승인된 샌드박스 외부 실행 재통과.
- GREEN: `./tools/test/smoke/windows_command_wrappers.sh` 통과. `generate_cpp_class.bat`, `validate_buildcs.bat` 정적 검증을 포함한다.
- GREEN: `git diff --check` 통과.
- GREEN: secret pattern scan 통과. 기존 문서의 설명성 `secret/API key` 문자열과 테스트명의 `invalid token` 문자열만 매칭됐다.
- GREEN: `find sample/Plugins/UECommandForge/Source -maxdepth 1 -type d -name 'CodexBuildCsTestModule' -print` 결과 없음. Build.cs apply automation이 임시 module을 남기지 않는다.
- GREEN: 수정 리뷰 반복 후 `CodexBuildCsTestModule`, `CodexValidateBuildCsTestModule`, `CodexMissingBuildCsTestModule`, Runtime `GeneratedTests` 잔여 경로 없음.
- GREEN: `git check-ignore` 기준 `Private/BuildCs/BuildCsDependencyUtils.*`는 ignore되지 않는다.
- 참고: 첫 `validate_buildcs.sh` 실행은 상대 정책 경로가 UE commandlet cwd 기준으로 해석되어 `POLICY_READ_FAILED`가 발생했다. wrapper에서 파일 인자를 절대 경로로 정규화해 수정했다.

- [x] **Step 4: Reflection 정책 검증 구현**

`ValidateCppReflectionCommandlet` 검증 항목:
- `UPROPERTY` Category 누락
- `BlueprintCallable` Category 누락
- runtime mutable state의 `EditAnywhere + BlueprintReadWrite`
- component property의 `VisibleAnywhere` 권장 위반
- `Config` property의 config class 위치 위반
- `UFUNCTION`/`UPROPERTY` macro와 선언 연결 오류

구현 상태:
- 2026-05-28: `ValidateCppReflectionCommandlet` 추가. `kind=cpp_reflection_policy` 정책을 읽어 `files[]` 직접 지정과 `modules[]` 기반 header scan을 모두 지원한다.
- 2026-05-28: Build.cs module root resolver를 재사용해 module `Public`/`Private` header를 재귀적으로 수집한다.
- 2026-05-28: Result JSON에 `checked_file_count`, `issue_count`, 파일별 `property_count`, `function_count`를 기록하고, 정책 위반이 있으면 `ValidationFailed`로 종료한다.
- 2026-05-28: automation test `ValidateCppReflection.PassesCleanHeader`, `ValidateCppReflection.ReportsPolicyIssues` 추가.
- 2026-05-28: 리뷰 반영으로 `modules[].include_paths`는 module root 안의 안전한 상대 경로만 허용하도록 보강하고, `ValidateCppReflection.RejectsUnsafeIncludePath`를 추가했다.
- 2026-05-28: unity build에서 anonymous namespace helper명이 충돌하지 않도록 `ValidateBuildCsCommandlet.cpp`의 helper 호출을 namespace-qualified 형태로 정리했다.

검증:
- RED: commandlet 구현 전 `./tools/ue/build_plugin.sh`가 `ValidateCppReflectionCommandlet.h` 누락으로 실패했다.
- GREEN: 구현 후 `./tools/ue/build_plugin.sh` 통과.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 43 / FAIL 0 / SKIP 0. Reflection policy automation 2건을 포함한다.
- GREEN: 리뷰 정리 후 sample `UnrealEditor` 타깃 빌드 재통과.
- GREEN: include path 보강 후 `./tools/ue/build_plugin.sh` 재통과.
- GREEN: include path 보강 후 sample `UnrealEditor` 타깃 빌드 재통과.
- GREEN: include path 보강 후 `./tools/test/automation/run.sh` 재통과, PASS 44 / FAIL 0 / SKIP 0. Reflection policy automation 3건을 포함한다.
- GREEN: `UE_COMMANDLET_TIMEOUT=30 ./tools/ue/hello.sh`는 Codex 샌드박스 내부에서 macOS LaunchServices/hiservices 경고 후 timeout됐지만, 승인된 샌드박스 외부 실행 `/bin/zsh -lc "UE_COMMANDLET_TIMEOUT=90 ./tools/ue/hello.sh"`는 통과했다. 공통 `UnrealEditor-Cmd -run` launch timeout은 wrapper나 Reflection 로직 문제가 아니라 샌드박스 실행 제약으로 분리했다.
- GREEN: `/bin/zsh -lc "UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/cpp_reflection_policy.sh"` 승인된 샌드박스 외부 실행 통과. 위반 header fixture 기반 reflection issue code 6종 런타임 검증까지 완료했다.

- [x] **Step 5: UHT 로그 분석 구현**

`AnalyzeUhtLogCommandlet`은 build log에서 UHT 오류를 추출하고 원인/수정 후보를 Result JSON과 Markdown으로 기록한다.

구현 상태:
- 2026-05-28: `AnalyzeUhtLogCommandlet` 추가. `-Log`, `-Output`, 선택 `-Markdown` 인자를 받아 UHT error/warning line을 추출한다.
- 2026-05-28: 대표 UHT 원인 분류를 `UHT_UNKNOWN_REFLECTED_TYPE`, `UHT_PRIVATE_BLUEPRINT_ACCESS`, `UHT_GENERATED_HEADER_ORDER`, `UHT_MISSING_CATEGORY`, `UHT_ERROR`, `UHT_WARNING` issue code로 기록한다.
- 2026-05-28: Result JSON의 `validation`에 `log_path`, `markdown_path`, `line_count`, `error_count`, `warning_count`, `issue_count`를 기록하고, Markdown 표 형태의 원인/수정 후보 리포트를 생성한다.
- 2026-05-28: automation test `AnalyzeUhtLog.ReportsErrorsAndMarkdown`, `AnalyzeUhtLog.PassesCleanLog` 추가.

검증:
- RED: `AnalyzeUhtLogCommandletTest.cpp`를 먼저 추가했고, 구현 전 `./tools/ue/build_plugin.sh`가 누락된 `AnalyzeUhtLogCommandlet.h`로 실패했다.
- GREEN: `./tools/ue/build_plugin.sh` 통과. 중간에 Unreal 전역 `LogPath` 로그 카테고리와 지역 변수명 shadow를 발견해 `UhtLogPath`로 정리 후 재통과했다.
- GREEN: sample `UnrealEditor` 타깃 빌드 통과. 샌드박스 내부 UBT 로그 파일 접근은 `UnauthorizedAccessException`으로 실패해 승인된 샌드박스 외부 실행으로 검증했다.
- GREEN: `./tools/test/automation/run.sh` 통과, PASS 46 / FAIL 0 / SKIP 0. UHT 로그 분석 automation 2건을 포함한다.
- GREEN: 리뷰 수정 후 파일 라인이 없는 diagnostic의 Markdown location 표기를 `log line` 기반으로 보강했고, `./tools/ue/build_plugin.sh`, sample `UnrealEditor` 타깃 빌드, `./tools/test/automation/run.sh`를 재실행해 PASS 46 / FAIL 0 / SKIP 0을 확인했다.

- [x] **Step 6: wrapper와 테스트 추가**

추가 파일:
- `tools/ue/generate_cpp_class.sh/.bat`
- `tools/ue/validate_cpp_reflection.sh/.bat`
- `tools/ue/validate_buildcs.sh/.bat`
- `tools/ue/analyze_uht_log.sh/.bat`
- `tools/test/smoke/cpp_generation.sh`
- `tools/test/smoke/cpp_reflection_policy.sh`
- `tools/test/smoke/uht_log_analysis.sh`

인수 조건:
- dry-run이 생성 preview와 Build.cs plan을 기록
- apply가 샘플 class `.h/.cpp`를 생성
- 생성 후 `./tools/ue/build_plugin.sh` 통과
- Build.cs 검증 리포트 생성
- reflection policy 위반 샘플 검출
- UHT 오류 샘플 분석 리포트 생성

진행 상태:

- 2026-05-28: `tools/ue/generate_cpp_class.sh`, `tools/ue/generate_cpp_class.bat` 추가.
- 2026-05-28: dry-run smoke `tools/test/smoke/cpp_generation.sh` 추가. 샘플 spec을 실행해 `ok=true`, `dry_run=true`, `applied=false`, `class_symbol`, `header_preview`, `header_sha1`, `changed_files=[]`를 검증한다.
- 2026-05-28: `windows_command_wrappers.sh`에 `generate_cpp_class.bat` 정적 검증 추가.
- 2026-05-28: `tools/ue/validate_buildcs.sh`, `tools/ue/validate_buildcs.bat` 추가.
- 2026-05-28: `windows_command_wrappers.sh`에 `validate_buildcs.bat` 정적 검증 추가.
- 2026-05-28: `tools/ue/validate_cpp_reflection.sh`, `tools/ue/validate_cpp_reflection.bat` 추가.
- 2026-05-28: `tools/test/smoke/cpp_reflection_policy.sh` 추가. 위반 header fixture로 reflection issue code 6종을 검증하도록 구성했다.
- 2026-05-28: `windows_command_wrappers.sh`에 `validate_cpp_reflection.bat` 정적 검증 추가.
- 2026-05-28: `tools/ue/analyze_uht_log.sh`, `tools/ue/analyze_uht_log.bat` 추가.
- 2026-05-28: `tools/test/smoke/uht_log_analysis.sh` 추가. 샘플 UHT log fixture로 JSON error/warning count, issue code, Markdown 수정 제안을 검증한다.
- 2026-05-28: `windows_command_wrappers.sh`에 `analyze_uht_log.bat` 정적 검증 추가.
- 2026-05-28: `tools/test/smoke/cpp_generation.sh`를 dry-run 전용에서 apply + 실제 plugin build 검증까지 확장했다. 생성된 `UCFHealthComponent.h/.cpp`는 smoke 종료 시 cleanup된다.

검증:
- GREEN: `./tools/test/smoke/windows_command_wrappers.sh` 통과.
- GREEN: `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/cpp_generation.sh` 승인된 샌드박스 외부 실행 통과.
- GREEN: 리뷰 수정 후 `./tools/test/smoke/windows_command_wrappers.sh` 재실행 통과.
- GREEN: 리뷰 수정 후 `UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/cpp_generation.sh` 재실행 통과.
- GREEN: `UE_COMMANDLET_TIMEOUT=90 ./tools/ue/validate_buildcs.sh specs/policies/buildcs.policy.json` 통과.
- GREEN: `./tools/test/smoke/windows_command_wrappers.sh` 재실행 통과. `validate_cpp_reflection.bat` 정적 검증을 포함한다.
- GREEN: `UE_COMMANDLET_TIMEOUT=30 ./tools/ue/hello.sh`는 Codex 샌드박스 내부에서 timeout됐고, `/bin/zsh -lc "UE_COMMANDLET_TIMEOUT=90 ./tools/ue/hello.sh"` 승인된 샌드박스 외부 실행은 통과했다. 공통 launch timeout은 샌드박스 실행 제약으로 분리했다.
- GREEN: `/bin/zsh -lc "UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/cpp_reflection_policy.sh"` 승인된 샌드박스 외부 실행 통과. `validate_cpp_reflection.sh` wrapper와 `cpp_reflection_policy.sh` fixture 기반 issue 검출 경로가 검증됐다.
- GREEN: `./tools/test/smoke/windows_command_wrappers.sh` 재실행 통과. `analyze_uht_log.bat` 정적 검증을 포함한다.
- GREEN: `/bin/zsh -lc "UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/uht_log_analysis.sh"` 승인된 샌드박스 외부 실행 통과. `AnalyzeUhtLog` JSON/Markdown smoke가 검증됐다.
- GREEN: 리뷰 수정 후 `./tools/test/smoke/windows_command_wrappers.sh`와 `/bin/zsh -lc "UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/uht_log_analysis.sh"`를 재실행해 통과했다.
- RED: `cpp_generation.sh` apply 확장 1차 실행은 header 검증 grep이 실제 생성 형태 `UCLASS(BlueprintType, ClassGroup=...)`와 맞지 않아 실패했다.
- GREEN: grep 조건을 `UCLASS(BlueprintType`로 수정 후 `/bin/zsh -lc "UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/cpp_generation.sh"` 승인된 샌드박스 외부 실행 통과. `build_plugin.log`에서 `UCFHealthComponent.cpp`가 Mac Development/Shipping 빌드에 포함되어 컴파일됐고, smoke 종료 후 `Generated/Components` 잔여 파일 없음도 확인했다.

남은 항목:
- 없음

## Task 2.5: 중간 실사용 검증

배포 스크립트를 만들기 전에, repo-root에서만 동작하는 개발자 smoke가 아니라 실제 설치 형태에 가까운 사용 흐름을 먼저 검증한다.

검증 가정:
- UE 플러그인은 대상 프로젝트에 설치되어 있다.
- Codex가 호출하는 `tools/ue`, `tools/test`, `specs`는 `~/.codex/UECommandForge`와 같은 Codex 전용 루트에 설치된다.
- Codex 도구 루트의 wrapper는 `UECF_PROJECT_FILE` 또는 `PROJECT_FILE`로 대상 `.uproject`를 찾고, Result JSON은 대상 프로젝트의 `Saved/CodexReports`에 기록한다.

- [x] **Step 1: Codex 도구 루트 분리 실행 지원**

구현 상태:
- 2026-05-28: `tools/ue/ue_env.sh`와 `tools/ue/ue_env.bat`가 `UECF_PROJECT_FILE`을 우선 사용하도록 수정했다. 기존 `PROJECT_FILE` override도 보존하고, 둘 다 없으면 repo sample project를 기본값으로 사용한다.
- 2026-05-28: `tools/ue/run_commandlet.sh`와 `tools/ue/run_commandlet.bat`가 Result JSON을 repo-root가 아니라 대상 project directory의 `Saved/CodexReports`에 쓰도록 수정했다. 고급 override는 `UECF_REPORT_DIR`로 제공한다.
- 2026-05-28: `windows_command_wrappers.sh`에 `UECF_PROJECT_FILE`, `UECF_REPORT_DIR` 정적 검증을 추가했다.

검증:
- RED: `tools/test/smoke/mid_real_project_flow.sh`를 먼저 추가해 Codex 전용 루트 복사본에서 `Hello`를 실행했고, 기존 wrapper가 복사된 루트 아래의 `sample/UECommandForgeSample.uproject`를 찾으려 해 실패했다.
- GREEN: wrapper override 수정 후 `/bin/zsh -lc "UE_COMMANDLET_TIMEOUT=90 ./tools/test/smoke/mid_real_project_flow.sh"` 승인된 샌드박스 외부 실행 통과.
- GREEN: 기존 repo-root 기본 실행도 `/bin/zsh -lc "UE_COMMANDLET_TIMEOUT=90 ./tools/ue/hello.sh"` 승인된 샌드박스 외부 실행으로 재검증해 통과했다.
- REVIEW-FIX: `mid_real_project_flow.sh`가 `latest_report`로 stale JSON을 재사용할 수 있어, 각 wrapper stdout에서 실제 Result JSON path 패턴만 캡처해 검증하도록 수정했다. Unreal Trace Server 종료 로그가 path 뒤에 섞이는 경우도 같이 보정했다. Windows wrapper도 `.uproject\..` 대신 `%%~dpI`로 project directory를 계산하도록 보강했다.

- [x] **Step 2: 중간 실사용 smoke 추가**

추가 파일:
- `tools/test/smoke/mid_real_project_flow.sh`

검증 흐름:
- repo `tools`와 `specs`를 `sample/Saved/CodexReports/MidRealProject/codex-home/UECommandForge`에 복사한다.
- `UECF_PROJECT_FILE`로 sample `.uproject`를 지정한다.
- Codex 도구 루트에서 `Hello`, `AssetSnapshot`, `ValidateAssetRules`, `ValidateBuildCs`, `GenerateCppClass` dry-run, `ValidateCppReflection` 실패 리포트, `AnalyzeUhtLog` 실패 리포트/Markdown을 실행한다.
- 결과 요약은 `sample/Saved/CodexReports/MidRealProject/mid_real_project_flow.md`에 기록한다.

검증 결과:
- GREEN: `mid_real_project_flow.md` 기준 PASS 8개.
- PASS: Codex tools root에서 `Hello` 실행
- PASS: Codex tools root에서 asset snapshot 실행
- PASS: Codex specs root에서 asset policy 실행
- PASS: Codex specs root에서 Build.cs policy 실행
- PASS: Codex specs root에서 C++ generation dry-run 실행
- PASS: project fixture 기반 reflection policy violation 리포트 생성
- PASS: UHT log analysis JSON/Markdown 리포트 생성

## 진행 순서 변경: 배포/설치 게이트 우선

2026-05-28 결정: Task 3 DataAsset/DataTable/Config 제품화는 뒤로 미룬다. 중간 실사용 smoke로 repo-root 분리 실행 가능성은 확인했지만, 아직 실제 배포 산출물과 자동 설치 흐름이 고정되지 않았다.

다음 우선순위:
1. Task 6 배포 및 릴리즈 계획
2. Task 6의 패키징/자동 설치/설치 검증 smoke
3. Task 3 DataAsset/DataTable/Config 관리 제품화
4. Task 4 통합 워크플로우

이유:
- Codex 도구는 `~/.codex/UECommandForge`에 설치되고, UE plugin은 대상 프로젝트에 설치되는 구조가 먼저 안정되어야 한다.
- 이후 DataAsset/DataTable/Config commandlet도 같은 설치/검증 경로에서 바로 테스트할 수 있어야 한다.
- repo-local 기능 추가를 더 진행하면 배포 전환 시 wrapper, spec path, report path 검증을 반복 수정할 가능성이 높다.

## Task 3: DataAsset / DataTable / Config 관리 제품화

상태: 후순위로 이동. Task 6의 배포 산출물, 자동 설치 스크립트, 설치 검증 smoke를 먼저 완료한 뒤 진행한다.

- [ ] **Step 1: `DataSchemaSpec` 정의**

지원 필드 타입:
- `string`
- `text`
- `int`
- `float`
- `bool`
- `enum`
- `soft_object_path`
- `gameplay_tag`
- `class_path`
- `data_table_row_handle`

정책:
- required
- unique
- min/max
- regex
- allowed_values
- enum source
- gameplay tag source
- severity
- exception

- [ ] **Step 2: `ValidateDataSourceCommandlet` 작성**

CSV/JSON source를 스키마와 비교한다.

검증 항목:
- 필수 필드 누락
- 중복 ID
- 잘못된 타입
- 범위 위반
- regex 위반
- enum 값 불일치
- GameplayTag 미등록
- SoftObjectPath 누락
- UTF-8 인코딩 문제

- [ ] **Step 3: `ImportDataSourceCommandlet` 작성**

검증된 source를 UE DataTable 또는 DataAsset set으로 import한다.

필수 동작:
- dry-run diff
- apply import
- 변경 전 backup/rollback plan
- import 후 datatable validation
- source hash 기록

- [ ] **Step 4: `ValidateDataTableCommandlet` 작성**

UE DataTable asset을 로드하고 RowStruct/RowName/필드 값을 검증한다.

검증 항목:
- DataTable asset 존재
- RowStruct 필드와 schema 일치
- RowName 또는 key field 중복
- SoftObjectPath 참조 존재
- GameplayTag 등록 여부
- enum 값 일치

- [ ] **Step 5: `ValidateConfigRulesCommandlet` 작성**

project config를 읽어 schema와 비교한다.

검증 항목:
- 필수 config key 존재
- bool/int/float/string 타입 일치
- min/max 범위
- allowlist 값 일치
- 누락 config default 제안

- [ ] **Step 6: wrapper와 테스트 추가**

추가 파일:
- `tools/ue/validate_data_source.sh/.bat`
- `tools/ue/import_data_source.sh/.bat`
- `tools/ue/validate_datatable.sh/.bat`
- `tools/ue/validate_config_rules.sh/.bat`
- `tools/test/smoke/data_validation.sh`
- `tools/test/smoke/data_import_rollback.sh`

인수 조건:
- 중복 ID 샘플 실패
- 필수 필드 누락 샘플 실패
- 범위 위반 샘플 실패
- 정상 샘플 PASS
- DataTable import apply 후 UE asset 검증 PASS
- rollback 후 기존 DataTable 상태 복구
- config schema 위반 리포트 생성

## Task 4: 통합 워크플로우

- [ ] **Step 1: `PrototypeAutomationCommandlet` 작성**

asset policy, C++ policy, data policy를 한 번에 실행하는 통합 검증 Commandlet을 추가한다.

- [ ] **Step 2: `tools/ue/validate_project_rules.sh/.bat` 추가**

프로젝트 전체 품질 게이트 명령을 제공한다.

- [ ] **Step 3: 통합 Markdown 리포트 생성**

`sample/Saved/CodexReports/PrototypeAutomation_<UTC>.md`에 사람이 읽는 요약 리포트를 생성한다.

## Task 5: 문서와 README 업데이트

- [ ] **Step 1: README 명령 표면 갱신**

Phase 8 명령 표면을 macOS/Linux/Git Bash와 Windows Command Prompt 기준으로 추가한다.

- [ ] **Step 2: 마스터 플랜 갱신**

Phase 8 상태, 검증 결과, 남은 리스크를 기록한다.

- [ ] **Step 3: 리뷰/eval 리포트 작성**

`docs/superpowers/plans/ucf-phase8-review-eval-report.md`를 작성한다.

## Task 6: 배포 및 릴리즈 계획

Phase 8은 내부 개발용 코드만 머지하는 것으로 끝내지 않는다. 사용자가 새 프로젝트에 설치하고 검증할 수 있는 배포 산출물까지 만든다.

- [x] **Step 1: 배포 채널 정의**

1차 배포 채널:
- GitHub Release artifact
- zip 패키지 다운로드
- 자동 설치 스크립트
- source checkout 후 `tools/release/install_local.sh`로 local package 설치

2차 배포 채널:
- 사내/팀 plugin registry
- Unreal Marketplace 제출 후보 패키지

초기 공식 배포 방식은 GitHub Release artifact + 자동 설치 스크립트다. Marketplace는 심사/메타데이터/라이선스 검토가 필요하므로 Phase 8 완료 후 별도 배포 트랙으로 분리한다.

상태:
- 2026-05-28: 중간 실사용 smoke 결과를 기준으로 GitHub Release artifact + 자동 설치 스크립트를 1차 배포 채널로 확정했다. Marketplace는 별도 후속 트랙으로 유지한다.

- [x] **Step 2: 배포 산출물 정의**

릴리즈마다 아래 산출물을 생성한다.

| 산출물 | 설명 |
|---|---|
| `UECommandForge-<version>-UE5.7-Mac.zip` | macOS 검증 plugin package |
| `UECommandForge-<version>-UE5.7-Win64.zip` | Windows 검증 plugin package |
| `UECommandForge-<version>-Source.zip` | source 포함 배포 패키지 |
| `UECommandForge-<version>-Tools.zip` | `tools/ue`, `tools/test`, `tools/release`, 기본 specs 포함 도구 패키지 |
| `install-uecommandforge.sh` | macOS/Linux/Git Bash 자동 설치 스크립트 |
| `install-uecommandforge.ps1` | Windows PowerShell 자동 설치 스크립트 |
| `install-uecommandforge.bat` | Windows Command Prompt 자동 설치 스크립트 |
| `uecommandforge-manifest.json` | 플러그인, tools, specs, wrapper 설치 manifest |
| `checksums.txt` | SHA256 checksum |
| `release-notes.md` | 기능, breaking change, migration, 검증 결과 |
| `install.md` | 프로젝트 설치 및 wrapper 사용법 |
| `validation-report.json` | build/test/smoke/eval 결과 |

상태:
- 2026-05-28: Task 6 산출물 목록을 정의했다. 이번 진행에서는 우선 `UECommandForge-<version>-Tools.zip`, `uecommandforge-manifest.json`, `checksums.txt`, `install.md`, `release-notes.md`를 생성/검증하는 최소 tools package 게이트를 구현했다.

- [ ] **Step 3: 패키징 스크립트 추가**

추가 파일:
- `tools/release/package_plugin.sh`
- `tools/release/package_plugin.bat`
- `tools/release/package_source.sh`
- `tools/release/package_source.bat`
- `tools/release/package_tools.sh`
- `tools/release/package_tools.bat`
- `tools/release/write_checksums.sh`
- `tools/release/verify_release_package.sh`
- `tools/release/verify_release_package.bat`
- `tools/release/write_manifest.sh`
- `tools/release/write_manifest.bat`

`package_plugin` 동작:
- `./tools/ue/build_plugin.sh` 실행
- `sample/Saved/PluginBuild` 산출물 확인
- 불필요한 Intermediate/Saved/DerivedDataCache 제거
- `UECommandForge.uplugin` version/versionName 확인
- zip 생성
- checksum 생성
- install manifest 생성
- release notes 초안 생성

진행 상태:
- 2026-05-28: `tools/release/package_tools.sh`, `tools/release/write_manifest.sh`, `tools/release/write_checksums.sh`, `tools/release/verify_release_package.sh`를 추가했다.
- 2026-05-28: `tools/test/smoke/release_package_tools.sh`를 추가해 Tools zip에 `tools/ue`, `tools/test`, `tools/release`, `specs`, manifest, install notes, release notes가 포함되는지 검증한다.
- 2026-05-28: `tools/release/package_plugin.sh`와 `tools/test/smoke/release_package_plugin.sh`를 추가했다. 기본 동작은 `tools/ue/build_plugin.sh`를 먼저 실행한다. 빠른 로컬 재검증만 `UECF_RELEASE_PLUGIN_SKIP_BUILD=1`로 기존 `sample/Saved/PluginBuild` 산출물을 재사용할 수 있다.
- 2026-05-28: Windows Command Prompt 호환 wrapper `package_plugin.bat`, `package_tools.bat`, `verify_release_package.bat`, `write_checksums.bat`, `write_manifest.bat`를 추가했다. Windows wrapper는 Git Bash의 `bash`를 호출하는 compatibility layer로 시작한다.

검증:
- RED: `./tools/test/smoke/release_package_tools.sh` 1차 실행은 `tools/release/package_tools.sh`가 없어 실패했다.
- GREEN: 최소 tools packaging 스크립트 구현 후 `./tools/test/smoke/release_package_tools.sh` 통과.
- REVIEW-FIX: `unzip -l | grep -q`가 `pipefail` 환경에서 SIGPIPE 141을 낼 수 있어, zip file list를 `unzip -Z1`로 파일에 저장한 뒤 grep하도록 수정했다.
- REVIEW-FIX: release 입력값 보안 검토 후 `--version`, `--channel`은 안전한 문자만 허용하고, zip 검증은 압축 해제 전에 absolute path와 `..` entry를 차단하도록 보강했다.
- RED: `./tools/test/smoke/release_package_plugin.sh` 1차 실행은 `tools/release/package_plugin.sh`가 없어 실패했다.
- GREEN: plugin packaging 스크립트 구현 후 `./tools/test/smoke/release_package_plugin.sh` 통과. `UECommandForge-<VersionName>-UE5.7-Mac.zip`에 `.uplugin`, Mac binaries, Source Build.cs, manifest, install/release notes 포함을 확인했다.
- GREEN: manifest 확장 후 `./tools/test/smoke/release_package_tools.sh`, `./tools/test/smoke/windows_command_wrappers.sh`, `git diff --check` 재실행 통과.
- REVIEW-FIX: `package_plugin.sh` full build 경로에서 `--out-dir` 상대 경로를 전달하면 build 성공 후 zip 생성이 실패하는 문제를 확인했다. `package_plugin.sh`와 `package_tools.sh` 모두 `OUT_DIR`를 절대 경로로 정규화하도록 수정했다.
- GREEN: 상대 `--out-dir` 보정 후 `./tools/test/smoke/release_package_plugin.sh`와 `./tools/test/smoke/release_package_tools.sh` 재실행 통과.
- GREEN: `/bin/zsh -lc "./tools/release/package_plugin.sh --version 0.1.0 --channel local-smoke --out-dir sample/Saved/CodexReports/ReleasePackagePluginFull"` 승인된 샌드박스 외부 실행 통과. `build_plugin.sh`가 Mac Development/Shipping plugin build를 완료하고 `UECommandForge-0.1.0-UE5.7-Mac.zip`을 생성했다.
- INDEPENDENT-REVIEW: `code-reviewer`, `security-reviewer`, `reviewer` 독립 리뷰에서 platform mislabel, plugin VersionName/package version mismatch, 미구현 installer 명령 광고, zip traversal/checksum 검증 부족, skip-build smoke 의존성 문제가 지적됐다.
- REVIEW-FIX: `package_plugin.sh`가 `--version`과 packaged `.uplugin` VersionName 불일치 시 실패하도록 수정했다. `--platform`은 `Binaries/<Platform>` 산출물이 실제로 있을 때만 통과한다.
- REVIEW-FIX: manifest의 `install_commands`는 installer 구현 전까지 빈 배열로 두고, install notes는 수동 복사/`UECF_PROJECT_FILE` 사용 안내로 변경했다.
- REVIEW-FIX: `verify_release_package.sh`가 backslash/drive-style/UNC path, symlink zip entry, manifest path traversal을 거부하고 manifest 내부 파일 checksum 및 외부 `checksums.txt`를 검증하도록 보강했다.
- REVIEW-FIX: `release_package_plugin.sh`는 기본 full build smoke로 변경하고, 빠른 재검증은 `UECF_RELEASE_PLUGIN_SKIP_BUILD=1`일 때만 허용한다.
- GREEN: 독립 리뷰 수정 후 `./tools/test/smoke/release_package_plugin.sh` full build smoke 통과. `UECommandForge-0.1.0-UE5.7-Mac.zip` 생성과 manifest/checksum 검증을 확인했다.
- GREEN: 독립 리뷰 수정 후 `./tools/test/smoke/release_package_tools.sh`, `./tools/test/smoke/windows_command_wrappers.sh`, `git diff --check` 재실행 통과.
- INDEPENDENT-REVIEW-2: 재리뷰에서 `sample/Saved/PluginBuild` stale output 재사용 가능성, manifest checksum coverage 부족, exact `..` 경로 우회, zip 생성 전 symlink dereference 가능성, installer 명령 허용 범위가 다시 지적됐다.
- REVIEW-FIX-2: `package_plugin.sh` full build는 packaging 전에 `sample/Saved/PluginBuild`를 비우고 다시 빌드한다. `--skip-build`는 platform output이 정확히 1개일 때만 허용하고, 기본 platform은 실제 build output에서 감지한다.
- REVIEW-FIX-2: `package_plugin.sh`와 `package_tools.sh`는 zip 생성 전 staging tree의 symlink를 거부한다. `write_manifest.sh`는 `install.md`, `release-notes.md`까지 checksum에 포함한다.
- REVIEW-FIX-2: `verify_release_package.sh`는 `..`, `*/..`, backslash, drive-style, UNC-like entry를 거부하고, manifest의 모든 plugin/tool/spec 파일과 `install.md`, `release-notes.md` checksum coverage를 필수화했다. installer가 아직 없으므로 `install_commands`는 빈 배열만 허용한다.
- GREEN: 재리뷰 수정 후 `bash -n tools/release/package_plugin.sh tools/release/package_tools.sh tools/release/write_manifest.sh tools/release/verify_release_package.sh tools/test/smoke/release_package_plugin.sh tools/test/smoke/release_package_tools.sh` 통과.
- GREEN: 재리뷰 수정 후 `./tools/test/smoke/release_package_tools.sh`, `UECF_RELEASE_PLUGIN_SKIP_BUILD=1 ./tools/test/smoke/release_package_plugin.sh`, `./tools/test/smoke/release_package_plugin.sh` full build smoke, `./tools/test/smoke/windows_command_wrappers.sh`, `git diff --check` 통과.
- INDEPENDENT-REVIEW-3: 독립 `code-reviewer`/`security-reviewer` 재리뷰에서 standalone verifier가 manifest 밖의 추가 ZIP 파일을 허용하고, 외부 `checksums.txt`가 실제 release zip basename을 검증하는지 강제하지 않는 문제가 HIGH로 지적됐다.
- REVIEW-FIX-3: `verify_release_package.sh`가 실제 ZIP 파일 목록, manifest 파일 목록, manifest checksum key 목록이 정확히 일치하는지 비교하도록 수정했다. manifest 밖의 추가 파일, checksum 누락, checksum extra key는 모두 실패한다.
- REVIEW-FIX-3: 외부 `checksums.txt`는 정확히 한 항목만 허용하고, 항목 filename이 현재 ZIP basename과 일치해야 하며, SHA256 hash를 직접 비교하도록 수정했다. 다른 파일 checksum, 경로 포함 filename, 추가 항목은 거부한다.
- GREEN: REVIEW-FIX-3 후 `bash -n ...`, `./tools/test/smoke/release_package_tools.sh`, `UECF_RELEASE_PLUGIN_SKIP_BUILD=1 ./tools/test/smoke/release_package_plugin.sh`, `./tools/test/smoke/release_package_plugin.sh` full build smoke, `./tools/test/smoke/windows_command_wrappers.sh`, `git diff --check`, secret pattern scan, UE 잔여 프로세스 확인 통과.
- INDEPENDENT-REVIEW-4: 재확인에서 verifier 비교용 임시 파일명이 패키지 내부 파일명과 충돌할 때 `.expected-files.txt` 같은 unlisted file이 제외될 수 있다고 지적됐다.
- REVIEW-FIX-4: file-list 비교용 임시 파일을 추출 루트 밖 별도 temp directory로 이동하고, root dotfile `.expected-files.txt`를 추가한 변조 ZIP도 실패하도록 smoke를 보강했다.
- GREEN: REVIEW-FIX-4 후 `bash -n tools/release/verify_release_package.sh tools/test/smoke/release_package_tools.sh`, `./tools/test/smoke/release_package_tools.sh` 통과.
- INDEPENDENT-REVIEW-5: 재확인에서 `find ! -name 'uecommandforge-manifest.json'`가 root manifest뿐 아니라 nested 동일 basename 파일까지 제외해 unlisted file 우회가 가능하다고 지적됐다.
- REVIEW-FIX-5: actual file list 생성 시 root `./uecommandforge-manifest.json`만 제외하도록 `! -path './uecommandforge-manifest.json'`로 수정하고, `tools/extra/uecommandforge-manifest.json` 변조 ZIP smoke를 추가했다.
- GREEN: REVIEW-FIX-5 후 `bash -n tools/release/verify_release_package.sh tools/test/smoke/release_package_tools.sh`, `./tools/test/smoke/release_package_tools.sh`, `UECF_RELEASE_PLUGIN_SKIP_BUILD=1 ./tools/test/smoke/release_package_plugin.sh` 통과.
- INDEPENDENT-REVIEW-FINAL: `code-reviewer`와 `security-reviewer` 재확인 결과 CRITICAL/HIGH/MEDIUM findings 없음. LIST_DIR 분리, root manifest만 제외, nested manifest-named unlisted file 차단, root dotfile unlisted file 차단, 외부 checksum basename/hash 직접 검증이 확인됐다.
- RED: `./tools/test/smoke/release_package_source.sh` 1차 실행은 `tools/release/package_source.sh`가 없어 실패했다.
- GREEN: source package 스크립트 구현 후 `./tools/test/smoke/release_package_source.sh` 통과. `UECommandForge-0.8.0-test-Source.zip`에 README, docs, sample plugin source, tools, specs, manifest, install notes, release notes, validation report가 포함되고 `.git`, `sample/Saved`, plugin binaries는 제외됨을 확인했다.
- GREEN: `validation-report.json`을 plugin/tools/source package의 필수 checksum payload로 정식화하고, `./tools/test/smoke/release_package_tools.sh`, `UECF_RELEASE_PLUGIN_SKIP_BUILD=1 ./tools/test/smoke/release_package_plugin.sh`, `./tools/test/smoke/windows_command_wrappers.sh` 통과를 확인했다.
- SUBAGENT-REVIEW: `reviewer`와 `security_reviewer` 서브에이전트 리뷰에서 source package version/`.uplugin` VersionName 불일치, `docs/memory` 내부 문서 포함, generated output 제외 smoke 부족이 지적됐다.
- REVIEW-FIX: `package_source.sh`는 packaged `.uplugin` VersionName과 `--version` 불일치 시 실패하도록 수정했다. Source.zip 입력은 tracked-file allowlist 기반으로 바꾸고 `docs/memory`를 제외했다. source smoke는 plugin `Intermediate`, plugin `Saved`, `DerivedDataCache`, `docs/memory` 제외와 version mismatch 실패를 검증하도록 보강했다.

남은 항목:
- Windows 실기 package wrapper 검증

- [ ] **Step 4: 자동 설치 스크립트 추가**

목표는 설치 책임을 명확히 분리하는 것이다.

- UE 플러그인은 대상 Unreal 프로젝트에 설치한다.
- Codex가 호출할 셸/배치 래퍼, 테스트 래퍼, release helper, 기본 specs/policies/templates는 Codex 전용 폴더에 설치한다.
- 프로젝트에는 Codex 설치 위치를 가리키는 얇은 연결 정보만 남긴다.

추가 파일:
- `install-uecommandforge.sh`
- `install-uecommandforge.ps1`
- `install-uecommandforge.bat`
- `tools/release/install_local.sh`
- `tools/release/install_local.bat`
- `tools/release/uninstall.sh`
- `tools/release/uninstall.bat`
- `tools/release/update_install.sh`
- `tools/release/update_install.bat`

설치 입력:
- `--project <path-to-uproject>` 또는 `UECF_PROJECT`
- `--codex-home <path>` 또는 `CODEX_HOME`
- `--release <version>` 또는 `--local-package <zip>`
- `--install-tools true|false`
- `--install-specs true|false`
- `--backup true|false`

설치 위치:

| 대상 | 설치 경로 |
|---|---|
| UE plugin | `<Project>/Plugins/UECommandForge` |
| Codex command wrappers | `~/.codex/UECommandForge/tools/ue` |
| Codex smoke/test wrappers | `~/.codex/UECommandForge/tools/test` |
| Codex release helpers | `~/.codex/UECommandForge/tools/release` |
| 기본 specs/policies/templates | `~/.codex/UECommandForge/specs` |
| Codex 설치 manifest | `~/.codex/UECommandForge/uecommandforge-installed.json` |
| 프로젝트 연결 manifest | `<Project>/UECommandForge/uecommandforge-project.json` |
| 설치 로그 | `<Project>/Saved/UECommandForge/install.log` |

Codex 설치 root:
- 기본값은 `~/.codex`로 고정한다.
- `--codex-home` 또는 `CODEX_HOME`은 고급 override로만 허용한다.
- 문서와 installer 출력은 `~/.codex/UECommandForge`를 표준 경로로 안내한다.

설치 동작:
- target `.uproject` 존재 확인
- 기존 설치 감지
- 기존 설치가 있으면 backup 생성
- release checksum 검증
- plugin zip 압축 해제
- Codex 전용 폴더에 tools/specs/templates 설치
- wrapper 실행 권한 부여
- project link 파일 생성: `<Project>/UECommandForge/uecommandforge-project.json`
- Codex env 파일 생성: `~/.codex/UECommandForge/uecommandforge.env`
- wrapper가 대상 project를 찾을 수 있도록 `UECF_PROJECT_FILE` 기본값 기록
- 설치 manifest 기록
- `Hello` commandlet으로 설치 검증

Windows 설치 동작:
- PowerShell installer를 1차 지원
- Command Prompt installer는 PowerShell installer를 호출하거나 동일 manifest 로직을 유지
- `.bat` wrapper 경로는 `%USERPROFILE%\.codex\UECommandForge\tools\ue`
- `UNREAL_EDITOR_CMD`, `UE_ROOT`, `PROJECT_FILE` override를 installer가 env 파일과 안내 문서에 기록

업데이트 동작:
- 현재 설치 manifest와 새 release manifest 비교
- plugin/tools/specs 변경 목록 출력
- backup 후 덮어쓰기
- Codex 폴더의 사용자 수정 specs/policies/templates는 기본적으로 보존하고 `.new` 파일로 충돌 기록
- 업데이트 후 `Hello`와 `ValidateProjectRules` 실행

삭제 동작:
- plugin 제거
- Codex tools 제거는 `--remove-codex-tools=true`일 때만 수행
- Codex specs는 기본값으로 보존하고 `--remove-specs=true`일 때만 삭제
- project link manifest 제거
- uninstall manifest와 로그 기록

- [ ] **Step 5: 설치 검증 스크립트 추가**

추가 파일:
- `tools/release/install_into_sample_project.sh`
- `tools/release/install_into_sample_project.bat`
- `tools/test/smoke/release_package_install.sh`
- `tools/test/smoke/release_package_install.bat`

검증 흐름:
- 깨끗한 임시 UE sample project 준비
- release zip 압축 해제
- installer로 `<Project>/Plugins/UECommandForge` 설치
- installer로 `~/.codex/UECommandForge/tools`와 `~/.codex/UECommandForge/specs` 설치
- `.uplugin` enable 상태 확인
- Codex wrapper 경로 확인
- project link manifest 확인
- `Hello` commandlet 실행
- `ValidateProjectRules` 실행
- Result JSON 생성 확인

- [ ] **Step 6: 설치 manifest 스키마 정의**

`uecommandforge-manifest.json` 필드:

- `version`
- `engine_version`
- `release_channel`
- `plugin_files[]`
- `tool_files[]`
- `spec_files[]`
- `checksums`
- `install_commands`
- `post_install_checks`
- `known_limits`

`uecommandforge-installed.json` 필드:

- `installed_at`
- `version`
- `project_file`
- `plugin_path`
- `tools_path`
- `specs_path`
- `codex_home`
- `source_release`
- `checksums`
- `backup_path`
- `last_validation_report`

`uecommandforge-project.json` 필드:

- `project_file`
- `plugin_path`
- `codex_home`
- `codex_tools_path`
- `codex_specs_path`
- `installed_version`
- `installed_manifest_path`
- `default_env_path`

- [ ] **Step 7: 버전 정책 정의**

버전 규칙:
- `major.minor.patch`
- Phase 8 완료 릴리즈는 `0.8.0`
- hotfix는 `0.8.x`
- UE minor 호환성이 깨지면 minor 이상 증가
- Result JSON schema breaking change는 minor 이상 증가

버전 기록 위치:
- `sample/Plugins/UECommandForge/UECommandForge.uplugin`
- `CHANGELOG.md`
- release notes
- `uecommandforge-manifest.json`

- [ ] **Step 8: 배포 전 게이트 정의**

릴리즈 전 필수 게이트:
- macOS build 통과
- Windows build 통과 또는 Windows 제한 리포트 작성
- 자동화 테스트 통과
- 모든 Phase 8 smoke 통과
- release package install smoke 통과
- installer smoke 통과
- update/uninstall smoke 통과
- checksum 생성
- secret scan 통과
- README install 섹션 최신화
- review/eval report 최신화

- [ ] **Step 9: 배포 후 검증 정의**

배포 후 확인:
- GitHub Release artifact 다운로드 가능
- checksum 일치
- 새 clone 또는 깨끗한 sample project에서 installer smoke 통과
- release notes에 알려진 제한 사항 포함
- rollback/upgrade 안내 포함

배포 완료 기준:
- installer로 새 프로젝트 설치 성공
- plugin은 대상 프로젝트에 설치 성공
- shell wrappers, batch wrappers, specs/policies/templates는 Codex 전용 폴더에 설치 성공
- project link manifest 생성 성공
- `Hello`, `CreateAIFlow`, `ValidateProjectRules` 실행 성공
- validation report 첨부
- `CHANGELOG.md`와 README 설치 문서 업데이트 완료

## Task 7: 최종 검증과 eval

필수 검증:

```bash
git diff --check
./tools/ue/build_plugin.sh
./tools/test/automation/run.sh
./tools/test/smoke/windows_command_wrappers.sh
./tools/test/smoke/prototype_automation.sh
./tools/test/smoke/asset_policy.sh
./tools/test/smoke/asset_change_apply_rollback.sh
./tools/test/smoke/cpp_generation.sh
./tools/test/smoke/cpp_reflection_policy.sh
./tools/test/smoke/uht_log_analysis.sh
./tools/test/smoke/mid_real_project_flow.sh
./tools/test/smoke/release_package_source.sh
./tools/test/smoke/release_package_tools.sh
./tools/test/smoke/release_package_plugin.sh
./tools/test/smoke/data_validation.sh
./tools/test/smoke/data_import_rollback.sh
./tools/test/smoke/release_package_install.sh
./tools/test/smoke/installer_install_update_uninstall.sh
```

Windows 실기 검증:

```bat
tools\ue\snapshot_assets.bat
tools\ue\validate_asset_rules.bat specs\policies\assets.policy.json
tools\ue\generate_cpp_class.bat specs\examples\cpp_health_component.json
tools\ue\validate_data_source.bat specs\schemas\items.schema.json specs\data\items.csv
tools\test\smoke\prototype_automation.bat
tools\test\smoke\release_package_install.bat
tools\test\smoke\installer_install_update_uninstall.bat
```

최종 인수 조건:
- 자동화 테스트 전체 PASS
- Asset snapshot/validation/apply/rollback PASS
- C++ class generation/apply/build/reflection validation PASS
- Data source validation/import/datatable validation/rollback PASS
- 통합 project rules validation PASS
- README, 마스터 플랜, Phase 8 문서 업데이트 완료
- Phase 8 리뷰/eval 리포트 작성 완료
- Windows `.bat` 실기 검증 또는 명시적 환경 제한 리포트 작성 완료
- GitHub Release용 zip/checksum/release notes/install guide 생성
- release package install smoke 통과
- 자동 설치/update/uninstall smoke 통과

## 리스크 및 완화

| 리스크 | 완화 |
|---|---|
| 에셋 move/rename/delete로 참조 손상 | preflight, dependency snapshot, redirector fixup, post validation, rollback plan 필수 |
| delete 오탐 | wildcard 금지, explicit allowlist, referencer 있으면 실패 |
| 정책이 실제 프로젝트 관례와 충돌 | exception list, severity 조정, policy versioning 지원 |
| C++ generator가 기존 파일을 덮어씀 | 기존 파일 존재 시 실패, checksum 기록, rollback plan 제공 |
| Build.cs 자동 수정 오류 | `-ApplyBuildCs=true` 명시 필요, 빌드 검증 필수 |
| DataTable import로 데이터 손상 | source hash, backup, rollback, post validation 필수 |
| Windows `.bat` 실기 미검증 | Phase 8 완료 전 Windows 실기 검증 또는 제한 리포트 필수 |
| 배포 zip에 개발 산출물이 섞임 | release verify script가 Intermediate/Saved/DerivedDataCache 포함 여부를 실패 처리 |
| 설치 문서와 실제 패키지 구조 불일치 | release package install smoke를 필수 게이트로 둠 |
| installer가 사용자 specs를 덮어씀 | Codex 폴더의 기본 specs는 충돌 시 `.new`로 저장하고 사용자 파일은 보존 |
| 프로젝트별 tools 경로가 다름 | tools는 Codex 전용 폴더에 고정하고 project link manifest로 대상 프로젝트를 연결 |

## Phase 8 완료 기준

Phase 8은 prototype의 세 영역을 **분석/검증/리포트 수준을 넘어 실제 적용과 rollback까지 가능한 제품화 기능**으로 제공하고, 자동 설치 스크립트로 UE 플러그인은 대상 프로젝트에, Codex 관련 셸/배치 파일과 specs/policies/templates는 Codex 전용 폴더에 설치·검증할 수 있으면 완료다. 완료 선언 전에는 build, automation, smoke, apply/rollback, 배포 패키징, 자동 설치/update/uninstall 검증, 문서, 리뷰/eval이 모두 끝나야 한다.
