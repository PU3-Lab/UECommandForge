# Phase 9D 통합 게이트 구현 계획 (2026-06-15-phase9d-integration-gate)

> **For agentic workers (Antigravity):** 본 계획은 9A의 두 검증 커맨드렛을 기존 통합 게이트 `PrototypeAutomation`(`validate_project_rules` 래퍼)에 suite로 합류시킨다. Task 단위 TDD("실패 테스트 → 구현 → 통과 → 커밋"). 코드는 Antigravity에서 작성한다.

**목표:** 출시 전 한 번의 호출로 자산/빌드/데이터/Config 규칙 + **플러그인 의존성 + 플랫폼 Config diff**를 함께 검증하고, child별 JSON + 사람이 읽는 Markdown 요약을 산출한다.

**기준:** 스펙 [2026-06-14-phase9a-pre-release-validation-design.md](../specs/2026-06-14-phase9a-pre-release-validation-design.md) §6(9D 통합 방향), 기존 패턴 `PrototypeAutomationCommandlet`(§Phase 8). 선행: [2026-06-15_reviews_07.md](../../reviews/2026-06-15_reviews_07.md)에서 9A 코드 완료 확인.

**Tech Stack:** UE 5.7 C++20. 신규 의존성 없음(세 커맨드렛 모두 동일 `UECommandForgeEditor` 모듈).

---

## 핵심 설계

기존 `PrototypeAutomationCommandlet`은 **인자 존재로 suite를 게이팅**하고 `RunSuite(CommandletName, Args)`로 child `Main(Args)`을 호출한 뒤 child 리포트를 병합한다(`cpp:160-271`). 9D는 같은 패턴에 suite 2개를 추가한다:

| Suite Name | 게이트 인자 | Child Commandlet | 전달 인자 |
|---|---|---|---|
| `plugin_deps` | `-PluginPolicy=<path>` | `ValidatePluginDependencies` | `Policy`, `Project`, `Configuration`, `TargetPlatform`, `Output` |
| `platform_config_diff` | `-DiffPlatforms=<csv>` | `DiffPlatformConfig` | `Project`, `Platforms`, `Allowlist`(opt: `-DiffAllowlist`), `Categories`(opt: `-DiffCategories`), `Output` |

> 게이트 인자가 없으면 해당 suite는 비활성(기존 suite와 동일하게 opt-in). 두 suite 모두 미지정이면 9D 동작은 기존과 동일.

---

## File Structure

| 파일 | 변경 |
|---|---|
| `EDITOR/Private/Commandlets/PrototypeAutomationCommandlet.cpp` | include 2개, suite 2개 추가, `RunSuite` 디스패치 2개 추가 |
| `EDITOR/Tests/PrototypeAutomationCommandletTest.cpp` | 통합 suite 회귀 테스트 |
| `tools/ue/validate_project_rules.sh` / `.bat` | 신규 인자 패스스루 + 사용법 |
| `tools/test/smoke/prototype_automation.sh` | plugin_deps/platform_config_diff suite 스모크 |
| `README.md` | 통합 게이트 인자 표 갱신 |
| `docs/superpowers/specs/2026-06-14-...-design.md` | §6 9D "구현 완료"로 보강 |

`EDITOR = sample/Plugins/UECommandForge/Source/UECommandForgeEditor`.

---

## Task 1: RunSuite 디스패치 + include 확장

- [ ] **Step 1: 실패 테스트** — `PrototypeAutomationCommandletTest.cpp`에 "`-PluginPolicy` 제공 시 통합 리포트에 `plugin_deps` suite step이 존재" 단언 추가(현재 RED).
- [ ] **Step 2: 테스트 실패 확인** — `tools/test/automation/run.sh`
- [ ] **Step 3: 구현** — `PrototypeAutomationCommandlet.cpp`:
  - include 추가:
    ```cpp
    #include "Commandlets/ValidatePluginDependenciesCommandlet.h"
    #include "Commandlets/DiffPlatformConfigCommandlet.h"
    ```
  - `RunSuite`(현 `cpp:160-180`)에 분기 추가:
    ```cpp
    if (CommandletName == TEXT("ValidatePluginDependencies"))
    {
        return NewObject<UValidatePluginDependenciesCommandlet>()->Main(Args);
    }
    if (CommandletName == TEXT("DiffPlatformConfig"))
    {
        return NewObject<UDiffPlatformConfigCommandlet>()->Main(Args);
    }
    ```
- [ ] **Step 4: 통과 확인** — Step 1 테스트 GREEN까지는 Task 2 suite 추가 필요(여기선 컴파일/디스패치만 확인).

---

## Task 2: plugin_deps suite 추가

**근거:** 기존 suite 패턴(`cpp:212-223`) 그대로.

- [ ] **Step 1: 실패 테스트** — 합성 `.uproject`+정책으로 `-PluginPolicy` 지정 시 통합 리포트(JSON)에 `plugin_deps` suite의 ok/issues가 병합됨을 단언.
- [ ] **Step 2: 구현** — `Main`의 suite 구성부(`cpp:259-270` 뒤)에 추가:
    ```cpp
    const FString PluginPolicyPath = ParamsMap.FindRef(TEXT("PluginPolicy"));
    if (!PluginPolicyPath.IsEmpty())
    {
        FSuiteRun Suite;
        Suite.Name = TEXT("plugin_deps");
        Suite.Commandlet = TEXT("ValidatePluginDependencies");
        Suite.ReportPath = ChildReportPath(OutPath, Suite.Name);
        Suite.Args = QuoteArg(TEXT("Policy"), PluginPolicyPath) +
            QuoteArg(TEXT("Project"), ParamsMap.FindRef(TEXT("Project"))) +
            QuoteArg(TEXT("Configuration"), ParamsMap.FindRef(TEXT("Configuration"))) +
            QuoteArg(TEXT("TargetPlatform"), ParamsMap.FindRef(TEXT("TargetPlatform"))) +
            QuoteArg(TEXT("Output"), Suite.ReportPath);
        Suites.Add(Suite);
    }
    ```
  > `Configuration`/`TargetPlatform`가 비면 child가 기본값(Development/호스트 플랫폼) 처리하므로 빈 QuoteArg 허용 여부를 기존 `QuoteArg` 구현으로 확인(빈 값은 인자 생략이 안전).
- [ ] **Step 3: 통과 확인** — `tools/ue/build_plugin.sh && tools/test/automation/run.sh`
- [ ] **Step 4: 커밋** — `feat: add plugin_deps suite to integration gate (9D)`

---

## Task 3: platform_config_diff suite 추가

- [ ] **Step 1: 실패 테스트** — 합성 외부 프로젝트 config로 `-DiffPlatforms=Windows,Mac` 지정 시 통합 리포트에 `platform_config_diff` suite가 병합되고, allowlist 밖 차이에서 통합 `ok=false`가 됨을 단언.
- [ ] **Step 2: 구현** — suite 추가:
    ```cpp
    const FString DiffPlatforms = ParamsMap.FindRef(TEXT("DiffPlatforms"));
    if (!DiffPlatforms.IsEmpty())
    {
        FSuiteRun Suite;
        Suite.Name = TEXT("platform_config_diff");
        Suite.Commandlet = TEXT("DiffPlatformConfig");
        Suite.ReportPath = ChildReportPath(OutPath, Suite.Name);
        Suite.Args = QuoteArg(TEXT("Project"), ParamsMap.FindRef(TEXT("Project"))) +
            QuoteArg(TEXT("Platforms"), DiffPlatforms) +
            QuoteArg(TEXT("Allowlist"), ParamsMap.FindRef(TEXT("DiffAllowlist"))) +
            QuoteArg(TEXT("Categories"), ParamsMap.FindRef(TEXT("DiffCategories"))) +
            QuoteArg(TEXT("Output"), Suite.ReportPath);
        Suites.Add(Suite);
    }
    ```
- [ ] **Step 3: 통과 확인** — 빌드 + 자동화.
- [ ] **Step 4: 커밋** — `feat: add platform_config_diff suite to integration gate (9D)`

> **통합 ok 집계 확인:** 기존 `PopulateSuiteFromChildReport`/집계 로직(`cpp:92-130`)이 child의 `ok=false`를 통합 `Report.bOk=false`로 전파하는지 재확인. 두 신규 suite도 동일 경로를 타야 함(별도 처리 불필요해야 정상).

---

## Task 4: 래퍼 · 스모크 · README · 스펙

- [ ] **Step 1: 래퍼 패스스루** — `tools/ue/validate_project_rules.sh`/`.bat`에 `-PluginPolicy`, `-DiffPlatforms`, `-DiffAllowlist`, `-DiffCategories`, `-Configuration`, `-TargetPlatform` 전달 및 사용법 추가(기존 인자 전달 방식 그대로).
- [ ] **Step 2: 스모크** — `tools/test/smoke/prototype_automation.sh`에 두 suite를 포함한 1회 호출 추가:
  ```bash
  tools/ue/validate_project_rules.sh \
    -Project="$(pwd)/sample/UECommandForgeSample.uproject" \
    -PluginPolicy=specs/policies/plugin.policy.example.json -Configuration=Shipping \
    -DiffPlatforms=Windows,Mac -DiffAllowlist=specs/policies/diff.allowlist.example.json
  jq -e '.command == "PrototypeAutomation" and (.steps | map(.step) | index("plugin_deps")) != null' \
     "$(ls -t sample/Saved/UECommandForge/Reports/PrototypeAutomation_*.json | head -1)"
  ```
- [ ] **Step 3: README** — 통합 게이트 인자 표에 `-PluginPolicy`, `-DiffPlatforms` 등 등재.
- [ ] **Step 4: 스펙 §6 갱신** — "9D 통합 완료(suite: plugin_deps, platform_config_diff)"로 보강. 각 커맨드렛이 독립 실행도 유지됨을 명시(스펙 §6 요구).
- [ ] **Step 5: 커밋** — `feat: wrapper/smoke/readme for 9D integration gate`

---

## Task 5: 통합 검증

- [ ] **Step 1: 전체 그린** — `tools/ue/build_plugin.sh && tools/test/automation/run.sh` (FAIL 0).
- [ ] **Step 2: 통합 스모크** — `UE_COMMANDLET_TIMEOUT=120 tools/test/smoke/prototype_automation.sh` 그린.
- [ ] **Step 3: 작업 트리 위생** — 커밋 전 `git diff`로 무관한 `sample/Config`/`.uasset` 변경 선별.
- [ ] **Step 4: 리뷰 문서** — `docs/reviews/`에 9D 통합 검토 보고서 작성(규칙: 리뷰 시 문서화).

---

## 인수 조건 (Acceptance)

```bash
tools/ue/build_plugin.sh && tools/test/automation/run.sh        # FAIL 0
UE_COMMANDLET_TIMEOUT=120 tools/test/smoke/prototype_automation.sh
# 통합 리포트에 두 suite가 병합되고, 위반 시 통합 ok=false
jq -e '(.steps | map(.step)) as $s | ($s|index("plugin_deps"))!=null and ($s|index("platform_config_diff"))!=null' \
   "$(ls -t sample/Saved/UECommandForge/Reports/PrototypeAutomation_*.json | head -1)"
```

- 두 suite가 통합 게이트에 opt-in으로 합류.
- child `ok=false` → 통합 `ok=false` 전파.
- 각 커맨드렛 **독립 실행 유지**(스펙 §6).
- Markdown 요약에 두 suite 행 표시.

---

## 범위 밖 (YAGNI)

- 9B(Cook 로그 분석)·9C(패키징) — 후속 Phase.
- 통합 게이트의 병렬 실행/캐싱.
- plugin policy/diff allowlist 자동 탐색(현행 명시 인자 유지).
