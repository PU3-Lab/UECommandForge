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

---

# 2차 리뷰 — 리팩토링 커밋 검증

- **대상 커밋:** `128bf35` (`refactor: apply code review feedback for generic blueprint creation`)
- **작성자/일시:** kimkyungpyo, 2026-06-04 11:21
- **리뷰 일자:** 2026-06-04
- **범위:** 1차 리뷰 지적의 반영 여부 + 리팩토링이 도입한 신규 결함 점검

## 1차 지적 해소 여부 (5/5 ✓)

| # | 1차 지적 | 처리 내용 | 결과 |
|---|----------|-----------|------|
| 1 | 커맨드릿이 제네릭 빌더 우회 | `Main` → `FGenericBlueprintBuilder::Build` 단일 위임. Actor/Blueprintable 검증을 빌더로 이관해 `bRequireActorParent`/`bRequireBlueprintable` 옵션화 | ✓ |
| 2 | 스펙 타입 중복 | `FCommandForgeBlueprintCreateSpec` 삭제 → `FBlueprintSpec`로 단일화, 파서 시그니처 변경 | ✓ |
| 3 | 스모크 `ls -t` 추정 | `> TEMP_OUT` 후 `tail -1`로 래퍼 stdout 경로 직접 캡처, teardown에 `TEMP_OUT` 정리 추가 | ✓ |
| 4 | `/dev/null` 크로스플랫폼 | `OutPath` 비면 write 스킵 | ✓ |
| 5 | `.bat` 스펙 검사 누락 | `if not exist "%SPEC_FILE%"` 가드 추가 | ✓ |

## 무결성 검증
- **삭제 헤더 잔여 참조 0건** — `BlueprintCreateSpec.h` / `FCommandForgeBlueprintCreateSpec` 댕글링 참조 없음 → 빌드 깨짐 없음.
- **기존 호출자 회귀 없음** — Character/AIController 빌더는 `FGenericBlueprintBuildOptions` 새 필드 기본값(`true`)을 그대로 사용. `ACharacter`·`AAIController` 모두 `AActor` 하위 + Blueprintable이라 새 게이트 통과.

## 개선으로 평가되는 점
- **검증 순서 개선.** 기존 커맨드릿은 `기존 에셋 삭제 → 부모 검증` 순서라 부모 클래스 오류 시 삭제된 에셋을 복구 못 하는 위험이 있었음. 새 `Build()`는 `부모 검증 → 존재/삭제 → 생성` 순서로 파괴적 삭제 전에 검증.

## 신규 발견 사항 (심각도순)

### A. 🟠 [출력 정확성] `parent_class_blueprintable=true`가 검사 스킵 시에도 기록됨

- **위치:** `GenericBlueprintBuilder.cpp` `Build()`
- **내용:**
  ```cpp
  if (Options.bRequireBlueprintable && !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass)) { ...; return false; }
  OutValidation.Add(TEXT("parent_class_blueprintable"), TEXT("true"));   // 무조건 실행
  ```
  `bRequireBlueprintable=false`면 검사를 안 했는데도 리포트에 `true`를 남김 → 검증하지 않은 사실을 "참"으로 보고. 현재 모든 호출자가 `true`라 잠복 상태지만, 옵션을 끄면 거짓 리포트가 됨.
- **권장:** 스킵 시 `skipped`로 기록하거나 `else` 분기에서 처리.

### B. 🟡 [견고성] Exit code를 문자열 부분일치로 분류

- **위치:** `CreateBlueprintCommandlet.cpp` `Main()`
- **내용:** `BuildErrors[0].Code.Contains(TEXT("PARENT_CLASS"))`로 `ValidationFailed`/`BuildFailed` 판정. 첫 에러만, 부분일치로 의존. 빌더가 첫 에러에서 즉시 반환하므로 현재는 정확하나 구조적으로 취약.
- **권장:** 에러 코드 → ExitCode 명시 매핑.

### C. 🟡 [설계/풋건] 파괴적 기본값 불일치

- **위치:** `BlueprintSpec.h` / `GenericBlueprintBuilder.h`
- **내용:** `FBlueprintSpec::bAllowReplace = false`(안전) vs `FGenericBlueprintBuildOptions::bAllowReplace = true`(덮어쓰기). 커맨드릿은 명시 연결로 안전하나, Options 기본값만 믿고 호출하는 미래 코드는 무경고로 기존 에셋을 삭제하게 됨.
- **권장:** 두 기본값을 `false`로 정렬(현재 Character/AIController는 의도적 항상 replace이므로 명시 설정 유지).

### D. ⚪ [사소] 포맷
- 여러 파일 끝에 빈 줄 추가(무해).

## 결론

리팩토링은 1차 피드백을 정확히 반영했고 **중복 제거·검증 순서 개선 측면에서 명백한 개선이며, 회귀·빌드 깨짐 없음.** 신규 항목 중 실제 출력 정확성 이슈는 A 한 건(현재 잠복)이며, B·C는 미래 안전장치 성격. 머지를 막을 사유 없음.

**후속 권장:** A(잠복 false 리포트) 우선 수정 → B(exit code 매핑) → C(기본값 정렬).

---

# 3차 검증 — 2차 지적(A/B/C) 수정 확인

- **대상:** 미커밋 작업 트리 변경분 (HEAD `128bf35` 대비)
- **리뷰 일자:** 2026-06-04
- **범위:** 2차 리뷰 신규 발견 A·B·C의 반영 정확성 + 부수 영향 점검

## 2차 지적 반영 (3/3 ✓)

| 지적 | 수정 내용 | 평가 |
|------|-----------|------|
| **A** blueprintable 검사 스킵 시 `true` 오기록 | `bRequireBlueprintable=false`면 `parent_class_blueprintable=skipped` 기록하도록 if/else 분리 | ✓ 정확 |
| **B** exit code 문자열 부분일치 | `Code.Contains("PARENT_CLASS")` → `PARENT_CLASS_NOT_FOUND`/`_NOT_ACTOR`/`_NOT_BLUEPRINTABLE` 정확 `==` 비교 | ✓ 견고해짐 |
| **C** 파괴적 기본값 불일치 | `FGenericBlueprintBuildOptions::bAllowReplace` 기본값 `true→false`. Character/AIController 빌더는 명시적으로 `Options.bAllowReplace = true` 설정 | ✓ 일관됨 |

C는 안전 기본값(`false`)을 두고, 항상 덮어써야 하는 두 빌더만 명시적으로 `true`를 켜 의도가 코드에 드러남.

## 이번 변경이 부른 부수 영향 (회귀 아님, 선택적 개선)

### N1. 🟡 [테스트 견고성] 자동화 테스트가 기본값 `false`에 의존
- **위치:** `Tests/BlueprintDefaultsCommandletTest.cpp` (`...CreatesActorTest`, `FSetBlueprintDefaultsCommandletTest`)
- **내용:** 두 테스트가 `Build()` 호출 시 `Options.bAllowReplace`를 설정하지 않아 기본값 `false`에 의존. 이전엔 기본값 `true`라 직전 실행 잔여 에셋을 덮어쓰며 self-healing 했으나, 이제 크래시로 에셋이 남으면 다음 실행이 `BP_ALREADY_EXISTS`로 실패 가능. 클린 환경/정상 종료(+`FScopedBlueprintAssetCleanup`) 시엔 문제 없음.
- **권장:** 의도 시 테스트 Options에 `bAllowReplace = true` 명시.

### N2. 🟡 [일관성] Character/AIController가 `Spec.bAllowReplace`를 무시
- **위치:** `Builders/CharacterBlueprintBuilder.cpp`, `Builders/AIControllerBlueprintBuilder.cpp`
- **내용:** 두 빌더가 `Options.bAllowReplace = true`를 하드코딩 → 단일화된 `FBlueprintSpec.bAllowReplace` 필드를 읽지 않음. CreateBlueprint 커맨드릿은 JSON `allow_replace`를 존중하나, Character/AIController 경로는 같은 스키마라도 항상 덮어씀. (이전부터의 동작이라 회귀 아님, 스키마 단일화 이후 혼동 소지.)
- **권장:** 두 빌더도 `Options.bAllowReplace = Spec.bAllowReplace`로 연결하면 일관.

## 결론

2차 지적 A·B·C는 모두 정확하고 안전하게 반영됨. 빌드 영향 없음(if/else·비교·기본값 변경뿐). N1·N2는 회귀가 아닌 선택적 일관성/견고성 항목으로, 후속 처리 시 N2(스펙 존중) → N1(테스트 명시) 순 권장.

> ⚠️ **정정(4차 검증 참조):** 위 "N2(스펙 존중) 권장"은 잘못된 판단이었음. N2를 적용한 결과 production 회귀가 발생함 — 아래 4차 검증 참조.

---

# 4차 검증 — N1/N2 반영분 + 회귀 발견

- **대상:** 미커밋 작업 트리 변경분 (HEAD `128bf35` 대비, 빌더 2종 + 테스트 5개 파일)
- **리뷰 일자:** 2026-06-04
- **범위:** N1·N2 반영 정확성 + production 영향 추적

## 기계적 반영 (정확)
- **N2** — `CharacterBlueprintBuilder` / `AIControllerBlueprintBuilder`가 `Options.bAllowReplace = true`(하드코딩) → `= Spec.bAllowReplace`로 변경.
- **N1** — `BlueprintDefaultsCommandletTest` 2건이 `Options.bAllowReplace = true` 명시.
- **테스트 일관성** — 에셋 생성 테스트(AIFlowBinding, CharacterBuilder, CreateAIFlow×2) 전부 `bAllowReplace=true` 세팅. 검증 전용 `AIFlowSpecValidatorTest`(`Validate`만 호출)는 올바르게 미변경.

## 🔴 핵심 결함 — N2가 production 회귀 유발

문서화된 snake_case `allow_replace`를 JSON에서 읽는 파서는 **`BlueprintCreateSpecParser` 하나뿐**이다. Character/AIController/AIFlow 커맨드릿은 `FAIFlowSpecParser`로 파싱하며 이 파서는 중첩 `Character.allow_replace` / `AIController.allow_replace`를 수동 매핑하지 않는다. 따라서 현재 문서·예제 스펙 형식 기준으로는 `Spec.*.bAllowReplace`가 기본 `false`로 남는다.

추적:
- `CreateCharacterBlueprintCommandlet.cpp:55` → `FAIFlowSpecParser::Parse` → 문서화된 `allow_replace` 입력으로는 `Spec.Character.bAllowReplace`가 기본 `false` 유지
- `:75` → `FCharacterBlueprintBuilder::Build(Spec.Character, ...)` → `Options.bAllowReplace = false`
- CreateAIController, CreateAIFlow 동일

| | 기존(`128bf35`) | N2 적용 후 |
|---|---|---|
| 빌더 `bAllowReplace` | `true` 하드코딩 → 항상 덮어쓰기 | `Spec.bAllowReplace` = `false` |
| 기존 에셋 재생성 | 덮어쓰기 성공(멱등) | **`BP_ALREADY_EXISTS` 실패** |
| JSON opt-in | 불필요 | 문서화된 `allow_replace` 키로는 **불가능** |

→ 캐릭터/AIController/AIFlow를 두 번째로 생성하면 실패. 의도된 멱등 파이프라인이 깨짐. **자동화 테스트는 C++에서 `bAllowReplace=true`를 직접 세팅해 회귀를 가림**(production JSON 사용자는 문서화된 `allow_replace` 키로 같은 상태를 만들 수 없음).

> 2차 리뷰의 N2 권장은 표면적 일관성에만 주목한 오판. 두 빌더의 `true` 하드코딩이 의도된 멱등 동작이었음.

### 영향 범위 — 기본 스모크와 프로파일 반복 실행

- `tools/test/smoke/create_ai_flow.sh`의 기본 입력은 `specs/examples/guard_ai.json`이며, 이 파일에는 `Character`/`AIController` 하위 `allow_replace`가 없다.
- `specs/profiles/npc_character.json`, `specs/profiles/patrol_ai.json`도 동일하게 덮어쓰기 옵션을 명시하지 않는다.
- 위 스펙들은 Phase 7 마스터 인수 조건과 래퍼(`setup_npc_character.*`, `setup_patrol_ai.*`)의 기본 사용 경로다.

따라서 N2 적용 상태에서는 같은 프로젝트에서 스모크 또는 프로파일 래퍼를 재실행할 때 기존 Character/AIController Blueprint가 남아 있으면 `BP_ALREADY_EXISTS`로 실패할 수 있다. 안전 기본값(`false`) 자체가 목표라면 스모크는 고유 임시 경로를 쓰거나 생성물을 정리해야 한다. 반대로 Phase 7의 원스톱 워크플로우 멱등성이 목표라면 Character/AIController 빌더는 기존 `true` 하드코딩을 유지하거나, `FAIFlowSpecParser`가 중첩 `allow_replace`를 명시적으로 파싱하고 기본 프로파일도 함께 갱신해야 한다.

## 권장 수정 (택1)
1. **N2 되돌리기 (권장)** — 두 빌더를 `Options.bAllowReplace = true`로 복원(이 흐름은 의도적 덮어쓰기). 2줄 복원으로 Phase 7 멱등 회귀를 해소.
2. **파서 배선** — `FAIFlowSpecParser`가 서브스펙별 `allow_replace`를 읽도록 수동 매핑하고, `AIFlowSpecParserTest`에 snake_case 중첩 옵션 케이스를 추가. 멱등 기본값을 유지하려면 프로파일에도 명시적 `allow_replace: true`를 넣어야 함.

## 결론
N1·N2 기계적 반영은 정확하나, **N2는 현 상태로 채택하면 안 됨** — production 멱등 생성 흐름과 기본 스모크 반복 실행성을 깨뜨림. 가장 작은 수정은 위 1번 복원이며, 스키마 일관성을 제품 요구로 삼는다면 위 2번까지 함께 해야 한다.

---

# 5차 검증 — N2 되돌림(권장안 #1) 반영 확인

- **대상:** 미커밋 작업 트리 변경분 (HEAD `128bf35` 대비)
- **리뷰 일자:** 2026-06-04
- **범위:** 4차 회귀가 권장안 #1로 해소됐는지 + 잔여 정리 항목

## 해소 확인 (✓ 회귀 제거)

4차 검증의 권장안 **#1(N2 되돌리기)**이 적용됨. 현재 배선:

| 위치 | 현재 값 | 효과 |
|------|---------|------|
| `GenericBlueprintBuilder.h:16` | `bAllowReplace = false` | 안전 기본값(finding C) 유지 |
| `CharacterBlueprintBuilder.cpp:13` | `Options.bAllowReplace = true` | 항상 덮어쓰기 복원 → 멱등 |
| `AIControllerBlueprintBuilder.cpp:13` | `Options.bAllowReplace = true` | 동일 |
| `CreateBlueprintCommandlet.cpp:69` | `Options.bAllowReplace = Spec.bAllowReplace` | CreateBlueprint은 JSON `allow_replace` 존중 |

→ **Character/AIController/AIFlow 재생성 회귀 제거.** 기본 스모크(`create_ai_flow.sh`)·프로파일 래퍼(`setup_npc_character.*`, `setup_patrol_ai.*`) 반복 실행성도 복원됨. 동시에 `CreateBlueprint` 단일 커맨드릿은 안전 기본값(`false`)으로 의도적 덮어쓰기 차단을 유지 — 두 흐름의 의도가 코드에 명확히 분리됨.

## 잔여 정리 항목 (회귀 아님, 선택)

### R1. ⚪ [데드코드] 빌더-경로 테스트의 `Spec.*.bAllowReplace = true`가 무효
- **위치:** `Tests/CharacterBlueprintBuilderTest.cpp`, `Tests/AIFlowBindingTest.cpp`, `Tests/CreateAIFlowCommandletTest.cpp`
- **내용:** 두 빌더가 `Options.bAllowReplace = true`로 스펙 값을 덮으므로, 이 테스트들이 세팅한 `Spec.Character.bAllowReplace` / `Spec.AIController.bAllowReplace`는 **빌더가 무시 → no-op**. 동작엔 무해하나, "스펙 필드가 replace를 제어한다"는 오해를 부르는 데드코드.
- **예외:** `BlueprintDefaultsCommandletTest`는 `FGenericBlueprintBuilder::Build`를 직접 호출하며 `Options.bAllowReplace = true`를 Options에 세팅 → 헤더 기본값 `false`를 상쇄하므로 **유효**(유지 필요).
- **권장:** 빌더-경로 3개 테스트의 `Spec.*.bAllowReplace = true` 라인 제거(가독성), 또는 그대로 둠(무해).

## 결론
4차 핵심 회귀는 권장안 #1로 **해소됨.** 현재 배선은 멱등 흐름(빌더 하드코딩)과 안전 흐름(커맨드릿 스펙 존중)을 의도대로 분리한다. 남은 R1은 데드코드 정리 수준의 선택 항목으로 머지를 막지 않는다.

---

# 6차 검증 — 외부 HIGH/MEDIUM 지적 평가 (CreateAIFlow allow_replace)

- **대상:** 외부 리뷰 지적 2건 (CreateAIFlow의 nested `allow_replace` 미반영 / smoke·profile 반복성)
- **리뷰 일자:** 2026-06-04
- **결론 요약:** 두 지적 모두 **현재 코드 기준 stale(무효)** — 전제(빌더가 `Spec.bAllowReplace`를 따름)가 5차에서 되돌려짐. 단 HIGH가 가리킨 **파서 스키마 불일치는 실재**하며 옵션 #2 채택 시에만 유효.

## 전제 검증
지적은 *"이번 변경으로 빌더가 `Spec.bAllowReplace`를 따른다"*를 전제하나, 실제 현재 코드는:
```cpp
// CharacterBlueprintBuilder.cpp:13 / AIControllerBlueprintBuilder.cpp:13
Options.bAllowReplace = true;   // 하드코딩 (N2 되돌려짐, 5차 참조)
```
→ 빌더는 `Spec.bAllowReplace`를 보지 않고 **항상 덮어씀.**

## HIGH 평가 — 🟡 현재 무효, 옵션 #2 한정 유효
- **현재:** 빌더 `true` 하드코딩 → CreateAIFlow/Character/AIController는 항상 덮어쓰기 → 기존 `BP_Guard`/`BP_GuardController`가 있어도 `BP_ALREADY_EXISTS` 미발생. 지적이 묘사한 회귀는 현 코드에 없음.
- **단, 실재하는 부분 — 파서 스키마 불일치:**
  - `AIFlowSpecParser.cpp:9` → `FJsonObjectConverter::JsonObjectStringToUStruct` = UPROPERTY 이름 기반 **PascalCase** 자동 매핑. `specs/examples/guard_ai.json`도 PascalCase(`AssetPath`/`ParentClass`).
  - `BlueprintCreateSpecParser.cpp:31` → `allow_replace` **snake_case 수동 매핑**.
  - 옵션 #2로 전환 시 AIFlow opt-in 키는 `Character.AllowReplace`(PascalCase)여야 함. **지적이 제안한 `Character.allow_replace`는 JsonObjectConverter가 매핑조차 못 함** — "opt-in 불가" 진단은 맞으나 제안 키는 그 스키마에서 부정확.

## MEDIUM 평가 — 🟢 현재 무효
- *"기본값 false라 guard_ai.json 재실행 시 실패"* → 빌더 `true` 하드코딩이므로 smoke/프로파일 반복 실행은 멱등 유지. 전제(기본 false 적용)가 성립 안 함.

## 요약 표
| 지적 | 현 코드(빌더 `true` 하드코딩) | 옵션 #2(파서 배선) 채택 시 |
|------|------------------------------|----------------------------|
| HIGH — CreateAIFlow opt-in 불가 | 무효(항상 덮어쓰기) | **유효** + 제안 키 PascalCase(`AllowReplace`)로 정정 |
| MEDIUM — smoke 반복성 | 무효(멱등 유지) | 유효 |

## 결론
현재 채택된 해법(권장안 #1: 빌더 `true` 하드코딩)에서 두 지적은 live 이슈가 아님. **옵션 #2(`FAIFlowSpecParser`가 스펙 존중)로 전환할 경우의 선결 과제**로서만 유효하며, 그때는 (a) nested replace 키를 **PascalCase**로 명시 파싱, (b) 파서 테스트로 허용 키 고정이 필요. 현 상태 머지는 무방.

---

# 7차 검증 — 커밋 `a34ee5a` 코드 리뷰

- **대상 커밋:** `a34ee5a` (`fix(editor): address 4th code review feedback and roll back N2`)
- **작성자/일시:** kimkyungpyo, 2026-06-04 12:02
- **범위:** 1~4차 지적 해소분을 커밋으로 묶은 결과 + 신규 변경(스모크 grep) 점검

## 기존 검증 항목 (모두 정확 ✓)

| 변경 | 내용 | 평가 |
|------|------|------|
| **A** | `GenericBlueprintBuilder.cpp` — blueprintable 미검사 시 `parent_class_blueprintable=skipped` | ✓ |
| **B** | `CreateBlueprintCommandlet.cpp` — exit code 3개 코드 정확 `==` 비교 | ✓ |
| **C** | `GenericBlueprintBuilder.h` — `bAllowReplace` 기본값 `false` | ✓ |
| **N2 revert** | 두 빌더 `Options.bAllowReplace = true` 명시 → 멱등 복원 | ✓ 회귀 해소 |
| **N1** | `BlueprintDefaultsCommandletTest` Options에 `=true`(헤더 기본 `false` 상쇄) | ✓ 유효 |

## 신규 변경 평가 — 스모크 `grep` 필터 🟢

```diff
- REPORT_FILE="$(tail -1 "${TEMP_OUT}")"
+ REPORT_FILE="$(grep -E '\.json$' "${TEMP_OUT}" | tail -1)"
```
- **타당한 견고성 개선.** 직전 `tail -1`은 래퍼가 리포트 경로를 출력한 뒤 **UE 에디터 종료 로그가 마지막 줄로 끼어들면** 엉뚱한 줄을 잡았음(커밋 메시지의 "종료 로그 간섭"). `.json`으로 끝나는 줄만 필터 후 마지막 것을 취해, `run_commandlet.sh:138`이 에디터 종료 후 마지막에 echo하는 `..._<UTC>.json` 경로를 안정 추출. 4개 단계 일관 적용.
- 매칭 실패 시 `REPORT_FILE`이 비어 `jq`가 실패→테스트가 시끄럽게 실패 → stale 오탐보다 안전.
- **잔여 미세 위험:** stdout에 `.json`으로 끝나는 다른 로그 줄이 최종 echo 이후 나타나면 오선택 가능하나, 해당 echo는 에디터 `wait` 종료 후 마지막 stdout이라 실질적으로 마지막 `.json` 줄 → 현실적 안전.

## 잔여 항목 (회귀 아님)
- **R1 (5차에서 문서화)** — 빌더-경로 테스트 3종(`AIFlowBindingTest`, `CharacterBlueprintBuilderTest`, `CreateAIFlowCommandletTest`)의 `Spec.*.bAllowReplace = true`는 빌더가 `true` 하드코딩으로 스펙을 무시 → **no-op 데드코드**. 무해하나 "스펙이 replace 제어"라는 오해 소지. (`BlueprintDefaultsCommandletTest`는 `Options` 직접 세팅이라 유효 — 예외.)

## 결론
커밋 `a34ee5a`는 **1~4차 리뷰 지적을 정확히 해소했고 신규 회귀·빌드 깨짐 없음.** 스모크 grep 변경도 견고성 개선으로 타당. 남은 것은 R1(테스트 3파일의 무효 라인 3줄)뿐이며 머지를 막지 않음.

---

# 8차 검증 — 커밋 `c0ba72e` (R1 데드코드 제거)

- **대상 커밋:** `c0ba72e` (`refactor(tests): remove deadcode bAllowReplace assignments`)
- **작성자/일시:** kimkyungpyo, 2026-06-04 12:12
- **범위:** R1(빌더-경로 테스트의 무효 `Spec.*.bAllowReplace`) 제거 정확성

## 제거 정확성 ✓
데드코드 `Spec.*.bAllowReplace = true`를 빌더-경로 테스트 3종에서 정확히 제거:
- `AIFlowBindingTest` — Character + AIController 2줄
- `CharacterBlueprintBuilderTest` — 1줄
- `CreateAIFlowCommandletTest` — 2개 테스트 × 2 = 4줄

## 불변식 보존 검증 (핵심) ✓
| 항목 | 상태 | 판정 |
|------|------|------|
| 빌더 `Options.bAllowReplace = true` (line 13×2) | 유지 | replace 동작 변화 없음 → 제거가 진짜 no-op |
| `BlueprintDefaultsCommandletTest` `Options.bAllowReplace = true` (line 100·130) | 유지 | 유효한 Options-레벨 세팅(예외)을 오삭제하지 않음 |

R1에서 "유효하므로 유지 필요"라 지목했던 `BlueprintDefaultsCommandletTest` 두 라인은 건드리지 않고, 무효한 spec-레벨 라인만 제거. diff의 index 해시가 `a34ee5a` 추가분의 정확한 역(`e92d68f→e5bb521` 등)이라 해당 라인이 `128bf35` 상태로 깔끔히 복원됨.

## 부수 효과
- 빌더 `true` 하드코딩이라 테스트 재실행 시 에셋 덮어쓰기 유지 → **테스트 동작 불변, 회귀 없음.**
- 테스트가 production 동작(빌더 멱등 덮어쓰기)을 정확 반영 → "스펙 필드가 replace 제어"라는 오해 소지 제거.

## 결론
R1 정리는 **정확하고 완전.** 무효 라인만 제거, 유효 라인 보존, 동작 불변, 신규 결함 없음.

---

# 전체 리뷰 사이클 종결 요약

| 단계 | 대상 | 지적 | 처리 |
|------|------|------|------|
| 1차 | 브랜치 diff (`3307a90`) | 1~5 (빌더 우회·스펙 중복·smoke `ls -t`·`/dev/null`·`.bat`) | 커밋 `128bf35`에서 해소 |
| 2차 | 커밋 `128bf35` | A·B·C·D (blueprintable 오기록·exit code·기본값·포맷) | 커밋 `a34ee5a`에서 해소 |
| 3·4·5차 | 미커밋 수정 | N1·N2 → **N2 회귀** 발견 → 권장안 #1 복원 | 커밋 `a34ee5a`(N2 롤백) |
| 6차 | 외부 HIGH/MEDIUM | CreateAIFlow allow_replace | 현 코드 기준 stale, 옵션 #2 한정 유효로 정리 |
| 7차 | 커밋 `a34ee5a` | 스모크 grep 개선 확인 + R1 잔존 | 타당 |
| 8차 | 커밋 `c0ba72e` | R1 데드코드 제거 | 해소 |

**최종 상태:** 모든 코드 이슈 해소/처리 완료. 남은 live 결함 없음. 옵션 #2(`FAIFlowSpecParser` nested replace 파싱)는 향후 스키마 일관성을 제품 요구로 삼을 경우의 선택 과제로만 보류.
