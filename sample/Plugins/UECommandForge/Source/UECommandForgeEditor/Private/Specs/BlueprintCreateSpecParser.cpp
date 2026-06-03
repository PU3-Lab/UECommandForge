#include "Specs/BlueprintCreateSpecParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace UECommandForge
{
    bool FBlueprintCreateSpecParser::Parse(const FString& Json, FCommandForgeBlueprintCreateSpec& OutSpec,
                                           TArray<FCommandForgeError>& OutErrors)
    {
        const int32 InitialErrorCount = OutErrors.Num();

        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            FCommandForgeError Err;
            Err.Code    = TEXT("PARSE_FAILED");
            Err.Message = TEXT("JSON 파싱 실패");
            OutErrors.Add(Err);
            return false;
        }

        Root->TryGetStringField(TEXT("version"), OutSpec.Version);
        Root->TryGetStringField(TEXT("kind"), OutSpec.Kind);
        Root->TryGetStringField(TEXT("transaction_id"), OutSpec.TransactionId);
        Root->TryGetStringField(TEXT("asset_path"), OutSpec.AssetPath);
        Root->TryGetStringField(TEXT("parent_class"), OutSpec.ParentClass);
        Root->TryGetBoolField(TEXT("compile"), OutSpec.bCompile);
        Root->TryGetBoolField(TEXT("save"), OutSpec.bSave);
        Root->TryGetBoolField(TEXT("allow_replace"), OutSpec.bAllowReplace);

        if (OutSpec.AssetPath.IsEmpty())
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("asset_path는 필수 필드입니다."), TEXT("asset_path") });
        }
        if (OutSpec.ParentClass.IsEmpty())
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("parent_class는 필수 필드입니다."), TEXT("parent_class") });
        }

        return OutErrors.Num() == InitialErrorCount;
    }
}
