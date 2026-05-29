#include "Commandlets/ValidateDataSourceCommandlet.h"

#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Reports/JsonReportWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Specs/DataSchemaSpec.h"
#include "Specs/DataSchemaSpecParser.h"
#include "UObject/SoftObjectPath.h"

UValidateDataSourceCommandlet::UValidateDataSourceCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::ValidateDataSourcePrivate
{
    struct FDataSourceRow
    {
        int32 RowNumber = 0;
        TMap<FString, FString> Values;
    };

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
        const FString& Message, const FString& Field, const FString& FilePath,
        const FString& SuggestedFix)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = Severity.IsEmpty() ? TEXT("error") : Severity;
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Field = Field;
        Issue.FilePath = FilePath;
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

    bool ParseCsvLine(const FString& Line, TArray<FString>& OutCells)
    {
        FString Cell;
        bool bInQuotes = false;
        for (int32 Index = 0; Index < Line.Len(); ++Index)
        {
            const TCHAR Ch = Line[Index];
            if (Ch == TEXT('"'))
            {
                if (bInQuotes && Index + 1 < Line.Len() && Line[Index + 1] == TEXT('"'))
                {
                    Cell.AppendChar(TEXT('"'));
                    ++Index;
                }
                else
                {
                    bInQuotes = !bInQuotes;
                }
                continue;
            }

            if (Ch == TEXT(',') && !bInQuotes)
            {
                OutCells.Add(Cell.TrimStartAndEnd());
                Cell.Reset();
                continue;
            }

            Cell.AppendChar(Ch);
        }

        OutCells.Add(Cell.TrimStartAndEnd());
        return !bInQuotes;
    }

    bool LoadCsvRows(const FString& SourcePath, const FString& Content,
        TArray<FDataSourceRow>& OutRows, FCommandForgeReport& Report)
    {
        TArray<FString> Lines;
        Content.ParseIntoArrayLines(Lines, true);
        if (Lines.IsEmpty())
        {
            AddError(Report, TEXT("DATA_SOURCE_EMPTY"), TEXT("CSV source가 비어 있습니다."), TEXT("Source"));
            return false;
        }

        TArray<FString> Headers;
        if (!ParseCsvLine(Lines[0], Headers))
        {
            AddError(Report, TEXT("DATA_SOURCE_CSV_PARSE_FAILED"),
                TEXT("CSV header의 quote 구문이 올바르지 않습니다."), TEXT("Source"));
            return false;
        }

        for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
        {
            TArray<FString> Cells;
            if (!ParseCsvLine(Lines[LineIndex], Cells))
            {
                AddIssue(Report, TEXT("error"), TEXT("DATA_SOURCE_CSV_PARSE_FAILED"),
                    TEXT("CSV row의 quote 구문이 올바르지 않습니다."),
                    FString::Printf(TEXT("rows[%d]"), LineIndex), SourcePath,
                    TEXT("quote 쌍을 확인하세요."));
                continue;
            }

            FDataSourceRow Row;
            Row.RowNumber = LineIndex + 1;
            for (int32 HeaderIndex = 0; HeaderIndex < Headers.Num(); ++HeaderIndex)
            {
                const FString Value = Cells.IsValidIndex(HeaderIndex) ? Cells[HeaderIndex] : FString();
                Row.Values.Add(Headers[HeaderIndex], Value);
            }
            OutRows.Add(Row);
        }
        return true;
    }

    bool IsUtf8ContinuationByte(uint8 Byte)
    {
        return Byte >= 0x80 && Byte <= 0xBF;
    }

    bool IsValidUtf8(const TArray<uint8>& Bytes)
    {
        int32 Index = Bytes.Num() >= 3 && Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF
            ? 3
            : 0;
        while (Index < Bytes.Num())
        {
            const uint8 Byte = Bytes[Index];
            if (Byte <= 0x7F)
            {
                ++Index;
                continue;
            }

            if (Byte >= 0xC2 && Byte <= 0xDF)
            {
                if (Index + 1 >= Bytes.Num() || !IsUtf8ContinuationByte(Bytes[Index + 1]))
                {
                    return false;
                }
                Index += 2;
                continue;
            }

            if (Byte == 0xE0)
            {
                if (Index + 2 >= Bytes.Num() || Bytes[Index + 1] < 0xA0 || Bytes[Index + 1] > 0xBF ||
                    !IsUtf8ContinuationByte(Bytes[Index + 2]))
                {
                    return false;
                }
                Index += 3;
                continue;
            }

            if ((Byte >= 0xE1 && Byte <= 0xEC) || (Byte >= 0xEE && Byte <= 0xEF))
            {
                if (Index + 2 >= Bytes.Num() || !IsUtf8ContinuationByte(Bytes[Index + 1]) ||
                    !IsUtf8ContinuationByte(Bytes[Index + 2]))
                {
                    return false;
                }
                Index += 3;
                continue;
            }

            if (Byte == 0xED)
            {
                if (Index + 2 >= Bytes.Num() || Bytes[Index + 1] < 0x80 || Bytes[Index + 1] > 0x9F ||
                    !IsUtf8ContinuationByte(Bytes[Index + 2]))
                {
                    return false;
                }
                Index += 3;
                continue;
            }

            if (Byte == 0xF0)
            {
                if (Index + 3 >= Bytes.Num() || Bytes[Index + 1] < 0x90 || Bytes[Index + 1] > 0xBF ||
                    !IsUtf8ContinuationByte(Bytes[Index + 2]) || !IsUtf8ContinuationByte(Bytes[Index + 3]))
                {
                    return false;
                }
                Index += 4;
                continue;
            }

            if (Byte >= 0xF1 && Byte <= 0xF3)
            {
                if (Index + 3 >= Bytes.Num() || !IsUtf8ContinuationByte(Bytes[Index + 1]) ||
                    !IsUtf8ContinuationByte(Bytes[Index + 2]) || !IsUtf8ContinuationByte(Bytes[Index + 3]))
                {
                    return false;
                }
                Index += 4;
                continue;
            }

            if (Byte == 0xF4)
            {
                if (Index + 3 >= Bytes.Num() || Bytes[Index + 1] < 0x80 || Bytes[Index + 1] > 0x8F ||
                    !IsUtf8ContinuationByte(Bytes[Index + 2]) || !IsUtf8ContinuationByte(Bytes[Index + 3]))
                {
                    return false;
                }
                Index += 4;
                continue;
            }

            return false;
        }
        return true;
    }

    FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid() || Value->IsNull())
        {
            return FString();
        }

        switch (Value->Type)
        {
        case EJson::String:
            return Value->AsString();
        case EJson::Number:
            return FString::SanitizeFloat(Value->AsNumber());
        case EJson::Boolean:
            return Value->AsBool() ? TEXT("true") : TEXT("false");
        default:
            return FString();
        }
    }

    bool LoadJsonRows(const FString& Content, TArray<FDataSourceRow>& OutRows,
        FCommandForgeReport& Report)
    {
        TSharedPtr<FJsonValue> RootValue;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        if (!FJsonSerializer::Deserialize(Reader, RootValue) || !RootValue.IsValid())
        {
            AddError(Report, TEXT("DATA_SOURCE_JSON_PARSE_FAILED"),
                TEXT("JSON source를 파싱할 수 없습니다."), TEXT("Source"));
            return false;
        }

        TArray<TSharedPtr<FJsonValue>> RowValues;
        if (RootValue->Type == EJson::Array)
        {
            RowValues = RootValue->AsArray();
        }
        else if (RootValue->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> RootObject = RootValue->AsObject();
            const TArray<TSharedPtr<FJsonValue>>* RowsArray = nullptr;
            if ((!RootObject->TryGetArrayField(TEXT("rows"), RowsArray) &&
                    !RootObject->TryGetArrayField(TEXT("data"), RowsArray)) ||
                RowsArray == nullptr)
            {
                AddError(Report, TEXT("DATA_SOURCE_JSON_ROWS_MISSING"),
                    TEXT("JSON source에는 rows 또는 data 배열이 필요합니다."), TEXT("Source"));
                return false;
            }
            RowValues = *RowsArray;
        }
        else
        {
            AddError(Report, TEXT("DATA_SOURCE_JSON_PARSE_FAILED"),
                TEXT("JSON source root는 object 또는 array여야 합니다."), TEXT("Source"));
            return false;
        }

        for (int32 Index = 0; Index < RowValues.Num(); ++Index)
        {
            const TSharedPtr<FJsonObject>* RowObject = nullptr;
            if (!RowValues[Index]->TryGetObject(RowObject) ||
                RowObject == nullptr || !RowObject->IsValid())
            {
                AddIssue(Report, TEXT("error"), TEXT("DATA_SOURCE_JSON_ROW_INVALID"),
                    TEXT("JSON row는 object여야 합니다."),
                    FString::Printf(TEXT("rows[%d]"), Index), FString(),
                    TEXT("row 값을 object로 작성하세요."));
                continue;
            }

            FDataSourceRow Row;
            Row.RowNumber = Index + 1;
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*RowObject)->Values)
            {
                Row.Values.Add(Field.Key, JsonValueToString(Field.Value));
            }
            OutRows.Add(Row);
        }
        return true;
    }

    bool TryParseInteger(const FString& Value)
    {
        int32 Parsed = 0;
        return LexTryParseString(Parsed, *Value.TrimStartAndEnd());
    }

    bool TryParseNumber(const FString& Value, double& OutNumber)
    {
        return LexTryParseString(OutNumber, *Value.TrimStartAndEnd());
    }

    bool IsBoolLiteral(const FString& Value)
    {
        const FString Lower = Value.TrimStartAndEnd().ToLower();
        return Lower == TEXT("true") || Lower == TEXT("false") ||
            Lower == TEXT("1") || Lower == TEXT("0");
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
        const FString& Code, const FString& Message, int32 RowNumber, const FString& SourcePath,
        const FString& SuggestedFix)
    {
        FString Severity;
        if (IsExcepted(Field, Exceptions, Value, Severity) &&
            Severity.Equals(TEXT("ignore"), ESearchCase::IgnoreCase))
        {
            return;
        }

        AddIssue(Report, Severity.IsEmpty() ? Field.Severity : Severity, Code, Message,
            FString::Printf(TEXT("rows[%d].%s"), RowNumber, *Field.Name), SourcePath, SuggestedFix);
    }

    void ValidateFieldValue(const FCommandForgeDataSchemaField& Field, const FString& Value,
        const TArray<FCommandForgeDataSchemaException>& Exceptions, int32 RowNumber,
        const FString& SourcePath, FCommandForgeReport& Report)
    {
        const FString Trimmed = Value.TrimStartAndEnd();
        if (Field.bRequired && Trimmed.IsEmpty())
        {
            AddFieldIssue(Report, Field, Exceptions, Trimmed,
                TEXT("DATA_SOURCE_REQUIRED_FIELD_MISSING"), TEXT("필수 필드 값이 비어 있습니다."),
                RowNumber, SourcePath,
                TEXT("source에 필수 값을 추가하세요."));
            return;
        }
        if (Trimmed.IsEmpty())
        {
            return;
        }

        double Number = 0.0;
        if (Field.Type.Equals(TEXT("int"), ESearchCase::IgnoreCase))
        {
            if (!TryParseInteger(Trimmed))
            {
                AddFieldIssue(Report, Field, Exceptions, Trimmed, TEXT("DATA_SOURCE_TYPE_MISMATCH"),
                    TEXT("int 필드 값이 정수가 아닙니다."), RowNumber, SourcePath,
                    TEXT("정수 값으로 수정하세요."));
                return;
            }
            TryParseNumber(Trimmed, Number);
        }
        else if (Field.Type.Equals(TEXT("float"), ESearchCase::IgnoreCase))
        {
            if (!TryParseNumber(Trimmed, Number))
            {
                AddFieldIssue(Report, Field, Exceptions, Trimmed, TEXT("DATA_SOURCE_TYPE_MISMATCH"),
                    TEXT("float 필드 값이 숫자가 아닙니다."), RowNumber, SourcePath,
                    TEXT("숫자 값으로 수정하세요."));
                return;
            }
        }
        else if (Field.Type.Equals(TEXT("bool"), ESearchCase::IgnoreCase) && !IsBoolLiteral(Trimmed))
        {
            AddFieldIssue(Report, Field, Exceptions, Trimmed, TEXT("DATA_SOURCE_TYPE_MISMATCH"),
                TEXT("bool 필드 값이 true/false/1/0 형식이 아닙니다."), RowNumber, SourcePath,
                TEXT("true, false, 1, 0 중 하나로 수정하세요."));
            return;
        }
        else if (Field.Type.Equals(TEXT("soft_object_path"), ESearchCase::IgnoreCase))
        {
            const FSoftObjectPath SoftObjectPath(Trimmed);
            if (!SoftObjectPath.IsValid())
            {
                AddFieldIssue(Report, Field, Exceptions, Trimmed,
                    TEXT("DATA_SOURCE_SOFT_OBJECT_PATH_INVALID"),
                    TEXT("SoftObjectPath 형식이 올바르지 않습니다."), RowNumber, SourcePath,
                    TEXT("/Game/Path/Asset.Asset 형식으로 수정하세요."));
                return;
            }

            const FString LongPackageName = SoftObjectPath.GetLongPackageName();
            if (!LongPackageName.IsEmpty() && !FPackageName::DoesPackageExist(LongPackageName))
            {
                AddFieldIssue(Report, Field, Exceptions, Trimmed,
                    TEXT("DATA_SOURCE_SOFT_OBJECT_PATH_MISSING"),
                    TEXT("SoftObjectPath 대상 package를 찾을 수 없습니다."), RowNumber, SourcePath,
                TEXT("asset 경로 또는 import 순서를 확인하세요."));
            }
        }
        else if (Field.Type.Equals(TEXT("gameplay_tag"), ESearchCase::IgnoreCase))
        {
            const FGameplayTag GameplayTag = UGameplayTagsManager::Get().RequestGameplayTag(
                FName(*Trimmed), false);
            if (!GameplayTag.IsValid())
            {
                AddFieldIssue(Report, Field, Exceptions, Trimmed,
                    TEXT("DATA_SOURCE_GAMEPLAY_TAG_UNREGISTERED"),
                    TEXT("GameplayTag가 등록되어 있지 않습니다."), RowNumber, SourcePath,
                    TEXT("DefaultGameplayTags.ini 또는 tag source를 확인하세요."));
            }
        }

        if ((Field.Type.Equals(TEXT("int"), ESearchCase::IgnoreCase) ||
                Field.Type.Equals(TEXT("float"), ESearchCase::IgnoreCase)) &&
            ((Field.bHasMin && Number < Field.Min) || (Field.bHasMax && Number > Field.Max)))
        {
            AddFieldIssue(Report, Field, Exceptions, Trimmed, TEXT("DATA_SOURCE_RANGE_VIOLATION"),
                TEXT("숫자 필드 값이 min/max 범위를 벗어났습니다."), RowNumber, SourcePath,
                TEXT("schema 범위 안의 값으로 수정하세요."));
        }

        if (!Field.Regex.IsEmpty())
        {
            const FRegexPattern Pattern(Field.Regex);
            FRegexMatcher Matcher(Pattern, Trimmed);
            if (!Matcher.FindNext())
            {
                AddFieldIssue(Report, Field, Exceptions, Trimmed, TEXT("DATA_SOURCE_REGEX_MISMATCH"),
                    TEXT("필드 값이 regex 정책과 일치하지 않습니다."), RowNumber, SourcePath,
                    TEXT("schema regex에 맞는 값으로 수정하세요."));
            }
        }

        if (!Field.AllowedValues.IsEmpty() && !Field.AllowedValues.Contains(Trimmed))
        {
            AddFieldIssue(Report, Field, Exceptions, Trimmed,
                TEXT("DATA_SOURCE_ALLOWED_VALUE_MISMATCH"),
                TEXT("필드 값이 allowed_values 정책과 일치하지 않습니다."), RowNumber, SourcePath,
                FString::Printf(TEXT("허용 값: %s"), *FString::Join(Field.AllowedValues, TEXT(", "))));
        }
    }

    bool ValidateRows(const FString& SourcePath, const FCommandForgeDataSchemaSpec& Schema,
        const TArray<FDataSourceRow>& Rows, FCommandForgeReport& Report)
    {
        TSet<FString> RequiredHeaders;
        if (!Rows.IsEmpty())
        {
            for (const FCommandForgeDataSchemaField& Field : Schema.Fields)
            {
                if (Field.bRequired && !Rows[0].Values.Contains(Field.Name))
                {
                    AddIssue(Report, Field.Severity, TEXT("DATA_SOURCE_REQUIRED_FIELD_MISSING"),
                        TEXT("필수 필드 header가 없습니다."), Field.Name, SourcePath,
                        TEXT("source header에 필드를 추가하세요."));
                }
            }
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
        for (const FDataSourceRow& Row : Rows)
        {
            for (const FCommandForgeDataSchemaField& Field : Schema.Fields)
            {
                const FString Value = Row.Values.FindRef(Field.Name);
                ValidateFieldValue(Field, Value, Schema.Exceptions, Row.RowNumber, SourcePath, Report);

                if (UniqueFieldNames.Contains(Field.Name) && !Value.TrimStartAndEnd().IsEmpty())
                {
                    TSet<FString>& SeenValues = SeenValuesByField.FindOrAdd(Field.Name);
                    if (SeenValues.Contains(Value))
                    {
                        AddFieldIssue(Report, Field, Schema.Exceptions, Value,
                            TEXT("DATA_SOURCE_DUPLICATE_VALUE"),
                            TEXT("unique/key 필드 값이 중복되었습니다."), Row.RowNumber, SourcePath,
                            TEXT("고유한 값으로 수정하세요."));
                    }
                    else
                    {
                        SeenValues.Add(Value);
                    }
                }
            }
        }
        return !HasErrorIssue(Report.ValidationIssues);
    }
}

int32 UValidateDataSourceCommandlet::Main(const FString& Params)
{
    namespace DataSourcePrivate = UECommandForge::ValidateDataSourcePrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SchemaPath = ParamsMap.FindRef(TEXT("Schema"));
    const FString SourcePath = ParamsMap.FindRef(TEXT("Source"));
    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString FormatOverride = ParamsMap.FindRef(TEXT("Format"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidateDataSource");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        DataSourcePrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (SchemaPath.IsEmpty())
    {
        DataSourcePrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Schema 인자가 없습니다."), TEXT("Schema"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (SourcePath.IsEmpty())
    {
        DataSourcePrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Source 인자가 없습니다."), TEXT("Source"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString SchemaJson;
    if (!FFileHelper::LoadFileToString(SchemaJson, *SchemaPath))
    {
        DataSourcePrivate::AddError(Report, TEXT("DATA_SCHEMA_READ_FAILED"),
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
    if (!Schema.Kind.Equals(TEXT("data_schema"), ESearchCase::IgnoreCase))
    {
        DataSourcePrivate::AddError(Report, TEXT("DATA_SCHEMA_KIND_MISMATCH"),
            TEXT("schema kind는 'data_schema'여야 합니다."), TEXT("kind"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TArray<uint8> SourceBytes;
    if (!FFileHelper::LoadFileToArray(SourceBytes, *SourcePath))
    {
        DataSourcePrivate::AddError(Report, TEXT("DATA_SOURCE_READ_FAILED"),
            TEXT("data source 파일을 읽을 수 없습니다."), TEXT("Source"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (!DataSourcePrivate::IsValidUtf8(SourceBytes))
    {
        DataSourcePrivate::AddError(Report, TEXT("DATA_SOURCE_UTF8_INVALID"),
            TEXT("data source 파일은 UTF-8 인코딩이어야 합니다."), TEXT("Source"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString SourceContent;
    if (!FFileHelper::LoadFileToString(SourceContent, *SourcePath))
    {
        DataSourcePrivate::AddError(Report, TEXT("DATA_SOURCE_READ_FAILED"),
            TEXT("data source 파일을 읽을 수 없습니다."), TEXT("Source"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString Format = FormatOverride.IsEmpty() ? Schema.SourceType : FormatOverride;
    if (Format.IsEmpty())
    {
        Format = FPaths::GetExtension(SourcePath).ToLower();
    }
    Format = Format.ToLower();

    TArray<DataSourcePrivate::FDataSourceRow> Rows;
    bool bLoadedRows = false;
    if (Format == TEXT("csv"))
    {
        bLoadedRows = DataSourcePrivate::LoadCsvRows(SourcePath, SourceContent, Rows, Report);
    }
    else if (Format == TEXT("json"))
    {
        bLoadedRows = DataSourcePrivate::LoadJsonRows(SourceContent, Rows, Report);
    }
    else
    {
        DataSourcePrivate::AddError(Report, TEXT("DATA_SOURCE_FORMAT_UNSUPPORTED"),
            TEXT("source format은 csv 또는 json이어야 합니다."), TEXT("Format"));
    }

    if (bLoadedRows)
    {
        DataSourcePrivate::ValidateRows(SourcePath, Schema, Rows, Report);
    }

    Report.Validation.Add(TEXT("source_format"), Format);
    Report.Validation.Add(TEXT("row_count"), FString::FromInt(Rows.Num()));
    Report.Validation.Add(TEXT("field_count"), FString::FromInt(Schema.Fields.Num()));
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() &&
        !DataSourcePrivate::HasErrorIssue(Report.ValidationIssues);

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
