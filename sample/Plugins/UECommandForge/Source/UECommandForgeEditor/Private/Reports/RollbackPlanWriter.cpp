#include "Reports/RollbackPlanWriter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace UECommandForge
{
    bool FRollbackPlanWriter::Write(const FString& OutPath, const FCommandForgeRollbackPlan& Plan)
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("transaction_id"), Plan.TransactionId);
        Root->SetStringField(TEXT("commandlet"), Plan.Commandlet);
        Root->SetStringField(TEXT("timestamp"), Plan.Timestamp);

        auto ToArray = [](const TArray<FString>& In)
        {
            TArray<TSharedPtr<FJsonValue>> Out;
            for (const FString& Value : In)
            {
                Out.Add(MakeShared<FJsonValueString>(Value));
            }
            return Out;
        };

        auto ToObject = [](const TMap<FString, FString>& In)
        {
            TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
            for (const auto& Kv : In)
            {
                Out->SetStringField(Kv.Key, Kv.Value);
            }
            return Out;
        };

        TArray<TSharedPtr<FJsonValue>> OperationValues;
        for (const FCommandForgeRollbackOperation& Operation : Plan.Operations)
        {
            TSharedRef<FJsonObject> OperationObject = MakeShared<FJsonObject>();
            OperationObject->SetStringField(TEXT("planned_operation"), Operation.PlannedOperation);
            OperationObject->SetStringField(TEXT("before_asset_path"), Operation.BeforeAssetPath);
            OperationObject->SetStringField(TEXT("before_file_path"), Operation.BeforeFilePath);
            OperationObject->SetStringField(TEXT("after_asset_path"), Operation.AfterAssetPath);
            OperationObject->SetStringField(TEXT("after_file_path"), Operation.AfterFilePath);
            OperationObject->SetArrayField(TEXT("dependency_snapshot"), ToArray(Operation.DependencySnapshot));
            OperationObject->SetObjectField(TEXT("validation_summary"), ToObject(Operation.ValidationSummary));
            OperationValues.Add(MakeShared<FJsonValueObject>(OperationObject));
        }
        Root->SetArrayField(TEXT("operations"), OperationValues);

        FString Serialized;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(Root, Writer);

        const FString Dir = FPaths::GetPath(OutPath);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
        return FFileHelper::SaveStringToFile(Serialized, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}
