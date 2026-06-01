# UECommandForge Unreal 자동화 작업 규칙

- Codex가 하지 말아야 할 일
  - 사용자가 요청하더라도 Unreal Python으로 Unreal Editor/프로젝트/에셋에 접근하지 않는다.
  - `import unreal`이 포함된 Python 파일, 인라인 Python 코드, 콘솔 입력, `-ExecutePythonScript`, Editor Utility, PythonScriptPlugin, 원격 실행 도구로 Unreal API를 호출하지 않는다.
  - `-ExecCmds`는 비 Python 자동화 명령(`Automation RunTests ...; Quit`)에만 사용하고, Unreal Python 또는 PythonScriptPlugin 접근에는 사용하지 않는다.
  - 파일을 만들지 않더라도 Python 코드를 Unreal에 전달하거나 실행하지 않는다. 예: `unreal.EditorAssetLibrary`, `unreal.AssetToolsHelpers`, `unreal.BlueprintFactory`, `unreal.KismetEditorUtilities` 호출 금지.
  - `.py` 파일을 새로 만들어 Blueprint, Asset, C++ 반영 작업을 직접 처리하지 않는다.
  - C++ 클래스 생성 직후 같은 흐름에서 Python으로 부모 Blueprint를 만들지 않는다. 새 C++ 클래스는 파일 생성만으로 Unreal reflection에 안정적으로 로드되지 않으며, UHT/빌드/모듈 로드/Editor 재시작이 필요할 수 있다.
  - 실패한 commandlet을 우회하기 위해 임시 스크립트, 외부 런처, 수동 에디터 Python 실행으로 대체하지 않는다.
  - 프로젝트 루트나 사용자 홈에 임시 자동화 산출물(`.py`, `.bat`, `.ps1`, `.json`)을 남기지 않는다. 꼭 필요한 산출물은 기존 `Saved/CodexReports` 계열 경로만 사용한다.
  - Unreal Python 우회 실행 요청을 받으면 코드 조각도 제공하지 말고, 크래시 위험과 commandlet 우선 원칙을 짧게 설명한 뒤 기존 commandlet/wrapper 기반 대안을 실행한다.
- C++ 클래스와 Blueprint를 함께 다룰 때는 순서를 분리한다.
  1. `tools/ue/generate_cpp_class.*` 또는 기존 C++ 생성 commandlet으로 클래스 파일을 생성한다.
  2. Unreal build/UHT 또는 `tools/ue/validate_cpp_reflection.*`, `tools/ue/validate_buildcs.*`로 새 클래스가 반영 가능한지 검증한다.
  3. 새 프로세스에서 기존 Blueprint 생성 commandlet(`create_character_bp.*`, `create_ai_controller.*`, `create_ai_flow.*`)을 실행한다.
  4. Result JSON과 Unreal 로그를 확인한 뒤 다음 단계로 진행한다.
- Blueprint/Asset 생성은 UECommandForge의 기존 commandlet과 wrapper를 우선 사용한다. 새 자동화가 필요하면 commandlet과 `tools/ue/` wrapper로 구현하고 smoke test를 추가한다.
- Unreal 크래시가 발생하면 새 우회 스크립트를 만들기 전에 기존 로그를 먼저 수집한다.
  - 프로젝트 로그: `<Project>/Saved/Logs`
  - 프로젝트 크래시 리포트: `<Project>/Saved/Crashes`
  - Windows CrashReportClient 로그: `%LOCALAPPDATA%\CrashReportClient\Saved\Crashes`
  - UECommandForge Result JSON: `<Project>/Saved/CodexReports`
