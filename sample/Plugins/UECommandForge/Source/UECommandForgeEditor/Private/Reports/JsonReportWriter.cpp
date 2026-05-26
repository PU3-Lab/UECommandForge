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

        auto ToArray = [](const TArray<FString>& In)
        {
            TArray<TSharedPtr<FJsonValue>> Out;
            for (const FString& S : In) { Out.Add(MakeShared<FJsonValueString>(S)); }
            return Out;
        };
        Root->SetArrayField(TEXT("created_assets"),  ToArray(Report.CreatedAssets));
        Root->SetArrayField(TEXT("modified_assets"), ToArray(Report.ModifiedAssets));
        Root->SetArrayField(TEXT("next_suggestions"), ToArray(Report.NextSuggestions));

        TSharedRef<FJsonObject> Validation = MakeShared<FJsonObject>();
        for (const auto& Kv : Report.Validation)
        {
            Validation->SetStringField(Kv.Key, Kv.Value);
        }
        Root->SetObjectField(TEXT("validation"), Validation);

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

        FString Serialized;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(Root, Writer);

        const FString Dir = FPaths::GetPath(OutPath);
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
        return FFileHelper::SaveStringToFile(Serialized, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}
