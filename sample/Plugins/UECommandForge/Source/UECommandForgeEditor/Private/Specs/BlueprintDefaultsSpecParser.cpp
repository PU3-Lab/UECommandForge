#include "Specs/BlueprintDefaultsSpecParser.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace UECommandForge
{
    namespace
    {
        void AddParseError(TArray<FCommandForgeError>& OutErrors,
                           const FString& Code,
                           const FString& Message,
                           const FString& Field)
        {
            OutErrors.Add({ Code, Message, Field });
        }

        FString ReadAssetPath(const TSharedPtr<FJsonObject>& Root)
        {
            FString AssetPath;
            if (Root->TryGetStringField(TEXT("asset_path"), AssetPath))
            {
                return AssetPath;
            }

            const TSharedPtr<FJsonObject>* BlueprintObject = nullptr;
            if (Root->TryGetObjectField(TEXT("blueprint"), BlueprintObject) && BlueprintObject && BlueprintObject->IsValid())
            {
                (*BlueprintObject)->TryGetStringField(TEXT("asset_path"), AssetPath);
            }
            return AssetPath;
        }
    }

    bool FBlueprintDefaultsSpecParser::ParseJson(const FString& Json,
                                                 FBlueprintDefaultsSpec& OutSpec,
                                                 TArray<FCommandForgeError>& OutErrors)
    {
        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            AddParseError(OutErrors, TEXT("JSON_PARSE_FAILED"),
                TEXT("Blueprint defaults spec JSON 파싱 실패"), TEXT(""));
            return false;
        }

        Root->TryGetStringField(TEXT("version"), OutSpec.Version);
        Root->TryGetStringField(TEXT("kind"), OutSpec.Kind);
        Root->TryGetStringField(TEXT("transaction_id"), OutSpec.TransactionId);
        OutSpec.AssetPath = ReadAssetPath(Root);
        Root->TryGetBoolField(TEXT("compile"), OutSpec.bCompile);
        Root->TryGetBoolField(TEXT("save"), OutSpec.bSave);

        const TSharedPtr<FJsonObject>* BlueprintObject = nullptr;
        if (Root->TryGetObjectField(TEXT("blueprint"), BlueprintObject) && BlueprintObject && BlueprintObject->IsValid())
        {
            (*BlueprintObject)->TryGetBoolField(TEXT("compile"), OutSpec.bCompile);
            (*BlueprintObject)->TryGetBoolField(TEXT("save"), OutSpec.bSave);
        }

        if (OutSpec.AssetPath.IsEmpty())
        {
            AddParseError(OutErrors, TEXT("REQUIRED_FIELD"),
                TEXT("blueprint.asset_path 또는 asset_path는 필수입니다."), TEXT("blueprint.asset_path"));
        }

        const TArray<TSharedPtr<FJsonValue>>* Defaults = nullptr;
        if (Root->TryGetArrayField(TEXT("defaults"), Defaults) && Defaults != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Defaults)
            {
                const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
                if (!Object.IsValid())
                {
                    AddParseError(OutErrors, TEXT("INVALID_DEFAULT"),
                        TEXT("defaults 항목은 object여야 합니다."), TEXT("defaults"));
                    continue;
                }

                FBlueprintDefaultPropertySpec Property;
                Object->TryGetStringField(TEXT("property"), Property.Property);
                Object->TryGetStringField(TEXT("value"), Property.Value);
                if (Property.Property.IsEmpty())
                {
                    AddParseError(OutErrors, TEXT("REQUIRED_FIELD"),
                        TEXT("defaults.property는 필수입니다."), TEXT("defaults.property"));
                }
                OutSpec.Defaults.Add(Property);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
        if (Root->TryGetArrayField(TEXT("components"), Components) && Components != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Components)
            {
                const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
                if (!Object.IsValid())
                {
                    AddParseError(OutErrors, TEXT("INVALID_COMPONENT"),
                        TEXT("components 항목은 object여야 합니다."), TEXT("components"));
                    continue;
                }

                FBlueprintDefaultComponentSpec Component;
                Object->TryGetStringField(TEXT("name"), Component.Name);
                Object->TryGetStringField(TEXT("class"), Component.Class);
                if (Component.Name.IsEmpty() || Component.Class.IsEmpty())
                {
                    AddParseError(OutErrors, TEXT("REQUIRED_FIELD"),
                        TEXT("components.name과 components.class는 필수입니다."), TEXT("components"));
                }
                OutSpec.Components.Add(Component);
            }
        }

        return OutErrors.IsEmpty();
    }
}
