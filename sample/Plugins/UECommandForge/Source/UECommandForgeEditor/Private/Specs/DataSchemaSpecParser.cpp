#include "Specs/DataSchemaSpecParser.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace UECommandForge
{
    namespace
    {
        const TSet<FString>& SupportedFieldTypes()
        {
            static const TSet<FString> Types = {
                TEXT("string"),
                TEXT("text"),
                TEXT("int"),
                TEXT("float"),
                TEXT("bool"),
                TEXT("enum"),
                TEXT("soft_object_path"),
                TEXT("gameplay_tag"),
                TEXT("class_path"),
                TEXT("data_table_row_handle"),
            };
            return Types;
        }

        bool TryGetStringAlias(const FJsonObject& Object, const TCHAR* LowerName, const TCHAR* PascalName, FString& OutValue)
        {
            return Object.TryGetStringField(LowerName, OutValue) || Object.TryGetStringField(PascalName, OutValue);
        }

        bool TryGetBoolAlias(const FJsonObject& Object, const TCHAR* LowerName, const TCHAR* PascalName, bool& OutValue)
        {
            return Object.TryGetBoolField(LowerName, OutValue) || Object.TryGetBoolField(PascalName, OutValue);
        }

        bool TryGetNumberAlias(const FJsonObject& Object, const TCHAR* LowerName, const TCHAR* PascalName, double& OutValue)
        {
            return Object.TryGetNumberField(LowerName, OutValue) || Object.TryGetNumberField(PascalName, OutValue);
        }

        void ReadStringArrayField(const FJsonObject& Object, const TCHAR* LowerName, const TCHAR* PascalName,
                                  TArray<FString>& OutValues)
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrayValues = nullptr;
            if (!Object.TryGetArrayField(LowerName, ArrayValues))
            {
                Object.TryGetArrayField(PascalName, ArrayValues);
            }

            if (ArrayValues == nullptr)
            {
                return;
            }

            for (const TSharedPtr<FJsonValue>& Value : *ArrayValues)
            {
                FString StringValue;
                if (Value.IsValid() && Value->TryGetString(StringValue))
                {
                    OutValues.Add(StringValue);
                }
            }
        }

        void AddRequiredFieldError(TArray<FCommandForgeError>& OutErrors, const FString& Field)
        {
            OutErrors.Add({
                TEXT("REQUIRED_FIELD"),
                FString::Printf(TEXT("'%s' 필드는 필수입니다."), *Field),
                Field,
            });
        }

        bool ParseField(const TSharedPtr<FJsonObject>& FieldObject, const int32 Index,
                        FCommandForgeDataSchemaField& OutField, TArray<FCommandForgeError>& OutErrors)
        {
            if (!FieldObject.IsValid())
            {
                OutErrors.Add({
                    TEXT("INVALID_FIELD"),
                    TEXT("'fields' 항목은 JSON object여야 합니다."),
                    FString::Printf(TEXT("fields[%d]"), Index),
                });
                return false;
            }

            const FString FieldPrefix = FString::Printf(TEXT("fields[%d]"), Index);
            if (!TryGetStringAlias(*FieldObject, TEXT("name"), TEXT("Name"), OutField.Name))
            {
                AddRequiredFieldError(OutErrors, FieldPrefix + TEXT(".name"));
            }
            if (!TryGetStringAlias(*FieldObject, TEXT("type"), TEXT("Type"), OutField.Type))
            {
                AddRequiredFieldError(OutErrors, FieldPrefix + TEXT(".type"));
            }
            else if (!SupportedFieldTypes().Contains(OutField.Type))
            {
                OutErrors.Add({
                    TEXT("UNSUPPORTED_FIELD_TYPE"),
                    FString::Printf(TEXT("지원하지 않는 field type입니다: %s"), *OutField.Type),
                    FieldPrefix + TEXT(".type"),
                });
            }

            TryGetBoolAlias(*FieldObject, TEXT("required"), TEXT("Required"), OutField.bRequired);
            TryGetBoolAlias(*FieldObject, TEXT("unique"), TEXT("Unique"), OutField.bUnique);
            TryGetStringAlias(*FieldObject, TEXT("regex"), TEXT("Regex"), OutField.Regex);
            TryGetStringAlias(*FieldObject, TEXT("enum_source"), TEXT("EnumSource"), OutField.EnumSource);
            TryGetStringAlias(*FieldObject, TEXT("gameplay_tag_source"), TEXT("GameplayTagSource"), OutField.GameplayTagSource);
            TryGetStringAlias(*FieldObject, TEXT("severity"), TEXT("Severity"), OutField.Severity);
            TryGetStringAlias(*FieldObject, TEXT("exception"), TEXT("Exception"), OutField.Exception);
            TryGetStringAlias(*FieldObject, TEXT("section"), TEXT("Section"), OutField.Section);
            if (!TryGetStringAlias(*FieldObject, TEXT("default"), TEXT("Default"), OutField.DefaultValue))
            {
                TryGetStringAlias(*FieldObject, TEXT("default_value"), TEXT("DefaultValue"), OutField.DefaultValue);
            }
            ReadStringArrayField(*FieldObject, TEXT("allowed_values"), TEXT("AllowedValues"), OutField.AllowedValues);

            double NumberValue = 0.0;
            if (TryGetNumberAlias(*FieldObject, TEXT("min"), TEXT("Min"), NumberValue))
            {
                OutField.bHasMin = true;
                OutField.Min = NumberValue;
            }
            if (TryGetNumberAlias(*FieldObject, TEXT("max"), TEXT("Max"), NumberValue))
            {
                OutField.bHasMax = true;
                OutField.Max = NumberValue;
            }

            return true;
        }

        bool ParseException(const TSharedPtr<FJsonObject>& ExceptionObject, const int32 Index,
                            FCommandForgeDataSchemaException& OutException, TArray<FCommandForgeError>& OutErrors)
        {
            if (!ExceptionObject.IsValid())
            {
                OutErrors.Add({
                    TEXT("INVALID_EXCEPTION"),
                    TEXT("'exceptions' 항목은 JSON object여야 합니다."),
                    FString::Printf(TEXT("exceptions[%d]"), Index),
                });
                return false;
            }

            TryGetStringAlias(*ExceptionObject, TEXT("field"), TEXT("Field"), OutException.Field);
            TryGetStringAlias(*ExceptionObject, TEXT("value"), TEXT("Value"), OutException.Value);
            TryGetStringAlias(*ExceptionObject, TEXT("reason"), TEXT("Reason"), OutException.Reason);
            TryGetStringAlias(*ExceptionObject, TEXT("severity"), TEXT("Severity"), OutException.Severity);
            return true;
        }
    }

    bool FDataSchemaSpecParser::ParseJson(const FString& Json, FCommandForgeDataSchemaSpec& OutSpec,
                                          TArray<FCommandForgeError>& OutErrors)
    {
        OutErrors.Empty();
        OutSpec = FCommandForgeDataSchemaSpec();

        TSharedPtr<FJsonObject> RootObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
        {
            OutErrors.Add({
                TEXT("DATA_SCHEMA_PARSE_FAILED"),
                TEXT("데이터 스키마는 canonical JSON object 형식이어야 합니다."),
                TEXT("schema"),
            });
            return false;
        }

        if (!TryGetStringAlias(*RootObject, TEXT("version"), TEXT("Version"), OutSpec.Version))
        {
            AddRequiredFieldError(OutErrors, TEXT("version"));
        }
        if (!TryGetStringAlias(*RootObject, TEXT("kind"), TEXT("Kind"), OutSpec.Kind))
        {
            AddRequiredFieldError(OutErrors, TEXT("kind"));
        }

        TryGetStringAlias(*RootObject, TEXT("key_field"), TEXT("KeyField"), OutSpec.KeyField);
        TryGetStringAlias(*RootObject, TEXT("source_type"), TEXT("SourceType"), OutSpec.SourceType);
        TryGetStringAlias(*RootObject, TEXT("target_asset_path"), TEXT("TargetAssetPath"), OutSpec.TargetAssetPath);
        TryGetStringAlias(*RootObject, TEXT("row_struct_path"), TEXT("RowStructPath"), OutSpec.RowStructPath);
        TryGetStringAlias(*RootObject, TEXT("data_asset_class_path"), TEXT("DataAssetClassPath"), OutSpec.DataAssetClassPath);
        TryGetStringAlias(*RootObject, TEXT("config_file"), TEXT("ConfigFile"), OutSpec.ConfigFile);
        TryGetStringAlias(*RootObject, TEXT("config_section"), TEXT("ConfigSection"), OutSpec.ConfigSection);

        const TArray<TSharedPtr<FJsonValue>>* FieldValues = nullptr;
        if (!RootObject->TryGetArrayField(TEXT("fields"), FieldValues) &&
            !RootObject->TryGetArrayField(TEXT("Fields"), FieldValues))
        {
            AddRequiredFieldError(OutErrors, TEXT("fields"));
        }
        else
        {
            for (int32 Index = 0; Index < FieldValues->Num(); ++Index)
            {
                FCommandForgeDataSchemaField Field;
                ParseField((*FieldValues)[Index]->AsObject(), Index, Field, OutErrors);
                OutSpec.Fields.Add(Field);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ExceptionValues = nullptr;
        if (RootObject->TryGetArrayField(TEXT("exceptions"), ExceptionValues) ||
            RootObject->TryGetArrayField(TEXT("Exceptions"), ExceptionValues))
        {
            for (int32 Index = 0; Index < ExceptionValues->Num(); ++Index)
            {
                FCommandForgeDataSchemaException Exception;
                ParseException((*ExceptionValues)[Index]->AsObject(), Index, Exception, OutErrors);
                OutSpec.Exceptions.Add(Exception);
            }
        }

        return OutErrors.IsEmpty();
    }
}
