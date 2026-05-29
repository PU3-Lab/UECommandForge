# UECommandForge Phase 8 Windows 실기 검증 시나리오

작성일: 2026-05-29

## 목적

Windows 실제 호스트에서 Phase 8 배포/설치/Command Prompt wrapper가 동작하는지 검증한다. macOS 로컬 검증에서 대체한 `windows_host_required` 제한 리포트를 실제 실행 결과로 해소하는 것이 목표다.

## 사전 조건

- Windows 10/11
- Unreal Engine 5.7 설치
- `winget` 실행 가능
- Command Prompt wrapper는 실행 시 `tools\windows\bootstrap_dependencies.bat`로 누락 의존성을 확인한다.
  - core 프로파일: `Git.Git`, `jqlang.jq`
  - jq 프로파일: `jqlang.jq`
  - lint 프로파일: `LLVM.LLVM`, `Cppcheck.Cppcheck`
  - 자동 설치를 허용하려면 `UECF_AUTO_INSTALL_DEPS=1` 설정
  - 설치 없이 누락 의존성만 확인하려면 `UECF_SKIP_DEP_INSTALL=1` 설정
- 자동 설치를 사용하지 않는 경우 Git for Windows와 `jq`를 설치하고 `cmd.exe`에서 `git`, `bash`, `jq` 실행 가능해야 한다.
- UE 기본 경로가 아니면 `UE_ROOT` 설정

기본 UE 경로:

```bat
C:\Program Files\Epic Games\UE_5.7
```

기본 경로가 아니면:

```bat
set UE_ROOT=D:\Epic Games\UE_5.7
```

## 준비

깨끗한 checkout에서 시작한다.

```bat
git status --short
```

기대 결과:

```text
출력 없음
```

Windows wrapper 정적 검증은 Git Bash에서 실행한다.

```bat
bash tools/test/smoke/windows_command_wrappers.sh
```

기대 결과:
- exit code `0`
- 출력이 없거나 실패 메시지 없음

## 시나리오 1: 기본 Commandlet 실행

```bat
tools\ue\hello.bat
```

기대 결과:
- exit code `0`
- `sample\Saved\CodexReports\Hello_<UTC>.json` 생성
- report의 `.ok == true`

확인:

```bat
dir /b /o-d sample\Saved\CodexReports\Hello_*.json
```

## 시나리오 2: Asset 정책 검증

```bat
tools\ue\snapshot_assets.bat -RootPaths=/Game/Tests
tools\ue\validate_asset_rules.bat specs\policies\assets.policy.json -RootPaths=/Game/Tests
```

기대 결과:
- 두 명령 모두 exit code `0`
- `AssetSnapshot_*.json`, `ValidateAssetRules_*.json` 생성
- report의 `.ok == true`

## 시나리오 3: C++ 생성/검증

```bat
tools\ue\generate_cpp_class.bat specs\examples\cpp_health_component.json
tools\ue\validate_cpp_reflection.bat specs\policies\cpp_reflection.policy.json
tools\ue\validate_buildcs.bat specs\policies\buildcs.policy.json
```

기대 결과:
- 각 명령 exit code `0`
- `GenerateCppClass_*.json`, `ValidateCppReflection_*.json`, `ValidateBuildCs_*.json` 생성
- report의 `.ok == true`

주의:
- C++ 생성 명령이 이미 생성된 파일 때문에 실패하면, 깨끗한 checkout에서 재시도한다.

## 시나리오 4: 데이터/Config 검증

```bat
tools\ue\validate_data_source.bat specs\schemas\items.schema.json specs\data\items.csv
tools\ue\validate_config_rules.bat specs\schemas\project_config.schema.json specs\config\project_config.ini
```

기대 결과:
- 각 명령 exit code `0`
- `ValidateDataSource_*.json`, `ValidateConfigRules_*.json` 생성
- report의 `.ok == true`

## 시나리오 5: 통합 품질 게이트

```bat
tools\test\smoke\prototype_automation.bat
```

기대 결과:
- exit code `0`
- `prototype_automation smoke PASS`
- 통합 report의 `.ok == true`
- `.validation.suite_count == "4"`
- `.validation.failed_suite_count == "0"`
- Markdown report 생성

## 시나리오 6: 릴리즈 패키지 설치 검증

```bat
tools\test\smoke\release_package_install.bat
```

기대 결과:
- exit code `0`
- plugin/tools package 생성
- sibling `checksums.txt` 생성
- 임시 프로젝트 install/uninstall 성공
- version/channel mismatch regression이 의도대로 실패 처리됨

## 시나리오 7: Installer install/update/uninstall 검증

```bat
tools\test\smoke\installer_install_update_uninstall.bat
```

기대 결과:
- exit code `0`
- install 성공
- update 시 backup 생성
- default uninstall 후 Codex tools/specs metadata 보존
- full uninstall 옵션에서 지정 항목 제거

## 실패 시 수집할 정보

실패한 명령마다 아래 정보를 남긴다.

- 실행한 명령 전체
- exit code
- 콘솔 로그 마지막 80줄
- 생성된 최신 report JSON
- `UE_ROOT` 값
- Unreal Engine 설치 경로
- Git Bash `bash --version`

수집 명령 예시:

```bat
echo UE_ROOT=%UE_ROOT%
bash --version
dir /b /o-d sample\Saved\CodexReports\*.json
```

## 최종 PASS 기준

아래가 모두 충족되면 Windows 실기 검증 PASS로 본다.

- `windows_command_wrappers.sh` 정적 검증 PASS
- `hello.bat` PASS
- asset snapshot/policy 검증 PASS
- C++ 생성/reflection/Build.cs 검증 PASS
- data/config 검증 PASS
- `prototype_automation.bat` PASS
- `release_package_install.bat` PASS
- `installer_install_update_uninstall.bat` PASS
- 실패 report 없음

## 결과 보고 형식

```text
Windows 실기 검증 결과
- OS:
- UE_ROOT:
- Git Bash:
- hello.bat: PASS/FAIL
- asset 검증: PASS/FAIL
- C++ 검증: PASS/FAIL
- data/config 검증: PASS/FAIL
- prototype_automation.bat: PASS/FAIL
- release_package_install.bat: PASS/FAIL
- installer_install_update_uninstall.bat: PASS/FAIL
- 실패 report 경로:
- 메모:
```
