#include "Misc/AutomationTest.h"
#include "Specs/CppClassBatchSpec.h"
#include "Specs/CppClassBatchSpecParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCppClassBatchSpecParserTest,
    "UECommandForge.Specs.CppClassBatchSpecParser.ParsesValidAndInvalidSpecs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCppClassBatchSpecParserTest::RunTest(const FString& Parameters)
{
    // 1. 정상 케이스 파싱 검증 (2개 클래스를 가짐)
    {
        const FString ValidJson = TEXT(R"({
            "version": "1",
            "kind": "cpp_class_batch",
            "transaction_id": "tx-test-batch-123",
            "classes": [
                {
                    "name": "Weapon",
                    "type": "Actor",
                    "module": "UECommandForgeRuntime",
                    "output_path": "Generated/Weapons",
                    "base_class": "AActor",
                    "blueprint_type": true,
                    "tick": false
                },
                {
                    "name": "Armor",
                    "type": "Actor",
                    "module": "UECommandForgeRuntime",
                    "output_path": "Generated/Armors",
                    "base_class": "AActor",
                    "blueprint_type": true,
                    "tick": false
                }
            ],
            "includes": ["CoreMinimal.h"],
            "properties": [
                {
                    "name": "ItemPrice",
                    "type": "int32",
                    "default": "100",
                    "metadata": {
                        "EditAnywhere": true,
                        "Category": "Item"
                    }
                }
            ],
            "functions": [
                {
                    "name": "UseItem",
                    "return_type": "void",
                    "metadata": {
                        "BlueprintCallable": true
                    },
                    "params": []
                }
            ],
            "build_dependencies": {
                "public": ["Engine"],
                "private": ["CoreUObject"]
            }
        })");

        FCppClassBatchSpec Spec;
        TArray<FCommandForgeError> Errors;
        const bool bOk = UECommandForge::FCppClassBatchSpecParser::Parse(ValidJson, Spec, Errors);

        TestTrue(TEXT("정상 JSON 파싱 성공 확인"), bOk);
        TestEqual(TEXT("버전 일치 확인"), Spec.Version, TEXT("1"));
        TestEqual(TEXT("트랜잭션 ID 일치 확인"), Spec.TransactionId, TEXT("tx-test-batch-123"));
        TestEqual(TEXT("클래스 개수 확인"), Spec.Classes.Num(), 2);
        TestEqual(TEXT("첫번째 클래스 이름 확인"), Spec.Classes[0].Name, TEXT("Weapon"));
        TestEqual(TEXT("두번째 클래스 이름 확인"), Spec.Classes[1].Name, TEXT("Armor"));
        TestEqual(TEXT("공통 includes 개수 확인"), Spec.Includes.Num(), 1);
        TestEqual(TEXT("공통 properties 개수 확인"), Spec.Properties.Num(), 1);
        TestEqual(TEXT("공통 functions 개수 확인"), Spec.Functions.Num(), 1);
        TestEqual(TEXT("공통 public 디펜던시 개수 확인"), Spec.BuildDependencies.PublicDependencies.Num(), 1);
    }

    // 2. kind 불일치 오류 검증
    {
        const FString InvalidKindJson = TEXT(R"({
            "version": "1",
            "kind": "wrong_kind",
            "classes": [
                {
                    "name": "Weapon",
                    "type": "Actor",
                    "module": "UECommandForgeRuntime",
                    "output_path": "Generated/Weapons"
                }
            ]
        })");

        FCppClassBatchSpec Spec;
        TArray<FCommandForgeError> Errors;
        const bool bOk = UECommandForge::FCppClassBatchSpecParser::Parse(InvalidKindJson, Spec, Errors);

        TestFalse(TEXT("잘못된 kind 파싱 실패 확인"), bOk);
        TestTrue(TEXT("에러 발생 확인"), Errors.Num() > 0);
    }

    // 3. classes 빈 배열 오류 검증
    {
        const FString EmptyClassesJson = TEXT(R"({
            "version": "1",
            "kind": "cpp_class_batch",
            "classes": []
        })");

        FCppClassBatchSpec Spec;
        TArray<FCommandForgeError> Errors;
        const bool bOk = UECommandForge::FCppClassBatchSpecParser::Parse(EmptyClassesJson, Spec, Errors);

        TestFalse(TEXT("classes 빈 배열 파싱 실패 확인"), bOk);
        TestTrue(TEXT("에러 발생 확인"), Errors.Num() > 0);
    }

    return true;
}
