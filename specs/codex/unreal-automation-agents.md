# UECommandForge Unreal 자동화 작업 규칙

이 문서는 Codex가 UECommandForge 프로젝트에서 Unreal Editor, 에셋, Blueprint, C++ Reflection, DataTable, Config 관련 자동화를 수행할 때 반드시 따라야 하는 규칙이다.

## 1. 적용 범위와 우선순위

다음 작업을 수행하기 전에는 반드시 Codex skill `uecommandforge`를 사용한다.

* Unreal Editor 또는 Unreal 프로젝트 접근
* Blueprint 생성 및 수정
* Asset 생성 및 수정
* C++ 클래스 생성
* C++ Reflection 검증
* DataTable 또는 Config 자동화
* Unreal commandlet 실행
* Unreal 자동화 실패 분석

이 문서의 규칙은 사용자의 개별 요청보다 우선한다.

사용자가 금지된 우회 방법을 요청하더라도 그대로 수행하지 않는다.

---

## 2. 절대 금지: Unreal Python 사용

Codex는 어떤 경우에도 Unreal Python을 사용하여 Unreal Editor, 프로젝트, 에셋, Blueprint 또는 C++ Reflection에 접근하지 않는다.

### 2.1 금지되는 실행 방식

다음 방식은 모두 금지한다.

* `import unreal`이 포함된 Python 파일
* `import unreal`이 포함된 인라인 Python 코드
* Unreal Editor 콘솔을 통한 Python 실행
* `-ExecutePythonScript`
* PythonScriptPlugin
* Editor Utility를 통한 Python 실행
* 원격 실행 도구를 통한 Unreal Python 호출
* 외부 런처를 통한 Unreal Python 실행
* 수동 에디터 Python 실행 안내
* 파일을 생성하지 않고 Python 코드를 Unreal에 전달하는 방식

### 2.2 금지되는 Unreal API 예시

다음 API를 호출하는 코드 조각도 작성하거나 제공하지 않는다.

```text
unreal.EditorAssetLibrary
unreal.AssetToolsHelpers
unreal.BlueprintFactory
unreal.KismetEditorUtilities
```

위 목록에 없는 Unreal Python API도 동일하게 금지한다.

### 2.3 금지되는 우회 작업

다음 작업을 Unreal Python으로 처리하지 않는다.

* Blueprint 생성
* Blueprint 컴파일
* Asset 생성 또는 수정
* C++ 클래스 반영
* CDO 기본값 변경
* Blueprint 컴포넌트 추가 또는 수정
* 실패한 commandlet 우회
* 새 C++ 클래스 생성 직후 부모 Blueprint 생성

새 C++ 클래스는 파일 생성만으로 Unreal Reflection에 안정적으로 로드되지 않을 수 있다.

다음 과정이 필요할 수 있으므로 C++ 클래스 생성과 Blueprint 생성은 반드시 분리한다.

* UHT 실행
* Unreal Build 실행
* 모듈 로드
* Unreal Editor 재시작
* 새 Unreal 프로세스 실행

### 2.4 Unreal Python 요청을 받은 경우

사용자가 Unreal Python 우회 실행을 요청하면 다음 원칙을 따른다.

1. Unreal Python 코드 조각을 제공하지 않는다.
2. Unreal Python 우회는 크래시 및 Reflection 불일치 위험이 있다고 짧게 설명한다.
3. 기존 commandlet 또는 wrapper 기반 대안을 사용한다.
4. 기존 commandlet으로 처리할 수 없다면 작업을 중단한다.
5. 왜 기존 commandlet으로 처리할 수 없는지 설명하고 사용자 승인을 받는다.

---

## 3. 임시 파일 및 자동화 산출물 규칙

프로젝트 루트 또는 사용자 홈 디렉터리에 임시 자동화 산출물을 남기지 않는다.

### 3.1 생성 금지 위치

다음 위치에는 임시 자동화 파일을 생성하지 않는다.

* 프로젝트 루트
* 사용자 홈 디렉터리
* 소스 코드와 무관한 임의의 디렉터리

### 3.2 임시 파일로 사용하지 않는 확장자

다음 파일을 임시 우회 자동화 용도로 생성하지 않는다.

```text
.py
.bat
.ps1
.json
```

특히 `import unreal`이 포함된 `.py` 파일은 어떤 위치에도 생성하지 않는다.

### 3.3 허용되는 산출물 위치

자동화 실행 결과, 스펙, 보고서, 로그 분석 결과처럼 꼭 필요한 비 Python 산출물은 기존 `Saved/CodexReports` 계열 경로만 사용한다.

```text
<Project>/Saved/CodexReports
```

작업 종료 후 임시 산출물 삭제가 필요한 경우 사용자에게 삭제 여부를 확인한다.

---

## 4. UECommandForge 사전 확인 절차

Unreal 자동화 작업을 시작하기 전에 다음 파일을 확인한다.

```text
~/.codex/UECommandForge/uecommandforge-installed.json
~/.codex/UECommandForge/uecommandforge.env
<Project>/UECommandForge/uecommandforge-project.json
```

확인해야 할 내용은 다음과 같다.

* UECommandForge 설치 여부
* 대상 Unreal 프로젝트 경로
* Unreal Engine 경로
* wrapper 위치
* 프로젝트별 UECommandForge 설정
* 사용할 수 있는 기존 commandlet 및 wrapper

설정 파일을 확인하지 않은 상태에서 Unreal 자동화를 실행하지 않는다.

---

## 5. Wrapper 사용 원칙

직접 Unreal Editor 명령을 조합하기보다 UECommandForge의 기존 wrapper를 우선 사용한다.

### 5.1 Windows

Windows Command Prompt 또는 PowerShell에서는 다음 wrapper를 우선 사용한다.

```text
~/.codex/UECommandForge/tools/ue/*.bat
```

### 5.2 Git Bash, macOS, Linux

Git Bash, macOS 또는 Linux에서는 다음 wrapper를 우선 사용한다.

```text
~/.codex/UECommandForge/tools/ue/*.sh
```

### 5.3 `-ExecCmds` 사용 제한

`-ExecCmds`는 비 Python 자동화 명령에만 사용한다.

허용 예시:

```text
Automation RunTests ...; Quit
```

금지 예시:

* Unreal Python 실행
* PythonScriptPlugin 접근
* Python 코드를 전달하는 명령
* Python 기반 에셋 생성 또는 수정

---

## 6. 표준 자동화 실행 흐름

모든 Unreal 자동화 작업은 다음 순서로 수행한다.

### 6.1 기존 기능 확인

1. Codex skill `uecommandforge`를 사용한다.
2. 설치 및 프로젝트 설정 파일을 확인한다.
3. 기존 commandlet과 wrapper가 작업을 지원하는지 확인한다.
4. 기존 기능이 있으면 새 자동화를 만들지 않고 기존 기능을 사용한다.

### 6.2 Wrapper 실행

1. 현재 운영체제에 맞는 `.bat` 또는 `.sh` wrapper를 실행한다.
2. wrapper 실행 결과를 확인한다.
3. 최신 Result JSON을 확인한다.

Result JSON 위치:

```text
<Project>/Saved/CodexReports
```

### 6.3 결과 검증

wrapper 실행 후 다음을 확인한다.

* Result JSON의 성공 여부
* 생성 또는 수정 대상
* Unreal 로그의 오류 여부
* 크래시 발생 여부
* 다음 단계 진행 가능 여부

결과를 확인하지 않고 다음 자동화 단계로 진행하지 않는다.

---

## 7. C++ 클래스와 Blueprint를 함께 다룰 때의 필수 순서

C++ 클래스 생성과 Blueprint 생성을 같은 Unreal 프로세스 또는 같은 실행 흐름에서 처리하지 않는다.

반드시 다음 순서로 분리한다.

### 7.1 C++ 클래스 생성

다음 wrapper 또는 기존 C++ 생성 commandlet을 사용한다.

```text
tools/ue/generate_cpp_class.*
```

C++ 클래스 파일 생성까지만 수행한다.

### 7.2 Build 및 Reflection 검증

다음 중 필요한 검증을 수행한다.

```text
tools/ue/validate_cpp_reflection.*
tools/ue/validate_buildcs.*
```

또는 Unreal Build 및 UHT를 실행하여 새 클래스가 Reflection에 반영 가능한지 확인한다.

확인해야 할 항목:

* UHT 성공 여부
* 컴파일 성공 여부
* 모듈 로드 가능 여부
* Reflection 클래스 검색 가능 여부
* Build.cs 설정 유효성

### 7.3 새 프로세스에서 Blueprint 생성

C++ 클래스가 정상적으로 반영된 것을 확인한 후 새 Unreal 프로세스에서 기존 Blueprint 생성 commandlet을 실행한다.

예시:

```text
tools/ue/create_character_bp.*
tools/ue/create_ai_controller.*
tools/ue/create_ai_flow.*
```

### 7.4 결과 확인

Blueprint 생성 후 다음을 확인한다.

* 최신 Result JSON
* Unreal 프로젝트 로그
* Blueprint 생성 여부
* 부모 C++ 클래스 연결 여부
* 크래시 또는 Reflection 오류 여부

결과 확인 후에만 다음 단계로 진행한다.

---

## 8. Blueprint 및 Asset 생성 원칙

Blueprint와 Asset 생성은 UECommandForge의 기존 commandlet과 wrapper를 우선 사용한다.

Unreal Python으로 직접 생성하거나 수정하지 않는다.

### 8.1 기존 commandlet 우선

기존 commandlet이 작업을 지원하면 해당 commandlet을 사용한다.

### 8.2 Blueprint 기본값 및 컴포넌트 적용

Blueprint 생성 이후 CDO 기본값 또는 컴포넌트를 적용해야 하면 다음 wrapper와 spec을 사용한다.

```text
tools/ue/set_blueprint_defaults.*
blueprint_defaults spec
```

다음 작업을 Unreal Python으로 직접 처리하지 않는다.

* CDO 기본값 수정
* 컴포넌트 추가
* 컴포넌트 속성 변경
* Blueprint 변수 기본값 변경

---

## 9. 새 자동화 기능이 필요한 경우

기존 commandlet과 wrapper로 작업을 처리할 수 없는 경우에만 새 자동화를 구현한다.

### 9.1 구현 위치

새 commandlet은 다음 위치에 구현한다.

```text
sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/
```

새 wrapper는 다음 위치에 구현한다.

```text
tools/ue/
```

### 9.2 구현 요구사항

새 자동화 기능에는 다음이 포함되어야 한다.

* Unreal commandlet
* 운영체제별 wrapper
* 입력값 검증
* Result JSON 출력
* 실패 시 명확한 오류 메시지
* smoke test
* 필요한 문서 또는 사용 예시

### 9.3 금지되는 구현 방식

새 자동화가 필요하더라도 다음 방식은 사용하지 않는다.

* 일회성 Unreal Python 스크립트
* 임시 외부 런처
* 수동 Editor Python 실행
* 프로젝트 루트의 임시 `.bat`, `.ps1`, `.py`, `.json`
* commandlet 실패를 숨기기 위한 우회 실행

---

## 10. 실패 및 크래시 대응 원칙

commandlet 또는 wrapper가 실패하거나 Unreal 크래시가 발생하면 새 우회 스크립트를 만들지 않는다.

먼저 기존 로그와 결과 파일을 수집하고 원인을 분석한다.

### 10.1 확인해야 할 경로

프로젝트 로그:

```text
<Project>/Saved/Logs
```

프로젝트 크래시 리포트:

```text
<Project>/Saved/Crashes
```

Windows CrashReportClient 로그:

```text
%LOCALAPPDATA%\CrashReportClient\Saved\Crashes
```

UECommandForge Result JSON:

```text
<Project>/Saved/CodexReports
```

### 10.2 실패 대응 순서

1. 최신 Result JSON을 확인한다.
2. 프로젝트 로그를 확인한다.
3. 크래시 리포트를 확인한다.
4. 실패한 commandlet의 입력값과 실행 조건을 확인한다.
5. 기존 wrapper 또는 commandlet의 수정 가능성을 검토한다.
6. Unreal Python 우회 없이 해결 가능한 대안을 제시한다.
7. 기존 commandlet으로 해결할 수 없다면 이유를 설명하고 사용자 승인을 받는다.

---

## 11. Codex 최종 판단 체크리스트

Unreal 자동화 작업 전에는 다음 질문에 모두 답할 수 있어야 한다.

* `uecommandforge` skill을 사용했는가?
* 설치 및 프로젝트 설정 파일을 확인했는가?
* 기존 commandlet 또는 wrapper가 있는지 확인했는가?
* 운영체제에 맞는 wrapper를 사용하고 있는가?
* Unreal Python을 사용하거나 제공하지 않았는가?
* 임시 우회 스크립트를 만들지 않았는가?
* C++ 생성과 Blueprint 생성을 별도 단계 및 별도 프로세스로 분리했는가?
* Build, UHT, Reflection 검증을 완료했는가?
* 최신 Result JSON을 확인했는가?
* 실패 시 로그와 크래시 리포트를 먼저 확인했는가?
* 새 기능이 필요하다면 commandlet, wrapper, smoke test 방식으로 구현했는가?

하나라도 충족하지 못하면 다음 단계로 진행하지 않는다.
