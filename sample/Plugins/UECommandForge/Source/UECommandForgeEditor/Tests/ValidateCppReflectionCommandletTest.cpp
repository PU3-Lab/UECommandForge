#include "Misc/AutomationTest.h"
#include "Commandlets/ValidateCppReflectionCommandlet.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateCppReflectionPassesCleanHeaderTest,
    "UECommandForge.Commandlets.ValidateCppReflection.PassesCleanHeader",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateCppReflectionReportsPolicyIssuesTest,
    "UECommandForge.Commandlets.ValidateCppReflection.ReportsPolicyIssues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FValidateCppReflectionRejectsUnsafeIncludePathTest,
    "UECommandForge.Commandlets.ValidateCppReflection.RejectsUnsafeIncludePath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    FString ReflectionTestRoot()
    {
        return FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CodexReports"), TEXT("ReflectionPolicy")));
    }

    bool LoadReflectionJsonObject(const FString& Path, TSharedPtr<FJsonObject>& OutObject)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            return false;
        }

        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
    }

    bool SaveTextFile(const FString& Path, const FString& Content)
    {
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path));
        return FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool SaveReflectionPolicy(const FString& Path, const FString& HeaderPath)
    {
        const FString PolicyJson = FString::Printf(TEXT(R"({
            "version": "1",
            "kind": "cpp_reflection_policy",
            "files": [
                {
                    "path": "%s",
                    "role": "ActorComponent"
                }
            ]
        })"), *HeaderPath.Replace(TEXT("\\"), TEXT("\\\\")));
        return SaveTextFile(Path, PolicyJson);
    }

    bool HasIssueCode(const TSharedPtr<FJsonObject>& RootObject, const FString& Code)
    {
        const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
        if (!RootObject->TryGetArrayField(TEXT("issues"), Issues) || Issues == nullptr)
        {
            return false;
        }

        for (const TSharedPtr<FJsonValue>& IssueValue : *Issues)
        {
            const TSharedPtr<FJsonObject>* IssueObject = nullptr;
            if (IssueValue->TryGetObject(IssueObject) && IssueObject != nullptr &&
                (*IssueObject)->GetStringField(TEXT("code")) == Code)
            {
                return true;
            }
        }
        return false;
    }

    bool HasErrorCode(const TSharedPtr<FJsonObject>& RootObject, const FString& Code)
    {
        const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
        if (!RootObject->TryGetArrayField(TEXT("errors"), Errors) || Errors == nullptr)
        {
            return false;
        }

        for (const TSharedPtr<FJsonValue>& ErrorValue : *Errors)
        {
            const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
            if (ErrorValue->TryGetObject(ErrorObject) && ErrorObject != nullptr &&
                (*ErrorObject)->GetStringField(TEXT("code")) == Code)
            {
                return true;
            }
        }
        return false;
    }
}

bool FValidateCppReflectionPassesCleanHeaderTest::RunTest(const FString& Parameters)
{
    const FString HeaderPath = FPaths::Combine(ReflectionTestRoot(), TEXT("CleanReflectionComponent.h"));
    const FString PolicyPath = FPaths::Combine(ReflectionTestRoot(), TEXT("clean_reflection_policy.json"));
    const FString ReportPath = FPaths::Combine(ReflectionTestRoot(), TEXT("clean_reflection_report.json"));
    const FString Header = TEXT(R"(#pragma once

#include "Components/ActorComponent.h"
#include "CleanReflectionComponent.generated.h"

UCLASS(BlueprintType, Config=Game)
class UCleanReflectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stats")
    float MaxHealth = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USceneComponent> AnchorComponent;

    UPROPERTY(Config, EditDefaultsOnly, Category="Config")
    float ConfiguredScale = 1.0f;

    UFUNCTION(BlueprintCallable, Category="Stats")
    void ApplyDamage(float Amount);
};
)");

    IFileManager::Get().Delete(*HeaderPath);
    IFileManager::Get().Delete(*PolicyPath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("clean reflection header 저장"), SaveTextFile(HeaderPath, Header));
    TestTrue(TEXT("clean reflection policy 저장"), SaveReflectionPolicy(PolicyPath, HeaderPath));

    UValidateCppReflectionCommandlet* Commandlet = NewObject<UValidateCppReflectionCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Policy=\"%s\" -Output=\"%s\""), *PolicyPath, *ReportPath));

    TestEqual(TEXT("clean reflection exit code"), ExitCode, 0);

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("clean reflection report JSON 파싱"), LoadReflectionJsonObject(ReportPath, RootObject));
    TestTrue(TEXT("clean reflection ok true"), RootObject->GetBoolField(TEXT("ok")));
    TestEqual(TEXT("clean reflection no issues"), RootObject->GetArrayField(TEXT("issues")).Num(), 0);
    return true;
}

bool FValidateCppReflectionReportsPolicyIssuesTest::RunTest(const FString& Parameters)
{
    const FString HeaderPath = FPaths::Combine(ReflectionTestRoot(), TEXT("BrokenReflectionComponent.h"));
    const FString PolicyPath = FPaths::Combine(ReflectionTestRoot(), TEXT("broken_reflection_policy.json"));
    const FString ReportPath = FPaths::Combine(ReflectionTestRoot(), TEXT("broken_reflection_report.json"));
    const FString Header = TEXT(R"(#pragma once

#include "Components/ActorComponent.h"
#include "BrokenReflectionComponent.generated.h"

UCLASS(BlueprintType)
class UBrokenReflectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RuntimeHealth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Components")
    TObjectPtr<USceneComponent> AnchorComponent;

    UPROPERTY(Config, EditDefaultsOnly, Category="Config")
    float ConfiguredScale = 1.0f;

    UFUNCTION(BlueprintCallable)
    void ApplyDamage(float Amount);

    UPROPERTY(BlueprintReadOnly, Category="Broken")

    UFUNCTION(BlueprintCallable, Category="Broken")
};
)");

    IFileManager::Get().Delete(*HeaderPath);
    IFileManager::Get().Delete(*PolicyPath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("broken reflection header 저장"), SaveTextFile(HeaderPath, Header));
    TestTrue(TEXT("broken reflection policy 저장"), SaveReflectionPolicy(PolicyPath, HeaderPath));

    UValidateCppReflectionCommandlet* Commandlet = NewObject<UValidateCppReflectionCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Policy=\"%s\" -Output=\"%s\""), *PolicyPath, *ReportPath));

    TestEqual(TEXT("broken reflection exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("broken reflection report JSON 파싱"), LoadReflectionJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("broken reflection ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestTrue(TEXT("missing property category issue"),
        HasIssueCode(RootObject, TEXT("CPP_REFLECTION_PROPERTY_CATEGORY_MISSING")));
    TestTrue(TEXT("missing function category issue"),
        HasIssueCode(RootObject, TEXT("CPP_REFLECTION_FUNCTION_CATEGORY_MISSING")));
    TestTrue(TEXT("runtime mutable state issue"),
        HasIssueCode(RootObject, TEXT("CPP_REFLECTION_RUNTIME_MUTABLE_EDITABLE")));
    TestTrue(TEXT("component visible anywhere issue"),
        HasIssueCode(RootObject, TEXT("CPP_REFLECTION_COMPONENT_NOT_VISIBLEANYWHERE")));
    TestTrue(TEXT("config class issue"),
        HasIssueCode(RootObject, TEXT("CPP_REFLECTION_CONFIG_WITHOUT_CONFIG_CLASS")));
    TestTrue(TEXT("macro declaration issue"),
        HasIssueCode(RootObject, TEXT("CPP_REFLECTION_MACRO_WITHOUT_DECLARATION")));
    return true;
}

bool FValidateCppReflectionRejectsUnsafeIncludePathTest::RunTest(const FString& Parameters)
{
    const FString PolicyPath = FPaths::Combine(ReflectionTestRoot(), TEXT("unsafe_include_reflection_policy.json"));
    const FString ReportPath = FPaths::Combine(ReflectionTestRoot(), TEXT("unsafe_include_reflection_report.json"));
    const FString PolicyJson = TEXT(R"({
        "version": "1",
        "kind": "cpp_reflection_policy",
        "modules": [
            {
                "name": "UECommandForgeRuntime",
                "include_paths": ["../Private"]
            }
        ]
    })");

    IFileManager::Get().Delete(*PolicyPath);
    IFileManager::Get().Delete(*ReportPath);
    TestTrue(TEXT("unsafe include reflection policy 저장"), SaveTextFile(PolicyPath, PolicyJson));

    UValidateCppReflectionCommandlet* Commandlet = NewObject<UValidateCppReflectionCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Policy=\"%s\" -Output=\"%s\""), *PolicyPath, *ReportPath));

    TestEqual(TEXT("unsafe include reflection exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::SpecParseFailed));

    TSharedPtr<FJsonObject> RootObject;
    TestTrue(TEXT("unsafe include reflection report JSON 파싱"), LoadReflectionJsonObject(ReportPath, RootObject));
    TestFalse(TEXT("unsafe include reflection ok false"), RootObject->GetBoolField(TEXT("ok")));
    TestTrue(TEXT("unsafe include path error"),
        HasErrorCode(RootObject, TEXT("INVALID_INCLUDE_PATH")));
    return true;
}
