# Codex 작업 시간 지연 최적화 구현 계획 (2026-06-08)

> 출처: [docs/reviews/2026-06-08_reviews_01.md](../reviews/2026-06-08_reviews_01.md) 섹션 3 및 해당 리뷰 검토 결과.
> 목적: Codex 환경에서 C++ 클래스 생성 + 블루프린트 조작 시 발생하는 과도한 소요 시간을 측정 가능한 방식으로 단축한다.

---

## 0. 배경 및 근본 원인

`run_commandlet.sh`의 `run_unreal_commandlet()`은 명령 1건마다 `UnrealEditor-Cmd`를 헤드리스(`-nullrhi -nosplash -unattended`)로 기동/종료한다. Atomic 방식의 순차 실행(`generate_cpp_class` → `create_blueprint` → `set_blueprint_defaults`)은 단계마다 에디터 기동 비용(20~40초)을 중복 누적시키는 것이 핵심 병목이다. 부차 원인은 C++ 추가 시의 UBT 재컴파일 오버헤드다.

핵심 레버리지: **에디터 기동 횟수를 줄이는 것**(방안 1) > 기동 1회당 비용을 줄이는 것(방안 3).

---

## 1. 측정 기준 (Baseline) — 선행 작업

> 어떤 최적화든 적용 전에 baseline을 먼저 확보한다. 측정 없는 최적화는 검증 불가.

### 측정 지표
- **에디터 기동 횟수**: 시나리오 1회 완료까지 `UnrealEditor-Cmd` 프로세스 기동 수
- **총 소요 시간(wall-clock)**: 시나리오 시작 ~ 마지막 commandlet 종료까지
- **단계별 소요 시간**: 각 commandlet 개별 실행 시간

### 측정 시나리오 (대표 워크플로우)
"C++ 캐릭터 클래스 생성 → 블루프린트 생성 → 메쉬 컴포넌트 부착 → 디폴트 설정"

### 작업 항목
- [ ] 위 시나리오를 순차 실행하는 `tools/ue/bench/baseline_npc_setup.sh` 작성
- [ ] `run_commandlet.sh`에 단계별 시작/종료 타임스탬프를 stderr로 출력 (기존 동작 비변경, 로그만 추가)
- [ ] 3회 반복 측정 후 중앙값을 `docs/implementation/perf_baseline.md`에 기록

**완료 기준**: baseline 수치(기동 횟수 N, 총 시간 T초)가 문서화됨.

---

## 2. 방안 3 — 실행 플래그 최적화 (즉시 적용 / 효과 LOW~MEDIUM)

> 복잡도 최저. 단, 플래그 유효성 검증을 먼저 한다. 무효 플래그는 침묵 무시되거나 경고를 유발한다.

### 2.1 플래그 검증 (선행)
- [ ] UE 5.7 환경에서 아래 플래그를 1개씩 추가하며 로그 확인
  - `-NoShaderCompile` / `-SkipShaderCompile` — ✅ 표준, 적용 대상
  - `-NoSound` — ✅ 표준, 적용 대상
  - `-NoPhysXVehicles` — ⚠️ UE 5.x에서 PhysX Vehicle 제거됨. 무효 가능성 → **검증 후 무효 시 제외**
  - `-DisablePhysicsCoSim` — ❌ 비표준/미문서화 → **검증 전 채택 금지**

### 2.2 적용
- [ ] 검증 통과한 플래그만 `run_unreal_commandlet()` 호출부에 추가
- [ ] commandlet 종류에 따라 플래그 충돌 여부 확인 (셰이더가 필요한 commandlet이 있는지)
- [ ] baseline 재측정으로 단축 효과 확인

**파일**: `tools/ue/run_commandlet.sh` (`run_unreal_commandlet` 함수 1곳)

**완료 기준**: 검증된 플래그 적용 후 기동 1회당 시간이 baseline 대비 감소함을 실측.

---

## 3. 방안 2 — Blueprint-Only 분기 기준 정립 (단기 / 효과 MEDIUM)

> C++ 빌드·핫리로드를 통째로 생략하는 가장 큰 단발 효과. 단 "언제 C++을 생략해도 되는가"의 판단 기준이 자동화의 전제.

### 3.1 의사결정 기준표 작성
- [ ] `docs/implementation/cpp_vs_blueprint_decision.md` 작성. 다음을 구분:

| C++ 필수 (Blueprint 대체 불가) | Blueprint 대체 가능 |
|---|---|
| 신규 UFUNCTION/UPROPERTY 리플렉션이 필요한 네이티브 로직 | 엔진 기본 클래스(Actor/Character/Pawn) 상속만으로 충분 |
| 성능 크리티컬 Tick 로직 | 컴포넌트 부착 + 디폴트 값 설정 위주 |
| 인터페이스/추상 기반 클래스 정의 | 프로토타이핑/배치/AI 플로우 구성 |
| 네이티브 모듈 의존성(Build.cs) 추가 필요 | — |

### 3.2 적용
- [ ] Codex 워크플로우 가이드에 "기본은 Blueprint-Only, C++은 기준표 충족 시에만" 원칙 명시
- [ ] 대표 시나리오를 Blueprint-Only 경로로 측정해 C++ 경로 대비 시간 차 기록

**완료 기준**: 기준표가 존재하고, Blueprint-Only 경로의 시간 이득이 실측으로 문서화됨.

---

## 4. 방안 1 — Workflow Commandlet 배치 처리 (중기 / 효과 HIGH)

> 근본 해결책. 여러 작업을 단일 Spec JSON으로 선언해 **에디터 1회 기동**으로 완료. 선례는 기존 `setup_npc_character.sh`(현재는 `create_ai_flow.sh` 단일 호출 래퍼).

### 4.1 통합 Spec JSON 스키마 설계 (선행, 공수 주의)
- [ ] 기존 commandlet들의 개별 spec 포맷 조사 (`specs/` 디렉터리)
- [ ] 계층형 통합 스키마 정의: `{ cppClasses[], blueprints[], components[], defaults[] }`
- [ ] 스키마 검증 로직 추가 (시스템 경계 입력 검증 — 잘못된 spec은 fail-fast)

### 4.2 Workflow Commandlet 구현
- [ ] 단일 에디터 세션 내에서 C++ 정보 처리 → 블루프린트 생성 → 컴포넌트 부착 → 디폴트 설정을 순차 수행하는 commandlet 추가
- [ ] **롤백 전략**: 중간 단계 실패 시 `snapshot_assets` → `rollback_asset_changes` 연동. 단일 세션 내 트랜잭션 경계 정의 (부분 성공 상태를 남기지 않음)
- [ ] 멱등성 보장 (기존 `create_blueprint_generic` 멱등성 패턴 준수)
- [ ] CLI 래퍼 `tools/ue/setup_*.sh` 추가

### 4.3 검증
- [ ] Automation Test 추가 (단일 세션 배치 성공/부분 실패 롤백 케이스)
- [ ] Smoke Test 추가 (E2E)
- [ ] baseline 시나리오를 배치 경로로 측정 → 기동 횟수 N→1 확인

**완료 기준**: 대표 시나리오가 에디터 1회 기동으로 완료되고, 실패 시 롤백이 동작하며, 총 시간이 baseline 대비 대폭 감소.

---

## 5. 방안 4 — 증분 빌드 최적화 (재정의 필요 / 효과 미검증)

> 원안의 "에셋 계획 시스템 고도화"는 모호. UBT 증분 빌드는 이미 기본 동작이므로 구체적 레버리지로 재정의한다.

### 재정의된 액션 (조사 후 채택 판단)
- [ ] **DDC(Derived Data Cache) 워밍업/공유**: Codex 환경에서 DDC가 매번 콜드 상태인지 조사. 영구 캐시 디렉터리 설정으로 셰이더/에셋 파생 데이터 재사용 가능 여부 확인
- [ ] **Live Coding vs Hot Reload**: C++ 변경 반영 경로가 현재 무엇인지 확인하고 더 빠른 경로로 통일
- [ ] **빌드 산출물 캐시**: UBT 중간 산출물(`Intermediate/`)이 명령 간 보존되는지 확인 (컨테이너/세션 격리로 매번 소실된다면 이것이 진짜 병목)

**완료 기준**: 위 3개 항목의 조사 결과를 문서화하고, 실효성 있는 것만 채택.

> 주의: 방안 4는 효과가 환경 의존적이다. 1~4번 적용 후에도 C++ 빌드가 병목으로 남을 때만 착수한다.

---

## 6. 실행 순서 및 우선순위

| 순서 | 방안 | 효과 | 복잡도 | 비고 |
|---|---|---|---|---|
| 0 | §1 Baseline 측정 | — | LOW | **모든 작업의 선행 조건** |
| 1 | §2 방안 3 플래그 | LOW~MED | LOW | 플래그 검증 먼저 |
| 2 | §3 방안 2 기준표 | MED | LOW | Codex 자동 판단 근거 |
| 3 | §4 방안 1 배치 | HIGH | HIGH | 근본 해결, 스키마+롤백 주의 |
| 4 | §5 방안 4 빌드 | 불명확 | 불명확 | 조사 후 조건부 착수 |

각 단계 완료 시 baseline 대비 재측정하여 누적 개선 효과를 `docs/implementation/perf_baseline.md`에 갱신한다.

---

## 7. 리스크 및 미해결 질문

- **R1**: 통합 Spec 스키마 통합 공수가 예상보다 클 수 있음 (기존 spec 포맷 이질성). §4.1 조사 결과에 따라 범위 조정.
- **R2**: 단일 세션 배치에서 C++ 컴파일이 끼면 세션 내 핫리로드 가능 여부 불확실 → §5 조사와 연계.
- **R3**: 플래그 최적화가 특정 commandlet(셰이더 의존)을 깨뜨릴 수 있음 → commandlet별 플래그 프로파일 분리 필요할 수 있음.
- **Q1**: Codex 실행 환경은 세션 간 `Intermediate/`·DDC를 보존하는가, 매번 콜드인가? (방안 4 실효성의 핵심 변수)
