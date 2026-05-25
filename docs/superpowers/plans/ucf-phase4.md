# UECommandForge Phase 4 — StateTree 생성 구현 계획

> **에이전트 실행자 필독:** Phase 3 (`2026-05-25-uecommandforge-phase3.md`) 완료 후 실행하세요.
> REQUIRED SUB-SKILL: `superpowers:subagent-driven-development` (권장) 또는 `superpowers:executing-plans`

**목표:** `StateTreeSpec`으로부터 `UStateTree` 에셋을 생성하고, State·Transition·Task를 주입한 뒤 저장한다.

**아키텍처:** `StateTreeEditorAdapter`로 UE 5.7 StateTree Editor API를 격리한다. 실제 State/Transition 변경 API는 헤더 분석(스파이크) 후 확정한다. Builder는 어댑터를 통해 동작하고, Commandlet은 얇게 유지한다.

**기술 스택:** UE 5.7, `StateTreeModule`, `StateTreeEditorModule`, `GameplayStateTreeModule`.

---

## 리스크 — 반드시 스파이크 먼저

UE 5.7 StateTree Editor API의 State/Transition 변경 메서드는 공개 범위가 좁고, 버전마다 변경될 수 있다. **Task 1을 실행하기 전에 반드시 아래 스파이크를 완료해야 한다.**

```bash
# 스파이크: StateTree Editor 헤더 탐색
find "${UE_ROOT}/Engine/Plugins/Runtime/StateTree/Source/StateTreeEditorModule/Public" \
  -name "*.h" | xargs grep -l "AddState\|AddTransition\|AddTask" 2>/dev/null
```

스파이크 결과에 따라 Task 2 이후 어댑터 메서드 시그니처를 확정한다.

---

## 파일 구조

| 경로 | 책임 |
|---|---|
| `Plugins/.../Public/Builders/StateTreeEditorAdapter.h` | UE 5.7 StateTree Editor API 격리 인터페이스 |
| `Plugins/.../Private/Builders/StateTreeEditorAdapter.cpp` | 구현 (스파이크 결과 반영) |
| `Plugins/.../Public/Builders/StateTreeBuilder.h` | StateTree 에셋 생성·State·Task 주입 인터페이스 |
| `Plugins/.../Private/Builders/StateTreeBuilder.cpp` | 구현 |
| `Plugins/.../Private/Commandlets/CreateStateTreeCommandlet.h/.cpp` | StateTree 생성 Commandlet |
| `Plugins/.../Tests/StateTreeBuilderTest.cpp` | 자동화 테스트 |
| `tools/ue/create_statetree.sh` | Shell Wrapper |

---

## Task 1: Build.cs에 StateTree 모듈 추가

- [ ] **Step 1: `UECommandForgeEditor.Build.cs` 수정**

`PrivateDependencyModuleNames`에 아래 항목 추가:

```csharp
"StateTreeModule",
"StateTreeEditorModule",
"GameplayStateTreeModule"
```

- [ ] **Step 2: 플러그인 빌드 확인**

```bash
./tools/ue/build_plugin.sh
```

예상: PASS (빈 모듈 의존성 추가만이므로 링크 오류 없음).

- [ ] **Step 3: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/UECommandForgeEditor.Build.cs
git commit -m "feat: UECommandForgeEditor에 StateTree 모듈 의존성 추가"
```

---

## Task 2: 스파이크 — StateTree Editor API 확인

- [ ] **Step 1: StateTree Editor 헤더에서 State 추가 API 탐색**

```bash
find "${UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}/Engine/Plugins/Runtime/StateTree" \
  -name "*.h" | xargs grep -l "AddState\|FStateTreeEditorNode\|StateTreeViewModel" 2>/dev/null | head -20
```

- [ ] **Step 2: 찾은 헤더 파일에서 공개 메서드 목록 확인**

예시 (UE 5.7 기준 — 실제 확인 필요):

```bash
grep -n "STATETREEEDITORMODULE_API\|public:" \
  "${UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}/Engine/Plugins/Runtime/StateTree/Source/StateTreeEditorModule/Public/StateTreeViewModel.h" \
  | head -40
```

- [ ] **Step 3: 스파이크 결과 메모**

스파이크 완료 후 Task 3 어댑터 시그니처를 확정한다. 아래는 UE 5.7 기준 예상 API이며, 실제 결과와 다를 경우 수정한다:

- `UStateTreeEditorData::AddState(FName StateName)` → `FStateTreeStateHandle`
- `UStateTreeEditorData::AddTransition(FStateTreeStateHandle From, FStateTreeStateHandle To)`
- StateTree 에셋 생성: `IAssetTools::CreateAsset(Name, Path, UStateTree::StaticClass(), Factory)`

---

## Task 3: StateTreeEditorAdapter

스파이크 결과를 반영해 어댑터를 구현한다.

- [ ] **Step 1: `StateTreeEditorAdapter.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/StateTreeSpec.h"
#include "CommandForgeTypes.h"

class UStateTree;

namespace UECommandForge
{
    class FStateTreeEditorAdapter
    {
    public:
        // StateTree 에셋 생성 (이미 존재하면 기존 에셋 반환)
        static UStateTree* CreateOrLoad(const FString& AssetPath,
                                         TArray<FCommandForgeError>& OutErrors);

        // State 추가 — 스파이크 결과에 맞춰 시그니처 확정
        static bool AddState(UStateTree* StateTree, const FString& StateName,
                             TArray<FCommandForgeError>& OutErrors);

        // Task 추가 — TaskType 화이트리스트: Wait, MoveTo, PlayMontage
        static bool AddTask(UStateTree* StateTree, const FString& StateName,
                            const FString& TaskType,
                            TArray<FCommandForgeError>& OutErrors);

        // 에셋 저장
        static bool Save(UStateTree* StateTree, TArray<FCommandForgeError>& OutErrors);
    };
}
```

- [ ] **Step 2: `StateTreeEditorAdapter.cpp` 작성**

스파이크에서 확인한 실제 API를 사용해 구현한다. 아래는 뼈대이며, 스파이크 결과로 채운다:

```cpp
#include "Builders/StateTreeEditorAdapter.h"
// TODO: 스파이크 완료 후 실제 StateTree Editor 헤더 include

namespace UECommandForge
{
    UStateTree* FStateTreeEditorAdapter::CreateOrLoad(const FString& AssetPath,
                                                       TArray<FCommandForgeError>& OutErrors)
    {
        // IAssetTools::CreateAsset 또는 LoadObject 사용
        // 스파이크에서 확인한 Factory 클래스 사용
        OutErrors.Add({ TEXT("NOT_IMPLEMENTED"), TEXT("스파이크 완료 후 구현"), TEXT("") });
        return nullptr;
    }

    bool FStateTreeEditorAdapter::AddState(UStateTree* StateTree, const FString& StateName,
                                            TArray<FCommandForgeError>& OutErrors)
    {
        // 스파이크에서 확인한 AddState API 사용
        OutErrors.Add({ TEXT("NOT_IMPLEMENTED"), TEXT("스파이크 완료 후 구현"), TEXT("") });
        return false;
    }

    bool FStateTreeEditorAdapter::AddTask(UStateTree* StateTree, const FString& StateName,
                                           const FString& TaskType,
                                           TArray<FCommandForgeError>& OutErrors)
    {
        // Wait, MoveTo, PlayMontage Task 클래스 로드 후 State에 추가
        OutErrors.Add({ TEXT("NOT_IMPLEMENTED"), TEXT("스파이크 완료 후 구현"), TEXT("") });
        return false;
    }

    bool FStateTreeEditorAdapter::Save(UStateTree* StateTree, TArray<FCommandForgeError>& OutErrors)
    {
        // UPackage::SavePackage 사용
        OutErrors.Add({ TEXT("NOT_IMPLEMENTED"), TEXT("스파이크 완료 후 구현"), TEXT("") });
        return false;
    }
}
```

**주의:** 스파이크 이전에는 `NOT_IMPLEMENTED` 오류를 반환하는 뼈대만 커밋한다. 빌드가 통과되는지 확인 후 실제 구현으로 교체한다.

- [ ] **Step 3: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/StateTreeEditorAdapter.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/StateTreeEditorAdapter.cpp
git commit -m "feat: StateTreeEditorAdapter 뼈대 (스파이크 대기)"
```

---

## Task 4: StateTreeBuilder (TDD)

- [ ] **Step 1: 실패하는 자동화 테스트 작성**

```cpp
// Tests/StateTreeBuilderTest.cpp
#include "Misc/AutomationTest.h"
#include "Builders/StateTreeBuilder.h"
#include "Specs/StateTreeSpec.h"
#include "CommandForgeTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStateTreeBuilderTest,
    "UECommandForge.Builders.StateTreeBuilder.CreatesStateTree",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStateTreeBuilderTest::RunTest(const FString& Parameters)
{
    FStateTreeSpec Spec;
    Spec.AssetPath = TEXT("/Game/Tests/ST_TestTree");
    Spec.Tasks.Add({ TEXT("Wait"),   TEXT("Idle") });
    Spec.Tasks.Add({ TEXT("MoveTo"), TEXT("Patrol") });

    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FStateTreeBuilder::Build(Spec, Errors);

    TestTrue(TEXT("빌드 성공"), bOk);
    TestTrue(TEXT("에러 없음"), Errors.IsEmpty());
    return true;
}
```

- [ ] **Step 2: `StateTreeBuilder.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/StateTreeSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FStateTreeBuilder
    {
    public:
        static bool Build(const FStateTreeSpec& Spec, TArray<FCommandForgeError>& OutErrors);
    };
}
```

- [ ] **Step 3: `StateTreeBuilder.cpp` 작성**

```cpp
#include "Builders/StateTreeBuilder.h"
#include "Builders/StateTreeEditorAdapter.h"

namespace UECommandForge
{
    bool FStateTreeBuilder::Build(const FStateTreeSpec& Spec, TArray<FCommandForgeError>& OutErrors)
    {
        UStateTree* ST = FStateTreeEditorAdapter::CreateOrLoad(Spec.AssetPath, OutErrors);
        if (!ST) { return false; }

        for (const FStateTreeTaskSpec& TaskSpec : Spec.Tasks)
        {
            if (!FStateTreeEditorAdapter::AddState(ST, TaskSpec.StateName, OutErrors)) { return false; }
            if (!FStateTreeEditorAdapter::AddTask(ST, TaskSpec.StateName, TaskSpec.TaskType, OutErrors)) { return false; }
        }

        return FStateTreeEditorAdapter::Save(ST, OutErrors);
    }
}
```

- [ ] **Step 4: 테스트 실행 — 스파이크 완료 후 통과 확인**

```bash
./tools/ue/run_automation_tests.sh
```

예상: 어댑터 구현 완료 후 `FStateTreeBuilderTest` PASS.

- [ ] **Step 5: 커밋**

```bash
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Builders/StateTreeBuilder.h \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Builders/StateTreeBuilder.cpp \
        Plugins/UECommandForge/Source/UECommandForgeEditor/Tests/StateTreeBuilderTest.cpp
git commit -m "feat: StateTreeBuilder + 자동화 테스트"
```

---

## Task 5: CreateStateTreeCommandlet + Shell Wrapper

- [ ] **Step 1: `CreateStateTreeCommandlet.h` 작성**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateStateTreeCommandlet.generated.h"

UCLASS()
class UCreateStateTreeCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCreateStateTreeCommandlet();
    virtual int32 Main(const FString& Params) override;
};
```

- [ ] **Step 2: `CreateStateTreeCommandlet.cpp` 작성**

Phase 3 Commandlet과 동일한 패턴. `FStateTreeBuilder::Build` 호출. `Report.Commandlet = TEXT("CreateStateTree")`.

- [ ] **Step 3: `tools/ue/create_statetree.sh` 작성**

```bash
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/run_commandlet.sh" CreateStateTree -SpecFile="$1" "$@"
```

- [ ] **Step 4: 실행 권한 부여 및 커밋**

```bash
chmod +x tools/ue/create_statetree.sh
git add Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateStateTreeCommandlet.* \
        tools/ue/create_statetree.sh
git commit -m "feat: CreateStateTreeCommandlet + Shell Wrapper"
```

---

## Phase 4 인수 조건

```bash
# 1. 플러그인 빌드
./tools/ue/build_plugin.sh

# 2. 자동화 테스트
./tools/ue/run_automation_tests.sh

# 3. StateTree 생성
./tools/ue/create_statetree.sh specs/examples/guard_ai.json
jq -e '.ok == true' "$(ls -t Saved/CodexReports/CreateStateTree_*.json | head -1)"
```

Phase 4 완료 조건: `ST_Guard.uasset`가 `Content/` 에 생성되고, `ok: true`가 Result JSON에 기록됨.
