# 코드 리뷰 — 제네릭 CreateBlueprint 커맨드릿

- **리뷰 대상 브랜치:** `feat/create-blueprint-generic`
- **기준 커밋:** `3307a90` (`git diff main...HEAD`)
- **리뷰 일자:** 2026-06-04
- **스코프:** `CreateBlueprintCommandlet`, `BlueprintCreateSpec(Parser)`, CLI 래퍼(`create_blueprint.sh/.bat`), 스모크 테스트, 의존 헬퍼(`FGenericBlueprintBuilder`, `FBlueprintValidationRunner`, `run_commandlet.sh`)

## 종합 판단

컴파일·스모크 테스트가 통과하는 상태라 **크래시급 결함은 없음.** 핵심 지적은 "제네릭" 블루프린트 생성 빌더(`FGenericBlueprintBuilder::Build`)가 이미 존재하고 Character/AIController 빌더가 이를 공유하는데, 새 커맨드릿이 그 경로를 우회해 생성 파이프라인을 재구현했다는 **구조적 중복**이다.

## 발견 사항 (심각도순)

### 1. 🔴 [구조/중복] 커맨드릿이 기존 제네릭 빌더를 우회하고 파이프라인을 재구현

- **위치:** `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateBlueprintCommandlet.cpp` (`Main`)
- **내용:** `Main`이 create→compile→save→validate 흐름을 인라인으로 다시 작성했다. 동일 흐름이 이미 `GenericBlueprintBuilder.cpp:80` `Build()`에 캡슐화돼 있고, `CharacterBlueprintBuilder.cpp:13` · `AIControllerBlueprintBuilder.cpp:13`가 이를 호출한다.
- **분기된 동작:**
  - `Build`는 `CompileSaveAndValidate`(compile 실패 시 save 차단)를 사용하고 GC 루팅을 하지 않음.
  - `Main`은 `AddToRoot` + `ON_SCOPE_EXIT` GC 가드를 추가하고 `bCompile`/`bSave` 토글로 `Compile`/`Save`를 개별 호출.
  - `Main`이 추가한 두 검증(`ParentClass->IsChildOf(AActor::StaticClass())`, `FKismetEditorUtilities::CanCreateBlueprintOfClass`)은 빌더에는 없음.
- **비용:** "블루프린트 생성" 코드 경로가 2벌이 되어 향후 변경 시 동기화 누락 위험. 브랜치 목표가 "generic"인데 정작 제네릭 빌더를 사용하지 않음.
- **권장:** 추가 검증 2종을 `FGenericBlueprintBuilder::Build`(또는 그 전단) 안으로 내리고, 커맨드릿은 파싱 → `Build` 위임만 남긴다. 그러면 Character/AIController 경로도 동일한 Actor/Blueprintable 보호를 얻는다.

### 2. 🟠 [중복] 스펙 타입 평행 중복

- **위치:** `sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Specs/BlueprintCreateSpec.h`
- **내용:** 새 `FCommandForgeBlueprintCreateSpec`이 `Build()`가 받는 기존 `FBlueprintSpec`와 필드가 대부분 겹친다. 그 결과 커맨드릿이 스펙을 `Build`에 넘기려면 수동 필드 복사가 필요한 구조가 된다.
- **권장:** 1번과 함께 스펙 타입을 단일화하면 커맨드릿이 파싱 결과를 그대로 `Build`에 전달할 수 있다.

### 3. 🟠 [테스트 신뢰성] 스모크가 리포트를 `ls -t`로 추정

- **위치:** `tools/test/smoke/create_blueprint_generic.sh` (각 단계 약 60·78·99·120행)
- **내용:** `REPORT_FILE="$(ls -t .../CreateBlueprint_*.json | head -1)"`로 최신 파일을 고른다. 그러나 래퍼는 정확한 리포트 경로를 stdout으로 출력한다(`run_commandlet.sh:138`).
- **실패 시나리오:** 커맨드릿이 JSON을 생성하지 못해 래퍼가 exit 5로 종료되거나, 이전 실행 파일의 mtime이 더 새로우면 `ls -t`가 과거 리포트를 선택 → stale 데이터에 대해 단언 → **false PASS/FAIL**.
- **권장:** `REPORT_FILE="$("${UE_TOOLS}/create_blueprint.sh" "${TEMP_SPEC}" | tail -1)"`로 출력 경로를 직접 캡처.

### 4. 🟡 [크로스플랫폼] `-Output` 누락 시 `/dev/null` 기록

- **위치:** `CreateBlueprintCommandlet.cpp` MISSING_PARAM 분기
- **내용:** `OutPath`가 비면 `TEXT("/dev/null")`에 리포트를 쓴다. 이 프로젝트는 `.bat` 래퍼로 Windows도 지원하는데, Windows에서 `/dev/null`은 유효 경로가 아니라 `JsonReportWriter::Write`가 진단 없이 실패한다.
- **권장:** write를 건너뛰거나 플랫폼 null 디바이스를 사용.

### 5. 🟡 [일관성] `.bat`는 스펙 파일 존재 검사 누락

- **위치:** `tools/ue/create_blueprint.bat` (대조: `tools/ue/create_blueprint.sh:10-13`)
- **내용:** `.sh`는 스펙 파일 부재를 명확한 메시지로 거르지만 `.bat`에는 같은 가드가 없다. Windows에서 경로 오타 시 엔진까지 전달돼 덜 친절한 실패가 발생.
- **권장:** `.bat`에도 동일한 존재 검사 추가.

## 권장 처리 순서

1. **1·2번**(제네릭 빌더 재사용 + 스펙 단일화) — 가장 가치 큼, 함께 진행 권장.
2. **3번**(스모크 리포트 경로 캡처) — 독립적으로 빠르게 반영 가능.
3. **4·5번**(크로스플랫폼/일관성) — 낮은 우선순위, 여유 시.
