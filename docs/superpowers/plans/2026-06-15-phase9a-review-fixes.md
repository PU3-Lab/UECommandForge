# Phase 9A 리뷰 지적 수정 구현 계획 (2026-06-15-phase9a-review-fixes)

> **For agentic workers (Antigravity):** 본 계획은 리뷰 보고서에서 도출된 결함을 Task 단위 TDD로 수정한다. 각 Task는 "실패 테스트 → 구현 → 통과 → 커밋" 순서를 지킨다. 코드는 Antigravity에서 작성한다.

**대상 결함 출처:**
- [2026-06-15_reviews_02.md](../../reviews/2026-06-15_reviews_02.md) — ValidatePluginDependencies: R1(command 키), R2(forbidden 오탐), R3(외부 프로젝트 스캔)
- [2026-06-15_reviews_04.md](../../reviews/2026-06-15_reviews_04.md) — DiffPlatformConfig: R1/D1(외부 프로젝트 config 미검증)

**기준 스펙:** [2026-06-14-phase9a-pre-release-validation-design.md](../specs/2026-06-14-phase9a-pre-release-validation-design.md) §2.3, §3.4–3.5, §4.3

**Tech Stack:** UE 5.7 C++20, `UnrealEd`, `Json`, `Projects`(기존 의존성), `Misc/ConfigContext.h`. 신규 의존성 없음.

---

## 핵심 아키텍처 결정 (R2 + R3 통합)

**문제:** 현재 `ValidatePluginDependencies`는 `IPluginManager::Get().GetDiscoveredPlugins()`(실행 중 에디터의 마운트 상태)로 플러그인 집합을 얻는다. 검증 대상은 외부 `-Project`인데, 이 API는 **실행 중(샘플) 프로젝트**를 반영하므로:
- 외부 프로젝트의 플러그인이 아닌 실행 프로젝트의 것을 스캔(R3),
- forbidden 판정의 `PluginHasRuntimeModule`은 대상 프로젝트 `Plugins/`만 file-search → 엔진 플러그인 미발견 시 보수적 `true` → 엔진 Editor 전용 플러그인 오탐(R2).

**결정:** **대상 `-Project` 기준 디스크 descriptor 해석으로 일원화한다. `IPluginManager`를 제거한다.**
- 플러그인 descriptor는 (1) `<ProjectDir>/Plugins/**`, (2) `FPaths::EnginePluginsDir()/**` 에서 이름으로 해석.
- `EnabledByDefault`·플랫폼/타깃 조건은 디스크 `FPluginDescriptor`/`FPluginReferenceDescriptor`에서 직접 평가(현재도 `FPluginReferenceDescriptor::IsEnabledFor*` 사용 중 — 유지).
- 이로써 R2(엔진 descriptor 발견 → editor-only 정확 제외)와 R3(실행 프로젝트 누수 제거)를 동시에 해소.

> 트레이드오프: 엔진 전체 plugin 스캔 비용. 단, required/forbidden은 **정책에 명시된 이름만** lazy 해석하면 되므로 전수 스캔 불필요. editor-module warning 루프만 대상 프로젝트 `Plugins/` 전수(소규모).

---

## File Structure

| 파일 | 변경 |
|---|---|
| `EDITOR/Private/Reports/JsonReportWriter.cpp` | `command` 키 추가 (R1) |
| `EDITOR/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp` | descriptor 해석 헬퍼, IPluginManager 제거 (R2/R3) |
| `EDITOR/Tests/ValidatePluginDependenciesCommandletTest.cpp` | 엔진경로/외부프로젝트 회귀 테스트 (R2/R3) |
| `EDITOR/Tests/DiffPlatformConfigCommandletTest.cpp` | 외부 프로젝트 config 회귀 테스트 (D1) |
| `docs/superpowers/specs/2026-06-14-phase9a-pre-release-validation-design.md` | §2.3 스키마를 실제 출력과 일치(보강) |

`EDITOR = sample/Plugins/UECommandForge/Source/UECommandForgeEditor` 로 줄여 쓴다.

---

## Task 1: R1 — 리포트에 `command` 키 추가

**근거:** 스펙 §2.3 예시 JSON은 `command`를 쓰지만 직렬화는 `commandlet`만 출력(`JsonReportWriter.cpp:15`). plan 스모크 `.command` jq가 항상 실패.

**결정:** `commandlet`은 하위호환으로 유지하고 `command`를 **별칭으로 추가**(additive, 모든 커맨드렛 공통). 값은 동일하게 `Report.Commandlet`.

- [ ] **Step 1: 실패 테스트** — `EDITOR/Tests/`의 ValidatePluginDeps 테스트 1건에 `Report->TryGetStringField(TEXT("command"), ...) == "ValidatePluginDependencies"` 단언 추가. (현재 RED)
- [ ] **Step 2: 테스트 실패 확인** — `tools/test/automation/run.sh`
- [ ] **Step 3: 구현** — `JsonReportWriter.cpp` line 15 다음에 한 줄 추가:
  ```cpp
  Root->SetStringField(TEXT("command"), Report.Commandlet);
  ```
- [ ] **Step 4: 통과 확인** — `tools/ue/build_plugin.sh && tools/test/automation/run.sh`
- [ ] **Step 5: 스펙 동기화** — 스펙 §2.3 예시에 `command`/`commandlet` 병기 및 실제 snake_case 필드(`file_path`, `suggested_fix`, `validation{...}`)를 사실대로 기술. plan 스모크의 `.command` jq가 이제 유효함을 확인.
- [ ] **Step 6: 커밋** — `feat: add command key to report (review _02 R1)`

---

## Task 2: R2+R3 — descriptor 기반 해석으로 일원화

**Files:** `EDITOR/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp`, 테스트.

### 2.1 헬퍼 도입

- [ ] **Step 1: descriptor 해석 헬퍼** — `Private` 네임스페이스에 추가:
  ```cpp
  // 대상 프로젝트 Plugins → 엔진 Plugins 순으로 <Name>.uplugin 을 찾아 로드.
  bool ResolvePluginDescriptor(const FString& ProjectPath, const FString& PluginName,
      FPluginDescriptor& OutDesc, FString& OutDescPath)
  {
      const TArray<FString> SearchDirs = {
          FPaths::Combine(FPaths::GetPath(ProjectPath), TEXT("Plugins")),
          FPaths::EnginePluginsDir()
      };
      for (const FString& Dir : SearchDirs)
      {
          TArray<FString> Found;
          IFileManager::Get().FindFilesRecursive(Found, *Dir,
              *(PluginName + TEXT(".uplugin")), true, false);
          if (Found.Num() > 0)
          {
              FText FailReason;
              if (OutDesc.Load(Found[0], FailReason)) { OutDescPath = Found[0]; return true; }
          }
      }
      return false;
  }

  bool DescriptorHasRuntimeModule(const FPluginDescriptor& Desc)
  {
      for (const FModuleDescriptor& M : Desc.Modules)
      {
          switch (M.Type)
          {
              case EHostType::Runtime: case EHostType::RuntimeNoCommandlet:
              case EHostType::RuntimeAndProgram: case EHostType::ClientOnly:
              case EHostType::ServerOnly: case EHostType::ClientOnlyNoCommandlet:
              case EHostType::CookedOnly: return true;
              default: break;
          }
      }
      return false;
  }
  ```

### 2.2 forbidden 판정 교체 (R2)

- [ ] **Step 2: 실패 테스트 — 엔진 Editor 전용 플러그인 오탐 방지** — 합성 fixture를 "엔진 경로 모사" 디렉터리에 두기 어렵다. 대신 헬퍼를 **검색 디렉터리 주입형**으로 테스트하거나, 임시 디렉터리를 두 번째 SearchDir로 받는 오버로드를 두고 그 안에 editor-only `.uplugin`을 합성한다. 기대: forbidden+Shipping에서 `PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED` **미발생**.
- [ ] **Step 3: 구현** — forbidden 루프(`cpp:325-336`)를 교체:
  ```cpp
  for (const FString& Name : Policy.ForbiddenInShipping)
  {
      const bool* Enabled = EnabledPlugins.Find(Name);
      if (Enabled == nullptr || !*Enabled) { continue; }
      FPluginDescriptor Desc; FString DescPath;
      if (Private::ResolvePluginDescriptor(ProjectPath, Name, Desc, DescPath))
      {
          if (!Private::DescriptorHasRuntimeModule(Desc)) { continue; } // editor-only → 제외(오탐 방지)
      }
      // 못 찾으면 보수적으로 위반 처리(누락<오탐 방지) — 단 사유 메시지에 'descriptor 미해석' 명시
      Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED"), ...);
  }
  ```
  → 기존 `PluginHasRuntimeModule`(프로젝트 디렉터리 한정) 제거.

### 2.3 IPluginManager 제거 (R3)

- [ ] **Step 4: 실패 테스트 — 외부 프로젝트 enabled 집합 격리** — 임시 외부 프로젝트(`.uproject`만, 실행 프로젝트와 다른 디렉터리)에 plugin A를 enabled로 두고, 실행(샘플) 프로젝트에만 있는 plugin B는 enabled 집합에 **포함되면 안 됨**을 단언.
- [ ] **Step 5: 구현** — `LoadEnabledPlugins`(`cpp:104-174`)와 editor-warning 루프(`cpp:284-396`)에서 `IPluginManager::Get().GetDiscoveredPlugins()` 의존 제거:
  - `.uproject`에 명시된 플러그인: `FPluginReferenceDescriptor::IsEnabledFor*`로 평가(현행 유지).
  - `.uproject` 미명시 + `EnabledByDefault` 평가가 필요한 경우: `ResolvePluginDescriptor`로 디스크 descriptor 로드 후 `Desc.EnabledByDefault == EPluginEnabledByDefault::Enabled && Desc.SupportsTargetPlatform(TargetPlatform)`.
  - editor-module warning 루프: 대상 프로젝트 `<ProjectDir>/Plugins/**` 의 `.uplugin`만 순회(실행 에디터 마운트 목록 대신).
  - `#include "Interfaces/IPluginManager.h"` 제거.
- [ ] **Step 6: 통과 확인** — `tools/ue/build_plugin.sh && tools/test/automation/run.sh` (기존 6 테스트 + 신규 2 PASS)
- [ ] **Step 7: 스모크 보강** — `tools/test/smoke/validate_plugin_deps.sh`에 "엔진 editor-only 플러그인을 forbidden+Shipping으로 줘도 `.ok == true`" 케이스 추가.
- [ ] **Step 8: 커밋** — `fix: descriptor-based plugin resolution, drop IPluginManager (review _02 R2/R3)`

---

## Task 3: D1 — DiffPlatformConfig 외부 프로젝트 회귀 테스트

**Files:** `EDITOR/Tests/DiffPlatformConfigCommandletTest.cpp` (+ 필요 시 `DiffPlatformConfigCommandlet.cpp` 보정).

**근거:** D1 수정(`ProjectRootDir/ProjectConfigDir` 주입, `cpp:218-224`)이 코드엔 있으나 모든 테스트가 `-Project=GetProjectFilePath()`(실행 프로젝트)만 사용해 외부 조준이 미검증([_04](../../reviews/2026-06-15_reviews_04.md) R1).

- [ ] **Step 0: 스파이크 — override 반영 확인** — 임시 외부 프로젝트 디렉터리에 `Config/DefaultUECommandForgeTest.ini`(고유 키 `ExternalKey=ExternalValue`)를 만들고, `FConfigContext::ReadIntoLocalFile` 이후 `ProjectConfigDir` override가 실제 로드에 반영되는지 1회 확인. **미반영 시** `DiffPlatformConfigCommandlet.cpp`의 주입을 `FConfigContext` 생성 인자/올바른 멤버로 교정(예: 로드 전 `EngineConfigDir`/플랫폼 확장 경로 포함).
- [ ] **Step 1: 실패 테스트 — 외부 프로젝트 조준** — 새 fixture:
  ```cpp
  // TempDir()/ExtProj/ 에 T.uproject + Config/DefaultUECommandForgeTest.ini
  //   [UECommandForgeTestSection] ExternalOnlyKey=Win  (Windows)
  //   Mac 쪽엔 다른 값 → 외부 프로젝트에서만 존재하는 diff
  // -Project=TempDir()/ExtProj/T.uproject 로 실행
  // 기대: diff_count>0 (외부 프로젝트 config가 실제로 로드됨)
  // 음성 대조: 실행(샘플) 프로젝트엔 ExternalOnlyKey 부재 → 실행 프로젝트를 읽었다면 diff 0 (FAIL 되어야 함)
  ```
- [ ] **Step 2: 테스트 실패 확인** — 현재 override가 미반영이면 RED.
- [ ] **Step 3: 구현/보정** — Step 0 결과에 따라 주입 로직 교정(필요 시).
- [ ] **Step 4: 통과 확인** — `tools/test/automation/run.sh`
- [ ] **Step 5: 커밋** — `test: external-project config targeting regression (review _04 D1)`

---

## Task 4: 통합 검증 및 문서 갱신

- [ ] **Step 1: 전체 그린** — `tools/ue/build_plugin.sh && tools/test/automation/run.sh` PASS/FAIL/SKIP 기록.
- [ ] **Step 2: 스모크 2종** — `tools/test/smoke/validate_plugin_deps.sh`, `tools/test/smoke/diff_platform_config.sh` 그린 + 리포트 파일 생성 확인.
- [ ] **Step 3: 작업 트리 위생** ([_04](../../reviews/2026-06-15_reviews_04.md) R5) — 커밋 전 `git diff`로 `sample/Config/*Engine.ini`·`.uasset` 무관 변경 선별, 의도치 않으면 `git checkout`.
- [ ] **Step 4: 리뷰 후속 문서** — `docs/reviews/`에 본 수정에 대한 재재리뷰 보고서 작성(규칙: 리뷰 시 문서화). _02 R1–R3, _04 D1 해소 여부를 코드 근거로 판정.

---

## 인수 조건 (Acceptance)

```bash
tools/ue/build_plugin.sh && tools/test/automation/run.sh        # FAIL 0
tools/test/smoke/validate_plugin_deps.sh                        # ok 케이스 그린
tools/test/smoke/diff_platform_config.sh                        # 그린
# 리포트에 command 키 존재
jq -e '.command == "ValidatePluginDependencies"' \
   "$(ls -t sample/Saved/UECommandForge/Reports/ValidatePluginDependencies_*.json | head -1)"
```

- _02 R1: 리포트 `.command` 존재·jq 통과.
- _02 R2: 엔진 Editor 전용 플러그인이 forbidden+Shipping에서 미발생(회귀 테스트 그린).
- _02 R3: 외부 프로젝트 enabled 집합에 실행 프로젝트 플러그인 누수 없음(회귀 테스트 그린).
- _04 D1: 외부 프로젝트 config가 실제 로드되어 diff 감지(회귀 테스트 그린).

---

## 범위 밖 (YAGNI)

- 스펙 §2.3의 `summary{issueCount,...}`/`timestamp`/`project` 전면 재설계(별도, 현행 `validation` 맵 유지).
- DiffPlatformConfig 정렬-비교의 "순서 무시" 정책 변경([_04](../../reviews/2026-06-15_reviews_04.md) R3, 현행 의도 유지).
- 9D 통합 게이트 합류.
