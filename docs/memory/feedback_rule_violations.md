---
name: feedback_rule_violations
description: 코드 작성 중 반복되는 규칙 위반 패턴 목록 — 리뷰에서 발견된 실제 위반 사례
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 897a1210-8b3d-469b-8864-3028ea6e197e
---

코드를 작성할 때 반복적으로 저지른 규칙 위반 패턴을 기록한다.
새 코드를 작성하기 전에 이 목록을 참조하여 같은 실수를 반복하지 않는다.

**Why:** Sprint 4 validate-buildcs/reflection 구현 후 코드 리뷰에서 CRITICAL 2건 + HIGH 3건 발견.
같은 패턴의 실수가 반복될 위험이 있어 문서화함.

**How to apply:** 정규식, 파싱, 타입 어노테이션 코드를 작성할 때 아래 체크리스트를 확인한다.

---

## 위반 #1 — 중첩 괄호 미처리 정규식 (CRITICAL)

**언제:** UFUNCTION/UPROPERTY 파싱에서 `([^)]*)` 패턴 사용
**문제:** `Meta=(DisplayName="Func()")` 같은 중첩 괄호에서 regex가 첫 번째 `)` 에서 멈춤
**올바른 패턴:** `([^()]*(?:\([^()]*\)[^()]*)*)` — 1단계 중첩까지 허용
**적용 범위:** 구조화된 텍스트(C++, C#, YAML)에서 괄호 내용을 파싱할 때

## 위반 #2 — 주석 미제거 후 구조 파싱 (CRITICAL)

**언제:** Build.cs `AddRange({...})` 블록을 `[^}]*`로 파싱
**문제:** 주석 안에 `}` 가 있으면 regex가 조기 종료됨 (`// Note: {essential}`)
**올바른 방법:** 구조화된 파일을 파싱하기 전에 `//` 및 `/* */` 주석을 제거
**적용 범위:** C#/C++ 소스에서 regex로 구조 파싱할 때

## 위반 #3 — 파싱한 자료구조 대신 문자열 검색 (HIGH)

**언제:** `args_set` (파싱된 집합)을 만들어놓고 `"Category" not in item["args"]` (원본 문자열 검색) 사용
**문제:** 이미 파싱한 구조체를 무시하고 문자열 검색으로 fallback → 불일치
**올바른 방법:** 파싱한 자료구조(`args_set`)를 일관되게 사용: `"Category" not in args_set`
**적용 범위:** 파싱 로직 작성 후 검증 로직에서 일관성 유지

## 위반 #4 — 커맨드 함수 타입 어노테이션 누락 (HIGH)

**언제:** `def _cmd_validate_buildcs(args) -> int:` — `args`에 타입 없음
**올바른 방법:** `def _cmd_validate_buildcs(args: argparse.Namespace) -> int:`
**적용 범위:** `argparse` 기반 모든 커맨드 함수 (`_cmd_*` 패턴)

## 위반 #5 — YAML 로드 후 구조 검증 누락 (HIGH)

**언제:** `policy = yaml.safe_load(...) or default` — 유효한 YAML이지만 잘못된 구조일 때 silent fallback
**문제:** 잘못된 policy 파일이 기본값으로 대체되어 오류를 숨김
**올바른 방법:** 로드 후 `isinstance(loaded, dict)` 확인, 필수 키 존재 확인

---

## 체크리스트 (코드 작성 전 확인)

- [ ] 괄호/중괄호 내용 파싱 regex → 중첩 허용 패턴 사용했는가?
- [ ] 구조화된 파일(C#/C++) 파싱 → 주석 제거 후 파싱하는가?
- [ ] 파싱 후 검증 로직 → 파싱된 자료구조를 일관되게 사용하는가?
- [ ] `_cmd_*` 함수 → `args: argparse.Namespace` 타입 어노테이션 있는가?
- [ ] 외부 파일 로드(YAML/JSON) → 로드 후 구조 검증 있는가?
