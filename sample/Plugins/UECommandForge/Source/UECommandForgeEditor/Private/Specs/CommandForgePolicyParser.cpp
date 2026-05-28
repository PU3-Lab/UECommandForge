#include "Specs/CommandForgePolicyParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace UECommandForge
{
    bool FCommandForgePolicyParser::ParseJson(const FString& Json, FCommandForgePolicyDocument& OutPolicy,
                                              TArray<FCommandForgeError>& OutErrors)
    {
        OutErrors.Empty();
        OutPolicy = FCommandForgePolicyDocument();

        TSharedPtr<FJsonObject> RootObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
        {
            OutErrors.Add({ TEXT("POLICY_PARSE_FAILED"),
                TEXT("정책 문서는 canonical JSON object 형식이어야 합니다."),
                TEXT("policy") });
            return false;
        }

        if (!RootObject->TryGetStringField(TEXT("version"), OutPolicy.Version) &&
            !RootObject->TryGetStringField(TEXT("Version"), OutPolicy.Version))
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("'version' 필드는 필수입니다."), TEXT("version") });
        }

        if (!RootObject->TryGetStringField(TEXT("kind"), OutPolicy.Kind) &&
            !RootObject->TryGetStringField(TEXT("Kind"), OutPolicy.Kind))
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("'kind' 필드는 필수입니다."), TEXT("kind") });
        }

        if (!OutErrors.IsEmpty())
        {
            return false;
        }

        FString CanonicalJson;
        TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&CanonicalJson);
        FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

        OutPolicy.CanonicalJson = CanonicalJson;
        OutPolicy.RootObject = RootObject;
        return true;
    }
}
