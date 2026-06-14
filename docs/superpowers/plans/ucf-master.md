# UECommandForge 마스터 계획

> **에이전트 작업자:** 각 Phase 파일을 순서대로 실행한다. Phase 1부터 시작하고, 이전 Phase 인수 조건을 통과한 뒤 다음 Phase로 진행한다.

**목표:** `ue_commandlet_based_llm_automation_plan.md`에 기술된 Commandlet 기반 Unreal Engine LLM 자동화 브리지 구축. LLM이 JSON Spec을 생성하면, Shell 래퍼가 `UnrealEditor-Cmd`를 호출하고, C++ 플러그인이 Commandlet을 실행하여 Builders/Validators가 UE 에디터 API를 호출한 뒤 Result JSON을 출력한다.

**아키텍처:** 새 샘플 `UECommandForgeSample.uproject` (UE 5.7)에 `Plugins/UECommandForge` 플러그인을 두 모듈로 분리한다 — `UECommandForgeRuntime` (POD 타입) + `UECommandForgeEditor` (Commandlet, Spec 파서, Builder, Validator, Report 작성기). Cross-platform Shell 래퍼는 `tools/ue/` 아래에 위치 (macOS + Windows via Git Bash/WSL). Result JSON은 항상 `Saved/UECommandForge/Reports/`에 저장된다.

**기술 스택:** Unreal Engine 5.7 (C++20), `UnrealEd`, `Json`/`JsonUtilities`, `BlueprintGraph`, `KismetCompiler`, `AssetTools`, `StateTreeModule`, `StateTreeEditorModule`, `AIModule`. macOS에서는 POSIX shell (bash), Windows에서는 Git Bash.

---

## 전체 마스터 인수 조건

```bash
./tools/ue/create_ai_flow.sh specs/examples/guard_ai.json
jq -e '.ok == true and (.steps | length == 5) and .validation.actor_placed == "ok"' \
   "$(ls -t sample/Saved/UECommandForge/Reports/CreateAIFlow_*.json | head -1)"
```

---

## Phase 파일 목록

| Phase | 파일 | 목표 | 의존성 |
|---|---|---|---|
| 1 | [ucf-phase1.md](ucf-phase1.md) | Commandlet 실행 하네스 — `hello.sh` → Result JSON | 없음 |
| 2 | [ucf-phase2.md](ucf-phase2.md) | Spec JSON 파서·검증기 | Phase 1 |
| 3 | [ucf-phase3.md](ucf-phase3.md) | 블루프린트 생성 | Phase 2 |
| 4 | [ucf-phase4.md](ucf-phase4.md) | StateTree 생성 | Phase 3 |
| 5 | [ucf-phase5.md](ucf-phase5.md) | AI 플로우 바인딩·검증 | Phase 4 |
| 6 | [ucf-phase6.md](ucf-phase6.md) | 맵 배치 | Phase 5 |
| 7 | [ucf-phase7.md](ucf-phase7.md) | 워크플로우 Commandlet (원스톱) | Phase 6 |
| 8 | [ucf-phase8-prototype-automation-plan.md](ucf-phase8-prototype-automation-plan.md) | prototype 기반 에셋 정책, C++ 생성, 데이터 검증 제품화 및 배포 | Phase 7 |
| 9 | [references/10_packaging_cook_build_plan.md](references/10_packaging_cook_build_plan.md) | **출시(릴리즈) 지향** — Packaging / Cook / 플랫폼 Build 설정 자동 검증·리포트로 배포 가능한 빌드 산출 | Phase 8 |

**현재 마일스톤:** 0.9.0 게시 완료. 다음 목표는 **출시 가능한 게임 빌드 자동화(1.0.0)** — Phase 9에서 Cook/Packaging/플랫폼 Build 파이프라인을 "읽기 전용 분석 → 리포트 → 제한적 생성 → 사람 승인 → 적용" 원칙으로 구축한다.

---

## 현재 스프린트 상태

| Phase | 상태 | 최근 검증 |
|---|---|---|
| 1 | 완료 | `./tools/test/automation/run.sh` 기준 자동화 리포트 생성 확인 |
| 2 | 완료 | Guard AI 예제 Spec 파싱·검증 경로에서 재검증 |
| 3 | 완료 | `./tools/test/smoke/create_blueprint.sh specs/examples/guard_ai.json` PASS 10 / FAIL 0 |
| 4 | 완료 | `./tools/test/automation/run.sh` PASS 9 / FAIL 0, `CreateStateTree` 리포트 `ok: true` |
| 5 | 완료 | `./tools/test/automation/run.sh` PASS 10 / FAIL 0, `./tools/test/smoke/ai_flow_binding.sh specs/examples/guard_ai.json` PASS 8 / FAIL 0 |
| 6 | 완료 | `PlaceActor` 리포트 `ok: true`, `actor_placed: ok` |
| 7 | 완료 | `./tools/ue/build_plugin.sh` 통과, `./tools/test/automation/run.sh` PASS 15 / FAIL 0, `CreateAIFlow` 인수 조건 통과 |
| 8 | 완료 — v0.9.0 GitHub Release 게시 (2026-06-14, Mac plugin + Tools + Source 에셋). 잔여: Windows `.bat` wrapper 실기 검증(Windows 호스트 필요) | `./tools/ue/build_plugin.sh`, `./tools/test/automation/run.sh` PASS 52 / FAIL 0 / SKIP 0, Phase 8 smoke/package/installer 게이트 PASS, Release: https://github.com/PU3-Lab/UECommandForge/releases/tag/v0.9.0 |
| 9 | 진행 중 — 출시(릴리즈) 지향 Packaging/Cook/Build 파이프라인. **9A 사전점검 검증 계층 완료**: `ValidatePluginDependencies` 커맨드렛 Task 1–7 구현, 빌드/테스트/스모크 실기 그린 재현 완료. 잔여: 스펙 M3 심화·Editor 모듈 warning·smoke test 파일, `DiffPlatformConfig`(H1 스파이크 선행) | 완료 (실기 검증 완료). 계획: [2026-06-14-phase9a-plugin-dependency-validation.md](2026-06-14-phase9a-plugin-dependency-validation.md), 스펙: [2026-06-14-phase9a-pre-release-validation-design.md](../specs/2026-06-14-phase9a-pre-release-validation-design.md) |

### 2026-05-27 Phase 3 마감 노트

- 테스트 스크립트 경로를 `tools/test/automation/run.sh`, `tools/test/smoke/create_blueprint.sh`로 정리했다.
- Blueprint 생성 Commandlet은 기존 로드/부분 로드 에셋을 삭제한 뒤 저장하도록 보강했다.
- 샘플 에셋 `BP_Guard.uasset`, `BP_GuardController.uasset`, `BP_TestCharacter.uasset`를 커밋 대상에 포함했다.
- 검증 결과:
  - `./tools/ue/build_plugin.sh` 성공
  - 샘플 에디터 타깃 빌드 성공
  - `./tools/test/automation/run.sh` PASS 8 / FAIL 0 / SKIP 0
  - `./tools/test/smoke/create_blueprint.sh specs/examples/guard_ai.json` PASS 10 / FAIL 0

### 2026-05-27 Phase 4 마감 노트

- StateTree 생성 흐름을 `StateTreeEditorAdapter`, `StateTreeBuilder`, `CreateStateTreeCommandlet`, `tools/ue/create_statetree.sh`로 추가했다.
- UE 5.7 StateTree Editor API 스파이크 결과를 반영해 `UStateTreeFactory`, `UStateTreeState::AddTask<T>`, `UStateTreeEditingSubsystem::CompileStateTree` 기반으로 구현했다.
- 샘플 에셋 `ST_Guard.uasset`, 자동화 테스트 에셋 `ST_TestTree.uasset`를 커밋 대상에 포함한다.
- 검증 결과:
  - `./tools/ue/build_plugin.sh` 성공
  - 샘플 `UnrealEditor` 타깃 빌드 성공
  - `./tools/test/automation/run.sh` PASS 9 / FAIL 0 / SKIP 0
  - `./tools/test/smoke/create_statetree.sh specs/examples/guard_ai.json` PASS 5 / FAIL 0
  - `sample/Saved/UECommandForge/Reports/CreateStateTree_20260527T091001Z.json` 기준 `ok: true`, `compile_status: ok`, `asset_on_disk: true`, `asset_in_registry: true`

### 2026-05-27 Phase 5 마감 노트

- AI Flow 바인딩 흐름을 `AIFlowBinder`, `AIFlowValidator`, `BindAIFlowCommandlet`, `ValidateAIFlowCommandlet`와 `tools/ue/bind_ai_flow.sh`, `tools/ue/validate_ai_flow.sh`로 추가했다.
- `UStateTreeComponent::SetStateTree()`가 SCS 템플릿에서 owner 검증을 수행하는 UE 5.7 동작을 피하기 위해, 바인더는 `StateTreeRef` UPROPERTY를 리플렉션으로 설정하고 validator도 같은 참조를 읽어 검증한다.
- 샘플 에셋 `BP_Guard.uasset`, `BP_GuardController.uasset`, `ST_Guard.uasset`와 자동화 테스트 에셋 `BP_TestCharacter.uasset`, `BP_TestController.uasset`, `ST_TestTree.uasset`를 커밋 대상에 포함한다.
- 검증 결과:
  - `./tools/ue/build_plugin.sh` 성공
  - 샘플 `UnrealEditor` 타깃 빌드 성공
  - `./tools/test/automation/run.sh` PASS 10 / FAIL 0 / SKIP 0
  - `./tools/test/smoke/ai_flow_binding.sh specs/examples/guard_ai.json` PASS 8 / FAIL 0
  - `sample/Saved/UECommandForge/Reports/BindAIFlow_20260527T094750Z.json` 기준 `ok: true`, `ai_controller_class: ok`, `auto_possess_ai: ok`, `statetree_component: ok`
  - `sample/Saved/UECommandForge/Reports/ValidateAIFlow_20260527T094757Z.json` 기준 `ok: true`, `ai_controller_class: ok`, `auto_possess_ai: ok`, `statetree_component: ok`

### 2026-05-27 Phase 6 마감 노트

- 맵 배치 흐름을 `MapActorPlacer`, `PlaceActorCommandlet`, `tools/ue/place_actor.sh`로 추가했다.
- `MapActorPlacer`는 `/Game/` 맵 경로 검증, 기존 맵 로드, 신규 빈 맵 생성·저장, Blueprint actor 배치, 저장 후 재검증을 수행한다.
- 반복 실행 시 같은 Blueprint actor가 중복 배치되지 않도록 기존 actor를 제거한 뒤 다시 배치한다.
- 검증 결과:
  - `sample/Saved/UECommandForge/Reports/PlaceActor_20260527T122059Z.json` 기준 `ok: true`, `actor_placed: ok`
  - `MapActorPlacerTest`는 배치, 검증, 반복 실행 후 actor 1개 유지 조건을 포함한다.

### 2026-05-28 Phase 7 마감 노트

- 원스톱 워크플로우를 `CreateAIFlowCommandlet`, `tools/ue/create_ai_flow.sh`, `tools/ue/setup_npc_character.sh`, `tools/ue/setup_patrol_ai.sh`로 추가했다.
- Windows Command Prompt용 `.bat` 래퍼와 Windows 정적 스모크 테스트를 추가했다.
- README 명령 표면을 Phase 7 기준으로 갱신하고, macOS/Linux/Git Bash와 Windows Command Prompt 명령을 함께 문서화했다.
- 검증 결과:
  - `./tools/ue/build_plugin.sh` 성공
  - `./tools/test/automation/run.sh` PASS 15 / FAIL 0 / SKIP 0
  - `./tools/ue/create_ai_flow.sh specs/examples/guard_ai.json` 성공
  - `sample/Saved/UECommandForge/Reports/CreateAIFlow_20260528T003339Z.json` 기준 `ok: true`, `steps` 길이 5, `validation.actor_placed: ok`
- 리뷰 및 eval 결과: [ucf-phase7-review-eval-report.md](ucf-phase7-review-eval-report.md)
- 완성도 판단: 단순 뼈대가 아니라 동작하는 MVP/스프린트 완료본이며, 남은 범위는 Windows 실기 검증, rollback 자동화, UE runner 기반 CI, 프로파일 확장 같은 제품화 작업이다.

### 2026-05-29 Phase 8 진행 노트

- 에셋 정책, 에셋 변경 preflight/apply/rollback, 표준 폴더 생성, C++ 클래스 생성, Build.cs 검증, Reflection 정책 검증, UHT 로그 분석, 데이터 소스 검증/import, DataTable 검증, Config 검증을 Phase 8 명령 표면으로 추가했다.
- 통합 품질 게이트 `PrototypeAutomationCommandlet`와 `tools/ue/validate_project_rules.sh/.bat`를 추가했다. 통합 리포트는 child commandlet별 JSON과 사람이 읽는 Markdown 요약을 함께 생성한다.
- README 명령 표면을 Phase 8 기준으로 갱신하고, `docs/superpowers/plans/ucf-phase8-review-eval-report.md`에 제품화/eval 결과를 반영했다.
- 배포 전 게이트와 배포 후 검증 기준을 README와 Phase 8 계획서에 고정했다.
- 검증 결과:
  - `./tools/ue/build_plugin.sh` 성공
  - 샘플 `UnrealEditor` 타깃 빌드 성공
  - `./tools/test/automation/run.sh` PASS 52 / FAIL 0 / SKIP 0
  - `./tools/test/smoke/windows_command_wrappers.sh` 성공
  - `UE_COMMANDLET_TIMEOUT=120 ./tools/test/smoke/prototype_automation.sh` 성공
- Task 7 로컬 최종 검증에서 asset/C++/data/config/통합 스모크, release package source/tools/plugin/install smoke, installer install/update/uninstall smoke, Windows 제한 리포트, secret scan을 모두 통과했다.
- 잔여 리스크는 Windows 실제 호스트에서 `.bat` wrapper를 실행하지 못한 점이다. 현재 macOS 검증에서는 정적 wrapper 검사와 `windows_host_required` 제한 리포트로 대체한다.

---

## 리스크 및 가정

| 리스크 | 완화 방안 |
|---|---|
| UE 5.7이 프리릴리스; StateTree Editor API가 변경될 수 있음 | Phase 4는 스파이크 + 어댑터 레이어로 시작. 검증 후 README에 UE 바이너리 빌드 해시 고정. |
| Apple Silicon UE 배포 경로가 Intel과 다름 | 래퍼가 `UE_ROOT` 환경변수로 폴백; 실패 시 읽기 쉬운 오류 한 줄 출력. |
| 워크플로우 Commandlet 부분 성공 시 고아 에셋 발생 | Result JSON `rollback_available: true` + 수동 롤백 Commandlet (Phase 7 이후 추가). |
| 블루프린트 컴파일 경고 정책 | 경고는 리포트에 포함되지만 실행 실패 아님; CRITICAL 전용 실패 시맨틱. |
| Windows에서 PowerShell로 `tools/ue/*.sh` 호출 | 문서화된 요구사항: Git Bash 또는 WSL. Phase 1에서는 PowerShell 미지원. |

---

## 범위 외

- Behavior Tree / EQS (StateTree로 대체됨).
- `RunUAT BuildCookRun` 패키징.
- Animation BP / Niagara 자동화.
- LLM 자체 호출 — 이 계획은 LLM이 사용할 커맨드 표면만 제공.
- CI 통합 (GitHub Actions / Jenkins).
