#include "Commandlets/DiffPlatformConfigCommandlet.h"

#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigContext.h"
#include "Misc/ConfigCacheIni.h"
#include "Reports/JsonReportWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UDiffPlatformConfigCommandlet::UDiffPlatformConfigCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::DiffPlatformConfigPrivate
{
    struct FAllowlistEntry
    {
        FString SectionPattern;
        FString KeyPattern;
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
        Issue.Severity = Severity;
        Issue.Code = Code;
        Issue.Message = Message;
        Issue.Field = Field;
        Issue.FilePath = FilePath;
        Issue.SuggestedFix = SuggestedFix;
        Report.ValidationIssues.Add(Issue);
    }

    bool HasErrorIssue(const TArray<FCommandForgeValidationIssue>& Issues)
    {
        for (const FCommandForgeValidationIssue& I : Issues)
        {
            if (I.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase)) { return true; }
        }
        return false;
    }

    bool LoadAllowlist(const FString& Path, TArray<FAllowlistEntry>& OutAllowlist,
        FCommandForgeReport& Report)
    {
        if (Path.IsEmpty()) { return true; }
        FString Content;
        if (!FFileHelper::LoadFileToString(Content, *Path))
        {
            AddError(Report, TEXT("CONFIG_ALLOWLIST_READ_FAILED"),
                TEXT("Allowlist 파일을 읽을 수 없습니다."), TEXT("Allowlist"));
            return false;
        }

        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        TArray<TSharedPtr<FJsonValue>> RootArray;
        if (FJsonSerializer::Deserialize(Reader, RootArray))
        {
            for (const auto& Val : RootArray)
            {
                TSharedPtr<FJsonObject> Obj = Val->AsObject();
                if (Obj.IsValid())
                {
                    FAllowlistEntry Entry;
                    if (Obj->TryGetStringField(TEXT("section"), Entry.SectionPattern) &&
                        Obj->TryGetStringField(TEXT("key"), Entry.KeyPattern))
                    {
                        OutAllowlist.Add(Entry);
                    }
                }
            }
            return true;
        }

        // fallback to object with allowlist field
        TSharedPtr<FJsonObject> RootObj;
        Reader = TJsonReaderFactory<>::Create(Content);
        if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
            if (RootObj->TryGetArrayField(TEXT("allowlist"), ArrayPtr) && ArrayPtr)
            {
                for (const auto& Val : *ArrayPtr)
                {
                    TSharedPtr<FJsonObject> Obj = Val->AsObject();
                    if (Obj.IsValid())
                    {
                        FAllowlistEntry Entry;
                        if (Obj->TryGetStringField(TEXT("section"), Entry.SectionPattern) &&
                            Obj->TryGetStringField(TEXT("key"), Entry.KeyPattern))
                        {
                            OutAllowlist.Add(Entry);
                        }
                    }
                }
                return true;
            }
        }

        AddError(Report, TEXT("CONFIG_ALLOWLIST_INVALID"),
            TEXT("Allowlist 파일 형식이 유효하지 않습니다."), TEXT("Allowlist"));
        return false;
    }

    bool IsAllowlisted(const FString& Section, const FString& Key,
        const TArray<FAllowlistEntry>& Allowlist)
    {
        for (const FAllowlistEntry& Entry : Allowlist)
        {
            if (Section.MatchesWildcard(Entry.SectionPattern, ESearchCase::IgnoreCase) &&
                Key.MatchesWildcard(Entry.KeyPattern, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }
}

int32 UDiffPlatformConfigCommandlet::Main(const FString& Params)
{
    namespace Private = UECommandForge::DiffPlatformConfigPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString AllowlistPath = ParamsMap.FindRef(TEXT("Allowlist"));
    const FString ProjectPath = ParamsMap.FindRef(TEXT("Project"));

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("DiffPlatformConfig");
    Report.bDryRun = true;

    if (OutPath.IsEmpty())
    {
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    TArray<Private::FAllowlistEntry> Allowlist;
    if (!AllowlistPath.IsEmpty())
    {
        if (!Private::LoadAllowlist(AllowlistPath, Allowlist, Report))
        {
            Report.bOk = false;
            UECommandForge::FJsonReportWriter::Write(OutPath, Report);
            return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
        }
    }

    // Platforms 파싱
    FString PlatformsStr = ParamsMap.FindRef(TEXT("Platforms"));
    if (PlatformsStr.IsEmpty())
    {
        PlatformsStr = TEXT("Windows,Mac,Linux");
    }
    TArray<FString> Platforms;
    PlatformsStr.ParseIntoArray(Platforms, TEXT(","), true);
    for (FString& P : Platforms)
    {
        P = P.TrimStartAndEnd();
    }

    // Categories 파싱
    FString CategoriesStr = ParamsMap.FindRef(TEXT("Categories"));
    if (CategoriesStr.IsEmpty())
    {
        CategoriesStr = TEXT("Engine,Game");
    }
    TArray<FString> Categories;
    CategoriesStr.ParseIntoArray(Categories, TEXT(","), true);
    for (FString& C : Categories)
    {
        C = C.TrimStartAndEnd();
    }

    Report.Validation.Add(TEXT("platforms"), PlatformsStr);
    Report.Validation.Add(TEXT("categories"), CategoriesStr);

    TMap<FString, TMap<FString, FConfigFile>> PlatformConfigs;
    bool bLoadError = false;

    // 각 플랫폼별로 각 카테고리 로드
    for (const FString& Platform : Platforms)
    {
        for (const FString& Category : Categories)
        {
            FConfigFile& File = PlatformConfigs.FindOrAdd(Platform).FindOrAdd(Category);
            FConfigContext Context = FConfigContext::ReadIntoLocalFile(File, Platform);
            if (!Context.Load(*Category))
            {
                Private::AddError(Report, TEXT("CONFIG_CATEGORY_LOAD_FAILED"),
                    FString::Printf(TEXT("플랫폼 '%s'의 카테고리 '%s' 로드 실패"), *Platform, *Category),
                    Category);
                bLoadError = true;
            }
        }
    }

    if (bLoadError)
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }

    int32 DiffCount = 0;
    int32 SuppressedDiffCount = 0;

    // 카테고리별로 비교 진행
    for (const FString& Category : Categories)
    {
        // 전체 섹션 수집
        TSet<FString> AllSections;
        for (const FString& Platform : Platforms)
        {
            const FConfigFile& File = PlatformConfigs[Platform][Category];
            for (const auto& Pair : AsConst(File))
            {
                AllSections.Add(Pair.Key);
            }
        }

        for (const FString& Section : AllSections)
        {
            // 이 섹션 내의 모든 키 수집
            TSet<FString> AllKeys;
            for (const FString& Platform : Platforms)
            {
                const FConfigFile& File = PlatformConfigs[Platform][Category];
                const FConfigSection* Sec = File.FindSection(Section);
                if (Sec)
                {
                    for (FConfigSection::TConstIterator It(*Sec); It; ++It)
                    {
                        AllKeys.Add(It.Key().ToString());
                    }
                }
            }

            for (const FString& Key : AllKeys)
            {
                TMap<FString, FString> Values;
                TArray<FString> MissingPlatforms;
                FString FirstVal;
                bool bHasFirst = false;
                bool bDiffFound = false;

                for (const FString& Platform : Platforms)
                {
                    const FConfigFile& File = PlatformConfigs[Platform][Category];
                    const FConfigSection* Sec = File.FindSection(Section);
                    if (Sec)
                    {
                        const FConfigValue* Val = Sec->Find(*Key);
                        if (Val)
                        {
                            FString SVal = Val->GetValue();
                            Values.Add(Platform, SVal);
                            if (!bHasFirst)
                            {
                                FirstVal = SVal;
                                bHasFirst = true;
                            }
                            else if (SVal != FirstVal)
                            {
                                bDiffFound = true;
                            }
                        }
                        else
                        {
                            MissingPlatforms.Add(Platform);
                            bDiffFound = true;
                        }
                    }
                    else
                    {
                        MissingPlatforms.Add(Platform);
                        bDiffFound = true;
                    }
                }

                if (bDiffFound)
                {
                    bool bAllowlisted = Private::IsAllowlisted(Section, Key, Allowlist);
                    if (bAllowlisted)
                    {
                        ++SuppressedDiffCount;
                    }
                    else
                    {
                        ++DiffCount;
                        if (MissingPlatforms.Num() > 0)
                        {
                            FString MissingList = FString::Join(MissingPlatforms, TEXT(", "));
                            Private::AddIssue(Report, TEXT("warning"), TEXT("CONFIG_PLATFORM_KEY_MISSING"),
                                FString::Printf(TEXT("Key '%s' in section '%s' is missing on platform(s): %s"), *Key, *Section, *MissingList),
                                FString::Printf(TEXT("%s.%s"), *Section, *Key),
                                ProjectPath,
                                TEXT("Add the key for the missing platforms or register to the allowlist."));
                        }
                        else
                        {
                            FString ValueDetails;
                            for (const auto& ValPair : Values)
                            {
                                ValueDetails += FString::Printf(TEXT("[%s]: %s "), *ValPair.Key, *ValPair.Value);
                            }
                            Private::AddIssue(Report, TEXT("warning"), TEXT("CONFIG_PLATFORM_VALUE_DIFF"),
                                FString::Printf(TEXT("Value mismatch for key '%s' in section '%s'. Values: %s"), *Key, *Section, *ValueDetails),
                                FString::Printf(TEXT("%s.%s"), *Section, *Key),
                                ProjectPath,
                                TEXT("Resolve the configuration difference or register to the allowlist."));
                        }
                    }
                }
            }
        }
    }

    Report.Validation.Add(TEXT("diff_count"), FString::FromInt(DiffCount));
    Report.Validation.Add(TEXT("suppressed_diff_count"), FString::FromInt(SuppressedDiffCount));

    // warning만 존재할 경우 ok = false (설계 §2.4 표: allowlist 밖 config 차이 존재 시 ok = false)
    Report.bOk = Report.Errors.IsEmpty() && (DiffCount == 0);

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote) { return static_cast<int32>(ECommandForgeExitCode::EngineError); }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
