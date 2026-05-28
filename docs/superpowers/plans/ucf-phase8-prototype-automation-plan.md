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

- [ ] **Step 3: `PlanAssetChangesCommandlet` 작성**

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

- [ ] **Step 4: `ApplyAssetChangesCommandlet` 작성**

승인된 plan을 실제 적용한다.

필수 동작:
- 작업 전 snapshot 저장
- UE AssetTools API 사용
- 작업 후 AssetRegistry 재스캔
- post validation 실행
- rollback plan 저장
- Result JSON에 changed assets 기록

- [ ] **Step 5: `RollbackAssetChangesCommandlet` 작성**

rollback plan을 읽어 가능한 작업을 되돌린다.

필수 동작:
- rollback 전 현재 상태 검증
- path collision 검사
- rollback 후 post validation
- rollback 실패 시 수동 복구 지침 기록

- [ ] **Step 6: wrapper와 테스트 추가**

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

## Task 2: C++ 클래스 생성 반복 제품화

- [ ] **Step 1: `CppClassSpec` 정의**

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

- [ ] **Step 2: `GenerateCppClassCommandlet` 작성**

스펙 기반으로 `.h`/`.cpp`를 생성한다.

필수 동작:
- 기존 파일 존재 시 실패
- module root 밖 경로 실패
- dry-run preview 생성
- apply 시 파일 생성
- 생성 파일 목록과 checksum 기록
- Build.cs dependency plan 생성

- [ ] **Step 3: Build.cs 적용/검증 구현**

`ValidateBuildCsCommandlet`은 아래를 검증한다.

- Public/Private dependency 누락
- 사용하지 않는 dependency 후보
- module path 정책
- include path 정책

`GenerateCppClassCommandlet`은 `-ApplyBuildCs=true`일 때만 Build.cs를 수정한다.

- [ ] **Step 4: Reflection 정책 검증 구현**

`ValidateCppReflectionCommandlet` 검증 항목:
- `UPROPERTY` Category 누락
- `BlueprintCallable` Category 누락
- runtime mutable state의 `EditAnywhere + BlueprintReadWrite`
- component property의 `VisibleAnywhere` 권장 위반
- `Config` property의 config class 위치 위반
- `UFUNCTION`/`UPROPERTY` macro와 선언 연결 오류

- [ ] **Step 5: UHT 로그 분석 구현**

`AnalyzeUhtLogCommandlet`은 build log에서 UHT 오류를 추출하고 원인/수정 후보를 Result JSON과 Markdown으로 기록한다.

- [ ] **Step 6: wrapper와 테스트 추가**

추가 파일:
- `tools/ue/generate_cpp_class.sh/.bat`
- `tools/ue/validate_cpp_reflection.sh/.bat`
- `tools/ue/validate_buildcs.sh/.bat`
- `tools/ue/analyze_uht_log.sh/.bat`
- `tools/test/smoke/cpp_generation.sh`
- `tools/test/smoke/cpp_reflection_policy.sh`

인수 조건:
- dry-run이 생성 preview와 Build.cs plan을 기록
- apply가 샘플 class `.h/.cpp`를 생성
- 생성 후 `./tools/ue/build_plugin.sh` 통과
- Build.cs 검증 리포트 생성
- reflection policy 위반 샘플 검출
- UHT 오류 샘플 분석 리포트 생성

## Task 3: DataAsset / DataTable / Config 관리 제품화

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

- [ ] **Step 1: 배포 채널 정의**

1차 배포 채널:
- GitHub Release artifact
- zip 패키지 다운로드
- 자동 설치 스크립트
- source checkout 후 `tools/release/install_local.sh`로 local package 설치

2차 배포 채널:
- 사내/팀 plugin registry
- Unreal Marketplace 제출 후보 패키지

초기 공식 배포 방식은 GitHub Release artifact + 자동 설치 스크립트다. Marketplace는 심사/메타데이터/라이선스 검토가 필요하므로 Phase 8 완료 후 별도 배포 트랙으로 분리한다.

- [ ] **Step 2: 배포 산출물 정의**

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

- [ ] **Step 3: 패키징 스크립트 추가**

추가 파일:
- `tools/release/package_plugin.sh`
- `tools/release/package_plugin.bat`
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
