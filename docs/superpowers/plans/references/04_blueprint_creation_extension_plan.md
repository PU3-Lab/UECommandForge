# 04-A. Blueprint 생성 확장 작업 계획

## 목표

UECommandForge가 Character / AIController 중심 Blueprint 생성에서 벗어나, 일반 Actor 기반 Blueprint를 시작으로 안전한 범용 Blueprint 생성 명령을 제공한다.

이 문서는 `04_blueprint_plan.md`의 검증 중심 원칙을 유지하면서, 제한적 생성 범위를 별도로 정의한다. Blueprint 그래프 노드 생성은 여전히 제외한다.

## 현재 구현 범위

| 항목 | 상태 | 비고 |
|---|---|---|
| Character Blueprint 생성 | 구현됨 | `CreateCharacterBlueprint`, `tools/ue/create_character_bp.*` |
| AIController Blueprint 생성 | 구현됨 | `CreateAIControllerBlueprint`, `tools/ue/create_ai_controller.*` |
| Blueprint 일괄 컴파일 | 구현됨 | `CompileBlueprints`, `tools/ue/compile_blueprints.*` |
| Blueprint 기본값 / 컴포넌트 적용 | 구현됨 | `SetBlueprintDefaults`, `tools/ue/set_blueprint_defaults.*` |
| 일반 Actor Blueprint 생성 | 미구현 | 신규 범용 생성 명령 필요 |
| Widget Blueprint 생성 | 미구현 | UMG 전용 계획과 분리 |
| Anim Blueprint 생성 | 미구현 | Animation 전용 계획과 분리 |
| Blueprint Graph / Event Graph 생성 | 제외 | 자동 생성 금지 |

## 공통 원칙

- Unreal Python으로 Blueprint를 생성하지 않는다.
- `FKismetEditorUtilities::CreateBlueprint` 기반 C++ Commandlet으로 생성한다.
- 생성 명령은 작은 단위로 유지하고, 입력 spec과 Result JSON을 필수로 가진다.
- 생성 직후 compile, save, AssetRegistry 등록, 디스크 저장 여부를 검증한다.
- C++ 클래스 생성 직후 같은 프로세스에서 해당 클래스를 Blueprint parent로 사용하지 않는다.
- parent class가 로드되지 않거나 Blueprint parent로 부적합하면 생성하지 않는다.
- 기존 에셋 덮어쓰기는 기본 금지한다. 명시 옵션이 필요하면 별도 승인 흐름을 둔다.

## 1차 MVP

1차 목표는 일반 Actor Blueprint 생성만 다룬다.

| 항목 | 내용 |
|---|---|
| Commandlet | `CreateBlueprint` |
| Wrapper | `tools/ue/create_blueprint.sh`, `tools/ue/create_blueprint.bat` |
| 기본 parent | `/Script/Engine.Actor` |
| 지원 parent | `AActor` 하위 Blueprintable class |
| 제외 parent | `WidgetBlueprint`, `AnimBlueprint`, `BlueprintFunctionLibrary`, `ActorComponent` |
| 후처리 | compile, save, validation report |

## Spec 초안

```json
{
  "version": "1",
  "kind": "blueprint",
  "transaction_id": "tx-create-actor-blueprint",
  "asset_path": "/Game/Actors/BP_Door",
  "parent_class": "/Script/Engine.Actor",
  "compile": true,
  "save": true,
  "allow_replace": false
}
```

검증 규칙:

- `asset_path`는 `/Game/` 또는 정책상 허용된 mount path로 시작해야 한다.
- `asset_path`는 `BP_` prefix를 기본 권장한다.
- `parent_class`는 로드 가능한 `UClass`여야 한다.
- `parent_class`는 Blueprint 생성 가능한 class여야 한다.
- `allow_replace`가 `false`이면 기존 asset이 있을 때 실패한다.

## 2차 확장

일반 Actor Blueprint 생성이 안정화된 뒤 parent 범위를 늘린다.

| parent 범위 | 처리 방식 | 비고 |
|---|---|---|
| `APawn` 하위 | `CreateBlueprint`로 지원 | Character와 중복 검토 |
| `ACharacter` 하위 | 기존 Character commandlet과 통합 검토 | 기존 wrapper 보존 |
| `AAIController` 하위 | 기존 AIController commandlet과 통합 검토 | 기존 wrapper 보존 |
| `UActorComponent` 하위 | 별도 `CreateComponentBlueprint` 후보 | Actor Blueprint와 저장/검증 방식 분리 |
| `UObject` 하위 | 보류 | DataAsset / Blueprintable UObject 정책 필요 |

## 3차 확장

특수 Blueprint는 범용 commandlet에 섞지 않고 별도 계획으로 분리한다.

| Blueprint 유형 | 권장 처리 |
|---|---|
| Widget Blueprint | `08_ui_umg_plan.md`에서 별도 commandlet 정의 |
| Anim Blueprint | `02_animation_plan.md`에서 별도 commandlet 정의 |
| Blueprint Function Library | 정책 검토 후 별도 생성 명령 |
| Blueprint Interface | Interface 전용 commandlet 후보 |
| DataAsset 기반 Blueprint | `06_dataasset_datatable_config_plan.md`와 연계 |

## 필요한 파일

| 파일 | 설명 |
|---|---|
| `sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Specs/BlueprintCreateSpec.h` | 범용 Blueprint 생성 spec |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Specs/BlueprintCreateSpecParser.*` | JSON parser |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/GenericBlueprintBuilder.*` | parent class 기반 생성 로직 |
| `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateBlueprintCommandlet.*` | commandlet |
| `tools/ue/create_blueprint.sh` | Bash wrapper |
| `tools/ue/create_blueprint.bat` | Windows wrapper |
| `specs/examples/actor_blueprint.json` | 일반 Actor Blueprint 예시 |
| `tools/test/smoke/create_blueprint_generic.sh` | smoke test |
| `tools/test/smoke/windows_command_wrappers.sh` | wrapper 정적 검증 추가 |

## Result JSON 필드

```json
{
  "commandlet": "CreateBlueprint",
  "ok": true,
  "created_assets": [
    "/Game/Actors/BP_Door"
  ],
  "validation": {
    "parent_class_loaded": "true",
    "parent_class_blueprintable": "true",
    "asset_created": "true",
    "compile_status": "ok",
    "asset_on_disk": "true",
    "asset_in_registry": "true"
  },
  "errors": []
}
```

## 테스트 기준

- Actor parent spec으로 Blueprint가 생성된다.
- 기존 asset이 있고 `allow_replace=false`이면 실패한다.
- 잘못된 parent class는 실패하고 Result JSON에 원인을 기록한다.
- Blueprintable이 아닌 parent class는 실패한다.
- 생성 후 compile status가 `BS_UpToDate`가 아니면 실패한다.
- 생성된 `.uasset`이 디스크에 존재해야 한다.
- `run_commandlet`의 Unreal Python 금지 guard를 우회하지 않는다.

## 금지 범위

- Event Graph node 생성
- Construction Script node 생성
- Blueprint 변수 / 함수 자동 추가
- `.uasset` 직접 바이너리 수정
- Unreal Python 기반 임시 생성
- C++ 클래스 생성 직후 같은 프로세스에서 parent Blueprint 생성

## 권장 진행 순서

1. `CreateBlueprint` commandlet로 일반 Actor Blueprint 생성만 구현한다.
2. `set_blueprint_defaults`와 조합해 기본값/컴포넌트 적용을 분리 실행한다.
3. `compile_blueprints`로 후속 compile 검증을 재사용한다.
4. Pawn / Character / AIController parent 확장은 기존 commandlet과 중복을 검토한 뒤 진행한다.
5. Widget / Anim Blueprint는 별도 계획에서 구현한다.
