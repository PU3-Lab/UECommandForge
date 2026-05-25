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

## Result JSON

모든 commandlet은 `sample/Saved/CodexReports/<Commandlet>_<UTC>.json`에 기록한다. 종료 코드:

| 코드 | 의미 |
|---|---|
| 0 | OK |
| 2 | Spec 파싱 실패 |
| 3 | 빌드/컴파일 실패 |
| 4 | 검증 실패 |
| 5 | UE/엔진 오류 |
