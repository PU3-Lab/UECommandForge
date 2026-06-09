#pragma once

#include "CoreMinimal.h"
#include "Specs/CppClassSpec.h"
#include "CppClassBatchSpec.generated.h"

USTRUCT()
struct UECOMMANDFORGEEDITOR_API FCppClassBatchSpec
{
    GENERATED_BODY()

    UPROPERTY() FString Version;
    UPROPERTY() FString Kind;
    UPROPERTY() FString TransactionId;

    // 배치 생성할 C++ 클래스 정보들의 배열
    UPROPERTY() TArray<FCommandForgeCppClassInfoSpec> Classes;

    // 공통적으로 적용할 include 헤더들
    UPROPERTY() TArray<FString> Includes;

    // 공통 속성들
    UPROPERTY() TArray<FCommandForgeCppPropertySpec> Properties;

    // 공통 함수들
    UPROPERTY() TArray<FCommandForgeCppFunctionSpec> Functions;

    // 공통 빌드 디펜던시
    UPROPERTY() FCommandForgeCppBuildDependencySpec BuildDependencies;
};
