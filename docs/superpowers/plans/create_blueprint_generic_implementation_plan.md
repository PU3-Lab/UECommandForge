# 범용 Blueprint 생성 (CreateBlueprint) MVP 구현 계획

UECommandForge에 일반 Actor 기반 Blueprint를 생성할 수 있는 범용 Blueprint 생성 명령(`CreateBlueprint`)을 추가합니다. 

## 사용자 리뷰 필요 사항

> [!IMPORTANT]
> 1. **기존 에셋 덮어쓰기 금지 (`allow_replace`)**: `allow_replace` 속성이 `false`일 때 기존 에셋이 존재한다면 즉시 에러(`BP_ALREADY_EXISTS`)를 반환하고 실패합니다. 
> 2. **제한된 Parent Class 지원**: 1차 MVP 단계에서는 `/Script/Engine.Actor`를 기본값으로 하며, `AActor` 하위의 Blueprintable 클래스만 허용합니다. `WidgetBlueprint`, `AnimBlueprint`, `BlueprintFunctionLibrary`, `ActorComponent` 등은 생성 대상에서 제외됩니다.
> 3. **Unreal Python 사용 배제**: C++ Commandlet과 `FKismetEditorUtilities::CreateBlueprint`를 활용하여 안전하고 원자적인 트랜잭션을 보장합니다.

---

## 제안된 변경 사항

### 1. Runtime 모듈 변경

#### [NEW] [BlueprintCreateSpec.h](file:///Users/kyungpyokim/Workspaces/project-lab/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeRuntime/Public/Specs/BlueprintCreateSpec.h)
범용 Blueprint 생성 스펙을 담는 C++ USTRUCT 정의.
```cpp
#pragma once

#include "CoreMinimal.h"
#include "BlueprintCreateSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGERUNTIME_API FCommandForgeBlueprintCreateSpec
{
    GENERATED_BODY()

    UPROPERTY()
    FString Version;

    UPROPERTY()
    FString Kind;

    UPROPERTY()
    FString TransactionId;

    UPROPERTY()
    FString AssetPath;

    UPROPERTY()
    FString ParentClass = TEXT("/Script/Engine.Actor");

    UPROPERTY()
    bool bCompile = true;

    UPROPERTY()
    bool bSave = true;

    UPROPERTY()
    bool bAllowReplace = false;
};
```

---

### 2. Editor 모듈 변경

#### [NEW] [BlueprintCreateSpecParser.h](file:///Users/kyungpyokim/Workspaces/project-lab/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Public/Specs/BlueprintCreateSpecParser.h)
JSON 데이터를 C++ 스펙 구조체로 파싱하는 파서 클래스 선언.
```cpp
#pragma once
#include "CoreMinimal.h"
#include "Specs/BlueprintCreateSpec.h"
#include "CommandForgeTypes.h"

namespace UECommandForge
{
    class FBlueprintCreateSpecParser
    {
    public:
        static bool Parse(const FString& Json, FCommandForgeBlueprintCreateSpec& OutSpec,
                          TArray<FCommandForgeError>& OutErrors);
    };
}
```

#### [NEW] [BlueprintCreateSpecParser.cpp](file:///Users/kyungpyokim/Workspaces/project-lab/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Specs/BlueprintCreateSpecParser.cpp)
`JsonObjectConverter`를 이용해 파싱하고, 필수 필드(`asset_path`, `parent_class`)의 존재 여부를 검증.
```cpp
#include "Specs/BlueprintCreateSpecParser.h"
#include "JsonObjectConverter.h"

namespace UECommandForge
{
    bool FBlueprintCreateSpecParser::Parse(const FString& Json, FCommandForgeBlueprintCreateSpec& OutSpec,
                                           TArray<FCommandForgeError>& OutErrors)
    {
        if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutSpec, 0, 0))
        {
            FCommandForgeError Err;
            Err.Code    = TEXT("PARSE_FAILED");
            Err.Message = TEXT("JSON을 FCommandForgeBlueprintCreateSpec으로 변환하지 못했습니다.");
            OutErrors.Add(Err);
            return false;
        }

        if (OutSpec.AssetPath.IsEmpty())
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("asset_path는 필수 필드입니다."), TEXT("asset_path") });
        }
        if (OutSpec.ParentClass.IsEmpty())
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("parent_class는 필수 필드입니다."), TEXT("parent_class") });
        }

        return OutErrors.IsEmpty();
    }
}
```

#### [NEW] [CreateBlueprintCommandlet.h](file:///Users/kyungpyokim/Workspaces/project-lab/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateBlueprintCommandlet.h)
범용 Blueprint 생성을 위한 Commandlet 선언.
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateBlueprintCommandlet.generated.h"

UCLASS()
class UCreateBlueprintCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCreateBlueprintCommandlet();
    virtual int32 Main(const FString& Params) override;
};
```

#### [NEW] [CreateBlueprintCommandlet.cpp](file:///Users/kyungpyokim/Workspaces/project-lab/UECommandForge/sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/CreateBlueprintCommandlet.cpp)
커맨드렛의 메인 로직 구현.
- JSON 로드 및 `FBlueprintCreateSpecParser`로 파싱
- 부모 클래스가 올바르게 로드되는지, Blueprintable 클래스인지 검증
- `allow_replace` 조건 확인:
  - 이미 에셋 파일이 디스크 또는 메모리에 존재하고 `allow_replace`가 false인 경우 오류 반환
  - 존재하지만 `allow_replace`가 true인 경우 `FGenericBlueprintBuilder::DeleteExistingBlueprint`를 호출하여 강제 삭제 후 진행
- `FKismetEditorUtilities::CreateBlueprint` 호출하여 신규 생성
- 생성된 에셋에 대해 `bCompile`, `bSave` 플래그에 따라 `FBlueprintValidationRunner` 컴파일 및 저장 실행
- JSON 결과 포맷을 맞춰 Output 경로에 기록

---

### 3. CLI 래퍼 스크립트

#### [NEW] [create_blueprint.sh](file:///Users/kyungpyokim/Workspaces/project-lab/UECommandForge/tools/ue/create_blueprint.sh)
리눅스/macOS 쉘 스크립트 래퍼. Commandlet을 호출하고 입력 파라미터를 넘겨줍니다.

#### [NEW] [create_blueprint.bat](file:///Users/kyungpyokim/Workspaces/project-lab/UECommandForge/tools/ue/create_blueprint.bat)
Windows 배치 파일 래퍼.

---

### 4. 테스트 자산 및 Smoke Test 스크립트

#### [NEW] [actor_blueprint.json](file:///Users/kyungpyokim/Workspaces/project-lab/UECommandForge/specs/examples/actor_blueprint.json)
테스트를 위한 예제 Spec JSON 파일.
```json
{
  "version": "1",
  "kind": "blueprint",
  "transaction_id": "tx-create-actor-blueprint",
  "asset_path": "/Game/Actors/BP_Door",
  "parent_class": "/Script/Engine.Actor",
  "compile": true,
  "save": true,
  "allow_replace": false
}
```

#### [NEW] [create_blueprint_generic.sh](file:///Users/kyungpyokim/Workspaces/project-lab/UECommandForge/tools/test/smoke/create_blueprint_generic.sh)
MVP 동작을 검증하기 위한 쉘 스크립트.
- 정상 케이스 생성 테스트
- 중복 생성 시 `allow_replace=false`에 따른 실패 케이스 테스트
- `allow_replace=true` 덮어쓰기 성공 케이스 테스트
- 잘못된 부모 클래스 전달 시 실패 케이스 테스트

---

## 검증 계획

### 1. 자동화 테스트
- `tools/test/smoke/create_blueprint_generic.sh` 실행
- 테스트 결과 JSON의 `ok`, `created_assets`, `validation` 필드가 설계 스펙과 일치하는지 확인

### 2. 수동 확인
- 생성된 에셋의 디스크 저장 위치(`.uasset`) 확인
- Unreal Editor 상에서 생성된 Blueprint 에셋을 열어 부모 클래스 확인
