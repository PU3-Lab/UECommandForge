# Phase 9B — AnalyzeCookLog 구현 계획 (2026-06-15-phase9b-cook-log-analysis)

> **For agentic workers (Antigravity):** Task 단위 TDD("실패 테스트 → 구현 → 통과 → 커밋"). 코드는 Antigravity에서 작성한다.

**Goal:** Cook/Packaging 로그 파일을 룰 JSON으로 분류해 Result JSON을 내는 `AnalyzeCookLog` 커맨드렛(읽기 전용)을 추가한다.

**기준 스펙:** [2026-06-15-phase9b-cook-log-analysis-design.md](../specs/2026-06-15-phase9b-cook-log-analysis-design.md)

**Architecture:** 기존 검증 커맨드렛 패턴 — `Main`에서 인자 파싱 → 룰 JSON 파싱 → 로그 라인 스캔(`FRegexPattern`) → `FCommandForgeReport`에 `ValidationIssues` 축적 → `FJsonReportWriter` 기록 → `ECommandForgeExitCode`. 에디터 API 미사용(순수 텍스트).

**Tech Stack:** UE 5.7 C++20, `Json`, `Internationalization/Regex.h`(`FRegexPattern` — 이미 `ValidateDataSourceCommandlet`에서 사용, 신규 의존성 없음). 기존 `JsonReportWriter`/`CommandForgeTypes`.

---

## Task 0: 스파이크 (선행 필수 — 사용자 환경)

> **이 Task는 코드가 아니라 "실제 로그 확보"다. 회원님(또는 UE 환경)이 수행.**

- [ ] **Step 1: 실제 Cook 로그 캡처** — UE 5.7에서 성공 cook 1회 + 의도적 실패 cook 1회 실행, 로그 저장:
  ```bash
  UnrealEditor-Cmd MyProject.uproject -run=Cook -targetplatform=Windows \
    -abslog="$(pwd)/sample/Saved/Logs/cook_spike.log" -unattended -nopause
  ```
- [ ] **Step 2: 라인 포맷 확정** — 캡처된 로그에서 §3.2 각 카테고리(Missing Asset, Unable to load package, Shader compile error 등)의 **실제 라인 문자열**을 발췌해 정규식을 확정. 스펙 §3.2 룰 패턴이 실제와 다르면 교정.
- [ ] **Step 3: example 룰 갱신** — 확정 패턴을 `specs/policies/cook_log.rules.example.json`에 반영.

> 스파이크 산출물(발췌 라인 + 확정 정규식)이 Task 3 분류 엔진의 fixture·룰 기준이 된다. **이 단계 없이 Task 3을 시작하면 정규식이 추측이 된다.**

---

## File Structure

| 파일 | 책임 |
|---|---|
| `EDITOR/Public/Commandlets/AnalyzeCookLogCommandlet.h` | 커맨드렛 선언 |
| `EDITOR/Private/Commandlets/AnalyzeCookLogCommandlet.cpp` | 인자 파싱, 룰 파싱, 라인 분류, 리포트 |
| `EDITOR/Tests/AnalyzeCookLogCommandletTest.cpp` | 자동화 테스트(합성 로그 fixture) |
| `tools/ue/analyze_cook_log.sh` / `.bat` | 래퍼 |
| `specs/policies/cook_log.rules.example.json` | 기본 룰 |
| `README.md` | 명령 표면 등재 |

`EDITOR = sample/Plugins/UECommandForge/Source/UECommandForgeEditor`.

---

## Task 1: 스켈레톤 + 인자 파싱 + 빈 리포트

- [ ] **Step 1: 헤더** — `AnalyzeCookLogCommandlet.h`(UCLASS, `Main` override). 기존 `ValidatePluginDependenciesCommandlet.h` 형태 그대로.
- [ ] **Step 2: 실패 테스트** — `-Log` 누락 시 `SpecParseFailed` + `ok:false` 리포트 기록을 단언.
- [ ] **Step 3: 테스트 실패 확인** — `tools/test/automation/run.sh`
- [ ] **Step 4: 최소 구현** — 인자(`Log`/`Rules`/`Output`) 파싱, `Report.Commandlet="AnalyzeCookLog"`, `bDryRun=true`. `-Output`/`-Log` 누락 처리 + `JsonReportWriter::Write`.
- [ ] **Step 5: 통과 확인** — 빌드 + 자동화.
- [ ] **Step 6: 커밋** — `feat: scaffold AnalyzeCookLog commandlet`

---

## Task 2: 룰 JSON 파싱

- [ ] **Step 1: 실패 테스트** — `kind != "cook_log_rules"`면 `SpecParseFailed`(`COOK_RULES_INVALID`).
- [ ] **Step 2: 구현** — `FCommandForgePolicyParser::ParseJson` 재사용 + 룰 구조체 파싱:
  ```cpp
  struct FCookLogRule { FString Code; FString Severity; FString Pattern; bool bCaptureReferences = false; };
  struct FCookLogRules { TArray<FCookLogRule> Rules; TArray<FString> FatalMarkers; };
  ```
  각 `rules[]`에서 `code`/`severity`/`pattern` 필수, `captureReferences` 옵셔널. `fatalMarkers[]` 파싱. 무효 시 `COOK_RULES_INVALID`.
- [ ] **Step 3: 통과 확인** + **커밋** — `feat: parse cook_log_rules JSON`

---

## Task 3: 분류 엔진 (라인 스캔 + 정규식 + 집계)

- [ ] **Step 1: 실패 테스트** — 합성 로그 fixture(temp): "Missing Asset" 라인, "Unable to load package" 라인, clean 라인 혼합 → `COOK_MISSING_ASSET`/`COOK_UNABLE_TO_LOAD_PACKAGE` 이슈 발생, `ok:false`.
  ```cpp
  // helper: SaveLog(Path, TEXT("...LogCook: Warning: Unable to load package ...\n..."))
  ```
- [ ] **Step 2: 구현** — 로그를 라인 배열로 로드(`FFileHelper::LoadFileToStringArray`), 각 룰의 `FRegexPattern`을 라인마다 매칭:
  ```cpp
  for (int32 i = 0; i < Lines.Num(); ++i)
  {
      for (const FCookLogRule& Rule : Rules.Rules)
      {
          FRegexMatcher M(FRegexPattern(Rule.Pattern, ERegexPatternFlags::CaseInsensitive), Lines[i]);
          if (M.FindNext())
          {
              // code별 카운트 증가; 대표 N건(기본 20)까지만 AddIssue, field="line:<i+1>"
          }
      }
  }
  ```
  - `summary`: `error_count`/`warning_count`/code별 카운트/`analyzed_line_count`를 `Report.Validation`에 기록.
  - 대표 N건 초과분은 카운트만 누적(리포트 비대화 방지, 스펙 §4).
- [ ] **Step 3: 통과 확인** — 빌드 + 자동화.
- [ ] **Step 4: 커밋** — `feat: cook log rule classification engine`

---

## Task 4: fatal 마커 + captureReferences

- [ ] **Step 1: 실패 테스트 (fatal)** — fatal 마커 포함 로그 → `COOK_FATAL`, `ok:false`, `summary.cookFatal=true`.
- [ ] **Step 2: 실패 테스트 (참조쌍)** — Missing Asset 라인(참조한↔참조된 경로 포함)에서 `missing_assets[]`에 `referencer`/`referenced` 추출 단언.
- [ ] **Step 3: 구현** —
  - fatal: `FatalMarkers` 중 하나라도 포함 라인 발견 시 `COOK_FATAL` error + `Validation.Add("cook_fatal","true")`. Markdown 최상단 우선 표기(스펙 §4).
  - captureReferences: 해당 룰 매칭 라인에서 경로쌍 추출(정규식 캡처그룹 또는 `->`/`referenced by` 토큰 분리). 추출 실패 시 라인 원문 보존.
  - `missing_assets[]`는 별도 detail(예: `Report`의 `Validation` 외 전용 배열 필드가 없으면 `ValidationIssues`의 `field`에 `referencer=...;referenced=...` 인코딩 또는 리포트 writer에 detail 배열 추가 검토).
  > **설계 노트:** 리포트 스키마에 detail 배열이 없으면 (a) issue `message`/`field`에 쌍 인코딩으로 시작하고, (b) 필요 시 후속에서 `JsonReportWriter`에 `cook_details` 배열 추가. Task 4에서는 (a)로 최소 구현.
- [ ] **Step 4: 통과 확인** + **커밋** — `feat: fatal markers and missing-asset reference capture`

---

## Task 5: 래퍼 + 예시 룰 + README + 스모크

- [ ] **Step 1: 예시 룰** — `specs/policies/cook_log.rules.example.json`(Task 0 스파이크 패턴 반영).
- [ ] **Step 2: 셸/배치 래퍼** — `tools/ue/analyze_cook_log.sh`/`.bat`(기존 `validate_buildcs` 패턴, `run_commandlet` 경유, `AnalyzeCookLog -Log=... -Rules=...`).
- [ ] **Step 3: README** — 명령 표면 표에 `analyze_cook_log` 등재.
- [ ] **Step 4: 스모크** — `tools/test/smoke/analyze_cook_log.sh`: 합성 실패 로그 → `ok==false` + `COOK_*` code 존재 + 리포트 파일 생성 단언.
- [ ] **Step 5: 커밋** — `feat: analyze_cook_log wrapper, example rules, README, smoke`

---

## Task 6: 통합 검증

- [ ] **Step 1: 전체 그린** — `tools/ue/build_plugin.sh && tools/test/automation/run.sh` (FAIL 0).
- [ ] **Step 2: 스모크** — `tools/test/smoke/analyze_cook_log.sh` 그린.
- [ ] **Step 3: 작업 트리 위생** — 커밋 전 `git diff`로 무관 변경 선별(`sample/Content` 등).
- [ ] **Step 4: 리뷰 문서** — `docs/reviews/`에 9B 검토 보고서(규칙: 리뷰 시 문서화).

---

## 인수 조건 (Acceptance)

```bash
tools/ue/build_plugin.sh && tools/test/automation/run.sh        # FAIL 0
tools/test/smoke/analyze_cook_log.sh                            # 실패 로그 → ok:false 그린
jq -e '.command == "AnalyzeCookLog" and (.issues | map(.code) | index("COOK_MISSING_ASSET")) != null' \
   "$(ls -t sample/Saved/UECommandForge/Reports/AnalyzeCookLog_*.json | head -1)"
```

- 합성 로그의 각 카테고리가 올바른 code로 분류.
- error 카테고리 존재 시 `ok:false`, warning만이면 `ok:true`.
- fatal 마커 시 `cook_fatal=true` + 최상단 표기.
- Missing Asset 참조쌍 기록.

---

## 후속 (별도)

- **9D 통합** — `PrototypeAutomation`에 `cook_log` suite(`-CookLog` 인자) 합류.
- **9C** — 실제 cook/packaging 실행(`cook-smoke`/`dry-run`).
- `JsonReportWriter`에 `cook_details` 배열 정식화(참조쌍 구조화 필요 시).

---

## 범위 밖 (YAGNI)

- 실제 Cook/Packaging 실행(9C).
- standalone(비-커맨드렛) 파서.
- 로그 스트리밍/실시간.
