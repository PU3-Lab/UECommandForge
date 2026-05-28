#include "Commandlets/ValidateCppReflectionCommandlet.h"

#include "BuildCs/BuildCsDependencyUtils.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/CommandForgePolicyParser.h"

UValidateCppReflectionCommandlet::UValidateCppReflectionCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::ValidateCppReflectionPrivate
{
    struct FReflectionPolicyFile
    {
        FString Path;
        FString Role;
    };

    struct FReflectionMacro
    {
        FString Kind;
        FString Text;
        FString Declaration;
        int32 LineNumber = 0;
        bool bHasDeclaration = false;
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

    void AddIssue(FCommandForgeReport& Report, const FString& Code, const FString& Message,
        const FString& Field, const FString& FilePath, const FString& SuggestedFix)
    {
        FCommandForgeValidationIssue Issue;
        Issue.Severity = TEXT("error");
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

    bool ContainsToken(const FString& Text, const FString& Token)
    {
        return Text.Contains(Token, ESearchCase::IgnoreCase);
    }

    int32 ParenDelta(const FString& Line)
    {
        int32 Delta = 0;
        bool bInString = false;
        TCHAR Previous = 0;
        for (const TCHAR Character : Line)
        {
            if (Character == TEXT('"') && Previous != TEXT('\\'))
            {
                bInString = !bInString;
            }
            if (!bInString)
            {
                if (Character == TEXT('('))
                {
                    ++Delta;
                }
                else if (Character == TEXT(')'))
                {
                    --Delta;
                }
            }
            Previous = Character;
        }
        return Delta;
    }

    bool IsMacroLine(const FString& Line)
    {
        const FString Trimmed = Line.TrimStartAndEnd();
        return Trimmed.StartsWith(TEXT("UCLASS")) ||
            Trimmed.StartsWith(TEXT("UPROPERTY")) ||
            Trimmed.StartsWith(TEXT("UFUNCTION")) ||
            Trimmed.StartsWith(TEXT("USTRUCT")) ||
            Trimmed.StartsWith(TEXT("UENUM")) ||
            Trimmed.StartsWith(TEXT("GENERATED_BODY"));
    }

    bool LooksLikeDeclaration(const FString& Line)
    {
        const FString Trimmed = Line.TrimStartAndEnd();
        if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("//")) || IsMacroLine(Trimmed))
        {
            return false;
        }
        if (Trimmed == TEXT("};") || Trimmed == TEXT("}"))
        {
            return false;
        }
        return Trimmed.EndsWith(TEXT(";")) || Trimmed.Contains(TEXT("(")) ||
            Trimmed.StartsWith(TEXT("class ")) || Trimmed.StartsWith(TEXT("struct "));
    }

    int32 FindDeclarationLine(const TArray<FString>& Lines, int32 StartIndex, FString& OutDeclaration)
    {
        for (int32 Index = StartIndex; Index < Lines.Num(); ++Index)
        {
            const FString Trimmed = Lines[Index].TrimStartAndEnd();
            if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("//")))
            {
                continue;
            }
            OutDeclaration = Trimmed;
            return LooksLikeDeclaration(Trimmed) ? Index : INDEX_NONE;
        }
        return INDEX_NONE;
    }

    FString NormalizePolicyPath(const FString& Path, const FString& PolicyDirectory)
    {
        FString Normalized = Path.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/"));
        if (Normalized.IsEmpty())
        {
            return TEXT("");
        }
        if (FPaths::IsRelative(Normalized))
        {
            Normalized = FPaths::Combine(PolicyDirectory, Normalized);
        }
        return FPaths::ConvertRelativePathToFull(Normalized);
    }

    bool TryGetStrictString(const TSharedPtr<FJsonObject>& Object, const FString& FieldName,
        FString& OutValue)
    {
        const TSharedPtr<FJsonValue> Value = Object->TryGetField(FieldName);
        if (!Value.IsValid())
        {
            return true;
        }
        if (Value->Type != EJson::String || !Value->TryGetString(OutValue))
        {
            return false;
        }
        OutValue = OutValue.TrimStartAndEnd();
        return true;
    }

    bool TryGetStringArray(const TSharedPtr<FJsonObject>& Object, const FString& FieldName,
        TArray<FString>& OutValues)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
        {
            return true;
        }

        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString StringValue;
            if (!Value.IsValid() || Value->Type != EJson::String || !Value->TryGetString(StringValue))
            {
                return false;
            }
            OutValues.Add(StringValue.TrimStartAndEnd());
        }
        return true;
    }

    bool IsSafeRelativeIncludePath(const FString& IncludePath)
    {
        const FString Normalized = IncludePath.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/"));
        if (Normalized.IsEmpty() || !FPaths::IsRelative(Normalized))
        {
            return false;
        }

        TArray<FString> Parts;
        Normalized.ParseIntoArray(Parts, TEXT("/"), true);
        for (const FString& Part : Parts)
        {
            if (Part == TEXT("..") || Part.Contains(TEXT(":")))
            {
                return false;
            }
        }
        return true;
    }

    void AddHeaderFilesUnderRoot(const FString& Root, const FString& Role,
        TArray<FReflectionPolicyFile>& OutFiles)
    {
        TArray<FString> HeaderFiles;
        IFileManager::Get().FindFilesRecursive(HeaderFiles, *Root, TEXT("*.h"), true, false);
        HeaderFiles.Sort();
        for (const FString& HeaderFile : HeaderFiles)
        {
            FReflectionPolicyFile File;
            File.Path = FPaths::ConvertRelativePathToFull(HeaderFile);
            File.Role = Role;
            OutFiles.Add(File);
        }
    }

    bool ParsePolicy(const FCommandForgePolicyDocument& Document, const FString& PolicyDirectory,
        TArray<FReflectionPolicyFile>& OutFiles, FCommandForgeReport& Report)
    {
        if (!Document.Kind.Equals(TEXT("cpp_reflection_policy"), ESearchCase::IgnoreCase))
        {
            AddError(Report, TEXT("POLICY_KIND_MISMATCH"),
                TEXT("정책 kind는 'cpp_reflection_policy'여야 합니다."), TEXT("kind"));
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>* FileValues = nullptr;
        if (Document.RootObject->TryGetArrayField(TEXT("files"), FileValues) && FileValues != nullptr)
        {
            for (int32 Index = 0; Index < FileValues->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject>* FileObject = nullptr;
                if (!(*FileValues)[Index]->TryGetObject(FileObject) ||
                    FileObject == nullptr || !FileObject->IsValid())
                {
                    AddError(Report, TEXT("INVALID_FILE"),
                        TEXT("files 항목은 object여야 합니다."),
                        FString::Printf(TEXT("files[%d]"), Index));
                    continue;
                }

                FReflectionPolicyFile File;
                if (!TryGetStrictString(*FileObject, TEXT("path"), File.Path))
                {
                    AddError(Report, TEXT("INVALID_FILE_PATH"),
                        TEXT("files[].path는 string이어야 합니다."),
                        FString::Printf(TEXT("files[%d].path"), Index));
                }
                if (!TryGetStrictString(*FileObject, TEXT("role"), File.Role))
                {
                    AddError(Report, TEXT("INVALID_FILE_ROLE"),
                        TEXT("files[].role은 string이어야 합니다."),
                        FString::Printf(TEXT("files[%d].role"), Index));
                }
                if (File.Path.IsEmpty())
                {
                    AddError(Report, TEXT("REQUIRED_FIELD"),
                        TEXT("files[].path는 필수입니다."),
                        FString::Printf(TEXT("files[%d].path"), Index));
                }
                File.Path = NormalizePolicyPath(File.Path, PolicyDirectory);
                OutFiles.Add(File);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ModuleValues = nullptr;
        if (Document.RootObject->TryGetArrayField(TEXT("modules"), ModuleValues) && ModuleValues != nullptr)
        {
            for (int32 Index = 0; Index < ModuleValues->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject>* ModuleObject = nullptr;
                if (!(*ModuleValues)[Index]->TryGetObject(ModuleObject) ||
                    ModuleObject == nullptr || !ModuleObject->IsValid())
                {
                    AddError(Report, TEXT("INVALID_MODULE"),
                        TEXT("modules 항목은 object여야 합니다."),
                        FString::Printf(TEXT("modules[%d]"), Index));
                    continue;
                }

                FString ModuleName;
                FString Role;
                TArray<FString> IncludePaths;
                if (!TryGetStrictString(*ModuleObject, TEXT("name"), ModuleName))
                {
                    AddError(Report, TEXT("INVALID_MODULE_NAME"),
                        TEXT("modules[].name은 string이어야 합니다."),
                        FString::Printf(TEXT("modules[%d].name"), Index));
                }
                if (!TryGetStrictString(*ModuleObject, TEXT("role"), Role))
                {
                    AddError(Report, TEXT("INVALID_MODULE_ROLE"),
                        TEXT("modules[].role은 string이어야 합니다."),
                        FString::Printf(TEXT("modules[%d].role"), Index));
                }
                if (!TryGetStringArray(*ModuleObject, TEXT("include_paths"), IncludePaths))
                {
                    AddError(Report, TEXT("INVALID_INCLUDE_PATHS"),
                        TEXT("modules[].include_paths는 string 배열이어야 합니다."),
                        FString::Printf(TEXT("modules[%d].include_paths"), Index));
                }
                if (ModuleName.IsEmpty())
                {
                    AddError(Report, TEXT("REQUIRED_FIELD"),
                        TEXT("modules[].name은 필수입니다."),
                        FString::Printf(TEXT("modules[%d].name"), Index));
                    continue;
                }
                if (!UECommandForge::BuildCs::IsSafeModuleName(ModuleName))
                {
                    AddError(Report, TEXT("INVALID_MODULE_NAME"),
                        TEXT("module name은 C++ identifier여야 합니다."),
                        FString::Printf(TEXT("modules[%d].name"), Index));
                    continue;
                }

                const FString ModuleRoot = UECommandForge::BuildCs::ResolveModuleRoot(ModuleName);
                if (ModuleRoot.IsEmpty())
                {
                    AddIssue(Report, TEXT("CPP_REFLECTION_MODULE_NOT_FOUND"),
                        TEXT("module root를 찾을 수 없습니다."),
                        TEXT("modules.name"), ModuleName,
                        TEXT("sample Source 또는 plugin Source 아래의 module 이름을 사용하세요."));
                    continue;
                }

                if (IncludePaths.IsEmpty())
                {
                    IncludePaths.Add(TEXT("Public"));
                    IncludePaths.Add(TEXT("Private"));
                }
                for (const FString& IncludePath : IncludePaths)
                {
                    if (!IsSafeRelativeIncludePath(IncludePath))
                    {
                        AddError(Report, TEXT("INVALID_INCLUDE_PATH"),
                            TEXT("modules[].include_paths는 module root 안의 상대 경로여야 합니다."),
                            FString::Printf(TEXT("modules[%d].include_paths"), Index));
                    }
                }
                if (!Report.Errors.IsEmpty())
                {
                    continue;
                }
                for (const FString& IncludePath : IncludePaths)
                {
                    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(ModuleRoot, IncludePath));
                    if (FPaths::DirectoryExists(Root))
                    {
                        AddHeaderFilesUnderRoot(Root, Role, OutFiles);
                    }
                }
            }
        }

        if (OutFiles.IsEmpty() && Report.Errors.IsEmpty() && Report.ValidationIssues.IsEmpty())
        {
            AddError(Report, TEXT("REQUIRED_FIELD"),
                TEXT("files 또는 modules 중 하나 이상 필요합니다."),
                TEXT("files"));
        }
        return Report.Errors.IsEmpty();
    }

    FReflectionMacro ReadMacroAt(const TArray<FString>& Lines, int32& Index)
    {
        FReflectionMacro Macro;
        Macro.LineNumber = Index + 1;
        Macro.Text = Lines[Index].TrimStartAndEnd();
        if (Macro.Text.StartsWith(TEXT("UCLASS")))
        {
            Macro.Kind = TEXT("UCLASS");
        }
        else if (Macro.Text.StartsWith(TEXT("UPROPERTY")))
        {
            Macro.Kind = TEXT("UPROPERTY");
        }
        else if (Macro.Text.StartsWith(TEXT("UFUNCTION")))
        {
            Macro.Kind = TEXT("UFUNCTION");
        }

        int32 Balance = ParenDelta(Macro.Text);
        while (Balance > 0 && Index + 1 < Lines.Num())
        {
            ++Index;
            const FString NextLine = Lines[Index].TrimStartAndEnd();
            Macro.Text += TEXT(" ") + NextLine;
            Balance += ParenDelta(NextLine);
        }

        FString Declaration;
        const int32 DeclarationIndex = FindDeclarationLine(Lines, Index + 1, Declaration);
        Macro.Declaration = Declaration;
        Macro.bHasDeclaration = DeclarationIndex != INDEX_NONE;
        return Macro;
    }

    bool IsComponentPropertyDeclaration(const FString& Declaration)
    {
        return Declaration.Contains(TEXT("Component"), ESearchCase::CaseSensitive) &&
            (Declaration.Contains(TEXT("*")) || Declaration.Contains(TEXT("TObjectPtr")) ||
                Declaration.Contains(TEXT("TWeakObjectPtr")));
    }

    void ValidateHeaderFile(const FReflectionPolicyFile& File, FCommandForgeReport& Report)
    {
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *File.Path))
        {
            AddIssue(Report, TEXT("CPP_REFLECTION_FILE_READ_FAILED"),
                TEXT("C++ header 파일을 읽을 수 없습니다."),
                TEXT("files.path"), File.Path,
                TEXT("정책의 files[].path 또는 modules include 경로를 확인하세요."));
            return;
        }

        TArray<FString> Lines;
        Content.ParseIntoArrayLines(Lines, false);

        const bool bPolicyRoleIsActorComponent = File.Role.Equals(TEXT("ActorComponent"), ESearchCase::IgnoreCase);
        bool bCurrentClassHasConfig = false;
        bool bCurrentClassIsActorComponent = bPolicyRoleIsActorComponent;
        FString PendingClassMacro;
        int32 ReflectedPropertyCount = 0;
        int32 ReflectedFunctionCount = 0;

        for (int32 Index = 0; Index < Lines.Num(); ++Index)
        {
            const FString Trimmed = Lines[Index].TrimStartAndEnd();
            if (Trimmed.StartsWith(TEXT("UCLASS")))
            {
                const FReflectionMacro Macro = ReadMacroAt(Lines, Index);
                PendingClassMacro = Macro.Text;
                bCurrentClassHasConfig = ContainsToken(Macro.Text, TEXT("Config"));
                continue;
            }

            if (Trimmed.StartsWith(TEXT("class ")) || Trimmed.Contains(TEXT(" class ")))
            {
                bCurrentClassHasConfig = !PendingClassMacro.IsEmpty() && ContainsToken(PendingClassMacro, TEXT("Config"));
                bCurrentClassIsActorComponent = bPolicyRoleIsActorComponent ||
                    Trimmed.Contains(TEXT("UActorComponent"), ESearchCase::CaseSensitive);
                PendingClassMacro.Reset();
                continue;
            }

            if (!Trimmed.StartsWith(TEXT("UPROPERTY")) && !Trimmed.StartsWith(TEXT("UFUNCTION")))
            {
                continue;
            }

            const FReflectionMacro Macro = ReadMacroAt(Lines, Index);
            if (!Macro.bHasDeclaration)
            {
                AddIssue(Report, TEXT("CPP_REFLECTION_MACRO_WITHOUT_DECLARATION"),
                    TEXT("reflection macro 뒤에 연결된 C++ 선언이 없습니다."),
                    Macro.Kind, File.Path,
                    TEXT("UPROPERTY/UFUNCTION 바로 다음 non-empty 줄에 선언을 배치하세요."));
                continue;
            }

            if (Macro.Kind == TEXT("UPROPERTY"))
            {
                ++ReflectedPropertyCount;
                if (!ContainsToken(Macro.Text, TEXT("Category")))
                {
                    AddIssue(Report, TEXT("CPP_REFLECTION_PROPERTY_CATEGORY_MISSING"),
                        TEXT("UPROPERTY에 Category metadata가 없습니다."),
                        TEXT("UPROPERTY"), File.Path,
                        TEXT("Category=\"...\"를 추가하세요."));
                }
                if (ContainsToken(Macro.Text, TEXT("EditAnywhere")) &&
                    ContainsToken(Macro.Text, TEXT("BlueprintReadWrite")))
                {
                    AddIssue(Report, TEXT("CPP_REFLECTION_RUNTIME_MUTABLE_EDITABLE"),
                        TEXT("runtime mutable state에 EditAnywhere + BlueprintReadWrite 조합을 사용할 수 없습니다."),
                        TEXT("UPROPERTY"), File.Path,
                        TEXT("EditDefaultsOnly 또는 BlueprintReadOnly로 노출 범위를 줄이세요."));
                }
                if (bCurrentClassIsActorComponent && IsComponentPropertyDeclaration(Macro.Declaration) &&
                    !ContainsToken(Macro.Text, TEXT("VisibleAnywhere")))
                {
                    AddIssue(Report, TEXT("CPP_REFLECTION_COMPONENT_NOT_VISIBLEANYWHERE"),
                        TEXT("component 참조 property는 VisibleAnywhere 노출을 권장합니다."),
                        TEXT("UPROPERTY"), File.Path,
                        TEXT("EditAnywhere 대신 VisibleAnywhere를 사용하세요."));
                }
                if (ContainsToken(Macro.Text, TEXT("Config")) && !bCurrentClassHasConfig)
                {
                    AddIssue(Report, TEXT("CPP_REFLECTION_CONFIG_WITHOUT_CONFIG_CLASS"),
                        TEXT("Config property가 Config UCLASS가 아닌 class에 선언됐습니다."),
                        TEXT("UPROPERTY"), File.Path,
                        TEXT("UCLASS(Config=...)를 추가하거나 property의 Config metadata를 제거하세요."));
                }
            }
            else if (Macro.Kind == TEXT("UFUNCTION"))
            {
                ++ReflectedFunctionCount;
                if (ContainsToken(Macro.Text, TEXT("BlueprintCallable")) &&
                    !ContainsToken(Macro.Text, TEXT("Category")))
                {
                    AddIssue(Report, TEXT("CPP_REFLECTION_FUNCTION_CATEGORY_MISSING"),
                        TEXT("BlueprintCallable UFUNCTION에 Category metadata가 없습니다."),
                        TEXT("UFUNCTION"), File.Path,
                        TEXT("Category=\"...\"를 추가하세요."));
                }
            }
        }

        Report.Validation.Add(File.Path + TEXT(".property_count"), FString::FromInt(ReflectedPropertyCount));
        Report.Validation.Add(File.Path + TEXT(".function_count"), FString::FromInt(ReflectedFunctionCount));
    }
}

int32 UValidateCppReflectionCommandlet::Main(const FString& Params)
{
    namespace ReflectionPrivate = UECommandForge::ValidateCppReflectionPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString PolicyPath = ParamsMap.FindRef(TEXT("Policy"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidateCppReflection");
    Report.bDryRun = true;
    Report.bApplied = false;

    if (OutPath.IsEmpty())
    {
        ReflectionPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Output 인자가 없습니다."), TEXT("Output"));
        Report.bOk = false;
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (PolicyPath.IsEmpty())
    {
        ReflectionPrivate::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Policy 인자가 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString PolicyJson;
    if (!FFileHelper::LoadFileToString(PolicyJson, *PolicyPath))
    {
        ReflectionPrivate::AddError(Report, TEXT("POLICY_READ_FAILED"), TEXT("reflection policy 파일을 읽을 수 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgePolicyDocument Document;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FCommandForgePolicyParser::ParseJson(PolicyJson, Document, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TArray<ReflectionPrivate::FReflectionPolicyFile> Files;
    const FString PolicyDirectory = FPaths::GetPath(FPaths::ConvertRelativePathToFull(PolicyPath));
    if (!ReflectionPrivate::ParsePolicy(Document, PolicyDirectory, Files, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    for (const ReflectionPrivate::FReflectionPolicyFile& File : Files)
    {
        ReflectionPrivate::ValidateHeaderFile(File, Report);
    }

    Report.Validation.Add(TEXT("checked_file_count"), FString::FromInt(Files.Num()));
    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() && !ReflectionPrivate::HasErrorIssue(Report.ValidationIssues);
    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
