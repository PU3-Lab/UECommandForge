# 9B 설계 — Cook/Packaging 로그 분석 계층 (Cook Log Analysis)

> 상위 로드맵: [references/10_packaging_cook_build_plan.md](../plans/references/10_packaging_cook_build_plan.md) 5단계(로그 분석) · 선행 9A: [2026-06-14-phase9a-pre-release-validation-design.md](2026-06-14-phase9a-pre-release-validation-design.md)
> 범위 기준: [harness_scope_and_priorities.md](../../implementation/harness_scope_and_priorities.md) (읽기 전용 분석 → 리포트)

## 0. 목적

9B는 **이미 생성된 Cook/Packaging 로그 파일을 읽어** 출시 차단성 문제(누락 에셋·로드 실패·플러그인 누락·셰이더/BP 컴파일·Map Check)를 분류하고 Result JSON·Markdown 리포트로 직렬화한다.

- **읽기 전용**: 로그 파일만 읽는다. **Cook을 실행하지 않는다**(실제 cook/packaging 실행은 9C).
- AI는 거대한 raw 로그가 아니라 **분류된 Result JSON**만 검토한다(harness 원칙).
- 9D 통합 게이트에 `cook_log` suite로 합류 가능.

---

## 1. 범위

신규 커맨드렛 1개.

| 커맨드렛 | 래퍼 | 역할 |
|---|---|---|
| `AnalyzeCookLogCommandlet` | `tools/ue/analyze_cook_log.sh` / `.bat` | Cook 로그를 룰 JSON으로 분류 → Result JSON |

> **커맨드렛 여부 판단:** 로그 분석은 UE 에디터 API를 쓰지 않는 순수 텍스트 처리다. 그럼에도 **기존 `FCommandForgeReport`/`JsonReportWriter`/자동화 테스트 하네스/9D 통합 표면과의 일관성**을 위해 커맨드렛으로 둔다. 에디터 기동 비용이 문제가 되면 후속에서 standalone 파서로 추출(YAGNI, §8).

---

## 2. 공통 설계

### 2.1 실행 방식

```bash
UnrealEditor-Cmd MyProject.uproject -run=AnalyzeCookLog \
  -Log=<path-to-cook.log> -Rules=<cook_log.rules.json> -Output=<report.json>
```

### 2.2 리포트 위치 / 형식

```txt
Saved/UECommandForge/Reports/AnalyzeCookLog_<YYYYMMDDThhmmssZ>.json
```

기존 컨벤션(UTC basic ISO, `Z`) 유지. Markdown 요약 동반 가능(기존 `PrototypeAutomation` Markdown 패턴 재사용).

### 2.3 공통 Result JSON / ValidationIssue

기존 `FCommandForgeReport`·`FCommandForgeValidationIssue`(`severity/code/message/field/filePath/suggestedFix`)를 그대로 사용한다. 9A와 동일하게 `command`/`commandlet` 키 병기.

### 2.4 성공/실패 판정 + Exit Code

| 조건 | `ok` |
|---|---:|
| error 카테고리 0건 (warning만/무이슈) | `true` |
| error 카테고리 1건 이상 | `false` |
| Cook fatal/실패 마커 존재 | `false` (리포트 최상단 표시) |
| 룰/로그 입력 오류 | `false` |

```cpp
return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
```

### 2.5 룰 파일 위치

- **운용(대상 프로젝트):** `<Project>/Config/UECommandForge/cook_log.rules.json`
- **레포 예시/fixture:** `specs/policies/cook_log.rules.example.json`
- 경로는 인자로 명시. `-Rules` 생략 시 example(동봉 기본 룰) 사용.

---

## 3. `AnalyzeCookLogCommandlet`

### 3.1 입력

```txt
-Log=<path-to-cook-or-packaging-log>
-Rules=<path-to-cook_log.rules.json>   (선택, 기본 example)
-Output=<report.json>
```

### 3.2 룰 JSON

```json
{
  "version": "1",
  "kind": "cook_log_rules",
  "rules": [
    { "code": "COOK_MISSING_ASSET",        "severity": "error",   "pattern": "Missing\\s+Asset", "captureReferences": true },
    { "code": "COOK_UNABLE_TO_LOAD_PACKAGE","severity": "error",   "pattern": "Unable to load package" },
    { "code": "COOK_SOFTOBJECTPATH_UNRESOLVED","severity": "error","pattern": "SoftObjectPath.*(could not|failed to) resolve" },
    { "code": "COOK_PLUGIN_MISSING",        "severity": "error",   "pattern": "(Plugin|Module).*(missing|not found)" },
    { "code": "COOK_SHADER_COMPILE_ERROR",  "severity": "error",   "pattern": "Shader compile.*error" },
    { "code": "COOK_BLUEPRINT_COMPILE_WARNING","severity": "warning","pattern": "Blueprint.*Warning" },
    { "code": "COOK_MAP_CHECK_ERROR",       "severity": "error",   "pattern": "MapCheck.*Error" }
  ],
  "fatalMarkers": ["Cook failed", "COOK FAILED", "Fatal error"]
}
```

> **매칭 의미:** `pattern`은 대소문자 무시 정규식(라인 단위). `captureReferences:true`이면 해당 라인에서 "참조한 에셋 ↔ 참조된 에셋" 경로쌍을 추출해 detail에 기록(로드맵 테스트 기준: "Missing Asset은 참조한/참조된 에셋 함께 표시").

### 3.3 분류 카테고리 (Issue Code)

`COOK_MISSING_ASSET`, `COOK_UNABLE_TO_LOAD_PACKAGE`, `COOK_SOFTOBJECTPATH_UNRESOLVED`, `COOK_PLUGIN_MISSING`, `COOK_SHADER_COMPILE_ERROR`, `COOK_BLUEPRINT_COMPILE_WARNING`, `COOK_MAP_CHECK_ERROR`, `COOK_FATAL`, `COOK_RULES_INVALID`, `COOK_LOG_READ_FAILED`.

### 3.4 판정 규칙

| 규칙 | severity | ok 영향 |
|---|---:|---:|
| fatal 마커 존재 | `error`(`COOK_FATAL`) | `false`, 리포트 최상단 |
| error 룰 매칭 | `error` | `false` |
| warning 룰 매칭 | `warning` | 기본 `true` |
| 룰 JSON 무효 | `error` | `false` |
| 로그 파일 못 읽음 | `error` | `false` |

---

## 4. 리포트 구조

- `issues[]`: 게이트용 분류 이슈(code·severity·message·라인번호는 `field`에 `line:<n>`).
- `summary`: `errorCount`, `warningCount`, code별 카운트, `cookFatal`(bool), `analyzedLineCount`.
- `missing_assets[]`(detail): `{ "referencer": "...", "referenced": "..." }` 쌍(`captureReferences` 결과).
- fatal/최상단 표시: Markdown 요약 첫 섹션에 실패 원인 우선 출력(로드맵 "실패 원인을 리포트 최상단에 표시").

> 대용량 로그 대비: 동일 code 반복은 카운트 집계 + 대표 N건(예: 20)만 `issues`에 수록, 전체 카운트는 `summary`. (노이즈/리포트 비대화 방지)

---

## 5. README 명령 표면

| Command | Wrapper | Description | Output |
|---|---|---|---|
| `AnalyzeCookLog` | `tools/ue/analyze_cook_log.sh/.bat` | Cook/Packaging 로그를 룰로 분류 | `Saved/UECommandForge/Reports/*.json` |

---

## 6. 9D 통합 방향

9D 통합 게이트(`PrototypeAutomation`)에 `cook_log` suite로 합류:
- 게이트 인자: `-CookLog=<path>`(+ `-CookLogRules=<path>`) → suite `cook_log` → `AnalyzeCookLog`.
- child `ok=false` → 통합 `ok=false` 전파(기존 집계 경로 재사용). 독립 실행도 유지.

---

## 7. 테스트 설계

### 7.1 원칙
무거운 실제 cook 로그를 커밋하지 않는다. **합성 로그 fixture**(문제 라인 포함/미포함)를 temp에 생성해 분류·판정·참조쌍 추출을 단언.

### 7.2 케이스

| 케이스 | 기대 |
|---|---|
| Clean(문제 라인 없음) | `ok:true`, issue 없음 |
| MissingAsset(참조쌍 포함 라인) | `COOK_MISSING_ASSET`, `ok:false`, `missing_assets`에 referencer/referenced |
| UnableToLoad | `COOK_UNABLE_TO_LOAD_PACKAGE`, `ok:false` |
| ShaderError | `COOK_SHADER_COMPILE_ERROR`, `ok:false` |
| BlueprintWarningOnly | `ok:true`(warning만), warning 집계 |
| Fatal | `COOK_FATAL`, `ok:false`, summary.cookFatal=true |
| RulesInvalid | `COOK_RULES_INVALID`, `ok:false` |
| LogMissing | `COOK_LOG_READ_FAILED`, `ok:false` |

### 7.3 Smoke
래퍼 실행 → `ok == expected`, `issues[].code ⊇ expected`, 리포트 파일 존재 단언. (`tools/test/smoke/analyze_cook_log.sh`)

---

## 8. 범위 밖 (YAGNI)

- **실제 Cook/Packaging 실행**(`cook-smoke`, `dry-run`) — 9C. 9B는 로그 입력만.
- Plugin/Config 수정안 자동 생성(사람/LLM 판단).
- standalone(비-커맨드렛) 파서 추출(에디터 기동 비용 문제 확인 시).
- 로그 스트리밍/실시간 분석(배치 1회 분석만).

---

## 9. 구현 우선순위

0. **스파이크 (필수, H1 등가):** UE 5.7에서 **실제 Cook 로그(성공 1 + 의도적 실패 1)를 1회 캡처**해, §3.2 각 카테고리의 **실제 라인 포맷/정규식을 확정**한다. 로그 포맷이 예상과 다르면 룰 패턴을 교정한 뒤 본 설계 확정. (로드맵 리스크 "로그 패턴 다양함" 대응)
1. 공통 `FCommandForgeReport` 재사용 + 신규 summary 필드.
2. 룰 JSON 파서(+ 기본 example 룰 동봉).
3. `AnalyzeCookLogCommandlet` 분류 엔진(라인 스캔·정규식·카운트 집계·대표 N건).
4. `captureReferences`(Missing Asset 참조쌍 추출).
5. fixture(합성 로그) + 자동화 테스트.
6. shell/bat 래퍼 + 스모크.
7. README 등재.
8. (선택) 9D `cook_log` suite 합류.

---

## 10. 리스크 및 가정

| 리스크 | 완화 |
|---|---|
| Cook 로그 포맷이 UE 버전/플랫폼/verbosity마다 다름 | §9-0 스파이크로 실제 포맷 고정 + 룰을 외부 JSON으로 분리(누적 갱신) |
| 정규식 오탐/누락 | 룰별 대표 케이스 회귀 테스트, severity 보수적 설정 |
| 대용량 로그 성능/리포트 비대화 | 대표 N건 수록 + 카운트 집계, 스트리밍 라인 처리 |
| Missing Asset 참조쌍 포맷 다양 | `captureReferences`를 옵셔널·룰별로, 추출 실패 시 라인 원문 보존 |
