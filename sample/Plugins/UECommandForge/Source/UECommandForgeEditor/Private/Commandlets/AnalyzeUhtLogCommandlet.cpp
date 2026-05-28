#include "Commandlets/AnalyzeUhtLogCommandlet.h"

#include "CommandForgeTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Reports/JsonReportWriter.h"

UAnalyzeUhtLogCommandlet::UAnalyzeUhtLogCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::AnalyzeUhtLogPrivate
{
    struct FParsedUhtLine
    {
        FString Severity;
        FString Code;
        FString Message;
        FString FilePath;
        FString SuggestedFix;
        int32 SourceLineNumber = 0;
        int32 LogLineNumber = 0;
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

    FString DefaultMarkdownPath(const FString& OutPath)
    {
        return FPaths::Combine(FPaths::GetPath(OutPath),
            FPaths::GetBaseFilename(OutPath) + TEXT(".md"));
    }

    int32 FindDiagnosticMarker(const FString& Line, const FString& Marker)
    {
        return Line.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
    }

    bool IsDiagnosticLine(const FString& Line, const FString& Marker)
    {
        return FindDiagnosticMarker(Line, Marker) != INDEX_NONE;
    }

    FString ExtractMessage(const FString& Line, const FString& Marker)
    {
        const int32 MarkerIndex = FindDiagnosticMarker(Line, Marker);
        if (MarkerIndex == INDEX_NONE)
        {
            return Line.TrimStartAndEnd();
        }
        return Line.RightChop(MarkerIndex + Marker.Len()).TrimStartAndEnd();
    }

    void ExtractLocation(const FString& Line, FString& OutFilePath, int32& OutSourceLineNumber)
    {
        OutFilePath.Empty();
        OutSourceLineNumber = 0;

        const int32 MarkerIndex = FindDiagnosticMarker(Line, TEXT(": Error:"));
        const int32 WarningMarkerIndex = FindDiagnosticMarker(Line, TEXT(": Warning:"));
        const int32 DiagnosticIndex = MarkerIndex != INDEX_NONE ? MarkerIndex : WarningMarkerIndex;
        if (DiagnosticIndex == INDEX_NONE)
        {
            return;
        }

        const FString Prefix = Line.Left(DiagnosticIndex);
        int32 OpenParenIndex = INDEX_NONE;
        int32 CloseParenIndex = INDEX_NONE;
        if (Prefix.FindLastChar(TEXT('('), OpenParenIndex) &&
            Prefix.FindLastChar(TEXT(')'), CloseParenIndex) &&
            OpenParenIndex < CloseParenIndex)
        {
            const FString SourceLineText = Prefix.Mid(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1);
            if (SourceLineText.IsNumeric())
            {
                OutFilePath = Prefix.Left(OpenParenIndex).TrimStartAndEnd();
                OutSourceLineNumber = FCString::Atoi(*SourceLineText);
                return;
            }
        }

        OutFilePath = Prefix.TrimStartAndEnd();
    }

    FString ClassifyCode(const FString& Severity, const FString& Message)
    {
        if (Severity.Equals(TEXT("warning"), ESearchCase::IgnoreCase))
        {
            if (Message.Contains(TEXT("Category"), ESearchCase::IgnoreCase))
            {
                return TEXT("UHT_MISSING_CATEGORY");
            }
            return TEXT("UHT_WARNING");
        }

        if (Message.Contains(TEXT("Unrecognized type"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("Unable to find"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("must be a UCLASS"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("must be a USTRUCT"), ESearchCase::IgnoreCase) ||
            Message.Contains(TEXT("must be a UENUM"), ESearchCase::IgnoreCase))
        {
            return TEXT("UHT_UNKNOWN_REFLECTED_TYPE");
        }
        if (Message.Contains(TEXT("BlueprintReadWrite"), ESearchCase::IgnoreCase) &&
            Message.Contains(TEXT("private"), ESearchCase::IgnoreCase))
        {
            return TEXT("UHT_PRIVATE_BLUEPRINT_ACCESS");
        }
        if (Message.Contains(TEXT("generated.h"), ESearchCase::IgnoreCase))
        {
            return TEXT("UHT_GENERATED_HEADER_ORDER");
        }
        if (Message.Contains(TEXT("Category"), ESearchCase::IgnoreCase))
        {
            return TEXT("UHT_MISSING_CATEGORY");
        }
        return TEXT("UHT_ERROR");
    }

    FString SuggestedFixForCode(const FString& Code)
    {
        if (Code == TEXT("UHT_UNKNOWN_REFLECTED_TYPE"))
        {
            return TEXT("타입 정의 header를 include하고, reflection 대상이면 UCLASS/USTRUCT/UENUM으로 선언하세요.");
        }
        if (Code == TEXT("UHT_PRIVATE_BLUEPRINT_ACCESS"))
        {
            return TEXT("private member에는 BlueprintReadWrite를 제거하거나 BlueprintGetter/BlueprintSetter를 사용하세요.");
        }
        if (Code == TEXT("UHT_GENERATED_HEADER_ORDER"))
        {
            return TEXT(".generated.h include를 header의 마지막 include로 이동하세요.");
        }
        if (Code == TEXT("UHT_MISSING_CATEGORY"))
        {
            return TEXT("Blueprint 노출 UPROPERTY/UFUNCTION에 Category를 추가하세요.");
        }
        if (Code == TEXT("UHT_WARNING"))
        {
            return TEXT("UHT warning 메시지의 대상 macro와 선언부를 확인하세요.");
        }
        return TEXT("UHT error 메시지의 파일/라인에서 macro, include, reflection specifier를 확인하세요.");
    }

    FParsedUhtLine ParseDiagnosticLine(const FString& Line, int32 LogLineNumber)
    {
        FParsedUhtLine Parsed;
        Parsed.LogLineNumber = LogLineNumber;
        Parsed.Severity = IsDiagnosticLine(Line, TEXT("Warning:")) ? TEXT("warning") : TEXT("error");
        Parsed.Message = ExtractMessage(Line, Parsed.Severity == TEXT("warning") ? TEXT("Warning:") : TEXT("Error:"));
        ExtractLocation(Line, Parsed.FilePath, Parsed.SourceLineNumber);
        Parsed.Code = ClassifyCode(Parsed.Severity, Parsed.Message);
        Parsed.SuggestedFix = SuggestedFixForCode(Parsed.Code);
        return Parsed;
    }

    FString EscapeMarkdownCell(const FString& Value)
    {
        FString Escaped = Value;
        Escaped.ReplaceInline(TEXT("|"), TEXT("\\|"));
        Escaped.ReplaceInline(TEXT("\r"), TEXT(" "));
        Escaped.ReplaceInline(TEXT("\n"), TEXT(" "));
        return Escaped;
    }

    FString FormatLocation(const FParsedUhtLine& Diagnostic)
    {
        if (Diagnostic.FilePath.IsEmpty())
        {
            return FString::Printf(TEXT("log line %d"), Diagnostic.LogLineNumber);
        }
        if (Diagnostic.SourceLineNumber <= 0)
        {
            return FString::Printf(TEXT("%s (log line %d)"), *Diagnostic.FilePath, Diagnostic.LogLineNumber);
        }
        return FString::Printf(TEXT("%s:%d"), *Diagnostic.FilePath, Diagnostic.SourceLineNumber);
    }

    bool WriteMarkdownReport(const FString& MarkdownPath, const FString& UhtLogPath,
        const TArray<FParsedUhtLine>& Diagnostics, int32 ErrorCount, int32 WarningCount)
    {
        FString Markdown;
        Markdown += TEXT("# AnalyzeUhtLog Report\n\n");
        Markdown += FString::Printf(TEXT("- Log: `%s`\n"), *UhtLogPath);
        Markdown += FString::Printf(TEXT("- Errors: %d\n"), ErrorCount);
        Markdown += FString::Printf(TEXT("- Warnings: %d\n\n"), WarningCount);

        if (Diagnostics.IsEmpty())
        {
            Markdown += TEXT("No UHT diagnostics found.\n");
        }
        else
        {
            Markdown += TEXT("| Severity | Code | Location | Message | Suggested fix |\n");
            Markdown += TEXT("| --- | --- | --- | --- | --- |\n");
            for (const FParsedUhtLine& Diagnostic : Diagnostics)
            {
                const FString Location = FormatLocation(Diagnostic);
                Markdown += FString::Printf(TEXT("| %s | %s | %s | %s | %s |\n"),
                    *EscapeMarkdownCell(Diagnostic.Severity),
                    *EscapeMarkdownCell(Diagnostic.Code),
                    *EscapeMarkdownCell(Location),
                    *EscapeMarkdownCell(Diagnostic.Message),
                    *EscapeMarkdownCell(Diagnostic.SuggestedFix));
            }
        }

        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(MarkdownPath));
        return FFileHelper::SaveStringToFile(Markdown, *MarkdownPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

int32 UAnalyzeUhtLogCommandlet::Main(const FString& Params)
{
    namespace AnalyzeUhtLogPrivate = UECommandForge::AnalyzeUhtLogPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString UhtLogPath = ParamsMap.FindRef(TEXT("Log"));
    FString MarkdownPath = ParamsMap.FindRef(TEXT("Markdown"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("AnalyzeUhtLog");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        AnalyzeUhtLogPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (UhtLogPath.IsEmpty())
    {
        AnalyzeUhtLogPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Log 인자가 없습니다."), TEXT("Log"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (MarkdownPath.IsEmpty())
    {
        MarkdownPath = AnalyzeUhtLogPrivate::DefaultMarkdownPath(OutPath);
    }

    FString LogText;
    if (!FFileHelper::LoadFileToString(LogText, *UhtLogPath))
    {
        AnalyzeUhtLogPrivate::AddError(Report, TEXT("LOG_READ_FAILED"), TEXT("UHT log 파일을 읽을 수 없습니다."), TEXT("Log"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TArray<FString> Lines;
    LogText.ParseIntoArrayLines(Lines, false);

    TArray<AnalyzeUhtLogPrivate::FParsedUhtLine> Diagnostics;
    int32 ErrorCount = 0;
    int32 WarningCount = 0;

    for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
    {
        const FString& Line = Lines[LineIndex];
        if (AnalyzeUhtLogPrivate::IsDiagnosticLine(Line, TEXT("Error:")))
        {
            ++ErrorCount;
            Diagnostics.Add(AnalyzeUhtLogPrivate::ParseDiagnosticLine(Line, LineIndex + 1));
        }
        else if (AnalyzeUhtLogPrivate::IsDiagnosticLine(Line, TEXT("Warning:")))
        {
            ++WarningCount;
            Diagnostics.Add(AnalyzeUhtLogPrivate::ParseDiagnosticLine(Line, LineIndex + 1));
        }
    }

    for (const AnalyzeUhtLogPrivate::FParsedUhtLine& Diagnostic : Diagnostics)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = Diagnostic.Severity;
        Issue.Code = Diagnostic.Code;
        Issue.Message = Diagnostic.Message;
        Issue.Field = FString::Printf(TEXT("log_line:%d"), Diagnostic.LogLineNumber);
        Issue.FilePath = Diagnostic.SourceLineNumber > 0
            ? FString::Printf(TEXT("%s:%d"), *Diagnostic.FilePath, Diagnostic.SourceLineNumber)
            : Diagnostic.FilePath;
        Issue.SuggestedFix = Diagnostic.SuggestedFix;
        Report.ValidationIssues.Add(Issue);
    }

    Report.Validation.Add(TEXT("log_path"), UhtLogPath);
    Report.Validation.Add(TEXT("markdown_path"), MarkdownPath);
    Report.Validation.Add(TEXT("line_count"), FString::FromInt(Lines.Num()));
    Report.Validation.Add(TEXT("error_count"), FString::FromInt(ErrorCount));
    Report.Validation.Add(TEXT("warning_count"), FString::FromInt(WarningCount));
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() && ErrorCount == 0;

    if (!AnalyzeUhtLogPrivate::WriteMarkdownReport(MarkdownPath, UhtLogPath, Diagnostics, ErrorCount, WarningCount))
    {
        AnalyzeUhtLogPrivate::AddError(Report, TEXT("MARKDOWN_WRITE_FAILED"),
            TEXT("UHT markdown report를 저장할 수 없습니다."), TEXT("Markdown"));
        Report.bOk = false;
    }

    if (!UECommandForge::FJsonReportWriter::Write(OutPath, Report))
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    if (!Report.Errors.IsEmpty())
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
