---
name: feedback-code-review-agent
description: 코드 품질 리뷰는 ecc:cpp-reviewer 직접 호출 대신 codex:codex-rescue 에이전트를 사용
metadata:
  type: feedback
---

코드 품질 리뷰(Code Quality Review)는 `ecc:cpp-reviewer` 서브에이전트를 직접 호출하지 않는다. 대신 `codex:codex-rescue` 에이전트를 사용하고, 해당 에이전트에 `ecc:cpp-reviewer` 스킬을 활용하도록 지시한다.

**Why:** 사용자가 코드 리뷰는 Codex를 통해 진행하길 원함 (2026-05-26 지시).

**How to apply:** subagent-driven-development 워크플로우의 code quality reviewer 단계에서 `ecc:cpp-reviewer` 직접 호출 대신 `codex:codex-rescue` 에이전트를 디스패치하고 `ecc:cpp-reviewer` 스킬 사용을 지시한다.
