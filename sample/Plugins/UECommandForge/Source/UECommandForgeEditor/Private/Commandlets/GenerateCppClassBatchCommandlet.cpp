#include "Commandlets/GenerateCppClassBatchCommandlet.h"
#include "Commandlets/GenerateCppClassCommandlet.h"
#include "Specs/CppClassBatchSpecParser.h"
#include "CommandForgeTypes.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Reports/JsonReportWriter.h"
#include "BuildCs/BuildCsDependencyUtils.h"

UGenerateCppClassBatchCommandlet::UGenerateCppClassBatchCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::GenerateCppClassBatchPrivate
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
}

int32 UGenerateCppClassBatchCommandlet::Main(const FString& Params)
{
    using namespace UECommandForge::GenerateCppClassBatchPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString SpecPath = ParamsMap.FindRef(TEXT("Spec"));
    const bool bApply = Switches.Contains(TEXT("Apply"));
    const bool bApplyBuildCs = ParamsMap.FindRef(TEXT("ApplyBuildCs")).Equals(TEXT("true"), ESearchCase::IgnoreCase);

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("GenerateCppClassBatch");
    Report.bDryRun = !bApply;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    if (SpecPath.IsEmpty())
    {
        AddError(Report, TEXT("MISSING_ARG"), TEXT("-Spec 인자가 없습니다."), TEXT("Spec"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString SpecJson;
    if (!FFileHelper::LoadFileToString(SpecJson, *SpecPath))
    {
        AddError(Report, TEXT("SPEC_READ_FAILED"), TEXT("C++ class batch spec 파일을 읽을 수 없습니다."), TEXT("Spec"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCppClassBatchSpec BatchSpec;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FCppClassBatchSpecParser::Parse(SpecJson, BatchSpec, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    BatchSpec.TransactionId = BatchSpec.TransactionId.TrimStartAndEnd();
    Report.TransactionId = BatchSpec.TransactionId.IsEmpty()
        ? FString::Printf(TEXT("tx-%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")))
        : BatchSpec.TransactionId;

    Report.Validation.Add(TEXT("spec"), SpecPath);

    bool bPreflightSuccess = true;
    TArray<FCommandForgeReport> PreflightReports;
    PreflightReports.AddDefaulted(BatchSpec.Classes.Num());

    // 1. Dry-run 검증 (Preflight) 및 정규화 경로/클래스 심볼 기반 중복 체크
    TSet<FString> TargetPaths;
    TSet<FString> TargetClassSymbols;
    for (int32 Index = 0; Index < BatchSpec.Classes.Num(); ++Index)
    {
        FCommandForgeCppClassSpec SingleSpec;
        SingleSpec.Version = BatchSpec.Version;
        SingleSpec.Kind = TEXT("cpp_class");
        SingleSpec.TransactionId = BatchSpec.TransactionId;
        SingleSpec.ClassInfo = BatchSpec.Classes[Index];
        SingleSpec.Includes = BatchSpec.Includes;
        SingleSpec.Properties = BatchSpec.Properties;
        SingleSpec.Functions = BatchSpec.Functions;
        SingleSpec.BuildDependencies = BatchSpec.BuildDependencies;

        FString HeaderTemp, SourceTemp;
        // Preflight 검증은 항상 bApply = false 상태로 안전하게 진행
        const bool bClassSuccess = UGenerateCppClassCommandlet::GenerateSingleClass(
            SingleSpec, false, bApplyBuildCs, PreflightReports[Index], HeaderTemp, SourceTemp);

        if (!bClassSuccess)
        {
            bPreflightSuccess = false;
        }

        // 실제 GenerateSingleClass 내부에서 정규화 완료된 경로 및 클래스 심볼을 가져와 중복 검사 수행
        FString TargetHeader = PreflightReports[Index].Validation.FindRef(TEXT("header_path")).ToLower();
        FString TargetSource = PreflightReports[Index].Validation.FindRef(TEXT("source_path")).ToLower();
        FString TargetSymbol = PreflightReports[Index].Validation.FindRef(TEXT("class_symbol"));

        if (!TargetHeader.IsEmpty() && !TargetSource.IsEmpty())
        {
            if (TargetPaths.Contains(TargetHeader) || TargetPaths.Contains(TargetSource))
            {
                FCommandForgeValidationIssue Issue;
                Issue.Severity = TEXT("error");
                Issue.Code = TEXT("DUPLICATE_TARGET_CLASS");
                Issue.Message = FString::Printf(TEXT("배치 내에서 동일한 대상 클래스 파일 경로가 중복되었습니다: %s"), *SingleSpec.ClassInfo.Name);
                Issue.Field = FString::Printf(TEXT("classes[%d].name"), Index);
                Report.ValidationIssues.Add(Issue);
                bPreflightSuccess = false;
            }
            TargetPaths.Add(TargetHeader);
            TargetPaths.Add(TargetSource);
        }

        if (!TargetSymbol.IsEmpty())
        {
            if (TargetClassSymbols.Contains(TargetSymbol))
            {
                FCommandForgeValidationIssue Issue;
                Issue.Severity = TEXT("error");
                Issue.Code = TEXT("DUPLICATE_CLASS_SYMBOL");
                Issue.Message = FString::Printf(TEXT("배치 내에서 동일한 C++ 클래스 심볼이 중복되었습니다: %s"), *TargetSymbol);
                Issue.Field = FString::Printf(TEXT("classes[%d].name"), Index);
                Report.ValidationIssues.Add(Issue);
                bPreflightSuccess = false;
            }
            TargetClassSymbols.Add(TargetSymbol);
        }
    }

    // Preflight 검증 실패 시, 누적된 오류/이슈들을 최종 리포트에 쓰고 즉시 반환 (원자적 검증)
    if (!bPreflightSuccess || HasErrorIssue(Report.ValidationIssues))
    {
        for (int32 Index = 0; Index < BatchSpec.Classes.Num(); ++Index)
        {
            for (FCommandForgeValidationIssue& Issue : PreflightReports[Index].ValidationIssues)
            {
                Issue.Field = FString::Printf(TEXT("classes[%d].%s"), Index, *Issue.Field);
                Report.ValidationIssues.Add(Issue);
            }

            const FString Prefix = FString::Printf(TEXT("classes[%d]."), Index);
            for (const TPair<FString, FString>& Pair : PreflightReports[Index].Validation)
            {
                Report.Validation.Add(Prefix + Pair.Key, Pair.Value);
            }
        }
        Report.bOk = false;
        Report.bApplied = false;
        Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    // 2. Dry-Run 모드인 경우, preflight 결과를 취합 후 즉시 보고서 작성 및 성공 반환
    if (!bApply)
    {
        for (int32 Index = 0; Index < BatchSpec.Classes.Num(); ++Index)
        {
            const FString Prefix = FString::Printf(TEXT("classes[%d]."), Index);
            for (const TPair<FString, FString>& Pair : PreflightReports[Index].Validation)
            {
                Report.Validation.Add(Prefix + Pair.Key, Pair.Value);
            }
        }
        Report.bOk = true;
        Report.bApplied = false;
        Report.Validation.Add(TEXT("issue_count"), TEXT("0"));
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return 0;
    }

    // 3. Apply 모드인 경우, 실제 파일 작성을 트랜잭션 방식으로 실행
    TArray<FString> CreatedFiles;
    TMap<FString, FString> BackupBuildCs;
    bool bApplySuccess = true;
    int32 FailedIndex = -1;
    FCommandForgeReport ApplyFailureReport;

    for (int32 Index = 0; Index < BatchSpec.Classes.Num(); ++Index)
    {
        FCommandForgeCppClassSpec SingleSpec;
        SingleSpec.Version = BatchSpec.Version;
        SingleSpec.Kind = TEXT("cpp_class");
        SingleSpec.TransactionId = BatchSpec.TransactionId;
        SingleSpec.ClassInfo = BatchSpec.Classes[Index];
        SingleSpec.Includes = BatchSpec.Includes;
        SingleSpec.Properties = BatchSpec.Properties;
        SingleSpec.Functions = BatchSpec.Functions;
        SingleSpec.BuildDependencies = BatchSpec.BuildDependencies;

        // Build.cs 변경 감지 시 롤백용 원본 백업 준비
        const UECommandForge::BuildCs::FBuildCsDependencyCheck BuildCsCheck =
            UECommandForge::BuildCs::CheckDependencies(SingleSpec.ClassInfo.Module,
                SingleSpec.BuildDependencies.PublicDependencies, SingleSpec.BuildDependencies.PrivateDependencies);

        if (bApplyBuildCs && BuildCsCheck.bChanged && BuildCsCheck.bBuildCsExists && !BackupBuildCs.Contains(BuildCsCheck.BuildCsPath))
        {
            BackupBuildCs.Add(BuildCsCheck.BuildCsPath, BuildCsCheck.OriginalContent);
        }

        FCommandForgeReport ClassReport;
        FString HeaderPath, SourcePath;

        // 실제 파일 쓰기 실행
        const bool bClassSuccess = UGenerateCppClassCommandlet::GenerateSingleClass(
            SingleSpec, true, bApplyBuildCs, ClassReport, HeaderPath, SourcePath);

        if (!bClassSuccess)
        {
            bApplySuccess = false;
            FailedIndex = Index;
            ApplyFailureReport = ClassReport;
            // 실패 클래스 자체의 partial write로 인해 남은 변경 파일(혹은 cleanup 실패 파일)도 롤백 목록에 추가
            for (const FString& FilePath : ClassReport.ChangedFiles)
            {
                CreatedFiles.AddUnique(FilePath);
            }
            break;
        }

        // 성공한 경우 생성된 파일들 추적 추가
        for (const FString& FilePath : ClassReport.ChangedFiles)
        {
            CreatedFiles.AddUnique(FilePath);
        }

        // 성공한 개별 클래스 정보도 리포트에 임시 취합
        const FString Prefix = FString::Printf(TEXT("classes[%d]."), Index);
        for (const TPair<FString, FString>& Pair : ClassReport.Validation)
        {
            Report.Validation.Add(Prefix + Pair.Key, Pair.Value);
        }
    }

    // Apply 실행 도중 실패 시 원자적 롤백 (Transactional Rollback) 및 롤백 실패 여부 검증
    if (!bApplySuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GenerateCppClassBatch] apply 실패로 인해 생성된 파일들과 Build.cs를 롤백합니다. (FailedIndex: %d)"), FailedIndex);

        bool bRollbackFailed = false;
        TArray<FString> FailedRollbackFiles;

        // 생성한 .h, .cpp 파일들 즉각 삭제 및 실패 감지
        for (const FString& FilePath : CreatedFiles)
        {
            if (FilePath.EndsWith(TEXT(".h")) || FilePath.EndsWith(TEXT(".cpp")))
            {
                if (IFileManager::Get().Delete(*FilePath) == false)
                {
                    if (FPaths::FileExists(FilePath))
                    {
                        bRollbackFailed = true;
                        FailedRollbackFiles.Add(FilePath);
                    }
                }
            }
        }

        // 수정된 Build.cs 파일들을 원래대로 복구 및 실패 감지
        for (const TPair<FString, FString>& Backup : BackupBuildCs)
        {
            if (FFileHelper::SaveStringToFile(Backup.Value, *Backup.Key, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) == false)
            {
                bRollbackFailed = true;
                FailedRollbackFiles.Add(Backup.Key);
            }
        }

        // 실패 사유를 최종 리포트에 기록
        for (FCommandForgeValidationIssue& Issue : ApplyFailureReport.ValidationIssues)
        {
            Issue.Field = FString::Printf(TEXT("classes[%d].%s"), FailedIndex, *Issue.Field);
            Report.ValidationIssues.Add(Issue);
        }

        if (bRollbackFailed)
        {
            Report.Validation.Add(TEXT("rollback_failed"), TEXT("true"));
            Report.Validation.Add(TEXT("partial_changes_remaining"), TEXT("true"));

            // 롤백 실패 시 표준 Result 필드 업데이트: 변경사항이 남았으므로 applied=true 및 남은 파일들 기록
            Report.bApplied = true;
            for (const FString& FailedFile : FailedRollbackFiles)
            {
                Report.ChangedFiles.AddUnique(FailedFile);

                FCommandForgeValidationIssue RollbackIssue;
                RollbackIssue.Severity = TEXT("error");
                RollbackIssue.Code = TEXT("ROLLBACK_FAILED");
                RollbackIssue.Message = FString::Printf(TEXT("실패한 배치 작업을 롤백하는 동안 대상 파일 복원/삭제에 실패했습니다: %s"), *FailedFile);
                RollbackIssue.Field = TEXT("rollback");
                RollbackIssue.FilePath = FailedFile;
                Report.ValidationIssues.Add(RollbackIssue);
            }
        }
        else
        {
            Report.Validation.Add(TEXT("rollback_failed"), TEXT("false"));
            Report.Validation.Add(TEXT("partial_changes_remaining"), TEXT("false"));
            Report.bApplied = false;
            Report.ChangedFiles.Empty();
        }

        Report.bOk = false;
        Report.Validation.Add(TEXT("failed_index"), FString::FromInt(FailedIndex));
        Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    // 4. 모든 클래스가 성공적으로 Apply 된 경우, 변경 파일들을 리포트에 최종 기록
    for (const FString& FilePath : CreatedFiles)
    {
        Report.ChangedFiles.AddUnique(FilePath);
    }

    Report.Validation.Add(TEXT("issue_count"), TEXT("0"));
    Report.bApplied = !Report.ChangedFiles.IsEmpty();
    Report.bOk = true;

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return 0;
}
