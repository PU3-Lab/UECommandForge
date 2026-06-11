#include "Misc/AutomationTest.h"
#include "Commandlets/ValidateAssetRulesCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateAssetRulesCommandletTest,
    "UECommandForge.Commandlets.ValidateAssetRules.ReportsPolicyIssues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool LoadValidateAssetRulesJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }
}

bool FValidateAssetRulesCommandletTest::RunTest(const FString& Parameters)
{
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_validate_asset_rules.json"));
    const FString PolicyPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_asset_policy.json"));

    IFileManager::Get().Delete(*ReportPath);
    IFileManager::Get().Delete(*PolicyPath);

    const FString PolicyJson = TEXT(R"({
        "version": "1",
        "kind": "asset_policy",
        "rules": [
            {
                "asset_class": "/Script/Engine.Blueprint",
                "prefix": "AI_",
                "allowed_path": "/Game/AI"
            },
            {
                "asset_class": "/Script/StateTreeModule.StateTree",
                "prefix": "ST_",
                "allowed_path": "/Game/Tests"
            }
        ],
        "unused_asset_candidates": {
            "enabled": true,
            "severity": "warning"
        },
        "exceptions": [
            {
                "package_name": "/Game/Tests/Workflow/TestMap",
                "reason": "test fixture map"
            }
        ]
    })");
    TestTrue(TEXT("policy fixture 저장"), FFileHelper::SaveStringToFile(PolicyJson, *PolicyPath));

    UValidateAssetRulesCommandlet* Commandlet = NewObject<UValidateAssetRulesCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Policy=\"%s\" -Output=\"%s\" -RootPaths=\"/Game/Tests\""),
        *PolicyPath, *ReportPath));

    TestEqual(TEXT("policy violation exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("asset rules report JSON 파싱"), LoadValidateAssetRulesJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("policy report ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("commandlet 이름"), RootObject->GetStringField(TEXT("commandlet")),
        TEXT("ValidateAssetRules"));

    const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
    TestTrue(TEXT("issues 배열 존재"), RootObject->TryGetArrayField(TEXT("issues"), Issues));
    TestTrue(TEXT("policy issues 발생"), Issues != nullptr && Issues->Num() > 0);

    bool bFoundPrefixIssue = false;
    bool bFoundPathIssue = false;
    bool bFoundUnusedCandidate = false;
    for (const TSharedPtr<FJsonValue>& IssueValue : *Issues)
    {
        const TSharedPtr<FJsonObject>* IssueObject = nullptr;
        if (!TestTrue(TEXT("issue object 파싱"), IssueValue->TryGetObject(IssueObject)) ||
            !TestTrue(TEXT("issue object 유효"), IssueObject != nullptr && IssueObject->IsValid()))
        {
            return false;
        }

        const FString Code = (*IssueObject)->GetStringField(TEXT("code"));
        const FString AssetPath = (*IssueObject)->GetStringField(TEXT("asset_path"));
        if (Code == TEXT("ASSET_PREFIX_MISMATCH") && AssetPath.Contains(TEXT("BP_TestCharacter")))
        {
            bFoundPrefixIssue = true;
        }
        if (Code == TEXT("ASSET_PATH_NOT_ALLOWED") && AssetPath.Contains(TEXT("BP_TestCharacter")))
        {
            bFoundPathIssue = true;
        }
        if (Code == TEXT("UNUSED_ASSET_CANDIDATE"))
        {
            bFoundUnusedCandidate = true;
        }
    }

    TestTrue(TEXT("prefix policy issue"), bFoundPrefixIssue);
    TestTrue(TEXT("path policy issue"), bFoundPathIssue);
    TestTrue(TEXT("unused candidate warning"), bFoundUnusedCandidate);
    return true;
}
