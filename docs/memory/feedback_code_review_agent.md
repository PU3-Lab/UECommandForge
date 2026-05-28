---
name: feedback-code-review-agent
description: 코드 품질 리뷰는 리뷰 전용 에이전트를 우선 사용하고, 관련 스킬을 보조 체크리스트로 병행
metadata:
  type: feedback
---

코드 품질 리뷰(Code Quality Review)는 리뷰 전용 에이전트를 우선 사용한다. C++/Unreal 변경은 `cpp-reviewer`, 일반 변경은 `code-reviewer` 또는 `reviewer`, 보안/입력/경로/권한 영향은 `security-reviewer`를 우선한다.

관련 스킬은 보조 체크리스트로 병행한다. 기본 품질 리뷰는 `coding-standards`, 보안/입력/경로/권한 영향은 `security-review`, 테스트 전략은 `tdd-workflow` 또는 `verification-loop`를 사용한다.

**Why:** 사용자가 코드 수정 후 리뷰→수정 반복을 요구한다. 리뷰는 전용 reviewer 에이전트가 가장 적합하고, 스킬은 누락 방지 체크리스트로 유용하다.

**How to apply:** 변경 성격에 맞는 reviewer 에이전트를 먼저 사용한다. 동시에 관련 스킬 기준으로 `git diff`, 관련 테스트, 보안/입력 검증 관점을 점검한다. reviewer 에이전트를 사용할 수 없는 런타임에서는 Codex가 직접 diff 기반 구조화 리뷰를 수행하고, 사용하지 못한 사유를 보고한다. CRITICAL/HIGH 이슈가 있으면 수정 후 재검증한다.
