# 리팩토링 및 검증 완료 보고서 (Generic CreateBlueprint)

코드 리뷰 피드백(`create_blueprint_generic_review.md`)에 따라 범용 블루프린트 생성 기능의 코드 구조적 중복을 제거하고, 테스트 신뢰성과 안정성을 개선하는 리팩토링 작업을 성공적으로 완료했습니다.

## 변경 내용

### 1. 스펙 단일화 및 중복 제거
- **[BlueprintSpec.h](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Specs/BlueprintSpec.h) 수정**: 기존 `FBlueprintSpec` 구조체를 확장하여 `Version`, `Kind`, `TransactionId`, `bCompile`, `bSave`, `bAllowReplace` 속성을 갖도록 개선했습니다.
- **[BlueprintCreateSpec.h](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Specs/BlueprintCreateSpec.h) 삭제**: 중복 정의되었던 `FCommandForgeBlueprintCreateSpec`를 완전히 제거했습니다.
- **[BlueprintCreateSpecParser.h](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Specs/BlueprintCreateSpecParser.h) / [BlueprintCreateSpecParser.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Specs/BlueprintCreateSpecParser.cpp) 수정**: 파서가 단일화된 `FBlueprintSpec` 타입을 다루도록 시그니처와 바인딩 로직을 변경했습니다.

### 2. 제네릭 빌더 리팩토링 및 예외 처리 통합
- **[GenericBlueprintBuilder.h](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/GenericBlueprintBuilder.h) / [GenericBlueprintBuilder.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/GenericBlueprintBuilder.cpp) 수정**:
  - `FGenericBlueprintBuildOptions`에 `bAllowReplace`, `bRequireActorParent`, `bRequireBlueprintable`, `bCompile`, `bSave` 필드를 추가했습니다. (기존 빌더와의 호환을 위해 기본값은 `true`로 지정)
  - `Build` 함수 내부에 부모 클래스가 Actor 계층인지, Blueprintable인지 확인하는 유효성 검증 로직을 추가했습니다.
  - 에셋이 이미 존재하고 `bAllowReplace`가 `false`인 경우 `BP_ALREADY_EXISTS` 에러를 반환하는 분기 로직을 통합했습니다.
  - 가비지 컬렉션(GC)으로 인한 메모리 해제 방지를 위해 생성된 `Blueprint`와 `Package`를 `AddToRoot()`로 보호하고 스코프 탈출 시 `RemoveFromRoot()`를 호출하는 안전장치를 내장했습니다.
  - `bCompile` 및 `bSave` 플래그의 ON/OFF에 따라 컴파일과 저장을 개별 수행하도록 제어 로직을 통합했습니다.

### 3. 커맨드릿 및 래퍼 스크립트 수정
- **[CreateBlueprintCommandlet.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateBlueprintCommandlet.cpp) 수정**:
  - 커맨드릿 `Main`에 중복으로 존재하던 블루프린트 생성 및 검증 코드를 완전히 걷어내고, 파싱된 스펙과 적절한 옵션을 넘겨 `FGenericBlueprintBuilder::Build`를 단일 호출하도록 전면 리팩토링했습니다.
  - 오류 발생 시 검증 실패인지 실제 빌드 실패인지에 따라 `ValidationFailed`와 `BuildFailed` Exit Code를 알맞게 반환하도록 분기 처리했습니다.
  - `-Output` 경로가 빈 경우, Unix/Windows 환경 차이에 관계없이 파일 출력을 건너뜀으로써 진단 없이 실패하는 현상을 방지했습니다.
- **[create_blueprint.bat](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/tools/ue/create_blueprint.bat) 수정**: Windows 배치 파일에서도 쉘 스크립트와 동일하게 스펙 파일의 디렉토리 존재 여부를 선제 검증하는 오류 가드를 배치했습니다.

### 4. 스모크 테스트 신뢰성 개선
- **[create_blueprint_generic.sh](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/tools/test/smoke/create_blueprint_generic.sh) 수정**:
  - 기존에 `ls -t`로 최종 리포트를 임의 추정하던 방식을 폐지하고, `create_blueprint.sh` 가 실행을 완료하고 stdout으로 반환하는 절대 경로를 임시 파일 리디렉션을 통해 마지막 라인에서 정확히 추출하는 구조로 수정했습니다.
  - 이로써 이전 테스트의 찌꺼기 파일 등으로 인해 발생하는 Stale 데이터 오탐 현상(False Pass/Fail)을 완벽히 격리했습니다.

---

## 검증 결과

`tools/test/smoke/create_blueprint_generic.sh` 실행 결과, 정의된 4개의 시나리오와 하위 11개의 검증 검사 항목이 모두 성공적으로 통과되었습니다.

```
=== Generic Blueprint Create Smoke Test ===

[1] Create Standard Actor Blueprint (allow_replace=true)
Report: /Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Saved/CodexReports/CreateBlueprint_20260604T021742Z.json
  PASS: ok == true
  PASS: compile_status == ok
  PASS: asset_on_disk == true
  PASS: asset_in_registry == true
  PASS: .uasset 파일 존재

[2] Create existing blueprint with allow_replace=false (Should Fail)
  PASS: ok == false
  PASS: error.code == BP_ALREADY_EXISTS

[3] Create blueprint with non-actor parent class (Should Fail)
  PASS: ok == false
  PASS: error.code == PARENT_CLASS_NOT_ACTOR

[4] Create blueprint with invalid parent class (Should Fail)
  PASS: ok == false
  PASS: error.code == PARENT_CLASS_NOT_FOUND

=== 결과: PASS 11 / FAIL 0 ===
```

---

## 2차 코드 리뷰 피드백 반영 사항

2차 코드 리뷰에서 식별된 안정성 및 정확성 문제를 다음과 같이 수정했습니다:

- **검증 스킵 시 리포트 정확성 개선**: `bRequireBlueprintable=false`인 경우 검증을 스킵하고 리포트에 `parent_class_blueprintable: skipped`로 정확히 보고하도록 수정했습니다.
- **Exit Code 판정의 견고성 강화**: 에러 코드의 부분 문자열 비교(`Contains(PARENT_CLASS)`) 방식에서 구체적인 에러 코드들(`PARENT_CLASS_NOT_FOUND`, `PARENT_CLASS_NOT_ACTOR`, `PARENT_CLASS_NOT_BLUEPRINTABLE`)을 1:1로 정확하게 대조하여 매핑하는 방식으로 보완했습니다.
- **기본값 안전 지향 설계(풋건 방지)**: `FGenericBlueprintBuildOptions::bAllowReplace`의 기본값을 `true`에서 `false`로 수정하여 명시적인 파괴 허용 없이 덮어씌워지지 않도록 정렬했습니다. 동시에 기존 동작이 강제 덮어쓰기였던 `CharacterBlueprintBuilder.cpp`와 `AIControllerBlueprintBuilder.cpp` 헬퍼에는 `Options.bAllowReplace = true`를 명시적으로 부여하여 회귀를 방지했습니다.

### 2차 검증 결과
모든 수정 반영 후 컴파일 빌드 및 스모크 테스트(`tools/test/smoke/create_blueprint_generic.sh`)를 재실행하였고, 전체 **11개 검사 항목 모두 동일하게 PASS**되었습니다.

---

## 3차 코드 리뷰 피드백 반영 사항

3차 코드 리뷰에서 식별된 자동화 테스트 및 스펙 일관성 이슈를 다음과 같이 수정했습니다:

- **자동화 테스트 에셋 충돌 방지 (N1 해결)**: `BlueprintDefaultsCommandletTest.cpp`, `CharacterBlueprintBuilderTest.cpp`, `AIFlowBindingTest.cpp`, `CreateAIFlowCommandletTest.cpp` 등 여러 자동화 테스트 코드 내에서 `Build` 호출 시 기존 찌꺼기 파일로 인해 `BP_ALREADY_EXISTS` 에러가 발생하지 않도록 명시적으로 `bAllowReplace = true` 설정을 지정하여 테스트 실행의 견고성을 확보했습니다.
- **Character/AIController의 스펙 필드 존중 (N2 해결)**: `CharacterBlueprintBuilder.cpp`와 `AIControllerBlueprintBuilder.cpp` 헬퍼 빌더가 하드코딩된 `true` 대신 역직렬화된 단일 스펙 `Spec.bAllowReplace` 값을 따르도록 연동하여 스키마 구조의 일관성을 맞추었습니다.

### 3차 검증 결과
모든 수정 반영 후 컴파일 빌드를 성공적으로 진행했으며, 스모크 테스트와 Unreal Engine 에디터 내부의 자동화 테스트(`tools/test/automation/run.sh`)를 재실행한 결과:
* **스모크 테스트**: 11개 검증 케이스 모두 PASS
* **에디터 자동화 테스트**: 53개 전체 테스트 케이스 모두 성공 및 **통과 (EXIT CODE: 0)**
비정상적인 회귀 없이 완벽하게 정상 동작함을 검증하였습니다.
