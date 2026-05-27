# UECommandForge

LLM 기반 Unreal Engine 자동화 브리지. 전체 설계는 `ue_commandlet_based_llm_automation_plan.md` 참조.

## 디렉토리 구조

```
unreal-harness/
  sample/               UE 샘플 프로젝트 (UECommandForgeSample.uproject + Plugins/)
  tools/ue/             Shell 래퍼 — LLM이 호출하는 명령 표면
  docs/                 계획서, 메모리, 설계 문서
```

## Codex 명령 표면 (Phase 1)

| 명령 | 목적 |
|---|---|
| `tools/ue/hello.sh` | 헬스체크 — `Hello` commandlet 실행 후 Result JSON 기록. |

이후 Phase가 추가될수록 명령이 늘어난다. LLM은 이 표 외의 명령을 호출해서는 안 된다.

## 환경 요구 사항

- Unreal Engine 5.7 설치 필요.
- 기본 경로가 아닌 경우 `UE_ROOT` 환경변수 설정.
- macOS 기본값: `/Users/Shared/Epic Games/UE_5.7`
- Windows 기본값 (Git Bash): `/c/Program Files/Epic Games/UE_5.7`

## 개발용 C++ 정적 분석

`clang-tidy`와 `cppcheck`는 UECommandForge 플러그인 배포물에 포함하지 않는다. 배포 패키지는 Unreal 플러그인 코드만 포함하고, 정적 분석기는 개발/CI 환경의 외부 prerequisite로 설치한다.

macOS:

```bash
brew install llvm cppcheck
```

Windows:

```powershell
winget install LLVM.LLVM
winget install Cppcheck.Cppcheck
```

또는 Chocolatey 사용 시:

```powershell
choco install llvm cppcheck -y
```

도구 확인:

```bash
tools/lint/cpp_static_analysis.sh --check-tools
```

Windows PowerShell:

```powershell
.\tools\lint\cpp_static_analysis.ps1 -CheckTools
```

탐색 우선순위:

- `CLANG_TIDY`, `CPPCHECK` 환경변수
- `clang-tidy`/`cppcheck` 또는 Windows의 `clang-tidy.exe`/`cppcheck.exe` PATH
- macOS Homebrew LLVM: `/opt/homebrew/opt/llvm/bin/clang-tidy`, `/usr/local/opt/llvm/bin/clang-tidy`
- Windows LLVM/Cppcheck 기본 설치 경로와 Visual Studio LLVM 후보 경로

`tools/lint/cpp_static_analysis.sh`는 macOS/Linux/Windows Git Bash용이고, `tools/lint/cpp_static_analysis.ps1`은 Windows PowerShell용이다. `clang-tidy` 실행에는 `compile_commands.json`이 필요하다. 기본 위치는 repo root이며, 다른 위치를 쓰려면 `COMPILE_COMMANDS_DIR`를 설정한다. 파일이 없으면 래퍼는 `cppcheck`만 실행하고 `clang-tidy`는 건너뛴다.

## Result JSON

모든 commandlet은 `sample/Saved/CodexReports/<Commandlet>_<UTC>.json`에 기록한다. 종료 코드:

| 코드 | 의미 |
|---|---|
| 0 | OK |
| 2 | Spec 파싱 실패 |
| 3 | 빌드/컴파일 실패 |
| 4 | 검증 실패 |
| 5 | UE/엔진 오류 |
