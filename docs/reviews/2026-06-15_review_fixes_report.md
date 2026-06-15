# Phase 9A 리뷰 지적 수정 결과 보고서 (2026-06-15_review_fixes_report)

본 보고서는 Phase 9A pre-release 검증 기능의 리뷰 지적 사항([2026-06-15_reviews_02.md](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/docs/reviews/2026-06-15_reviews_02.md), [2026-06-15_reviews_04.md](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/docs/reviews/2026-06-15_reviews_04.md))에 대한 수정 및 검증 결과를 기록합니다.

---

## 1. 결함 해결 및 검증 세부 사항

### 1.1 R1 (리포트 내 `command` 키 누락)
- **조치 사항:**
  - [JsonReportWriter.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Reports/JsonReportWriter.cpp#L15)에 `Root->SetStringField(TEXT("command"), Report.Commandlet);` 코드를 추가하여 기존 `commandlet` 필드와 함께 직렬화되도록 보강했습니다.
  - [ValidatePluginDependenciesCommandletTest.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/ValidatePluginDependenciesCommandletTest.cpp#L173-L179)의 `ReadsEnabledPlugins` 테스트 케이스 내에 리포트 로드 후 `command` 필드가 `"ValidatePluginDependencies"`인지를 보장하는 단언을 추가했습니다.
- **검증 결과:**
  - 자동화 테스트 및 스모크 테스트(`validate_plugin_deps.sh`) 통과.
  - 최신 생성된 JSON 리포트 대상 jq 검증 통과:
    ```bash
    jq -e '.command == "ValidatePluginDependencies"' \
       "$(ls -t sample/Saved/UECommandForge/Reports/ValidatePluginDependencies_*.json | head -1)"
    # 출력: true (정상)
    ```

### 1.2 R2 (엔진 Editor 전용 플러그인 오탐)
- **조치 사항:**
  - [ValidatePluginDependenciesCommandlet.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp#L103-L139)에 디스크의 `.uplugin` 파일 경로를 검색하는 `ResolvePluginDescriptor`와 runtime module을 가졌는지 검사하는 `DescriptorHasRuntimeModule` 헬퍼 함수를 추가했습니다.
  - Shipping configuration에서 forbidden 플러그인의 유효성을 검사할 때, 해당 헬퍼를 이용하여 descriptor를 찾아 runtime module이 없을 경우(editor-only) 위반 대상에서 제외(오탐 방지)하도록 구현했습니다.
  - 테스트 mockability를 위해 `-TestEnginePluginsDir` 파라미터를 파싱할 수 있게 수정했습니다.
  - [ValidatePluginDependenciesCommandletTest.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/ValidatePluginDependenciesCommandletTest.cpp#L301-L339)에 임시로 가짜 엔진 플러그인 디렉터리에 editor-only 모듈 속성을 지닌 `.uplugin`을 합성하고, Shipping 구성에서 해당 플러그인이 `forbiddenInShipping` 정책에 등록되더라도 오탐이 발생하지 않는지 검증하는 `FValidatePluginDepsEngineEditorOnlyNotForbiddenTest` 회귀 테스트를 작성했습니다.
- **검증 결과:**
  - 회귀 테스트 및 스모크 테스트 통과.

### 1.3 R3 (외부 프로젝트 스캔 시 실행 프로젝트 플러그인 누수)
- **조치 사항:**
  - 실행 중인 에디터의 마운트 목록을 반환하는 `IPluginManager::Get().GetDiscoveredPlugins()` 의존성을 전면 제거했습니다.
  - 플러그인 descriptor 해석은 오직 검증 정책에 기술된 플러그인 목록(`PolicyPlugins`)에 한정하여 lazy하게 디스크(`.uproject` 파일이 위치한 프로젝트의 `Plugins/` 및 `EnginePluginsDir`)를 직접 조회하는 방식으로 변경하여 실행(샘플) 프로젝트와 완전히 격리했습니다.
  - 에디터 모듈 경고 검사 루프의 경우, 에디터 전체 마운트 목록 대신 대상 프로젝트 폴더 하위의 `<ProjectDir>/Plugins/`를 재귀 탐색하여 발견된 플러그인들만 순회하도록 변경했습니다.
  - [ValidatePluginDependenciesCommandletTest.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/ValidatePluginDependenciesCommandletTest.cpp#L341-L374)에 임시 외부 프로젝트 ProjA를 조준하고 ProjA의 `.uproject`에 정의되지 않은 임의의 플러그인을 required 정책에 두어, 호스트 프로젝트의 로딩 여부와 무관하게 ProjA 스캔 시 정상적으로 `PLUGIN_REQUIRED_MISSING`을 검출하여 격리성이 보장됨을 단언하는 `FValidatePluginDepsExternalProjectIsolationTest` 회귀 테스트를 작성했습니다.
- **검증 결과:**
  - 회귀 테스트 통과 및 전체 자동화 테스트(72 PASS) 완벽 통과.

### 1.4 D1 (외부 프로젝트 config 미검증)
- **조치 사항:**
  - [DiffPlatformConfigCommandletTest.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/DiffPlatformConfigCommandletTest.cpp#L259-L333)에 임시 외부 프로젝트를 구성하고 해당 Config 디렉토리 내에 플랫폼별 설정 차이를 유발하도록 `.ini`를 저장한 뒤, `-Project`에 외부 프로젝트 경로를 주입하여 구동했을 때 외부 설정값이 정상 로드되어 차이(`CONFIG_PLATFORM_VALUE_DIFF`)가 감지되는지 검증하는 `FDiffPlatformConfigExternalProjectTest` 회귀 테스트를 작성했습니다.
  - `FConfigContext`에 주입되는 `ProjectRootDir` 및 `ProjectConfigDir` override 로직이 설정 로딩에 잘 반영됨을 확인했습니다.
- **검증 결과:**
  - 회귀 테스트 통과.
### 1.5 F7 (엔진 Unspecified 플러그인 활성 보정)
- **조치 사항:**
  - [ValidatePluginDependenciesCommandlet.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp#L194-L207)의 `LoadEnabledPlugins` 함수에서 descriptor 해석 대상 플러그인이 엔진 디렉터리(`EnginePluginsDir`) 하위에 존재하며 `EnabledByDefault` 속성이 `Unspecified` 인 경우, 에디터 기본 활성으로 취급하여 `bEnabled = true`로 보정하도록 수정하였습니다.
  - [ValidatePluginDependenciesCommandletTest.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/ValidatePluginDependenciesCommandletTest.cpp#L375-L421)에 엔진 디렉터리 경로 내에 `EnabledByDefault`가 지정되지 않은 가짜 엔진 플러그인을 합성하고, 이를 required 정책으로 두었을 때 오탐(Required Disabled) 없이 통과하는지 검증하는 `FValidatePluginDepsEngineUnspecifiedEnabledTest` 회귀 테스트를 추가하였습니다.
- **검증 결과:**
  - 회귀 테스트 및 스모크 테스트 통과.

### 1.6 F6 (Editor 모듈 경고 사각지대 해소)
- **조치 사항:**
  - [ValidatePluginDependenciesCommandlet.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/ValidatePluginDependenciesCommandlet.cpp#L373-L394)의 Editor 모듈 경고 검사 루프(`FoundUPlugins` 순회)에서, 인트리 플러그인이 `EnabledPlugins` 맵에 존재하지 않는 경우 디스크 descriptor의 `EnabledByDefault` 및 플랫폼 지원 조건(supportsTargetPlatform)을 직접 평가하여 활성화 상태를 해석하고 경고(혼재 감지)를 수행할 수 있도록 개선하였습니다.
  - [ValidatePluginDependenciesCommandletTest.cpp](file:///Users/kimkyungpyo/Workspaces/projects/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/ValidatePluginDependenciesCommandletTest.cpp#L423-L466)에 uproject나 정책에는 명시되지 않았으나 `EnabledByDefault`가 `true`이면서 런타임/에디터 모듈을 혼재한 인트리 플러그인을 생성하고, 경고 검사 시 정상적으로 `PLUGIN_EDITOR_MODULE_IN_RUNTIME_PLUGIN` warning이 발행되는지 검증하는 `FValidatePluginDepsInTreeEnabledByDefaultWarningTest` 회귀 테스트를 추가하였습니다.
- **검증 결과:**
  - 회귀 테스트 통과 및 warning 정상 감지 완료.

---

## 2. 결론 및 워크트리 위생

- **테스트 통계:**
  - **총 테스트 통과 수:** 73 PASS / 9 SucceededWithWarnings / 0 FAIL (총 82개 자동화 테스트 모두 성공)
  - **스모크 테스트:** `validate_plugin_deps.sh` (성공), `diff_platform_config.sh` (성공)
- **작업 트리 정리 & F5 검증:**
  - `git status` 확인을 통해 테스트 및 커맨드렛 구동 후에도 `sample/Config/` 하위 설정 파일의 오염(비의도적 trailing newline 등)이 전혀 발생하지 않음을 실증하여 읽기 전용 원칙 및 F5 요건을 완수하였습니다.
  - `AGENTS.md` 규칙에 근거하여, 금번 리뷰 지적 수정 범위(Task 1~3, Task 5~6)를 벗어나는 타 빌더 테스트 기인의 uasset 파일 외에는 무관한 변경 사항을 만들지 않았습니다.
- **최종 상태:**
  - 2026-06-15_reviews_05 보고서에서 요구한 F5, F6, F7 수정 사항을 완벽하게 반영하고 모든 검증 절차를 통과하였습니다. (조건부 PASS -> 완전 PASS 전환 완료)
