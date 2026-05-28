#include "Misc/AutomationTest.h"
#include "Specs/CommandForgePolicyParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUECommandForgePolicyParserTest,
    "UECommandForge.Specs.CommandForgePolicyParser.ParsesJsonPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUECommandForgePolicyParserTest::RunTest(const FString& Parameters)
{
    const FString Json = TEXT(R"({
        "version": "1",
        "kind": "asset_policy",
        "rules": [
            { "asset_class": "Blueprint", "prefix": "BP_", "allowed_path": "/Game/AI" }
        ]
    })");

    FCommandForgePolicyDocument Policy;
    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FCommandForgePolicyParser::ParseJson(Json, Policy, Errors);

    TestTrue(TEXT("JSON policy parse succeeded"), bOk);
    TestEqual(TEXT("policy version"), Policy.Version, TEXT("1"));
    TestEqual(TEXT("policy kind"), Policy.Kind, TEXT("asset_policy"));
    TestTrue(TEXT("canonical JSON 기록"),
        Policy.CanonicalJson.Contains(TEXT("\"kind\":\"asset_policy\"")) &&
        Policy.CanonicalJson.Contains(TEXT("\"rules\"")));
    TestEqual(TEXT("parse errors empty"), Errors.Num(), 0);

    FCommandForgePolicyDocument InvalidPolicy;
    TArray<FCommandForgeError> InvalidErrors;
    const bool bInvalidOk = UECommandForge::FCommandForgePolicyParser::ParseJson(
        TEXT("kind: asset_policy\nversion: 1"), InvalidPolicy, InvalidErrors);

    TestFalse(TEXT("YAML-like input is rejected by JSON parser"), bInvalidOk);
    TestTrue(TEXT("invalid parse error emitted"), InvalidErrors.Num() > 0);
    return true;
}
