#include "Commandlets/ValidateConfigRulesCommandlet.h"

#include "CommandForgeTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/DataSchemaSpec.h"
#include "Specs/DataSchemaSpecParser.h"

UValidateConfigRulesCommandlet::UValidateConfigRulesCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::ValidateConfigRulesPrivate
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

    FString ResolveConfigPath(const FString& ConfigArg, const FCommandForgeDataSchemaSpec& Schema)
    {
        FString ConfigPath = ConfigArg.IsEmpty() ? Schema.ConfigFile : ConfigArg;
        if (ConfigPath.IsEmpty())
        {
            ConfigPath = TEXT("DefaultGame.ini");
        }

        if (FPaths::IsRelative(ConfigPath))
        {
            ConfigPath = FPaths::Combine(FPaths::ProjectConfigDir(), ConfigPath);
        }

        return FPaths::ConvertRelativePathToFull(ConfigPath);
    }

    FString NormalizeConfigValue(const FString& Value)
    {
        return Value.TrimStartAndEnd().TrimQuotes();
    }

    bool TryGetConfigValue(const FConfigFile& ConfigFile, const FString& Section,
        const FString& Key, FString& OutValue)
    {
        if (Section.IsEmpty() || Key.IsEmpty())
        {
            return false;
        }

        const FConfigSection* ConfigSection = ConfigFile.FindSection(Section);
        if (ConfigSection == nullptr)
        {
            return false;
        }

        const FConfigValue* ConfigValue = ConfigSection->Find(*Key);
        if (ConfigValue == nullptr)
        {
            return false;
        }

        OutValue = NormalizeConfigValue(ConfigValue->GetValue());
        return true;
    }

    bool TryParseInteger(const FString& Value, int32& OutValue)
    {
        return LexTryParseString(OutValue, *Value.TrimStartAndEnd());
    }

    bool TryParseNumber(const FString& Value, double& OutValue)
    {
        return LexTryParseString(OutValue, *Value.TrimStartAndEnd());
    }

    bool TryParseBoolLiteral(const FString& Value)
    {
        const FString Lower = Value.TrimStartAndEnd().ToLower();
        return Lower == TEXT("true") || Lower == TEXT("false") ||
            Lower == TEXT("1") || Lower == TEXT("0");
    }

    void AddMissingKeyIssue(FCommandForgeReport& Report, const FCommandForgeDataSchemaField& Field,
        const FString& ConfigPath, const FString& Section)
    {
        const FString SuggestedFix = Field.DefaultValue.IsEmpty()
            ? FString::Printf(TEXT("[%s] 아래에 %s 값을 추가하세요."), *Section, *Field.Name)
            : FString::Printf(TEXT("[%s] 아래에 %s=%s 값을 추가하세요."),
                *Section, *Field.Name, *Field.DefaultValue);

        AddIssue(Report, Field.Severity, TEXT("CONFIG_REQUIRED_KEY_MISSING"),
            TEXT("필수 config key가 없습니다."),
            FString::Printf(TEXT("%s.%s"), *Section, *Field.Name), ConfigPath, SuggestedFix);

        if (!Field.DefaultValue.IsEmpty())
        {
            AddIssue(Report, Field.Severity, TEXT("CONFIG_DEFAULT_SUGGESTED"),
                TEXT("누락된 config key에 사용할 default 값을 제안합니다."),
                FString::Printf(TEXT("%s.%s"), *Section, *Field.Name), ConfigPath, SuggestedFix);
        }
    }

    void ValidateConfigValue(FCommandForgeReport& Report, const FCommandForgeDataSchemaField& Field,
        const FString& Value, const FString& ConfigPath, const FString& Section)
    {
        const FString FieldPath = FString::Printf(TEXT("%s.%s"), *Section, *Field.Name);
        const FString Trimmed = Value.TrimStartAndEnd();
        if (Field.bRequired && Trimmed.IsEmpty())
        {
            AddMissingKeyIssue(Report, Field, ConfigPath, Section);
            return;
        }
        if (Trimmed.IsEmpty())
        {
            return;
        }

        double Number = 0.0;
        if (Field.Type.Equals(TEXT("int"), ESearchCase::IgnoreCase))
        {
            int32 IntValue = 0;
            if (!TryParseInteger(Trimmed, IntValue))
            {
                AddIssue(Report, Field.Severity, TEXT("CONFIG_TYPE_MISMATCH"),
                    TEXT("config 값이 int 형식이 아닙니다."), FieldPath, ConfigPath,
                    TEXT("정수 값으로 수정하세요."));
                return;
            }
            Number = static_cast<double>(IntValue);
        }
        else if (Field.Type.Equals(TEXT("float"), ESearchCase::IgnoreCase))
        {
            if (!TryParseNumber(Trimmed, Number))
            {
                AddIssue(Report, Field.Severity, TEXT("CONFIG_TYPE_MISMATCH"),
                    TEXT("config 값이 float 형식이 아닙니다."), FieldPath, ConfigPath,
                    TEXT("숫자 값으로 수정하세요."));
                return;
            }
        }
        else if (Field.Type.Equals(TEXT("bool"), ESearchCase::IgnoreCase) && !TryParseBoolLiteral(Trimmed))
        {
            AddIssue(Report, Field.Severity, TEXT("CONFIG_TYPE_MISMATCH"),
                TEXT("config 값이 bool 형식이 아닙니다."), FieldPath, ConfigPath,
                TEXT("true, false, 1, 0 중 하나로 수정하세요."));
            return;
        }

        if ((Field.Type.Equals(TEXT("int"), ESearchCase::IgnoreCase) ||
                Field.Type.Equals(TEXT("float"), ESearchCase::IgnoreCase)) &&
            ((Field.bHasMin && Number < Field.Min) || (Field.bHasMax && Number > Field.Max)))
        {
            AddIssue(Report, Field.Severity, TEXT("CONFIG_RANGE_VIOLATION"),
                TEXT("config 숫자 값이 min/max 범위를 벗어났습니다."), FieldPath, ConfigPath,
                TEXT("schema 범위 안의 값으로 수정하세요."));
        }

        if (!Field.AllowedValues.IsEmpty() && !Field.AllowedValues.Contains(Trimmed))
        {
            AddIssue(Report, Field.Severity, TEXT("CONFIG_ALLOWED_VALUE_MISMATCH"),
                TEXT("config 값이 allowed_values 정책과 일치하지 않습니다."), FieldPath, ConfigPath,
                FString::Printf(TEXT("허용 값: %s"), *FString::Join(Field.AllowedValues, TEXT(", "))));
        }
    }

    bool ValidateConfigRules(const FString& ConfigPath, const FCommandForgeDataSchemaSpec& Schema,
        const FConfigFile& ConfigFile, FCommandForgeReport& Report)
    {
        for (const FCommandForgeDataSchemaField& Field : Schema.Fields)
        {
            const FString Section = Field.Section.IsEmpty() ? Schema.ConfigSection : Field.Section;
            FString Value;
            if (!TryGetConfigValue(ConfigFile, Section, Field.Name, Value))
            {
                if (Field.bRequired)
                {
                    AddMissingKeyIssue(Report, Field, ConfigPath, Section);
                }
                continue;
            }

            ValidateConfigValue(Report, Field, Value, ConfigPath, Section);
        }

        return !HasErrorIssue(Report.ValidationIssues);
    }
}

int32 UValidateConfigRulesCommandlet::Main(const FString& Params)
{
    namespace ConfigRulesPrivate = UECommandForge::ValidateConfigRulesPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString SchemaPath = ParamsMap.FindRef(TEXT("Schema"));
    const FString ConfigArg = ParamsMap.FindRef(TEXT("Config"));
    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidateConfigRules");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        ConfigRulesPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (SchemaPath.IsEmpty())
    {
        ConfigRulesPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Schema 인자가 없습니다."), TEXT("Schema"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString SchemaJson;
    if (!FFileHelper::LoadFileToString(SchemaJson, *SchemaPath))
    {
        ConfigRulesPrivate::AddError(Report, TEXT("CONFIG_SCHEMA_READ_FAILED"),
            TEXT("config schema 파일을 읽을 수 없습니다."), TEXT("Schema"));
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
    if (!Schema.Kind.Equals(TEXT("config_rules"), ESearchCase::IgnoreCase))
    {
        ConfigRulesPrivate::AddError(Report, TEXT("CONFIG_SCHEMA_KIND_MISMATCH"),
            TEXT("schema kind는 'config_rules'여야 합니다."), TEXT("kind"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (Schema.ConfigSection.IsEmpty())
    {
        ConfigRulesPrivate::AddError(Report, TEXT("CONFIG_SECTION_MISSING"),
            TEXT("config_section 값이 필요합니다."), TEXT("config_section"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    const FString ConfigPath = ConfigRulesPrivate::ResolveConfigPath(ConfigArg, Schema);
    if (!FPaths::FileExists(ConfigPath))
    {
        ConfigRulesPrivate::AddError(Report, TEXT("CONFIG_READ_FAILED"),
            TEXT("project config 파일을 읽을 수 없습니다."), TEXT("Config"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FConfigFile ConfigFile;
    ConfigFile.Read(ConfigPath);

    ConfigRulesPrivate::ValidateConfigRules(ConfigPath, Schema, ConfigFile, Report);
    Report.Validation.Add(TEXT("config_path"), ConfigPath);
    Report.Validation.Add(TEXT("field_count"), FString::FromInt(Schema.Fields.Num()));
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() &&
        !ConfigRulesPrivate::HasErrorIssue(Report.ValidationIssues);

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
