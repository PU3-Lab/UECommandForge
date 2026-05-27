# UECommandForge 마스터 계획

> **에이전트 작업자:** 각 Phase 파일을 순서대로 실행한다. Phase 1부터 시작하고, 이전 Phase 인수 조건을 통과한 뒤 다음 Phase로 진행한다.

**목표:** `ue_commandlet_based_llm_automation_plan.md`에 기술된 Commandlet 기반 Unreal Engine LLM 자동화 브리지 구축. LLM이 JSON Spec을 생성하면, Shell 래퍼가 `UnrealEditor-Cmd`를 호출하고, C++ 플러그인이 Commandlet을 실행하여 Builders/Validators가 UE 에디터 API를 호출한 뒤 Result JSON을 출력한다.

**아키텍처:** 새 샘플 `UECommandForgeSample.uproject` (UE 5.7)에 `Plugins/UECommandForge` 플러그인을 두 모듈로 분리한다 — `UECommandForgeRuntime` (POD 타입) + `UECommandForgeEditor` (Commandlet, Spec 파서, Builder, Validator, Report 작성기). Cross-platform Shell 래퍼는 `tools/ue/` 아래에 위치 (macOS + Windows via Git Bash/WSL). Result JSON은 항상 `Saved/CodexReports/`에 저장된다.

**기술 스택:** Unreal Engine 5.7 (C++20), `UnrealEd`, `Json`/`JsonUtilities`, `BlueprintGraph`, `KismetCompiler`, `AssetTools`, `StateTreeModule`, `StateTreeEditorModule`, `AIModule`. macOS에서는 POSIX shell (bash), Windows에서는 Git Bash.

---

## 전체 마스터 인수 조건

```bash
./tools/ue/create_ai_flow.sh specs/examples/guard_ai.json
jq -e '.ok and .validation.actor_placed and .validation.blueprint_compile == "passed"' \
   Saved/CodexReports/CreateAIFlow_*.json
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

---

## 현재 스프린트 상태

| Phase | 상태 | 최근 검증 |
|---|---|---|
| 1 | 완료 | `./tools/test/automation/run.sh` 기준 자동화 리포트 생성 확인 |
| 2 | 완료 | Guard AI 예제 Spec 파싱·검증 경로에서 재검증 |
| 3 | 완료 | `./tools/test/smoke/create_blueprint.sh specs/examples/guard_ai.json` PASS 10 / FAIL 0 |
| 4 | 완료 | `./tools/test/automation/run.sh` PASS 9 / FAIL 0, `CreateStateTree` 리포트 `ok: true` |
| 5 | 완료 | `./tools/test/automation/run.sh` PASS 10 / FAIL 0, `./tools/test/smoke/ai_flow_binding.sh specs/examples/guard_ai.json` PASS 8 / FAIL 0 |
| 6 | 대기 | Phase 5 완료 후 진행 |
| 7 | 대기 | Phase 6 완료 후 진행 |

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
  - `sample/Saved/CodexReports/CreateStateTree_20260527T091001Z.json` 기준 `ok: true`, `compile_status: ok`, `asset_on_disk: true`, `asset_in_registry: true`

### 2026-05-27 Phase 5 마감 노트

- AI Flow 바인딩 흐름을 `AIFlowBinder`, `AIFlowValidator`, `BindAIFlowCommandlet`, `ValidateAIFlowCommandlet`와 `tools/ue/bind_ai_flow.sh`, `tools/ue/validate_ai_flow.sh`로 추가했다.
- `UStateTreeComponent::SetStateTree()`가 SCS 템플릿에서 owner 검증을 수행하는 UE 5.7 동작을 피하기 위해, 바인더는 `StateTreeRef` UPROPERTY를 리플렉션으로 설정하고 validator도 같은 참조를 읽어 검증한다.
- 샘플 에셋 `BP_Guard.uasset`, `BP_GuardController.uasset`, `ST_Guard.uasset`와 자동화 테스트 에셋 `BP_TestCharacter.uasset`, `BP_TestController.uasset`, `ST_TestTree.uasset`를 커밋 대상에 포함한다.
- 검증 결과:
  - `./tools/ue/build_plugin.sh` 성공
  - 샘플 `UnrealEditor` 타깃 빌드 성공
  - `./tools/test/automation/run.sh` PASS 10 / FAIL 0 / SKIP 0
  - `./tools/test/smoke/ai_flow_binding.sh specs/examples/guard_ai.json` PASS 8 / FAIL 0
  - `sample/Saved/CodexReports/BindAIFlow_20260527T094750Z.json` 기준 `ok: true`, `ai_controller_class: ok`, `auto_possess_ai: ok`, `statetree_component: ok`
  - `sample/Saved/CodexReports/ValidateAIFlow_20260527T094757Z.json` 기준 `ok: true`, `ai_controller_class: ok`, `auto_possess_ai: ok`, `statetree_component: ok`

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
