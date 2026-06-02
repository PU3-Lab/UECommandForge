# UECommandForge Codex Skill 설계

## 목표

Codex가 Unreal 자동화 요청을 받을 때 UECommandForge 설치 상태를 확인하고, 직접 Unreal Python이나 임시 우회 스크립트를 만들지 않고 설치된 commandlet wrapper를 사용하도록 한다.

## 구조

- 항상 적용되어야 하는 안전 규칙은 `specs/codex/unreal-automation-agents.md`에 남긴다.
- 실제 사용 절차는 `skills/uecommandforge/SKILL.md`로 분리한다.
- tools package는 `skills/uecommandforge`를 포함한다.
- installer는 tools package 안의 skill을 `~/.codex/skills/uecommandforge`에 설치한다.
- uninstall 기본 동작은 기존 Codex tools/specs 보존 정책과 동일하게 skill도 보존한다. 명시적 full uninstall에서만 skill을 제거한다.

## 검증

- `release_package_tools` smoke는 Tools.zip에 `skills/uecommandforge/SKILL.md`가 포함되고 manifest checksum에 기록되는지 검증한다.
- `release_package_install` smoke는 설치 후 `~/.codex/skills/uecommandforge/SKILL.md`가 존재하고 AGENTS managed block이 skill 사용을 지시하는지 검증한다.
- skill 자체는 `skill-creator`의 `quick_validate.py`로 frontmatter와 이름 규칙을 검증한다.
