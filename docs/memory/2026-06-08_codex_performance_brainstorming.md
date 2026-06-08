# 2026-06-08 Codex 작업 지연 최적화 2차 리뷰 반영 브레인스토밍

본 문서는 `2026-06-08_reviews_02.md` 피드백을 반영하여 언리얼 자동화 품질 검증 게이트를 통과하기 위한 구체적인 수정 방향 및 구현 논리를 정의합니다.

## 1. 분석 및 설계 (Brainstorming)

### H1. 검증되지 않은 플래그 제거 및 조건부 플래그 필터링
- **원인**: 최적화 플래그 `-NoPhysXVehicles`, `-DisablePhysicsCoSim`이 검증 없이 `run_commandlet.sh`에 추가되었고, 모든 커맨드렛에 무조건 적용되어 블루프린트나 셰이더 관련 에셋 가공에 잠재적 오류를 유발할 수 있습니다.
- **해결 방안**:
  - `run_commandlet.sh`에서 표준이 아니거나 언리얼 5.x에서 제거된 플래그 2종(`-NoPhysXVehicles`, `-DisablePhysicsCoSim`)을 완전히 배제합니다.
  - `CreateBlueprintBatch`를 포함하여 블루프린트 생성/가공/컴파일 등 셰이더 의존적인 커맨드렛이 기동할 때는 `-NoShaderCompile` 및 `-SkipShaderCompile`을 비활성화하고, 그 외 비의존적인 커맨드렛에만 적용하는 필터링 로직을 완벽히 정립합니다.

### H2. 트랜잭션/롤백 보장
- **원인**: `CreateBlueprintBatchCommandlet`이 배치 작업을 수행하는 도중 특정 아이템에서 오류가 나더라도 `continue`로 넘어가서 일부 에셋만 저장된 불안정한 상태가 남을 수 있습니다.
- **해결 방안**:
  - 배치 처리 루프를 돌기 전, 변경될 에셋들의 파일 경로(디스크 상의 `.uasset` 위치)를 파악합니다.
  - 에셋이 디스크에 이미 존재하는 경우 임시 백업 경로(`Saved/TmpBackup/`)로 파일을 미리 복사하여 백업합니다.
  - 배치 루프 내에서 하나라도 오류를 감지하면 `bBatchSuccess = false`로 판정하고 즉시 루프를 탈출(`break`)합니다.
  - 루프 탈출 후 `bBatchSuccess`가 false인 경우 전체 롤백 절차를 밟습니다:
    - 새로 생성되거나 변경된 에셋을 메모리와 디스크에서 강제 삭제하기 위해 `FGenericBlueprintBuilder::DeleteExistingBlueprint`를 호출합니다.
    - 백업본 파일이 존재했던 경우(기존 존재 에셋 복구) 백업 파일을 원본 경로에 복사하여 덮어쓰고, 백업 임시 파일을 삭제합니다.
  - 배치 처리가 성공한 경우에도 백업 임시 파일들을 전부 정리합니다.

### H3. 신규 파서 및 커맨드렛 오토메이션 테스트 구현 (TDD)
- **원인**: `BlueprintBatchSpecParser` 및 `CreateBlueprintBatchCommandlet`에 대한 테스트 코드가 `Tests/` 디렉터리에 부재하여 80% 이상의 테스트 커버리지 및 TDD 정책을 충족하지 못하고 있습니다.
- **해결 방안**:
  - `Source/UECommandForgeEditor/Tests/` 디렉터리에 `BlueprintBatchSpecParserTest.cpp` 및 `CreateBlueprintBatchCommandletTest.cpp`를 작성합니다.
  - `BlueprintBatchSpecParserTest`에서는 kind 불일치, version 불일치, items 필수 필드 누락, 정상적인 배치 JSON 스키마 등의 파싱 성공 및 실패 시의 오류 처리를 단위 테스트로 검증합니다.
  - `CreateBlueprintBatchCommandletTest`에서는 배치 기동, 성공 사례, 그리고 중간 실패 시 롤백 기능이 제대로 동작하는지 오토메이션 테스트를 통해 검증합니다.

### M1. 벤치 산출물 `.uasset` 추적 제외
- **원인**: `sample/Content/AI/Characters/` 디렉터리에 생성된 `BP_BenchCharacter*.uasset` 등 벤치마크 수행 중에 생겨난 바이너리 에셋들이 git 추적 대상에 포함되어 로컬 작업 트리가 지저분합니다.
- **해결 방안**:
  - 프로젝트 루트 `.gitignore` 파일에 해당 임시 벤치 산출물들을 무시하는 규칙을 명시적으로 추가합니다.

### M2. `version` 필드 값 유효성 검증
- **원인**: `BlueprintBatchSpecParser.cpp`에서 JSON 문자열의 `version` 필드를 로드하나 이에 대한 검증 논리가 누락되었습니다.
- **해결 방안**:
  - `version` 필드가 비어있거나 `"1"`이 아닌 경우 `OutErrors.Add(...)`를 통해 검증 에러를 수집하고 `false`를 리턴하도록 파서를 보완합니다.

### L1. `/dev/null` 하드코딩 교체
- **원인**: `CreateBlueprintBatchCommandlet.cpp`에서 출력 경로 누락 시 기본 출력 경로로 `/dev/null`을 하드코딩하고 있어 Windows 환경에서 잠재적인 에러를 유발합니다.
- **해결 방안**:
  - `#if PLATFORM_WINDOWS` 매크로 등을 통해 플랫폼이 Windows일 경우 `NUL`을 사용하고, macOS/Linux 환경일 경우 `/dev/null`을 사용하도록 플랫폼 독립적인 널 디바이스 경로 분기를 구현합니다.
