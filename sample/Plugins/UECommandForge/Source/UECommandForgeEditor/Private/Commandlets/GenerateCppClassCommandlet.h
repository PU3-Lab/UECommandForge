#pragma once

#include "Commandlets/Commandlet.h"
#include "CommandForgeTypes.h"
#include "Specs/CppClassSpec.h"

struct FCommandForgeReport;

#include "GenerateCppClassCommandlet.generated.h"


UCLASS()
class UGenerateCppClassCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UGenerateCppClassCommandlet();
    virtual int32 Main(const FString& Params) override;

    // 배치 커맨드렛에서 재사용할 단일 C++ 클래스 생성 및 검증 헬퍼
    static bool GenerateSingleClass(
        const FCommandForgeCppClassSpec& Spec,
        bool bApply,
        bool bApplyBuildCs,
        FCommandForgeReport& OutReport,
        FString& OutHeaderPath,
        FString& OutSourcePath
    );
};
