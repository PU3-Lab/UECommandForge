#include "Misc/AutomationTest.h"
#include "Specs/BlueprintBatchSpecParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlueprintBatchSpecParserTest,
    "UECommandForge.Specs.BlueprintBatchSpecParser.ParsesValidAndInvalidSpecs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintBatchSpecParserTest::RunTest(const FString& Parameters)
{
    // 1. 정상 케이스 파싱 검증
    {
        const FString ValidJson = TEXT(R"({
            "version": "1",
            "kind": "blueprint_batch",
            "transaction_id": "test_tx_123",
            "compile": true,
            "save": true,
            "items": [
                {
                    "asset_path": "/Game/AI/BP_TestChar",
                    "parent_class": "Character",
                    "allow_replace": true,
                    "components": [
                        { "name": "Mesh", "class": "SkeletalMeshComponent" }
                    ],
                    "defaults": [
                        { "property": "bUseControllerRotationYaw", "value": "true" }
                    ]
                }
            ]
        })");

        FBlueprintBatchSpec Spec;
        TArray<FCommandForgeError> Errors;
        const bool bOk = UECommandForge::FBlueprintBatchSpecParser::Parse(ValidJson, Spec, Errors);

        TestTrue(TEXT("정상 JSON 파싱 성공 확인"), bOk);
        TestEqual(TEXT("버전 일치 확인"), Spec.Version, TEXT("1"));
        TestEqual(TEXT("트랜잭션 ID 일치 확인"), Spec.TransactionId, TEXT("test_tx_123"));
        TestEqual(TEXT("아이템 갯수 확인"), Spec.Items.Num(), 1);
        TestEqual(TEXT("자식 컴포넌트 갯수 확인"), Spec.Items[0].Components.Num(), 1);
        TestEqual(TEXT("디폴트 속성 갯수 확인"), Spec.Items[0].Defaults.Num(), 1);
    }

    // 2. kind 불일치 오류 검증
    {
        const FString InvalidKindJson = TEXT(R"({
            "version": "1",
            "kind": "wrong_kind",
            "items": []
        })");

        FBlueprintBatchSpec Spec;
        TArray<FCommandForgeError> Errors;
        const bool bOk = UECommandForge::FBlueprintBatchSpecParser::Parse(InvalidKindJson, Spec, Errors);

        TestFalse(TEXT("잘못된 kind 파싱 실패 확인"), bOk);
        
        FString ErrMsg = TEXT("Errors: ");
        for (const auto& Err : Errors)
        {
            ErrMsg += FString::Printf(TEXT("[%s: %s] "), *Err.Code, *Err.Message);
        }
        TestTrue(FString::Printf(TEXT("2. kind 오류 목록 존재 확인 (실제오류: %s)"), *ErrMsg), Errors.Num() > 0);
        if (Errors.Num() > 0)
        {
            TestEqual(TEXT("2. 에러 코드 확인"), Errors[0].Code, TEXT("PLAN_KIND_MISMATCH"));
        }
    }

    // 3. version 누락/불일치 오류 검증
    {
        const FString InvalidVersionJson = TEXT(R"({
            "version": "2",
            "kind": "blueprint_batch",
            "items": []
        })");

        FBlueprintBatchSpec Spec;
        TArray<FCommandForgeError> Errors;
        const bool bOk = UECommandForge::FBlueprintBatchSpecParser::Parse(InvalidVersionJson, Spec, Errors);

        TestFalse(TEXT("잘못된 version 파싱 실패 확인"), bOk);
        
        FString ErrMsg = TEXT("Errors: ");
        for (const auto& Err : Errors)
        {
            ErrMsg += FString::Printf(TEXT("[%s: %s] "), *Err.Code, *Err.Message);
        }
        TestTrue(FString::Printf(TEXT("3. version 오류 목록 존재 확인 (실제오류: %s)"), *ErrMsg), Errors.Num() > 0);
        if (Errors.Num() > 0)
        {
            TestEqual(TEXT("3. 에러 코드 확인"), Errors[0].Code, TEXT("PLAN_VERSION_MISMATCH"));
        }
    }

    // 4. 필수 필드 누락 검증 (asset_path, parent_class)
    {
        const FString MissingFieldJson = TEXT(R"({
            "version": "1",
            "kind": "blueprint_batch",
            "items": [
                {
                    "allow_replace": true
                }
            ]
        })");

        FBlueprintBatchSpec Spec;
        TArray<FCommandForgeError> Errors;
        const bool bOk = UECommandForge::FBlueprintBatchSpecParser::Parse(MissingFieldJson, Spec, Errors);

        TestFalse(TEXT("필수 필드 누락 파싱 실패 확인"), bOk);
        
        FString ErrMsg = TEXT("Errors: ");
        for (const auto& Err : Errors)
        {
            ErrMsg += FString::Printf(TEXT("[%s: %s] "), *Err.Code, *Err.Message);
        }
        TestTrue(FString::Printf(TEXT("4. 필수필드 오류 목록 존재 확인 (실제오류: %s)"), *ErrMsg), Errors.Num() >= 2);
    }

    return true;
}
