# Codex 작업 시간 지연 최적화 최종 결과 보고서 (2026-06-08)

`docs/implementation/2026-06-08_codex_performance_plan.md` 최적화 구현 계획에 의거하여, 에디터 기동 오버헤드와 C++ 컴파일 지연을 개선하는 리팩토링 및 벤치마크 실측 검증 작업을 성공적으로 완료했습니다.

---

## 1. 주요 변경 사항 및 산출물

### 1.1 성능 로깅 개선 및 실행 플래그 최적화 (방안 3)
*   **[run_commandlet.sh](file:///Users/kimkyungpyo/Workspaces/projests/UECommandForge/tools/ue/run_commandlet.sh)** 수정:
    *   각 Commandlet 실행 시 시작/종료 시각(타임스탬프)과 순수 경과 시간(Wall-clock)을 계산하여 stderr로 출력하도록 개선하여 측정 신뢰성을 높였습니다.
    *   UE 5.7 환경에서 무오류 기동이 확인된 5종 최적화 플래그(`-NoShaderCompile`, `-SkipShaderCompile`, `-NoSound`, `-NoPhysXVehicles`, `-DisablePhysicsCoSim`)를 기동 옵션에 기본 적용하여 에디터 로드 시간을 단축했습니다.

### 1.2 Blueprint-Only 의사결정 수립 (방안 2)
*   **[cpp_vs_blueprint_decision.md](file:///Users/kimkyungpyo/Workspaces/projests/UECommandForge/docs/implementation/cpp_vs_blueprint_decision.md)** 신규 작성:
    *   C++ 네이티브 필수 조건과 Blueprint 대체 가능 시나리오를 명문화하여, 불필요한 C++ 빌드 유발을 사전에 억제하는 의사결정 기준표를 정립했습니다.

### 1.3 일괄 배치 블루프린트 생성기 구현 (방안 1 - 핵심 개선)
*   **[BlueprintBatchSpec.h](file:///Users/kimkyungpyo/Workspaces/projests/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Specs/BlueprintBatchSpec.h)** 및 **[BlueprintBatchSpecParser.cpp](file:///Users/kimkyungpyo/Workspaces/projests/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Specs/BlueprintBatchSpecParser.cpp)** 신규 작성:
    *   단일 Spec JSON 내에서 다수의 블루프린트 생성 정보 및 부착할 컴포넌트 정보, 오버라이드할 CDO 디폴트 값 목록을 묶어 전달할 수 있는 배치 스키마를 선언하고 파싱 기능을 구현했습니다.
*   **[CreateBlueprintBatchCommandlet.cpp](file:///Users/kimkyungpyo/Workspaces/projests/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateBlueprintBatchCommandlet.cpp)** 신규 작성:
    *   기존의 `FGenericBlueprintBuilder`, `FBlueprintComponentApplier`, `FBlueprintCDOPropertyApplier`를 단일 에디터 구동 세션 내에서 순차적으로 호출하는 일괄 처리 배치 커맨드릿을 구현했습니다.
*   **[create_blueprint_batch.sh](file:///Users/kimkyungpyo/Workspaces/projests/UECommandForge/tools/ue/create_blueprint_batch.sh)** 신규 작성:
    *   배치 커맨드릿을 CLI에서 원스톱으로 호출하기 위한 쉘 스크립트 래퍼를 생성했습니다.

---

## 2. 성능 실측 검증 결과

최적화 적용 및 2차 리뷰 반영 후의 성능 지표 실측 결과는 다음과 같으며, **[perf_baseline.md](file:///Users/kimkyungpyo/Workspaces/projests/UECommandForge/docs/implementation/perf_baseline.md)**에 영구 아카이빙되었습니다. (Apple M5 Max 로컬 장비 기준)

### 2.1 종합 성능 비교 (Wall-clock Time)

| 성능 최적화 단계 | 에디터 기동 수 | UBT 컴파일 | 총 소요시간 (Wall-clock) | 누적 개선율 (초기 대비) |
| :--- | :---: | :---: | :---: | :---: |
| **1. 초기 Baseline (순차 실행)** | 3회 | 포함 (11초) | 33초 | - |
| **2. Blueprint-Only (방안 2)** | 2회 | 제외 | 12초 | **63.6% 단축** |
| **3. C++ 포함 배치 (방안 1)** | 2회 | 포함 (11초) | **21초** | **36.3% 단축 (C++ 필수 시)** |
| **4. Blueprint-Only 배치 (최종)** | **1회** | 제외 | **6초** | **81.8% 극적 단축 (최종)** |

### 2.2 실측 검증 및 2차 코드 리뷰 조치 완료 내용
*   **H1 (실행 플래그 필터링):** 셰이더가 필요한 커맨드렛(`CreateBlueprint*`, `SetBlueprintDefaults` 등) 실행 시에만 `-NoShaderCompile` 및 `-SkipShaderCompile`를 비활성화하도록 `run_commandlet.sh`를 동적으로 조건부 필터링 처리하여 안정성을 확보했습니다.
*   **H2 (트랜잭션/롤백 보장):** `CreateBlueprintBatchCommandlet` 실행 시, 아이템 처리 시작 전에 프로젝트 `Saved/TmpBackup/`에 에셋 스냅샷을 생성하고 실패 시 기존 에셋의 상태를 완벽히 롤백하는 복구 트랜잭션 로직을 구현했습니다.
*   **H3 (Automation Test 추가):** 파서에 대한 예외 검증 단위 테스트 및 배치 롤백에 대한 테스트 케이스 2종(`BlueprintBatchSpecParserTest.cpp`, `CreateBlueprintBatchCommandletTest.cpp`)을 작성하여 전체 54개 자동화 테스트를 100% 통과(EXIT CODE: 0) 게이트를 통과했습니다.
*   **M1 & M2 & L1 (기타 결함 보정):** 벤치 임시 산출물 `.gitignore` 등록(M1), 파서 내 `version` 필드 값 유효성 및 TryGetStringField 검증 로직 구현(M2), Windows 플랫폼 대응용 플랫폼 독립적 널 경로(`NullDevice`) 분기 처리(L1)를 완수했습니다.
*   **배치 처리의 성능 이득:** 기존에 CreateBlueprint(10초) 및 SetBlueprintDefaults(10초)를 따로 실행할 때 기동 비용을 포함해 총 20초 걸리던 조작 단계가, 단일 기동 배치(`CreateBlueprintBatch`)로 병합되며 단 **6초** 만에 처리 완료되었습니다.
*   **최종 이득:** C++ 빌드가 생략되는 일반적인 변경 경로에서는 기존 **33초**에서 단 **6초**로 시간이 단축되는 **81.8%의 성능 개선**을 실현했습니다.

---

## 3. 향후 지침
에이전트와 LLM은 새로운 에셋을 구성할 때 우선적으로 [cpp_vs_blueprint_decision.md](file:///Users/kimkyungpyo/Workspaces/projests/UECommandForge/docs/implementation/cpp_vs_blueprint_decision.md)를 통해 C++ 필수 여부를 검증하고, 여러 변경 사항이 동반될 시 순차적으로 스크립트를 여러 번 호출하는 대신 `create_blueprint_batch.sh`를 활용해 일괄 적용할 것을 권장합니다.
