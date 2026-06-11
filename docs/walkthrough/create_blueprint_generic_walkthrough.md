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
Report: /Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Saved/UECommandForge/Reports/CreateBlueprint_20260604T021742Z.json
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

## 4차 리뷰 피드백 반영 사항

### 1. `bAllowReplace = true` 강제 롤백 (회귀 방지 - N2)
- **대상**: `Builders/CharacterBlueprintBuilder.cpp`, `Builders/AIControllerBlueprintBuilder.cpp`
- **배경**: AI 관련 스펙 파서(`FAIFlowSpecParser`)에서 구조적으로 JSON 스키마에 `allow_replace` 속성이 정의되어 있지 않아, 파싱 결과가 항상 `false`가 되는 구조적 한계가 존재했습니다. 이로 인해 Character 및 AIController 블루프린트 생성 시 덮어쓰기가 비활성화되는 버그가 유발되었습니다.
- **해결**: 기존의 Character 및 AIController 블루프린트 생성 멱등성을 보장하기 위해 빌더 내부에서 `Options.bAllowReplace = true` 설정을 항상 사용하도록 롤백했습니다.

### 2. 자동화 테스트 코드 내 `bAllowReplace` 명시적 설정 부여 (N1)
- **대상**:
  - `BlueprintDefaultsCommandletTest.cpp`
  - `CharacterBlueprintBuilderTest.cpp`
  - `AIFlowBindingTest.cpp`
  - `CreateAIFlowCommandletTest.cpp`
- **해결**: 테스트 가동 시 이전 테스트에서 남은 Stale 에셋 찌꺼기가 있는 상태에서 빌더가 실패하는 오작동(N1)을 격리하고 멱등성을 확보하기 위해, 테스트 설정의 `bAllowReplace = true`를 명시적으로 세팅했습니다.

### 3. 스모크 테스트 래퍼 개선 (추출 불안정성)
- **대상**: `tools/test/smoke/create_blueprint_generic.sh`
- **해결**: Unreal 에디터가 종료되면서 ZenServer 등 백그라운드 프로세스의 로그가 stdout에 섞여 나와 `tail -1` 만으로 JSON 리포트 파일명을 정확히 파싱하지 못하던 문제를 해결하기 위해, `grep -E '\.json$'` 파이핑 필터를 추가하여 JSON 경로만을 안정적으로 추출하도록 처리했습니다.

---

## 4차 검증 결과

### 1. 스모크 테스트 (`create_blueprint_generic.sh`)
- 4개의 시나리오, 11개 검증 검사 모두 성공적으로 통과 (`PASS 11 / FAIL 0`)

### 2. Unreal 에디터 내부 자동화 테스트 (성공)
- `tools/test/automation/run.sh`를 활용한 53개 Automation Test 검증이 모두 성공적으로 통과되었습니다.
  - **결과**: PASS: 53, FAIL: 0, SKIP: 0 (EXIT CODE: 0)

---

## 7차 리뷰 피드백 반영 사항

### 1. 테스트 코드 3종 내 무효한 `bAllowReplace` 명시 제거 (R1 데드코드 정리)
- **대상**:
  - [CharacterBlueprintBuilderTest.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/CharacterBlueprintBuilderTest.cpp#L20)
  - [AIFlowBindingTest.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/AIFlowBindingTest.cpp#L30)
  - [CreateAIFlowCommandletTest.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/CreateAIFlowCommandletTest.cpp#L24)
- **이유**: `CharacterBlueprintBuilder` 및 `AIControllerBlueprintBuilder`는 의도된 멱등 생성을 보장하기 위해 빌더 내부에서 `Options.bAllowReplace = true` 로 강제 복원(N2 revert)되었습니다. 이에 따라, 위의 빌더를 직접 호출하는 테스트 3종에서 세팅하던 `Spec.bAllowReplace = true` 등의 옵션 값은 실제 빌더 내부에서 완전히 무시되는 데드코드로 작동하고 있었습니다.
- **조치**: 코드 가동성에 무해하나 "스펙 필드가 replace 옵션을 실질적으로 제어한다"는 오해를 줄 수 있어, 해당 라인들을 모두 제거하여 코드 가독성 및 명확성을 높였습니다.

---

## 7차 재검증 결과

데드코드 정리 후, 전체 자동화 테스트 및 스모크 테스트를 재가동하여 빌드 깨짐 및 기능상 결함이 없음을 완벽히 확인했습니다.

### 1. 스모크 테스트 (`create_blueprint_generic.sh`)
- **결과**: `PASS 11 / FAIL 0` (모두 통과)

### 2. Unreal 에디터 내부 자동화 테스트 (`run.sh`)
- **결과**: `PASS: 53, FAIL: 0, SKIP: 0` (전원 통과)
