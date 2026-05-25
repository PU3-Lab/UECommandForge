# Commandlet 기반 Unreal Engine LLM 자동화 기획문서

## 1. 문서 목적

이 문서는 **LLM/Codex가 Unreal Engine 작업을 자동화하기 위한 Commandlet 기반 아키텍처**를 정의한다.

기존 Python Script 기반 접근은 빠른 실험에는 유용하지만, Blueprint 생성, StateTree 구성, AI Flow 연결, Map 배치, 검증처럼 깊은 Unreal Editor API 접근이 필요한 작업에는 **C++ Commandlet + Plugin 구조**가 더 적합하다.

따라서 본 문서는 다음 방향을 기준으로 한다.

> LLM은 판단하고, Commandlet은 실행하며, Plugin 내부 Builder가 Unreal API를 호출한다.

---

## 2. 핵심 결론

Commandlet 기반 자동화의 핵심 구조는 다음과 같다.

```text
User Request
  ↓
LLM / Codex
  ↓
작업 분해
  ↓
Spec JSON 생성
  ↓
Shell Wrapper 실행
  ↓
UnrealEditor-Cmd
  ↓
C++ Commandlet 실행
  ↓
Plugin 내부 Builder / Validator 실행
  ↓
UE Editor API 호출
  ↓
Blueprint / StateTree / Map Asset 생성·수정·검증
  ↓
Result JSON 출력
  ↓
LLM이 결과 분석 후 다음 단계 판단
```

---

## 3. 설계 방향

### 3.1 Python은 메인 경로에서 제외

본 프로젝트의 메인 자동화 경로에서는 Python을 사용하지 않는다.

이유는 다음과 같다.

| 항목 | Python | C++ Commandlet |
|---|---:|---:|
| 빠른 실험 | 좋음 | 보통 |
| 에셋 목록/경로 검사 | 좋음 | 좋음 |
| Blueprint 생성 | 가능 | 좋음 |
| Blueprint Graph 조작 | 제한적 | 좋음 |
| StateTree 내부 구성 | 제한적 | 좋음 |
| 대규모 자동화 | 애매함 | 좋음 |
| CI/Headless 실행 | 가능 | 좋음 |
| 장기 유지보수 | 애매함 | 좋음 |

정식 자동화 파이프라인은 다음 구조를 따른다.

```text
LLM
  ↓
Shell Wrapper
  ↓
UnrealEditor-Cmd
  ↓
C++ Commandlet
  ↓
C++ Builder
  ↓
UE API
```

### 3.2 Commandlet을 자동화 진입점으로 사용

Commandlet은 Unreal Editor 런타임 내부에서 실행되는 자동화 작업 클래스다.

CLI에서는 다음과 같이 호출한다.

```bash
UnrealEditor-Cmd MyProject.uproject \
  -run=CreateAIFlow \
  -Spec=specs/guard_ai.json \
  -Output=Saved/CodexReports/create_ai_flow_result.json
```

Commandlet은 다음 역할을 수행한다.

```text
1. 명령 인자 파싱
2. Spec JSON 로딩
3. JSON을 C++ 구조체로 변환
4. Builder 호출
5. UE API 실행
6. 에셋 저장
7. 검증 실행
8. Result JSON 출력
```

---

## 4. 프로젝트명 후보

본 문서에서는 임시 프로젝트명을 **UECommandForge**로 사용한다.

### 4.1 추천 이름

| 이름 | 의미 |
|---|---|
| UECommandForge | Commandlet 조합 기반 Unreal 자동화 도구 |
| UEForgeBridge | LLM Spec을 UE Asset으로 변환하는 브릿지 |
| UEAssetForge | Unreal Asset 생성 중심 자동화 도구 |
| UEFlowForge | AI Flow / StateTree 생성 중심 도구 |

### 4.2 본 문서 기준 명칭

```text
Project Name: UECommandForge
Plugin Name: UECommandForge
Main Editor Module: UECommandForgeEditor
Runtime Module: UECommandForgeRuntime
```

---

## 5. 전체 아키텍처

```text
┌─────────────────────┐
│ User Request         │
└─────────┬───────────┘
          ↓
┌─────────────────────┐
│ LLM / Codex          │
│ - 요구사항 분석       │
│ - 작업 분해           │
│ - Spec JSON 작성      │
│ - Commandlet 선택     │
└─────────┬───────────┘
          ↓
┌─────────────────────┐
│ Shell Wrapper        │
│ tools/ue/*.sh        │
└─────────┬───────────┘
          ↓
┌─────────────────────┐
│ UnrealEditor-Cmd     │
│ UE Editor CLI 실행    │
└─────────┬───────────┘
          ↓
┌─────────────────────┐
│ C++ Commandlet       │
│ 자동화 작업 진입점    │
└─────────┬───────────┘
          ↓
┌─────────────────────┐
│ Plugin Builder Layer │
│ 생성/연결/검증 로직   │
└─────────┬───────────┘
          ↓
┌─────────────────────┐
│ UE Editor API        │
│ Blueprint / StateTree│
│ Asset / Map 처리      │
└─────────┬───────────┘
          ↓
┌─────────────────────┐
│ Result JSON / Report │
└─────────────────────┘
```

---

## 6. 주요 구성 요소

### 6.1 LLM / Codex

LLM은 사용자의 자연어 요청을 분석하고, 필요한 작업을 작은 단계로 분해한다.

역할:

```text
요구사항 분석
작업 단위 분해
Spec JSON 작성
Commandlet 호출 순서 결정
Result JSON 분석
실패 시 대안 판단
```

LLM은 Unreal API를 직접 호출하지 않고, 허용된 Shell Wrapper / Commandlet 목록 중에서 선택한다.

---

### 6.2 Shell Wrapper

Shell Wrapper는 Codex가 실행하는 안전한 명령 버튼이다.

예:

```bash
./tools/ue/create_ai_flow.sh specs/guard_ai.json
```

내부에서는 UnrealEditor-Cmd를 실행한다.

```bash
UnrealEditor-Cmd MyProject.uproject \
  -run=CreateAIFlow \
  -Spec=specs/guard_ai.json \
  -Output=Saved/CodexReports/create_ai_flow_result.json
```

역할:

| 역할 | 설명 |
|---|---|
| UE 경로 관리 | OS별 UnrealEditor-Cmd 경로 통일 |
| 프로젝트 경로 관리 | `.uproject` 자동 탐색 |
| 공통 옵션 적용 | `-unattended`, `-nop4`, `-log` |
| 명령 제한 | 허용된 Commandlet만 실행 |
| 결과 경로 통일 | `Saved/CodexReports`로 통일 |
| 종료 코드 전달 | 성공/실패 판단 가능 |

---

### 6.3 UnrealEditor-Cmd

Unreal Editor를 CLI/headless 방식으로 실행하는 실행 파일이다.

역할:

```text
Engine 초기화
Project 로딩
Plugin / Module 로딩
Commandlet 검색
Commandlet::Main 실행
작업 완료 후 종료
```

---

### 6.4 Commandlet

Commandlet은 외부에서 호출 가능한 Unreal 자동화 작업 진입점이다.

예:

```cpp
UCLASS()
class UCreateAIFlowCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
```

Commandlet 내부에서는 직접 모든 로직을 구현하지 않는다.  
Commandlet은 얇게 유지하고, 실제 작업은 Builder / Validator / Report Writer에 위임한다.

좋은 구조:

```text
CreateAIFlowCommandlet
  ↓
SpecParser
  ↓
AIFlowBuilder
  ↓
BlueprintBuilder
  ↓
StateTreeBuilder
  ↓
AIFlowBinder
  ↓
MapPlacer
  ↓
Validator
  ↓
ReportWriter
```

---

### 6.5 Plugin

Plugin은 Commandlet과 Builder, Validator, Report Writer를 담는 Unreal 확장 모듈이다.

역할:

```text
Commandlet 제공
Spec Parser 제공
Builder 로직 제공
Validator 제공
Result Report 출력
UE Editor API 접근 코드 보관
```

Plugin은 LLM과 Unreal Engine 사이의 자동화 브릿지 역할을 한다.

---

### 6.6 Spec JSON

Spec JSON은 만들고 싶은 Unreal Asset 구조를 설명하는 입력 파일이다.

중요한 점:

> Unreal Engine이 JSON을 자동으로 이해해서 에셋을 만드는 것이 아니다.  
> Commandlet/Plugin 코드가 JSON을 파싱하고, 그 의미를 UE API 호출로 변환한다.

---

### 6.7 Builder

Builder는 Spec JSON을 해석해 실제 Unreal Asset을 생성/수정하는 코드다.

예:

```text
BlueprintBuilder
StateTreeBuilder
AIFlowBinder
MapPlacer
```

Builder는 UE API를 직접 호출한다.

---

### 6.8 Validator

Validator는 생성된 결과가 정상인지 검사한다.

검증 항목 예:

```text
Blueprint 생성 여부
Blueprint 컴파일 성공 여부
AIControllerClass 연결 여부
StateTree Asset 생성 여부
StateTree 참조 연결 여부
Map에 Actor 배치 여부
NavMesh 존재 여부
참조 깨짐 여부
```

---

### 6.9 Result JSON

Result JSON은 Commandlet 실행 결과를 LLM이 다시 읽을 수 있도록 출력한 파일이다.

역할:

```text
성공/실패 판단
생성된 에셋 목록 제공
검증 결과 제공
오류 메시지 제공
다음 행동 제안 제공
```

---

## 7. Commandlet 설계 방식

### 7.1 Atomic Commandlet

작은 단위의 Commandlet이다.

예:

```text
CreateCharacterBlueprint
CreateAIController
CreateStateTree
BindAIFlow
PlaceActor
ValidateAIFlow
CompileBlueprints
```

장점:

```text
테스트가 쉽다.
실패 지점이 명확하다.
재사용성이 높다.
LLM이 조합하기 쉽다.
```

단점:

```text
각 Commandlet 실행마다 UnrealEditor-Cmd가 새로 실행될 수 있다.
프로젝트 로딩 시간이 반복된다.
```

---

### 7.2 Workflow Commandlet

여러 Atomic 기능을 한 번의 UnrealEditor-Cmd 실행 안에서 묶어 실행하는 Commandlet이다.

예:

```text
CreateAIFlow
SetupNPCCharacter
SetupPatrolAI
```

장점:

```text
실행 시간이 줄어든다.
하나의 작업 플로우를 안정적으로 처리할 수 있다.
중간 상태를 메모리에서 유지할 수 있다.
```

단점:

```text
기능이 커질 수 있다.
실패 지점 분석이 Atomic 방식보다 어려울 수 있다.
```

---

### 7.3 권장 전략

두 방식을 함께 사용한다.

```text
Atomic Commandlet은 기능 단위로 제공한다.
Workflow Commandlet은 자주 쓰는 조합을 묶는다.
실제 로직은 Commandlet이 아니라 Builder 계층에 둔다.
```

구조:

```text
Commandlets/
  Atomic/
    CreateCharacterBlueprintCommandlet.cpp
    CreateAIControllerCommandlet.cpp
    CreateStateTreeCommandlet.cpp
    BindAIFlowCommandlet.cpp
    PlaceActorCommandlet.cpp
    ValidateAIFlowCommandlet.cpp

  Workflows/
    CreateAIFlowCommandlet.cpp
    SetupNPCCharacterCommandlet.cpp
    SetupPatrolAICommandlet.cpp
```

---

## 8. 예시 시나리오: 기본 캐릭터 + StateTree AI Flow + Blueprint + 씬 배치

### 8.1 목표

기본 Character를 기반으로 NPC Guard를 만들고, StateTree를 이용해 AI Flow를 구성한 뒤, Blueprint화해서 맵에 배치한다.

생성될 에셋:

```text
/Game/AI/Characters/BP_NPC_Guard
/Game/AI/Controllers/BP_GuardAIController
/Game/AI/StateTrees/ST_GuardAI
/Game/Maps/Test_AI_Map
```

---

### 8.2 AI Flow

```text
Idle
  ↓
Patrol
  ↓ 플레이어 발견
Chase
  ↓ 공격 거리 진입
Attack
  ↓ 타겟 이탈
Return
  ↓ 복귀 완료
Patrol
```

Transition:

| From | To | 조건 |
|---|---|---|
| Idle | Patrol | 대기 시간 종료 |
| Patrol | Chase | 플레이어 감지 |
| Chase | Attack | 공격 거리 안 |
| Attack | Chase | 공격 거리 밖 |
| Chase | Return | 타겟을 잃음 |
| Return | Patrol | 복귀 완료 |

---

### 8.3 LLM 작업 분해

```text
1. Character Blueprint 생성
2. AIController Blueprint 생성
3. StateTree Asset 생성
4. StateTree State 구성
5. StateTree Transition 구성
6. Character에 AIControllerClass 설정
7. Character 또는 Controller에 StateTree 연결
8. Map에 Actor 배치
9. Blueprint Compile
10. AI Flow Validation
```

---

### 8.4 Spec JSON 예시

```json
{
  "character_blueprint": {
    "name": "BP_NPC_Guard",
    "path": "/Game/AI/Characters",
    "parent_class": "Character",
    "ai_controller": "/Game/AI/Controllers/BP_GuardAIController",
    "auto_possess_ai": "PlacedInWorldOrSpawned"
  },
  "ai_controller_blueprint": {
    "name": "BP_GuardAIController",
    "path": "/Game/AI/Controllers",
    "parent_class": "AIController"
  },
  "state_tree": {
    "name": "ST_GuardAI",
    "path": "/Game/AI/StateTrees",
    "states": [
      {
        "name": "Idle",
        "tasks": ["Wait"]
      },
      {
        "name": "Patrol",
        "tasks": ["MoveToPatrolPoint"]
      },
      {
        "name": "Chase",
        "tasks": ["MoveToTarget"]
      },
      {
        "name": "Attack",
        "tasks": ["PlayAttack"]
      },
      {
        "name": "Return",
        "tasks": ["MoveToHome"]
      }
    ],
    "transitions": [
      {
        "from": "Idle",
        "to": "Patrol",
        "condition": "WaitFinished"
      },
      {
        "from": "Patrol",
        "to": "Chase",
        "condition": "HasTarget == true"
      },
      {
        "from": "Chase",
        "to": "Attack",
        "condition": "DistanceToTarget <= AttackRange"
      },
      {
        "from": "Attack",
        "to": "Chase",
        "condition": "DistanceToTarget > AttackRange"
      },
      {
        "from": "Chase",
        "to": "Return",
        "condition": "HasTarget == false"
      },
      {
        "from": "Return",
        "to": "Patrol",
        "condition": "ReachedHome == true"
      }
    ]
  },
  "placement": {
    "map": "/Game/Maps/Test_AI_Map",
    "actor": "/Game/AI/Characters/BP_NPC_Guard",
    "location": [0, 0, 120],
    "rotation": [0, 0, 0]
  }
}
```

---

## 9. 실행 흐름

### 9.1 Atomic 방식

```bash
./tools/ue/create_character_bp.sh specs/bp_npc_guard.json
./tools/ue/create_ai_controller.sh specs/bp_guard_controller.json
./tools/ue/create_statetree.sh specs/st_guard_ai.json
./tools/ue/bind_ai_flow.sh specs/guard_binding.json
./tools/ue/place_actor.sh specs/place_guard.json
./tools/ue/validate_ai_flow.sh specs/guard_ai.json
```

### 9.2 Workflow 방식

```bash
./tools/ue/create_ai_flow.sh specs/guard_ai.json
```

내부 실행:

```bash
UnrealEditor-Cmd MyProject.uproject \
  -run=CreateAIFlow \
  -Spec=specs/guard_ai.json \
  -Output=Saved/CodexReports/create_ai_flow_result.json
```

---

## 10. Plugin 디렉터리 구조

```text
Plugins/
  UECommandForge/
    UECommandForge.uplugin
    Source/
      UECommandForgeRuntime/
        UECommandForgeRuntime.Build.cs
        Public/
        Private/

      UECommandForgeEditor/
        UECommandForgeEditor.Build.cs
        Public/
        Private/
          Commandlets/
            Atomic/
              CreateCharacterBlueprintCommandlet.cpp
              CreateAIControllerCommandlet.cpp
              CreateStateTreeCommandlet.cpp
              BindAIFlowCommandlet.cpp
              PlaceActorCommandlet.cpp
              ValidateAIFlowCommandlet.cpp

            Workflows/
              CreateAIFlowCommandlet.cpp
              SetupNPCCharacterCommandlet.cpp
              SetupPatrolAICommandlet.cpp

          Specs/
            AIFlowSpec.h
            AIFlowSpecParser.cpp
            BlueprintSpec.h
            StateTreeSpec.h
            PlacementSpec.h

          Builders/
            Blueprint/
              CharacterBlueprintBuilder.cpp
              AIControllerBlueprintBuilder.cpp
              BlueprintComponentBuilder.cpp

            StateTree/
              StateTreeBuilder.cpp
              StateTreeTransitionBuilder.cpp
              StateTreeTaskBuilder.cpp

            AI/
              AIFlowBinder.cpp
              AIControllerBinder.cpp

            Map/
              MapActorPlacer.cpp

          Validation/
            BlueprintValidator.cpp
            StateTreeValidator.cpp
            AIFlowValidator.cpp
            MapPlacementValidator.cpp

          Reports/
            JsonReportWriter.cpp
            ErrorReportBuilder.cpp
```

---

## 11. Result JSON 설계

### 11.1 성공 예시

```json
{
  "ok": true,
  "commandlet": "CreateAIFlow",
  "created_assets": [
    "/Game/AI/Characters/BP_NPC_Guard",
    "/Game/AI/Controllers/BP_GuardAIController",
    "/Game/AI/StateTrees/ST_GuardAI"
  ],
  "modified_assets": [
    "/Game/Maps/Test_AI_Map"
  ],
  "placed_actors": [
    {
      "map": "/Game/Maps/Test_AI_Map",
      "actor": "BP_NPC_Guard",
      "location": [0, 0, 120]
    }
  ],
  "validation": {
    "character_blueprint_created": true,
    "ai_controller_created": true,
    "state_tree_created": true,
    "ai_controller_assigned": true,
    "state_tree_assigned": true,
    "auto_possess_ai": true,
    "actor_placed": true,
    "blueprint_compile": "passed"
  },
  "errors": []
}
```

### 11.2 실패 예시

```json
{
  "ok": false,
  "commandlet": "CreateAIFlow",
  "errors": [
    {
      "code": "STATE_TREE_PLUGIN_DISABLED",
      "message": "StateTree plugin is not enabled."
    }
  ],
  "created_assets": [
    "/Game/AI/Characters/BP_NPC_Guard",
    "/Game/AI/Controllers/BP_GuardAIController"
  ],
  "rollback_available": true,
  "next_suggestions": [
    "Enable StateTree plugin in .uproject",
    "Re-run CreateAIFlowCommandlet",
    "Run ValidateAIFlowCommandlet after retry"
  ]
}
```

---

## 12. 구현 단계

### Phase 1. Commandlet 실행 하네스

목표:

```text
Shell Wrapper에서 UnrealEditor-Cmd 실행
Custom Commandlet 호출
Result JSON 출력 확인
```

작업:

```text
tools/ue/ue_env.sh
tools/ue/run_commandlet.sh
tools/ue/create_ai_flow.sh
UECommandForge Plugin 생성
HelloCommandlet 생성
Result JSON 출력
```

---

### Phase 2. Spec JSON 파서

목표:

```text
Spec JSON을 읽고 C++ 구조체로 변환
잘못된 JSON에 대한 오류 리포트 출력
```

작업:

```text
AIFlowSpec.h
AIFlowSpecParser.cpp
Spec validation
Error report
```

---

### Phase 3. Blueprint 생성

목표:

```text
Character Blueprint 생성
AIController Blueprint 생성
컴파일/저장
```

Commandlet:

```text
CreateCharacterBlueprint
CreateAIController
ValidateBlueprint
```

---

### Phase 4. StateTree 생성

목표:

```text
StateTree Asset 생성
State 추가
Transition 추가
Task 설정
```

Commandlet:

```text
CreateStateTree
ValidateStateTree
```

---

### Phase 5. AI Flow 연결

목표:

```text
Character에 AIControllerClass 설정
Character 또는 Controller에 StateTree 연결
Auto Possess AI 설정
```

Commandlet:

```text
BindAIFlow
ValidateAIFlow
```

---

### Phase 6. Map 배치

목표:

```text
Map 열기
Actor 배치
Map 저장
배치 결과 검증
```

Commandlet:

```text
PlaceActor
ValidateMapPlacement
```

---

### Phase 7. Workflow Commandlet

목표:

```text
CreateAIFlow 한 번으로 전체 생성 플로우 수행
```

Commandlet:

```text
CreateAIFlow
SetupNPCCharacter
SetupPatrolAI
```

---

### Phase 8. LLM 연동 규칙

목표:

```text
LLM이 허용된 Commandlet만 호출
Result JSON을 기반으로 다음 단계 판단
위험한 변경은 승인 후 수행
```

규칙:

```text
LLM은 .uasset 직접 수정 금지
LLM은 Shell Wrapper만 호출
Commandlet은 Result JSON을 반드시 출력
에셋 수정 전/후 검증 수행
복잡한 변경은 Patch Proposal 후 승인
```

---

## 13. Codex용 명령 목록 예시

```text
tools/ue/create_character_bp.sh
tools/ue/create_ai_controller.sh
tools/ue/create_statetree.sh
tools/ue/bind_ai_flow.sh
tools/ue/place_actor.sh
tools/ue/validate_ai_flow.sh
tools/ue/create_ai_flow.sh
tools/ue/compile_blueprints.sh
```

Codex는 위 명령 목록 중에서 작업 목적에 맞는 명령을 선택한다.

---

## 14. 운영 원칙

### 14.1 안전 원칙

```text
.uasset 직접 수정 금지
UE API를 통해서만 에셋 수정
모든 변경은 Result JSON으로 기록
생성 후 Compile / Save / Validate 수행
```

### 14.2 작업 단위 원칙

```text
Atomic Commandlet은 작게 유지
Workflow Commandlet은 자주 쓰는 플로우만 묶기
Commandlet은 얇게 유지
실제 로직은 Builder/Service로 분리
```

### 14.3 LLM 제어 원칙

```text
LLM은 Shell Wrapper만 호출
허용된 명령 목록을 명확히 정의
Result JSON 없이는 다음 단계 진행 금지
실패 시 오류 코드 기반으로 다음 행동 판단
```

---

## 15. 용어 정리

| 용어 | 설명 |
|---|---|
| LLM / Codex | 요구사항을 분석하고 Commandlet 호출 순서를 결정하는 판단자 |
| Shell Wrapper | Codex가 실행하는 안전한 CLI 버튼 |
| UnrealEditor-Cmd | Unreal Editor를 CLI/headless로 실행하는 실행 파일 |
| Commandlet | Unreal 내부에서 실행되는 자동화 작업 클래스 |
| Plugin | Commandlet, Builder, Validator를 담는 Unreal 확장 모듈 |
| Spec JSON | 만들고 싶은 에셋 구조를 정의한 입력 파일 |
| Result JSON | Commandlet 실행 결과를 LLM이 읽을 수 있게 출력한 파일 |
| Builder | Spec을 해석해 UE API로 에셋을 생성하는 코드 |
| Binder | 생성된 에셋 간 연결을 담당하는 코드 |
| Validator | 생성 결과가 정상인지 확인하는 검사 코드 |
| Workflow Commandlet | 여러 Atomic 기능을 한 번에 묶어 실행하는 Commandlet |
| Atomic Commandlet | 하나의 작은 작업만 수행하는 Commandlet |
| RunUAT | 빌드/쿠킹/패키징용 Unreal Automation Tool |

---

## 16. 최종 정리

이 프로젝트의 핵심은 다음과 같다.

```text
LLM은 작업을 판단한다.
Shell Wrapper는 안전한 실행 버튼이다.
UnrealEditor-Cmd는 Unreal Editor를 CLI로 실행한다.
Commandlet은 자동화 작업의 진입점이다.
Plugin은 UE API 접근 로직을 담는다.
Builder는 Spec JSON을 Unreal Asset으로 변환한다.
Validator는 결과를 검증한다.
Result JSON은 LLM의 다음 판단 근거가 된다.
```

최종 목표:

> 여러 Commandlet을 작게 만들어두고, LLM이 요청에 따라 필요한 Commandlet을 조합해서 호출한다.  
> 깊은 Unreal Engine 기능은 Plugin 내부 Builder가 처리하고, LLM은 Spec JSON과 Result JSON을 기준으로 판단한다.

---

## 17. 한 줄 정의

> UECommandForge는 LLM이 생성한 JSON Spec을 기반으로 Unreal Editor Commandlet을 실행해 Blueprint, StateTree, AI Flow, Map 배치 작업을 자동화하는 Unreal Engine 전용 Commandlet 기반 자동화 브릿지다.
