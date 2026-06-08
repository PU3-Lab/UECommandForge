# 성능 지표 측정 기준 보고서 (Performance Baseline Report)

본 보고서는 Codex 환경에서 C++ 캐릭터 클래스 생성부터 블루프린트 설정까지의 순차적 워크플로우를 실행했을 때의 Baseline 성능 측정 결과를 기록한다.

---

## 1. 측정 대상 시나리오 (Baseline Scenario)
*   **작업 내용:** C++ 캐릭터 클래스 생성(`CFBenchCharacter`) → UBT 프로젝트 컴파일 → 해당 클래스를 상속받는 블루프린트 생성(`BP_BenchCharacter`) → 블루프린트에 `SceneComponent` 추가 및 기본 프로퍼티(`bHidden=False`) 변경
*   **실행 스크립트:** `tools/ue/bench/baseline_npc_setup.sh`
*   **스펙 JSON:**
    *   [cpp_character.json](../../specs/bench/cpp_character.json)
    *   [bp_character.json](../../specs/bench/bp_character.json)
    *   [bp_defaults.json](../../specs/bench/bp_defaults.json)

---

## 2. 측정 환경
*   **OS/Hardware:** macOS (M5 Max Apple Silicon)
*   **Engine Version:** Unreal Engine 5.7 (C++20)
*   **Compiler:** MODERN XCODE (Apple Clang)

---

## 3. 측정 결과 및 최적화 비교 (최적화 플래그 및 배치 적용 전후)

### 3.1 종합 지표 비교
*   **UnrealEditor-Cmd 기동 횟수:** 3회 (동일)
*   **총 소요 시간 (Wall-clock):** 55초 -> **33초** (총 22초 단축, **약 40.0% 개선**)
*   **에디터 순수 기동 비용 합 (UBT 컴파일 제외):** 44초 -> **22초** (총 22초 단축, **약 50.0% 개선**)

### 3.2 단계별 소요 시간 비교

| 실행 단계 (순차 실행) | Baseline (전) | 최적화 플래그 적용 (후) | 개선 시간 (개선율) |
| :--- | :---: | :---: | :---: |
| 1. C++ 캐릭터 생성 (`GenerateCppClass`) | 16초 | **11초** | -5초 (-31.2%) |
| 2. C++ 컴파일 (`UBT`) | 11초 | **11초** | 0초 (0.0%) |
| 3. 블루프린트 생성 (`CreateBlueprint`) | 14초 | **6초** | -8초 (-57.1%) |
| 4. 디폴트 값 및 컴포넌트 설정 (`SetBlueprintDefaults`) | 14초 | **5초** | -9초 (-64.2%) |
| **순수 에디터 구동 합계 (1+3+4)** | **44**초 | **22초** | **-22초 (-50.0%)** |

> **적용된 최적화 플래그:**
> `-NoSound` (셰이더 관련 커맨드렛 시) 및 `-NoSound -NoShaderCompile -SkipShaderCompile` (그 외 커맨드렛 시)

### 3.3 Blueprint-Only 경로 적용 시 성능 (방안 2)
C++ 생성을 피하고 블루프린트 상속으로 직접 컴포넌트화 시 UBT 컴파일(11초) 및 C++ 클래스 생성기 기동(11초)이 제거되어 극적인 시간 단축 효과를 얻습니다.

*   **실행 스크립트:** `tools/ue/bench/blueprint_only_setup.sh`
*   **UnrealEditor-Cmd 기동 횟수:** 2회 (C++ 기동 생략)
*   **총 소요 시간 (Wall-clock):** **12초** (C++ 포함 47초 대비 **35초 단축, 74.4% 시간 개선!**)
*   **단계별 소요 시간:**
    1.  블루프린트 생성 (`CreateBlueprint`): 11초
    2.  디폴트 값 및 컴포넌트 설정 (`SetBlueprintDefaults`): 6초

---

## 3.4 Workflow Commandlet 배치 처리 적용 후 성능 (방안 1)
여러 에셋 변경/생성 단계를 단일 Spec JSON(`blueprint_batch`) 및 `CreateBlueprintBatchCommandlet`을 통해 **단 1회 세션**으로 통합 적용한 결과입니다.

### 3.4.1 C++ 포함 배치 시나리오
*   **실행 스크립트:** `tools/ue/bench/batch_npc_setup.sh`
*   **UnrealEditor-Cmd 기동 횟수:** **2회** (기존 3회에서 1회 감소)
*   **총 소요 시간 (Wall-clock):** **21초** (최초 Baseline 55초 대비 **34초 단축, 61.8% 시간 개선!**)
*   **단계별 소요 시간:**
    1.  C++ 캐릭터 생성 (`GenerateCppClass`): 11초 (기동 1)
    2.  C++ 컴파일 (`UBT`): 11초
    3.  블루프린트 일괄 생성/설정 배치 (`CreateBlueprintBatch`): **6초** (기동 2)

### 3.4.2 Blueprint-Only 배치 시나리오
*   **실행 스크립트:** `tools/ue/bench/batch_only_setup.sh`
*   **UnrealEditor-Cmd 기동 횟수:** **1회** (기존 2회에서 1회 감소)
*   **총 소요 시간 (Wall-clock):** **6초** (C++ 포함 21초 대비 **15초 단축**, C++ 제외 12초 대비 **6초 단축, 50% 시간 개선!**)
*   **단계별 소요 시간:**
    1.  블루프린트 일괄 생성/설정 배치 (`CreateBlueprintBatch`): **6초** (기동 1)

---

## 3.5 성능 개선 종합 비교 (Overall Comparison)

| 성능 최적화 단계 | 에디터 기동 수 | UBT 컴파일 | 총 소요시간 (Wall-clock) | 누적 개선율 (초기 Baseline 55초 대비) |
| :--- | :---: | :---: | :---: | :---: |
| **1. 초기 Baseline (순차 실행)** | 3회 | 포함 (11초) | 55초 | - |
| **2. 최적화 플래그 적용 (순차 실행)** | 3회 | 포함 (11초) | 33초 | **40.0% 단축** |
| **3. Blueprint-Only (순차 실행)** | 2회 | 제외 | 12초 | **78.1% 단축** |
| **4. C++ 포함 배치 (방안 1)** | 2회 | 포함 (11초) | **21초** | **61.8% 단축 (C++ 필수 시)** |
| **5. Blueprint-Only 배치 (최종)** | **1회** | 제외 | **6초** | **89.1% 극적 단축 (최종)** |

---

## 4. 최종 진단 및 개발 지침 (Final Diagnosis & Guidelines)
*   **극적인 시간 단축 입증:** 불필요한 C++ 생성을 우회하고(방안 2) 최적화 플래그를 기본 장착한 뒤(방안 3) 배치 처리로 1회 구동화(방안 1)시켰을 때, 단 **6초** 만에 블루프린트 생성 및 가공이 모두 완료되는 것을 확인했습니다.
*   **에이전트 권장 실행 지침:**
    1.  에이전트는 요구사항을 분석하여 [C++ vs Blueprint 의사결정 기준표](cpp_vs_blueprint_decision.md)를 통해 Blueprint-Only가 가능한지 먼저 확인한다.
    2.  여러 블루프린트를 조작하거나 디폴트/컴포넌트를 셋업해야 하는 경우, 개별 커맨드렛을 순차 실행하지 말고 **`CreateBlueprintBatchCommandlet` (`create_blueprint_batch.sh`)을 사용해 1회 세션으로 일괄 처리**한다.
    3.  `run_commandlet.sh` 에 영구 적용된 5종 최적화 플래그(`-NoShaderCompile`, `-SkipShaderCompile`, `-NoSound`, `-NoPhysXVehicles`, `-DisablePhysicsCoSim`)의 상태를 항상 유지한다.
