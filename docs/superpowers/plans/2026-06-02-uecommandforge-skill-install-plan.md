# UECommandForge Skill Install Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** UECommandForge tools package 설치 시 Codex skill을 함께 설치한다.

**Architecture:** 안전 규칙은 `AGENTS.md` managed block에 유지하고, 절차형 사용법은 `skills/uecommandforge/SKILL.md`로 분리한다. 릴리즈 tools package가 skill을 포함하며, installer가 이를 Codex skill 디렉터리로 복사한다.

**2026-06-11 유지보수:** 전역 `AGENTS.md`에는 `specs/codex/unreal-automation-agents.md`의 managed content 구획만 설치한다. 상세 규칙은 설치된 spec에 유지하고 skill이 이를 명시적으로 읽는다. source spec과 기존 사용자 `AGENTS.md`의 marker 순서는 설치 대상 변경 전에 검증한다.

**Tech Stack:** Bash release scripts, Codex skill markdown, smoke tests, jq manifest validation.

---

### Task 1: 패키지 smoke test 확장

**Files:**
- Modify: `tools/test/smoke/release_package_tools.sh`
- Modify: `tools/test/smoke/release_package_install.sh`

- [ ] `release_package_tools.sh`가 Tools.zip의 `skills/uecommandforge/SKILL.md`와 manifest checksum을 검증하도록 추가한다.
- [ ] `release_package_install.sh`가 설치된 Codex skill 파일과 AGENTS의 skill 사용 지시를 검증하도록 추가한다.
- [ ] 구현 전 smoke를 실행해 skill 누락으로 실패하는지 확인한다.

### Task 2: skill 작성

**Files:**
- Create: `skills/uecommandforge/SKILL.md`
- Create: `skills/uecommandforge/agents/openai.yaml`
- Delete: `uecommandforge/SKILL.md`

- [ ] `SKILL.md`에 설치 확인, wrapper 선택, 실행, Result JSON 확인, 실패 시 우회 금지 절차를 작성한다.
- [ ] `agents/openai.yaml`을 생성해 Codex UI metadata를 제공한다.
- [ ] `quick_validate.py`로 skill frontmatter를 검증한다.

### Task 3: 패키징 및 설치 구현

**Files:**
- Modify: `tools/release/package_tools.sh`
- Modify: `tools/release/write_manifest.sh`
- Modify: `tools/release/install_local.sh`
- Modify: `tools/release/uninstall.sh`
- Modify: `specs/codex/unreal-automation-agents.md`

- [ ] `package_tools.sh`가 `skills` 디렉터리를 검증하고 package에 복사하도록 한다.
- [ ] `write_manifest.sh`가 `skill_files`와 checksum을 manifest에 기록하도록 한다.
- [ ] `install_local.sh`가 tools package의 `skills/uecommandforge`를 `~/.codex/skills/uecommandforge`에 설치하도록 한다.
- [ ] `uninstall.sh`가 full uninstall 옵션에서 skill을 제거하도록 한다.
- [ ] AGENTS managed block에 UECommandForge 작업 전 skill 사용 지시를 추가한다.

### Task 4: 검증

**Files:**
- No production file changes

- [ ] `bash -n`으로 수정한 shell scripts 문법을 확인한다.
- [ ] `quick_validate.py skills/uecommandforge`를 실행한다.
- [ ] `tools/test/smoke/release_package_tools.sh`를 실행한다.
- [ ] `tools/test/smoke/release_package_install.sh`를 실행한다.
- [ ] `git diff --check`를 실행한다.
