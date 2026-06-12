# UECommandForge Windows 설치 후속 검증 기록

작성일: 2026-06-01

## 목적

`install-uecommandforge.bat`로 Windows 프로젝트 설치가 완료된 뒤, 실제 사용자 환경에서 플러그인과 Codex 도구가 계속 사용할 수 있는지 확인한다.

이번 검증은 `sh` 직접 실행보다 Windows `.bat` wrapper와 설치된 사용자 경로를 우선 사용한다.

## 대상 환경

- 저장소: `C:\Workspaces\projects\UECommandForge`
- 설치 대상 프로젝트: `C:\Workspaces\projects\factory-space\frontend\Wanted_Factory.uproject`
- 프로젝트 플러그인 경로: `C:\Workspaces\projects\factory-space\frontend\Plugins\UECommandForge`
- Codex 도구 설치 경로: `C:\Users\Sam\.codex\UECommandForge`
- Unreal Engine: `C:\Program Files\Epic Games\UE_5.7`

## 현재까지 확인된 항목

| 항목 | 결과 | 근거 |
| --- | --- | --- |
| 프로젝트 플러그인 설치 | PASS | `Plugins\UECommandForge\UECommandForge.uplugin` 존재 |
| Editor/Runtime DLL 설치 | PASS | `Binaries\Win64\UnrealEditor-UECommandForgeEditor.dll`, `UnrealEditor-UECommandForgeRuntime.dll` 존재 |
| Codex 도구 설치 | PASS | `C:\Users\Sam\.codex\UECommandForge\uecommandforge-installed.json` 존재 |
| 설치된 `.bat` wrapper 목록 | PASS | 저장소 `tools\ue`와 사용자 설치 경로의 `.bat` 목록 일치 |
| 기본 commandlet 실행 | PASS | `hello.bat` 결과 JSON의 `.ok == true`, `.validation.engine_boot == "ok"` |

## 이번 라운드에서 발견한 코드 이슈

`run_commandlet.bat`가 추가 인자를 통째로 따옴표로 감싸면서, 일부 wrapper가 넘기는 `-Policy="%PATH%"`, `-Spec="%PATH%"`, `-RootPaths=/Game` 형태의 인자가 UE commandlet 내부에서 값 있는 파라미터로 파싱되지 않았다.

증상:

- `validate_asset_rules.bat` 실행 시 report에 `MISSING_ARG`, `-Policy 인자가 없습니다.` 기록
- UE 로그 command line이 `-Policy <path> -RootPaths /Game` 형태로 표시

수정:

- `tools\ue\run_commandlet.bat`에서 추가 인자를 수집할 때 `-Key`와 다음 값이 분리되어 들어오면 `-Key=Value`로 복원한다.
- `-Key=Value` 형태로 들어온 인자는 그대로 유지한다.
- `-Markdown`, `-AssetPaths`, `-AssetRootPaths`, `-Format`, `-ApplyBuildCs` 등 실제 commandlet 값 옵션 allowlist를 보강한다.
- Windows batch가 comma를 인자 구분자로 취급하는 경우를 고려해 `-AssetPaths`, `-AssetRootPaths`, `-RootPaths`의 분리된 조각을 comma list로 복원한다.
- comma list tail 재결합은 `/Game`, `/Plugin`, `/Engine` 경로 조각으로 제한해 독립 positional argument 흡수를 방지한다.
- 인자 수집 구간에서 delayed expansion을 끄고 `!` 포함 경로 손상을 방지한다.
- 사용자-facing UE wrapper들의 delayed expansion 기반 인자 수집/forwarding을 줄여 wrapper 내부에서 `!` 포함 인자가 손상되지 않도록 한다.
- `&`, `|`, `<`, `>` 포함 인자와 commandlet 이름은 batch 재조립 구조에서 안전하게 전달할 수 없으므로 명시적으로 거부한다.
- commandlet 전용 추가 인자는 `-unattended`, `-nop4`, `-stdout` 등 UE 공통 플래그보다 앞에 배치한다.
- 수정본을 `C:\Users\Sam\.codex\UECommandForge\tools\ue\run_commandlet.bat`에도 반영했다.

## 이번 라운드 검증 순서

### 1. 새 세션 기준 `hello.bat` 재확인

명령:

```bat
C:\Users\Sam\.codex\UECommandForge\tools\ue\hello.bat
```

기대 결과:

- exit code `0`
- 최신 `Hello_*.json` 생성
- report의 `.ok == true`
- report의 `.validation.engine_boot == "ok"`

결과: 진행 예정

실제 결과: PASS

- report: `C:\Workspaces\projects\factory-space\frontend\Saved\UECommandForge/Reports\Hello_3267517207.json`
- `.ok == true`
- `.validation.engine_boot == "ok"`
- 최종 wrapper 동기화 후 재확인 report: `C:\Workspaces\projects\factory-space\frontend\Saved\UECommandForge/Reports\Hello_257384142.json`

### 2. 설치본 기준 read-only commandlet 확인

명령:

```bat
C:\Users\Sam\.codex\UECommandForge\tools\ue\snapshot_assets.bat -RootPaths=/Game
```

기대 결과:

- exit code `0`
- `AssetSnapshot_*.json` 생성
- report의 `.ok == true`

결과: 진행 예정

실제 결과: PASS

- report: `C:\Workspaces\projects\factory-space\frontend\Saved\UECommandForge/Reports\AssetSnapshot_11708.json`
- `.ok == true`
- `.validation.asset_count == "491"`

### 3. 설치본 기준 검증 commandlet 확인

명령:

```bat
C:\Users\Sam\.codex\UECommandForge\tools\ue\validate_asset_rules.bat C:\Users\Sam\.codex\UECommandForge\specs\policies\assets.policy.json -RootPaths=/Game
```

기대 결과:

- `ValidateAssetRules_*.json` 생성
- wrapper가 `-Policy=...`, `-RootPaths=/Game` 인자를 정상 전달
- report의 `.errors == []`
- 대상 프로젝트 policy 위반은 commandlet wrapper 실패가 아니라 별도 asset policy FAIL로 기록

실제 결과: COMMANDLET PASS / POLICY FAIL

- 최초 실행은 wrapper 인자 전달 문제로 `MISSING_ARG`가 발생했다.
- `run_commandlet.bat` 수정 후 `-Policy=...`, `-RootPaths=/Game`이 정상 전달됐다.
- report: `C:\Workspaces\projects\factory-space\frontend\Saved\UECommandForge/Reports\ValidateAssetRules_118014782.json`
- `.errors == []`
- `.validation.asset_count == "491"`
- `.validation.issue_count == "18"`
- exit code는 정책 위반 때문에 실패로 반환됐다.
- 최종 수정본 설치 경로 반영 후 재확인 report: `C:\Workspaces\projects\factory-space\frontend\Saved\UECommandForge/Reports\ValidateAssetRules_232794288.json`
- 재확인 결과도 `.errors == []`, `.validation.root_paths == "/Game"`, `.validation.issue_count == "18"`이다.
- 사용자-facing wrapper delayed expansion 보강 후 재확인 report: `C:\Workspaces\projects\factory-space\frontend\Saved\UECommandForge/Reports\ValidateAssetRules_2580322502.json`
- 재확인 결과도 `.errors == []`, `.validation.root_paths == "/Game"`, `.validation.issue_count == "18"`이다.

정책 위반 요약:

- `MISSING_DEPENDENCY`: 16건
- `REDIRECTOR_FOUND`: 1건
- `ASSET_PREFIX_MISMATCH`: 1건

판정:

- 설치/실행 검증은 PASS
- 대상 프로젝트 asset 정책은 FAIL

### 4. 설치본 기준 생성 계열 dry-run 확인

명령:

```bat
C:\Users\Sam\.codex\UECommandForge\tools\ue\create_project_folders.bat C:\Users\Sam\.codex\UECommandForge\specs\policies\project_folders.json -DryRun
```

기대 결과:

- exit code `0`
- 실제 asset/file 변경 없음
- report의 `.ok == true`

결과: 진행 예정

실제 결과: PASS

- report: `C:\Workspaces\projects\factory-space\frontend\Saved\UECommandForge/Reports\CreateProjectFolders_125532619.json`
- `.ok == true`
- `.dry_run == true`
- `.validation.folder_count == "14"`
- `.validation.expanded_folder_count == "19"`
- `.validation.would_create_count == "18"`
- `.validation.created_folder_count == "0"`
- 사용자-facing wrapper delayed expansion 보강 후 재확인 report: `C:\Workspaces\projects\factory-space\frontend\Saved\UECommandForge/Reports\CreateProjectFolders_2580322502.json`
- 재확인 결과도 `.ok == true`, `.dry_run == true`, `.validation.created_folder_count == "0"`이다.

## 자동 테스트 결과

### 설치본 `.bat` commandlet

| 명령 | 결과 | 메모 |
| --- | --- | --- |
| `hello.bat` | PASS | UECommandForge 플러그인 로드 및 `HelloCommandlet` 성공 |
| `snapshot_assets.bat -RootPaths=/Game` | PASS | asset snapshot report 생성 |
| `validate_asset_rules.bat ... -RootPaths=/Game` | PARTIAL | wrapper 인자 전달은 수정 후 PASS, 프로젝트 정책 위반 18건으로 commandlet exit 실패 |
| `create_project_folders.bat ... -DryRun` | PASS | 실제 생성 없이 dry-run report 생성 |

### 저장소 wrapper 정적 스모크

명령:

```bat
C:\Program Files\Git\bin\bash.exe tools/test/smoke/windows_command_wrappers.sh
```

결과: PASS

확인:

- wrapper 파일 존재/문자열 검사 통과
- `run_commandlet.bat`의 값 옵션 allowlist와 delayed expansion 방어 문자열 확인
- `run_commandlet.bat`의 unsafe cmd metacharacter 거부 문자열 확인
- `run_commandlet.bat`의 commandlet name unsafe cmd metacharacter 거부 문자열 확인
- 대표 사용자-facing wrapper의 `!EXTRA_ARGS!` 제거 확인
- `%*` forwarding wrapper의 `DisableDelayedExpansion` 확인
- `tools\windows\bootstrap_dependencies.bat jq` 음성 테스트에서 `call`을 사용해 batch exit code `2` 전파 확인

### 인자 재조립 디버그 확인

명령:

```bat
cmd /v:off /c "set UECF_DEBUG_ARGS=1&& set UNREAL_EDITOR_CMD=C:\Windows\System32\where.exe&& set PROJECT_FILE=C:\Workspaces\projects\UECommandForge\sample\UECommandForgeSample.uproject&& call tools\ue\run_commandlet.bat Hello -Markdown C:\tmp\report.md -AssetPaths /Game/A,/Game/B -RootPaths=/Game,/Plugin -ApplyBuildCs true -Spec C:\tmp\bang!USERNAME!\spec.json"
```

결과: PASS

확인:

- `-Markdown=C:\tmp\report.md`
- `-AssetPaths=/Game/A,/Game/B`
- `-RootPaths=/Game,/Plugin`
- `-ApplyBuildCs=true`
- `-Spec=C:\tmp\bang!USERNAME!\spec.json`

비고:

- `UNREAL_EDITOR_CMD`를 `where.exe`로 대체했기 때문에 JSON 미생성 exit는 예상 결과다.

### cmd metacharacter 음성 확인

명령:

```bat
cmd /v:on /c "set UECF_DEBUG_ARGS=1&& set UNREAL_EDITOR_CMD=C:\Windows\System32\where.exe&& set PROJECT_FILE=C:\Workspaces\projects\UECommandForge\sample\UECommandForgeSample.uproject&& set P=C:\tmp\a^&echo_INJECTED^&b.md&& call tools\ue\run_commandlet.bat Hello -Markdown=""!P!"" & echo STATUS:!ERRORLEVEL!"
```

결과: PASS

확인:

- `STATUS:2`
- `[run_commandlet] Unsafe cmd metacharacter in argument.`
- `echo_INJECTED`는 실행되지 않음

### commandlet name metacharacter 음성 확인

명령:

```bat
cmd /v:on /c "set UECF_DEBUG_ARGS=1&& set UNREAL_EDITOR_CMD=C:\Windows\System32\where.exe&& set PROJECT_FILE=C:\Workspaces\projects\UECommandForge\sample\UECommandForgeSample.uproject&& call tools\ue\run_commandlet.bat ""Hello^&echo UECF_CMDLET_INJECTED"" -RootPaths=/Game & echo STATUS:!ERRORLEVEL!"
```

결과: PASS

확인:

- `STATUS:2`
- `[run_commandlet] Unsafe cmd metacharacter in commandlet name.`
- `UECF_CMDLET_INJECTED`는 실행되지 않음

### comma list tail 확인

명령:

```bat
cmd /v:off /c "set UECF_DEBUG_ARGS=1&& set UNREAL_EDITOR_CMD=C:\Windows\System32\where.exe&& set PROJECT_FILE=C:\Workspaces\projects\UECommandForge\sample\UECommandForgeSample.uproject&& call tools\ue\run_commandlet.bat Hello -RootPaths /Game LooseArg -Format json"
```

결과: PASS

확인:

- `-RootPaths=/Game`
- `LooseArg`
- `-Format=json`

비고:

- `LooseArg`가 `-RootPaths` 값에 흡수되지 않는다.

### 5. 재설치 확인

전제:

- Unreal Editor 종료
- 프로젝트 파일이나 플러그인 DLL을 잡고 있는 프로세스 없음

명령:

```bat
install-uecommandforge.bat --project C:\Workspaces\projects\factory-space\frontend
```

기대 결과:

- 기존 설치본 백업 또는 덮어쓰기 성공
- `Plugins\UECommandForge` 유지
- `C:\Users\Sam\.codex\UECommandForge\uecommandforge-installed.json` 갱신

결과: 수동 확인 필요

### 6. Unreal Editor 로드 확인

전제:

- Unreal Editor GUI 실행 가능

확인:

- `Wanted_Factory.uproject` 열기
- Plugins 목록에서 `UECommandForge` 활성화 확인
- 에디터 시작 로그에 `UECommandForgeRuntime`, `UECommandForgeEditor` 모듈 로드 오류 없음

결과: 수동 확인 필요

## PASS 기준

이번 라운드는 아래 기준을 만족하면 PASS로 본다.

- 설치 산출물 존재 확인 PASS
- 설치된 사용자 경로의 `.bat` wrapper로 `hello.bat` PASS
- read-only commandlet 최소 1개 PASS
- 검증 commandlet 최소 1개는 인자 전달과 report 생성 기준으로 PASS
- 대상 프로젝트의 정책 위반은 commandlet wrapper 실패가 아니라 별도 asset policy FAIL로 기록
- 생성 계열 commandlet dry-run PASS
- 재설치는 Editor를 닫은 상태에서 별도 PASS 확인
- Unreal Editor GUI 로드는 수동 확인으로 별도 기록

## 실패 시 기록할 정보

실패한 명령마다 아래를 기록한다.

- 실행 명령
- exit code
- 콘솔 로그 마지막 80줄
- 생성된 최신 report JSON 경로
- report의 `.errors`, `.issues`
- `UE_ROOT`, `UNREAL_EDITOR_CMD`
