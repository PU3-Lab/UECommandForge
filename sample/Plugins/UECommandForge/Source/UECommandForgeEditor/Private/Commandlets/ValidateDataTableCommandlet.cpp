#include "Commandlets/ValidateDataTableCommandlet.h"

#include "CommandForgeTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/DataSchemaSpec.h"
#include "Specs/DataSchemaSpecParser.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

UValidateDataTableCommandlet::UValidateDataTableCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::ValidateDataTablePrivate
{
    void AddError(FCommandForgeReport& Report, const FString& Code, const FString& Message,
        const FString& Field)
    {
        FCommandForgeError Error;
        Error.Code = Code;
        Error.Message = Message;
        Error.Field = Field;
        Report.Errors.Add(Error);
    }

    void AddIssue(FCommandForgeReport& Report, const FString& Severity, const FString& Code,
        const FString& Message, const FString& Field, const FString& AssetPath,
        const FString& SuggestedFix)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = Severity.IsEmpty() ? TEXT("error") : Severity;
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Field = Field;
        Issue.AssetPath = AssetPath;
        Issue.SuggestedFix = SuggestedFix;
        Report.ValidationIssues.Add(Issue);
    }

    bool HasErrorIssue(const TArray<FCommandForgeValidationIssue>& Issues)
    {
        for (const FCommandForgeValidationIssue& Issue : Issues)
        {
            if (Issue.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    FString PackageNameFromAssetArg(const FString& AssetArg)
    {
        const FString Trimmed = AssetArg.TrimStartAndEnd();
        int32 DotIndex = INDEX_NONE;
        if (Trimmed.FindChar(TEXT('.'), DotIndex))
        {
            return Trimmed.Left(DotIndex);
        }
        return Trimmed;
    }

    FString ObjectPathOf(const FString& PackageName)
    {
        return PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName);
    }

    FString FieldValueToString(const FProperty* Property, const uint8* RowData)
    {
        if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
        {
            return StringProperty->GetPropertyValue_InContainer(RowData);
        }
        if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
        {
            return TextProperty->GetPropertyValue_InContainer(RowData).ToString();
        }
        if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
        {
            return NameProperty->GetPropertyValue_InContainer(RowData).ToString();
        }
        if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
        {
            return BoolProperty->GetPropertyValue_InContainer(RowData) ? TEXT("true") : TEXT("false");
        }
        if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
        {
            if (NumericProperty->IsInteger())
            {
                return FString::Printf(TEXT("%lld"),
                    NumericProperty->GetSignedIntPropertyValue(
                        NumericProperty->ContainerPtrToValuePtr<void>(RowData)));
            }
            return FString::SanitizeFloat(
                NumericProperty->GetFloatingPointPropertyValue(
                    NumericProperty->ContainerPtrToValuePtr<void>(RowData)));
        }
        if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            if (StructProperty->Struct == FGameplayTag::StaticStruct())
            {
                const FGameplayTag* Tag = StructProperty->ContainerPtrToValuePtr<FGameplayTag>(RowData);
                return Tag != nullptr ? Tag->ToString() : FString();
            }
            if (StructProperty->Struct == FSoftObjectPath::StaticStruct())
            {
                const FSoftObjectPath* Path = StructProperty->ContainerPtrToValuePtr<FSoftObjectPath>(RowData);
                return Path != nullptr ? Path->ToString() : FString();
            }
            if (StructProperty->Struct == FDataTableRowHandle::StaticStruct())
            {
                const FDataTableRowHandle* Handle =
                    StructProperty->ContainerPtrToValuePtr<FDataTableRowHandle>(RowData);
                return Handle != nullptr ? Handle->RowName.ToString() : FString();
            }
        }

        FString Exported;
        Property->ExportText_Direct(Exported, Property->ContainerPtrToValuePtr<void>(RowData),
            nullptr, nullptr, PPF_None);
        return Exported.TrimQuotes();
    }

    bool IsPropertyCompatibleWithType(const FProperty* Property, const FString& Type)
    {
        if (Type.Equals(TEXT("string"), ESearchCase::IgnoreCase) ||
            Type.Equals(TEXT("text"), ESearchCase::IgnoreCase))
        {
            return Property->IsA<FStrProperty>() || Property->IsA<FTextProperty>() ||
                Property->IsA<FNameProperty>();
        }
        if (Type.Equals(TEXT("int"), ESearchCase::IgnoreCase))
        {
            const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
            return NumericProperty != nullptr && NumericProperty->IsInteger();
        }
        if (Type.Equals(TEXT("float"), ESearchCase::IgnoreCase))
        {
            const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
            return NumericProperty != nullptr && NumericProperty->IsFloatingPoint();
        }
        if (Type.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
        {
            return Property->IsA<FBoolProperty>();
        }
        if (Type.Equals(TEXT("enum"), ESearchCase::IgnoreCase))
        {
            return Property->IsA<FEnumProperty>() || Property->IsA<FByteProperty>() ||
                Property->IsA<FStrProperty>() || Property->IsA<FNameProperty>();
        }
        if (Type.Equals(TEXT("gameplay_tag"), ESearchCase::IgnoreCase))
        {
            const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
            return Property->IsA<FNameProperty>() || Property->IsA<FStrProperty>() ||
                (StructProperty != nullptr && StructProperty->Struct == FGameplayTag::StaticStruct());
        }
        if (Type.Equals(TEXT("soft_object_path"), ESearchCase::IgnoreCase))
        {
            const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
            return Property->IsA<FSoftObjectProperty>() ||
                (StructProperty != nullptr && StructProperty->Struct == FSoftObjectPath::StaticStruct());
        }
        if (Type.Equals(TEXT("class_path"), ESearchCase::IgnoreCase))
        {
            return Property->IsA<FSoftClassProperty>() || Property->IsA<FClassProperty>();
        }
        if (Type.Equals(TEXT("data_table_row_handle"), ESearchCase::IgnoreCase))
        {
            const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
            return StructProperty != nullptr && StructProperty->Struct == FDataTableRowHandle::StaticStruct();
        }
        return true;
    }

    bool TryParseNumber(const FString& Value, double& OutNumber)
    {
        return LexTryParseString(OutNumber, *Value.TrimStartAndEnd());
    }

    bool IsExcepted(const FCommandForgeDataSchemaField& Field,
        const TArray<FCommandForgeDataSchemaException>& Exceptions, const FString& Value,
        FString& OutSeverity)
    {
        for (const FCommandForgeDataSchemaException& Exception : Exceptions)
        {
            if (!Exception.Field.IsEmpty() && Exception.Field != Field.Name)
            {
                continue;
            }
            if (!Exception.Value.IsEmpty() && Exception.Value != Value)
            {
                continue;
            }
            OutSeverity = Exception.Severity.IsEmpty() ? Field.Severity : Exception.Severity;
            return true;
        }
        return false;
    }

    void AddFieldIssue(FCommandForgeReport& Report, const FCommandForgeDataSchemaField& Field,
        const TArray<FCommandForgeDataSchemaException>& Exceptions, const FString& Value,
        const FString& Code, const FString& Message, const FString& RowName,
        const FString& AssetPath, const FString& SuggestedFix)
    {
        FString Severity;
        if (IsExcepted(Field, Exceptions, Value, Severity) &&
            Severity.Equals(TEXT("ignore"), ESearchCase::IgnoreCase))
        {
            return;
        }

        AddIssue(Report, Severity.IsEmpty() ? Field.Severity : Severity, Code, Message,
            FString::Printf(TEXT("rows[%s].%s"), *RowName, *Field.Name), AssetPath, SuggestedFix);
    }

    void ValidateFieldValue(const FCommandForgeDataSchemaField& Field, const FProperty* Property,
        const uint8* RowData, const FString& RowName, const FString& AssetPath,
        const FCommandForgeDataSchemaSpec& Schema, FCommandForgeReport& Report)
    {
        const FString Value = FieldValueToString(Property, RowData).TrimStartAndEnd();
        if (Field.bRequired && Value.IsEmpty())
        {
            AddFieldIssue(Report, Field, Schema.Exceptions, Value,
                TEXT("DATATABLE_REQUIRED_FIELD_MISSING"), TEXT("필수 DataTable 필드 값이 비어 있습니다."),
                RowName, AssetPath, TEXT("DataTable row에 필수 값을 추가하세요."));
            return;
        }
        if (Value.IsEmpty())
        {
            return;
        }

        double Number = 0.0;
        if ((Field.Type.Equals(TEXT("int"), ESearchCase::IgnoreCase) ||
                Field.Type.Equals(TEXT("float"), ESearchCase::IgnoreCase)) &&
            TryParseNumber(Value, Number) &&
            ((Field.bHasMin && Number < Field.Min) || (Field.bHasMax && Number > Field.Max)))
        {
            AddFieldIssue(Report, Field, Schema.Exceptions, Value,
                TEXT("DATATABLE_RANGE_VIOLATION"),
                TEXT("DataTable 숫자 필드 값이 min/max 범위를 벗어났습니다."),
                RowName, AssetPath, TEXT("schema 범위 안의 값으로 수정하세요."));
        }

        if (!Field.AllowedValues.IsEmpty() && !Field.AllowedValues.Contains(Value))
        {
            AddFieldIssue(Report, Field, Schema.Exceptions, Value,
                TEXT("DATATABLE_ALLOWED_VALUE_MISMATCH"),
                TEXT("DataTable 필드 값이 allowed_values 정책과 일치하지 않습니다."),
                RowName, AssetPath,
                FString::Printf(TEXT("허용 값: %s"), *FString::Join(Field.AllowedValues, TEXT(", "))));
        }

        if (Field.Type.Equals(TEXT("gameplay_tag"), ESearchCase::IgnoreCase))
        {
            const FGameplayTag GameplayTag = UGameplayTagsManager::Get().RequestGameplayTag(
                FName(*Value), false);
            if (!GameplayTag.IsValid())
            {
                AddFieldIssue(Report, Field, Schema.Exceptions, Value,
                    TEXT("DATATABLE_GAMEPLAY_TAG_UNREGISTERED"),
                    TEXT("DataTable GameplayTag가 등록되어 있지 않습니다."),
                    RowName, AssetPath, TEXT("DefaultGameplayTags.ini 또는 tag source를 확인하세요."));
            }
        }
        else if (Field.Type.Equals(TEXT("soft_object_path"), ESearchCase::IgnoreCase))
        {
            const FSoftObjectPath SoftObjectPath(Value);
            if (!SoftObjectPath.IsValid())
            {
                AddFieldIssue(Report, Field, Schema.Exceptions, Value,
                    TEXT("DATATABLE_SOFT_OBJECT_PATH_INVALID"),
                    TEXT("DataTable SoftObjectPath 형식이 올바르지 않습니다."),
                    RowName, AssetPath, TEXT("/Game/Path/Asset.Asset 형식으로 수정하세요."));
            }
            else if (!SoftObjectPath.GetLongPackageName().IsEmpty() &&
                !FPackageName::DoesPackageExist(SoftObjectPath.GetLongPackageName()))
            {
                AddFieldIssue(Report, Field, Schema.Exceptions, Value,
                    TEXT("DATATABLE_SOFT_OBJECT_PATH_MISSING"),
                    TEXT("DataTable SoftObjectPath 대상 package를 찾을 수 없습니다."),
                    RowName, AssetPath, TEXT("asset 경로 또는 import 순서를 확인하세요."));
            }
        }
    }

    void ValidateDataTable(UDataTable* DataTable, const FString& AssetPath,
        const FCommandForgeDataSchemaSpec& Schema, FCommandForgeReport& Report)
    {
        const UScriptStruct* RowStruct = DataTable->GetRowStruct();
        if (RowStruct == nullptr)
        {
            AddIssue(Report, TEXT("error"), TEXT("DATATABLE_ROW_STRUCT_MISSING"),
                TEXT("DataTable RowStruct가 없습니다."), TEXT("row_struct_path"), AssetPath,
                TEXT("DataTable RowStruct를 지정하세요."));
            return;
        }

        if (!Schema.RowStructPath.IsEmpty())
        {
            UScriptStruct* ExpectedRowStruct = LoadObject<UScriptStruct>(nullptr, *Schema.RowStructPath);
            if (ExpectedRowStruct == nullptr)
            {
                AddError(Report, TEXT("ROW_STRUCT_LOAD_FAILED"),
                    TEXT("schema row_struct_path를 로드할 수 없습니다."), TEXT("row_struct_path"));
            }
            else if (ExpectedRowStruct != RowStruct)
            {
                AddIssue(Report, TEXT("error"), TEXT("DATATABLE_ROW_STRUCT_MISMATCH"),
                    TEXT("DataTable RowStruct가 schema와 일치하지 않습니다."), TEXT("row_struct_path"),
                    AssetPath, Schema.RowStructPath);
            }
        }

        TMap<FString, const FProperty*> PropertiesByField;
        for (const FCommandForgeDataSchemaField& Field : Schema.Fields)
        {
            const FProperty* Property = RowStruct->FindPropertyByName(FName(*Field.Name));
            if (Property == nullptr)
            {
                AddIssue(Report, Field.Severity, TEXT("DATATABLE_FIELD_MISSING"),
                    TEXT("schema field가 RowStruct에 없습니다."), Field.Name, AssetPath,
                    TEXT("RowStruct 필드 또는 schema field name을 맞추세요."));
                continue;
            }

            if (!IsPropertyCompatibleWithType(Property, Field.Type))
            {
                AddIssue(Report, Field.Severity, TEXT("DATATABLE_FIELD_TYPE_MISMATCH"),
                    TEXT("RowStruct property 타입이 schema field type과 일치하지 않습니다."),
                    Field.Name, AssetPath, Field.Type);
            }
            PropertiesByField.Add(Field.Name, Property);
        }

        TSet<FString> UniqueFieldNames;
        if (!Schema.KeyField.IsEmpty())
        {
            UniqueFieldNames.Add(Schema.KeyField);
        }
        for (const FCommandForgeDataSchemaField& Field : Schema.Fields)
        {
            if (Field.bUnique)
            {
                UniqueFieldNames.Add(Field.Name);
            }
        }

        TMap<FString, TSet<FString>> SeenValuesByField;
        for (const TPair<FName, uint8*>& RowPair : DataTable->GetRowMap())
        {
            const FString RowName = RowPair.Key.ToString();
            const uint8* RowData = RowPair.Value;
            if (RowData == nullptr)
            {
                AddIssue(Report, TEXT("error"), TEXT("DATATABLE_ROW_INVALID"),
                    TEXT("DataTable row data가 비어 있습니다."), RowName, AssetPath,
                    TEXT("DataTable row를 다시 저장하세요."));
                continue;
            }

            for (const FCommandForgeDataSchemaField& Field : Schema.Fields)
            {
                const FProperty* const* Property = PropertiesByField.Find(Field.Name);
                if (Property == nullptr || *Property == nullptr)
                {
                    continue;
                }

                ValidateFieldValue(Field, *Property, RowData, RowName, AssetPath, Schema, Report);

                if (UniqueFieldNames.Contains(Field.Name))
                {
                    const FString Value = FieldValueToString(*Property, RowData).TrimStartAndEnd();
                    if (!Value.IsEmpty())
                    {
                        TSet<FString>& SeenValues = SeenValuesByField.FindOrAdd(Field.Name);
                        if (SeenValues.Contains(Value))
                        {
                            AddFieldIssue(Report, Field, Schema.Exceptions, Value,
                                TEXT("DATATABLE_DUPLICATE_VALUE"),
                                TEXT("DataTable unique/key 필드 값이 중복되었습니다."),
                                RowName, AssetPath, TEXT("고유한 값으로 수정하세요."));
                        }
                        else
                        {
                            SeenValues.Add(Value);
                        }
                    }
                }
            }
        }
    }
}

int32 UValidateDataTableCommandlet::Main(const FString& Params)
{
    namespace DataTablePrivate = UECommandForge::ValidateDataTablePrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SchemaPath = ParamsMap.FindRef(TEXT("Schema"));
    const FString AssetArg = ParamsMap.FindRef(TEXT("Asset"));
    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidateDataTable");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        DataTablePrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (SchemaPath.IsEmpty())
    {
        DataTablePrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Schema 인자가 없습니다."), TEXT("Schema"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString SchemaJson;
    if (!FFileHelper::LoadFileToString(SchemaJson, *SchemaPath))
    {
        DataTablePrivate::AddError(Report, TEXT("DATA_SCHEMA_READ_FAILED"),
            TEXT("data schema 파일을 읽을 수 없습니다."), TEXT("Schema"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgeDataSchemaSpec Schema;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FDataSchemaSpecParser::ParseJson(SchemaJson, Schema, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString AssetPath = DataTablePrivate::PackageNameFromAssetArg(AssetArg);
    if (AssetPath.IsEmpty())
    {
        AssetPath = Schema.TargetAssetPath;
    }
    if (AssetPath.IsEmpty() || !FPackageName::IsValidLongPackageName(AssetPath, false))
    {
        DataTablePrivate::AddError(Report, TEXT("DATATABLE_ASSET_PATH_INVALID"),
            TEXT("-Asset 또는 schema target_asset_path는 long package name이어야 합니다."),
            TEXT("Asset"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *DataTablePrivate::ObjectPathOf(AssetPath));
    if (DataTable == nullptr)
    {
        DataTablePrivate::AddError(Report, TEXT("DATATABLE_ASSET_NOT_FOUND"),
            TEXT("DataTable asset을 로드할 수 없습니다."), TEXT("Asset"));
        Report.Validation.Add(TEXT("asset_path"), AssetPath);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    Report.Validation.Add(TEXT("asset_path"), AssetPath);
    Report.Validation.Add(TEXT("row_struct_path"),
        DataTable->GetRowStruct() != nullptr ? DataTable->GetRowStruct()->GetPathName() : FString());
    Report.Validation.Add(TEXT("expected_row_struct_path"), Schema.RowStructPath);
    Report.Validation.Add(TEXT("row_count"), FString::FromInt(DataTable->GetRowMap().Num()));
    Report.Validation.Add(TEXT("field_count"), FString::FromInt(Schema.Fields.Num()));

    DataTablePrivate::ValidateDataTable(DataTable, AssetPath, Schema, Report);

    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() &&
        !DataTablePrivate::HasErrorIssue(Report.ValidationIssues);

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
