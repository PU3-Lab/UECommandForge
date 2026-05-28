#include "Reports/JsonReportWriter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace UECommandForge
{
    bool FJsonReportWriter::Write(const FString& OutPath, const FCommandForgeReport& Report)
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetBoolField(TEXT("ok"), Report.bOk);
        Root->SetStringField(TEXT("commandlet"), Report.Commandlet);
        Root->SetStringField(TEXT("transaction_id"), Report.TransactionId);
        Root->SetBoolField(TEXT("dry_run"), Report.bDryRun);
        Root->SetBoolField(TEXT("applied"), Report.bApplied);
        Root->SetBoolField(TEXT("rollback_available"), Report.bRollbackAvailable);
        Root->SetStringField(TEXT("rollback_plan_path"), Report.RollbackPlanPath);

        auto ToArray = [](const TArray<FString>& In)
        {
            TArray<TSharedPtr<FJsonValue>> Out;
            for (const FString& S : In) { Out.Add(MakeShared<FJsonValueString>(S)); }
            return Out;
        };
        Root->SetArrayField(TEXT("created_assets"),  ToArray(Report.CreatedAssets));
        Root->SetArrayField(TEXT("modified_assets"), ToArray(Report.ModifiedAssets));
        Root->SetArrayField(TEXT("changed_assets"),  ToArray(Report.ChangedAssets));
        Root->SetArrayField(TEXT("changed_files"),   ToArray(Report.ChangedFiles));
        Root->SetArrayField(TEXT("next_suggestions"), ToArray(Report.NextSuggestions));

        TArray<TSharedPtr<FJsonValue>> AssetValues;
        for (const FCommandForgeAssetSnapshotRecord& Asset : Report.Assets)
        {
            TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("asset_name"), Asset.AssetName);
            O->SetStringField(TEXT("package_path"), Asset.PackagePath);
            O->SetStringField(TEXT("object_path"), Asset.ObjectPath);
            O->SetStringField(TEXT("asset_class"), Asset.AssetClass);
            O->SetStringField(TEXT("package_name"), Asset.PackageName);
            O->SetStringField(TEXT("disk_path"), Asset.DiskPath);
            O->SetBoolField(TEXT("is_redirector"), Asset.bIsRedirector);
            O->SetBoolField(TEXT("package_dirty"), Asset.bPackageDirty);
            O->SetArrayField(TEXT("dependencies"), ToArray(Asset.Dependencies));
            O->SetArrayField(TEXT("referencers"), ToArray(Asset.Referencers));
            AssetValues.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("assets"), AssetValues);

        auto ToObject = [](const TMap<FString, FString>& In)
        {
            TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
            for (const auto& Kv : In)
            {
                Out->SetStringField(Kv.Key, Kv.Value);
            }
            return Out;
        };
        Root->SetObjectField(TEXT("validation"), ToObject(Report.Validation));
        Root->SetObjectField(TEXT("post_validation"), ToObject(Report.PostValidation));

        TArray<TSharedPtr<FJsonValue>> IssueValues;
        for (const FCommandForgeValidationIssue& Issue : Report.ValidationIssues)
        {
            TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("severity"), Issue.Severity);
            O->SetStringField(TEXT("code"), Issue.Code);
            O->SetStringField(TEXT("message"), Issue.Message);
            O->SetStringField(TEXT("field"), Issue.Field);
            O->SetStringField(TEXT("asset_path"), Issue.AssetPath);
            O->SetStringField(TEXT("file_path"), Issue.FilePath);
            O->SetStringField(TEXT("suggested_fix"), Issue.SuggestedFix);
            IssueValues.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("issues"), IssueValues);

        TArray<TSharedPtr<FJsonValue>> ErrorValues;
        for (const FCommandForgeError& E : Report.Errors)
        {
            TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("code"),    E.Code);
            O->SetStringField(TEXT("message"), E.Message);
            O->SetStringField(TEXT("field"),   E.Field);
            ErrorValues.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("errors"), ErrorValues);

        TArray<TSharedPtr<FJsonValue>> StepValues;
        for (const FCommandForgeStepResult& Step : Report.Steps)
        {
            TSharedRef<FJsonObject> StepObject = MakeShared<FJsonObject>();
            StepObject->SetStringField(TEXT("step"), Step.Step);
            StepObject->SetBoolField(TEXT("ok"), Step.bOk);
            StepObject->SetArrayField(TEXT("created_assets"), ToArray(Step.CreatedAssets));

            TSharedRef<FJsonObject> StepValidation = MakeShared<FJsonObject>();
            for (const auto& Kv : Step.Validation)
            {
                StepValidation->SetStringField(Kv.Key, Kv.Value);
            }
            StepObject->SetObjectField(TEXT("validation"), StepValidation);

            TArray<TSharedPtr<FJsonValue>> StepErrors;
            for (const FCommandForgeError& E : Step.Errors)
            {
                TSharedRef<FJsonObject> ErrorObject = MakeShared<FJsonObject>();
                ErrorObject->SetStringField(TEXT("code"), E.Code);
                ErrorObject->SetStringField(TEXT("message"), E.Message);
                ErrorObject->SetStringField(TEXT("field"), E.Field);
                StepErrors.Add(MakeShared<FJsonValueObject>(ErrorObject));
            }
            StepObject->SetArrayField(TEXT("errors"), StepErrors);
            StepValues.Add(MakeShared<FJsonValueObject>(StepObject));
        }
        Root->SetArrayField(TEXT("steps"), StepValues);

        FString Serialized;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(Root, Writer);

        const FString Dir = FPaths::GetPath(OutPath);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
        return FFileHelper::SaveStringToFile(Serialized, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}
