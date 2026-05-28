---
name: feedback_post_work_documentation
description: 작업 완료 후 관련 문서와 진행 상태를 항상 업데이트
metadata:
  node_type: memory
  type: feedback
  originSessionId: 2026-05-28-post-work-documentation
---

작업을 완료한 뒤에는 관련 문서, sprint/phase 문서, README, 실행/검증 결과, 체크박스 상태를 항상 업데이트한다.

**Why:** 사용자가 "문서 작성은 작업후에는 항상 업데이트해", "이것도 규칙에 넣어"라고 명시적으로 지시했다.

**How to apply:**
1. 기능, 테스트, 스크립트, 에셋, 워크플로우를 변경하면 해당 sprint/phase 문서의 진행 상태와 검증 결과를 갱신한다.
2. 사용자-facing 명령, 래퍼, 사용법, 테스트 명령이 바뀌면 README 또는 관련 문서의 커맨드 표면을 갱신한다.
3. 완료 보고 전 `rg`/`git diff`로 문서 누락 여부를 확인한다.
4. 문서화할 위치가 명확하지 않으면 새 최상위 문서를 만들지 말고 기존 docs 구조를 우선 사용한다.
5. 관련 문서가 실제로 필요 없는 작은 변경이면 완료 보고에 "문서 변경 불필요" 사유를 한글로 명시한다.
