# 9A 설계 — 출시 전 사전점검 검증 계층 (Pre-release Validation Layer)

> 본 스펙은 `docs/9A_pre_release_validation_layer_design.md` 초안에 코드리뷰(H1–H2, M1–M4, L1–L5)를 반영한 확정본이다.
> 상위 로드맵: [ucf-master.md](../plans/ucf-master.md) Phase 9 · 참조: [references/10_packaging_cook_build_plan.md](../plans/references/10_packaging_cook_build_plan.md)
> 범위 기준: [harness_scope_and_priorities.md](../../implementation/harness_scope_and_priorities.md)

## 0. 목적

9A는 실제 Cook / Packaging / 로그 분석(9B·9C) 전에 실행되는 **읽기 전용 사전 검증 계층**이다.

- 출시 전 실패 가능성이 높은 설정/의존성 문제를 빠르게 탐지한다.
- CI / 로컬 / 에이전트 harness에서 동일하게 실행 가능하다.
- 결과를 `Result JSON`으로 남겨 후속 게이트(9D)에서 재사용한다.
- 파괴적 작업 없이 `.uproject`, `.uplugin` descriptor, config effective value만 검사한다.

---

## 1. 범위

신규 커맨드렛 2개를 추가한다.

| 커맨드렛 | 래퍼 | 역할 |
|---|---|---|
| `ValidatePluginDependenciesCommandlet` | `tools/ue/validate_plugin_deps.sh` / `.bat` | `.uproject` + `.uplugin`을 JSON 정책으로 검증 |
| `DiffPlatformConfigCommandlet` | `tools/ue/diff_platform_config.sh` / `.bat` | 플랫폼 간 config effective value 차이 탐지 |

각 커맨드렛은 작은 단위로 독립 실행, 입력 spec/출력 Result JSON/실패 조건 명확, 읽기 전용, 승인·rollback 불필요.

---

## 2. 공통 설계

### 2.1 실행 방식

각 커맨드렛은 Unreal Editor Commandlet으로 실행한다(기존 검증 커맨드렛과 동일 경로). 래퍼가 플랫폼별 실행 차이를 감춘다.

```bash
UnrealEditor-Cmd MyProject.uproject -run=ValidatePluginDependencies \
  -Project=MyProject.uproject -Policy=<plugin.policy.json> -Configuration=Shipping
UnrealEditor-Cmd MyProject.uproject -run=DiffPlatformConfig \
  -Project=MyProject.uproject -Platforms=Windows,Mac,Linux -Allowlist=<diff.allowlist.json>
```

> `-Configuration=Shipping`은 **정책 선택 플래그**일 뿐, 실제 Shipping 빌드를 수행하지 않는다(에디터는 Development로 동작). `forbiddenInShipping` 판정 분기에만 쓰인다. (L3)

### 2.2 리포트 위치 / 형식 (L1)

```txt
Saved/UECommandForge/Reports/<Command>_<YYYYMMDDThhmmssZ>.json
```

타임스탬프는 기존 컨벤션(UTC, basic ISO, `Z`)을 따른다. 예: `ValidatePluginDependencies_20260614T063000Z.json`. 추후 기존 Markdown 리포트 경로와 연동 가능하다.

### 2.3 공통 Result JSON / ValidationIssue

출력은 기존 `FCommandForgeReport`를 그대로 사용한다. `ValidationIssue` 필드: `severity`(error/warning/info), `code`, `message`, `field`, `filePath`, `suggestedFix`.

```json
{
  "ok": false,
  "command": "ValidatePluginDependencies",
  "commandlet": "ValidatePluginDependencies",
  "transaction_id": "",
  "dry_run": true,
  "applied": false,
  "rollback_available": false,
  "rollback_plan_path": "",
  "created_assets": [],
  "modified_assets": [],
  "changed_assets": [],
  "changed_files": [],
  "next_suggestions": [],
  "assets": [],
  "validation": {
    "enabled_plugin_count": "1",
    "configuration": "Shipping",
    "target_platform": "Mac",
    "issue_count": "2"
  },
  "post_validation": {},
  "issues": [
    {
      "severity": "error",
      "code": "PLUGIN_REQUIRED_MISSING",
      "message": "Required plugin is missing.",
      "field": "required[OnlineSubsystem]",
      "file_path": "MyProject.uproject",
      "suggested_fix": "Enable the plugin in the project or remove it from the required policy."
    }
  ],
  "errors": [],
  "steps": []
}
```

### 2.4 성공/실패 판정 + Exit Code (L2)

`ok`는 `Report.bOk`에 매핑되고, 프로세스 exit code는 기존 컨벤션을 상속한다:

```cpp
return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
```

CI 게이트는 이 exit code에 의존한다.

| 조건 | `ok` |
|---|---:|
| `error` 없음, allowlist 밖 config diff 없음 | `true` |
| required plugin 누락/비활성 | `false` |
| Shipping 금지 플러그인의 **런타임 모듈**이 대상 구성에서 활성 (M2) | `false` |
| allowlist 밖 config 차이/플랫폼 누락 존재 | `false` |
| warning만 존재 | Plugin 검사 기본 `true`, Config diff는 차이 존재 시 `false` |

### 2.5 정책/Allowlist 파일 위치 (M1)

- **운용(대상 프로젝트):** 대상 프로젝트 상대 경로 `<Project>/Config/UECommandForge/{plugin.policy.json, diff.allowlist.json}` 을 기본으로 한다(외부 프로젝트에 정책이 동반되도록).
- **레포 예시/fixture:** 본 레포의 예시·테스트 정책은 기존 컨벤션과 동일하게 `specs/policies/`에 둔다(`plugin.policy.example.json`, `diff.allowlist.example.json`).
- 경로는 인자로 명시되므로 두 위치 모두 동작한다.

---

## 3. `ValidatePluginDependenciesCommandlet`

### 3.1 목적 / 대상

프로젝트 플러그인 의존성을 출시 전 정책 기준으로 검증한다. 대상: `.uproject`, 엔진/프로젝트 플러그인의 활성 상태, `.uplugin` descriptor, 정책 `plugin.policy.json`.

### 3.2 입력

```txt
-Project=<path-to-uproject>
-Policy=<path-to-plugin.policy.json>
-Configuration=Development|Shipping
```

### 3.3 정책 JSON

```json
{
  "required": ["OnlineSubsystem", "Niagara"],
  "forbiddenInShipping": ["EditorScriptingUtilities", "PythonScriptPlugin"],
  "optional": ["GameplayInsights"],
  "allowedEditorOnly": ["BlueprintCompilerCppBackend"]
}
```

### 3.4 활성/플랫폼 판정 규칙 (M2, M3)

플러그인 "활성" 여부는 단순 `.uproject`의 `Enabled`만 보지 않는다:

- **EnabledByDefault:** `.uproject`에 미기재라도 `.uplugin`의 `EnabledByDefault: true`(+ 미비활성)면 활성으로 간주. (M3)
- **플랫폼 조건부:** `.uproject` 플러그인 항목의 `PlatformAllowList`/`PlatformDenyList`, `TargetAllowList`를 반영해 대상 구성/플랫폼에서의 실제 활성 여부를 계산. (M3)
- **Shipping 코드 포함 판정:** `forbiddenInShipping`은 "Enabled"가 아니라 **해당 플러그인이 `Runtime`/`Default` 타입 모듈을 가지며 대상 구성에서 활성**일 때만 위반으로 본다. Editor/DeveloperTool 전용 모듈은 Shipping에서 자동 제외되므로 위반이 아니다. (M2)

### 3.5 검증 규칙

| 규칙 | severity | ok 영향 |
|---|---:|---:|
| 필수 플러그인 누락 | `error` | `false` |
| 필수 플러그인 비활성 (대상 플랫폼 기준) | `error` | `false` |
| Shipping 금지 플러그인의 런타임 모듈이 대상 구성에서 활성 | `error` | `false` |
| Editor 전용 module type이 runtime 플러그인에 포함(의심) | `warning` | 기본 `true` |
| `.uplugin` Module Type 부정합(의심) | `warning` | 기본 `true` |

### 3.6 Issue Code

`PLUGIN_REQUIRED_MISSING`, `PLUGIN_REQUIRED_DISABLED`, `PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED`, `PLUGIN_EDITOR_MODULE_IN_RUNTIME_PLUGIN`, `PLUGIN_MODULE_TYPE_SUSPECT`, `PLUGIN_POLICY_INVALID`.

---

## 4. `DiffPlatformConfigCommandlet`

### 4.1 목적 / 대상

플랫폼별 config effective value를 비교해 의도하지 않은 차이를 탐지한다.
- 대상 플랫폼: Windows / Mac / Linux (인자로 지정)
- 비교 카테고리: **Engine, Game** (기본). Input은 Enhanced Input에서 매핑이 DataAsset로 이동했으므로 기본 제외, `-Categories`로 선택 추가. (L4)

### 4.2 입력

```txt
-Project=<path-to-uproject>
-Platforms=Windows,Mac,Linux
-Allowlist=<path-to-diff.allowlist.json>   (선택)
-Categories=Engine,Game                     (선택, 기본 Engine,Game)
```

### 4.3 비교 모델 — platform-aware effective value (H1)

ini 파일을 문자열 병합하지 않는다. **호스트 플랫폼만 로드하는 `FConfigCacheIni` 기본 경로는 사용하지 않는다.** 비호스트 플랫폼의 effective value를 얻기 위해 **`FConfigContext`**(`Engine/Source/Runtime/Core/Public/Misc/ConfigContext.h`)의 platform-aware 로드 경로를 사용한다(쿠커가 `-targetplatform` 처리에 쓰는 경로). 이를 통해 각 플랫폼의 `Base/Default/<Platform>/generated` 계층을 해석한 effective value를 얻는다.

비교 단위: `section.key`. 결과는 플랫폼별 값을 나란히 기록한다.

```json
{ "section": "/Script/Engine.RendererSettings", "key": "r.DefaultFeature.MotionBlur",
  "values": { "Windows": "False", "Mac": "True", "Linux": null }, "allowlisted": false }
```

> **선행 스파이크 필수 (H1):** 구현 0단계에서 "단일 호스트(이 경우 Mac)에서 `FConfigContext`가 비호스트 플랫폼의 전체 hierarchy를 정확히 해석하는가"를 검증한다. 해석 불가/부분만 가능하면 본 설계를 재검토한다(대안: SDK가 설치된 플랫폼만 비교, 또는 9B cook과 병합).

### 4.4 Allowlist (H2, L5)

- 형식: `[{ "section": <glob>, "key": <glob> }]`. 매칭은 **대소문자 무시 fnmatch 스타일 glob**(`*`, `?`). (L5)
- allowlist에 든 차이는 정상 차이로 간주, 게이트 실패에서 제외하고 `suppressedDiffCount`에 집계(남용 방지 가시화).
- **기본 allowlist 동봉 (H2):** 알려진 정상 플랫폼 차이를 담은 starter allowlist를 `specs/policies/diff.allowlist.example.json`로 제공한다. 최소 포함: `r.*`(렌더), `ShaderFormat*`, `TextureFormat*`, RHI/그래픽 API 계열, 플랫폼 패키징 키. 사용자는 여기서 출발해 좁힌다.

```json
[
  { "section": "/Script/Engine.RendererSettings", "key": "r.*" },
  { "section": "/Script/Engine.RendererSettings", "key": "*ShaderFormat*" },
  { "section": "*", "key": "*TextureFormat*" }
]
```

### 4.5 판정 / Issue Code

| 조건 | severity | ok 영향 |
|---|---:|---:|
| 플랫폼 간 값 차이, allowlist 밖 | `warning` | `false` |
| 특정 플랫폼 key 누락, allowlist 밖 | `warning` | `false` |
| allowlist 처리된 차이 | suppressed | `true` |
| allowlist/플랫폼/카테고리 입력 오류 | `error` | `false` |

Code: `CONFIG_PLATFORM_VALUE_DIFF`, `CONFIG_PLATFORM_KEY_MISSING`, `CONFIG_ALLOWLIST_INVALID`, `CONFIG_PLATFORM_INVALID`, `CONFIG_CATEGORY_LOAD_FAILED`.

출력은 `issues`(게이트용)와 `diffs`(상세)를 분리하고, summary에 `platforms`, `categories`, `diffCount`, `suppressedDiffCount`, `missingPlatformCount`를 남긴다.

---

## 5. README 명령 표면

| Command | Wrapper | Description | Output |
|---|---|---|---|
| `ValidatePluginDependencies` | `tools/ue/validate_plugin_deps.sh/.bat` | Validate `.uproject`/`.uplugin` dependency policy | `Saved/UECommandForge/Reports/*.json` |
| `DiffPlatformConfig` | `tools/ue/diff_platform_config.sh/.bat` | Compare platform config effective values | `Saved/UECommandForge/Reports/*.json` |

---

## 6. 9D 통합 방향

초기에는 독립 실행. 추후 9D에서 `validate_project_rules` 통합 게이트에 플래그로 합류한다(예: `-CheckPluginDeps -CheckPlatformConfigDiff -Configuration=Shipping -Platforms=...`). 통합 후에도 각 커맨드렛은 독립 실행 가능해야 한다.

---

## 7. 테스트 설계

### 7.1 원칙
AAA 패턴, 정상/위반 분리, `severity`·`code`·`field`·`filePath` 검증, 래퍼 smoke test, 리포트 파일 생성 검증.

### 7.2 Fixture 전략 (M4)
무거운 fixture `.uproject`/플러그인을 커밋하지 않는다. 대신 **테스트 시점에 최소 descriptor(.uproject/.uplugin/Config ini)를 temp 디렉터리에 합성**한다(기존 검증 테스트와 동일 방식). 합성 헬퍼를 `Tests/` 하위에 둔다.

| 케이스 (Plugin) | 기대 |
|---|---|
| Valid | `ok:true`, issue 없음 |
| MissingRequired | `PLUGIN_REQUIRED_MISSING`, `ok:false` |
| DisabledRequired | `PLUGIN_REQUIRED_DISABLED`, `ok:false` |
| ForbiddenShippingRuntimeModule | `PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED`, `ok:false` |
| ForbiddenShippingEditorOnlyModule | issue 없음(자동 제외, false positive 방지) |
| EnabledByDefaultRequired | `ok:true` (M3 경로) |
| EditorModuleInRuntime | `PLUGIN_EDITOR_MODULE_IN_RUNTIME_PLUGIN`, warning |

| 케이스 (Config) | 기대 |
|---|---|
| NoDiff | `ok:true`, diff 없음 |
| AllowedDiff | `ok:true`, `suppressedDiffCount > 0` |
| UnexpectedDiff | `CONFIG_PLATFORM_VALUE_DIFF`, `ok:false` |
| MissingPlatformKey | `CONFIG_PLATFORM_KEY_MISSING`, `ok:false` |

### 7.3 Smoke Test
래퍼 실행 → `ok == expected`, `issues[].code ⊇ expected`, 리포트 파일 존재 단언.

---

## 8. 범위 밖 (YAGNI)

Dev/Shipping configuration diff(후순위), Collision/GameplayTag 의미 검사(별도 룰 계층), Cook 로그 분석(9B), 실제 패키징(9C), YAML 정책(JSON만).

---

## 9. 구현 우선순위

0. **스파이크 (H1):** 단일 호스트에서 `FConfigContext` 비호스트 플랫폼 effective value 로드 검증 → 가능 여부에 따라 §4 확정/재검토.
1. 공통 `FCommandForgeReport`/`ValidationIssue` 재사용 확인 + 신규 summary 필드.
2. `ValidatePluginDependenciesCommandlet` + plugin policy parser (M2/M3 활성·플랫폼 판정 포함).
3. Plugin fixture(합성) + 자동화 테스트.
4. `DiffPlatformConfigCommandlet` + diff allowlist parser (glob, H2 기본 allowlist).
5. Config fixture(합성) + 자동화 테스트.
6. shell/bat 래퍼 추가.
7. README command surface 반영.
8. 기존 Markdown 리포트 경로 연동(선택).

---

## 10. 검토 반영 요약

- **H1** §4.3 `FConfigContext` platform-aware 로드로 수정 + 0단계 스파이크.
- **H2** 기본 allowlist(`diff.allowlist.example.json`) 동봉으로 out-of-box 노이즈 차단.
- **M1** 운용은 `<Project>/Config/UECommandForge/`, 레포 예시는 `specs/policies/`.
- **M2** `forbiddenInShipping`을 "런타임 모듈 + 대상 구성 활성"으로 정밀화(Editor 모듈 오탐 방지).
- **M3** required 활성 판정에 `EnabledByDefault`·플랫폼 조건 반영.
- **M4** fixture를 temp 합성으로(무거운 프로젝트 커밋 지양).
- **L1** 타임스탬프 `YYYYMMDDThhmmssZ`. **L2** exit code 상속 명시. **L3** `-Configuration` 정책 플래그 명확화. **L4** Input 기본 제외. **L5** allowlist glob 의미 정의.
