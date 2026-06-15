#include "Commandlets/ValidatePluginDependenciesCommandlet.h"

#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PluginDescriptor.h"
#include "ProjectDescriptor.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/CommandForgePolicyParser.h"

UValidatePluginDependenciesCommandlet::UValidatePluginDependenciesCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::ValidatePluginDepsPrivate
{
    struct FPluginPolicy
    {
        TArray<FString> Required;
        TArray<FString> ForbiddenInShipping;
        TArray<FString> Optional;
        TArray<FString> AllowedEditorOnly;
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

    bool TryGetStringArray(const TSharedPtr<FJsonObject>& Object, const FString& FieldName,
        TArray<FString>& OutValues)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(FieldName, Values) || Values == nullptr)
        {
            return true; // optional field
        }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString S;
            if (!Value->TryGetString(S)) { return false; }
            OutValues.Add(S.TrimStartAndEnd());
        }
        return true;
    }

    bool ParsePolicy(const FCommandForgePolicyDocument& Document, FPluginPolicy& Out,
        FCommandForgeReport& Report)
    {
        if (!Document.Kind.Equals(TEXT("plugin_dependency_policy"), ESearchCase::IgnoreCase))
        {
            AddError(Report, TEXT("POLICY_KIND_MISMATCH"),
                TEXT("정책 kind는 'plugin_dependency_policy'여야 합니다."), TEXT("kind"));
            return false;
        }
        bool bValid = true;
        bValid &= TryGetStringArray(Document.RootObject, TEXT("required"), Out.Required);
        bValid &= TryGetStringArray(Document.RootObject, TEXT("forbiddenInShipping"), Out.ForbiddenInShipping);
        bValid &= TryGetStringArray(Document.RootObject, TEXT("optional"), Out.Optional);
        bValid &= TryGetStringArray(Document.RootObject, TEXT("allowedEditorOnly"), Out.AllowedEditorOnly);
        if (!bValid)
        {
            AddError(Report, TEXT("PLUGIN_POLICY_INVALID"),
                TEXT("정책 배열은 string 배열이어야 합니다."), TEXT("policy"));
        }
        return bValid;
    }

    // 대상 프로젝트 Plugins → 엔진 Plugins 순으로 <Name>.uplugin 을 찾아 로드.
    bool ResolvePluginDescriptor(const FString& ProjectPath, const FString& EnginePluginsDir, const FString& PluginName,
        FPluginDescriptor& OutDesc, FString& OutDescPath)
    {
        const TArray<FString> SearchDirs = {
            FPaths::Combine(FPaths::GetPath(ProjectPath), TEXT("Plugins")),
            EnginePluginsDir
        };
        for (const FString& Dir : SearchDirs)
        {
            TArray<FString> Found;
            IFileManager::Get().FindFilesRecursive(Found, *Dir,
                *(PluginName + TEXT(".uplugin")), true, false);
            if (Found.Num() > 0)
            {
                FText FailReason;
                if (OutDesc.Load(Found[0], FailReason)) { OutDescPath = Found[0]; return true; }
            }
        }
        return false;
    }

    bool DescriptorHasRuntimeModule(const FPluginDescriptor& Desc)
    {
        for (const FModuleDescriptor& M : Desc.Modules)
        {
            switch (M.Type)
            {
                case EHostType::Runtime: case EHostType::RuntimeNoCommandlet:
                case EHostType::RuntimeAndProgram: case EHostType::ClientOnly:
                case EHostType::ServerOnly: case EHostType::ClientOnlyNoCommandlet:
                case EHostType::CookedOnly: return true;
                default: break;
            }
        }
        return false;
    }

    bool LoadEnabledPlugins(const FString& ProjectPath, const FString& EnginePluginsDir,
        const TArray<FString>& PolicyPlugins, const FString& TargetPlatform, const FString& TargetConfig,
        TMap<FString, bool>& OutEnabled, FCommandForgeReport& Report)
    {
        FProjectDescriptor Descriptor;
        FText FailReason;
        if (!Descriptor.Load(ProjectPath, FailReason))
        {
            AddError(Report, TEXT("PROJECT_READ_FAILED"),
                FString::Printf(TEXT(".uproject 로드 실패: %s"), *FailReason.ToString()),
                TEXT("Project"));
            return false;
        }

        EBuildConfiguration TargetConfigEnum = EBuildConfiguration::Development;
        if (TargetConfig.Equals(TEXT("Shipping"), ESearchCase::IgnoreCase))
        {
            TargetConfigEnum = EBuildConfiguration::Shipping;
        }

        TSet<FString> UProjectPlugins;
        int32 EnabledCount = 0;
        for (const FPluginReferenceDescriptor& Ref : Descriptor.Plugins)
        {
            UProjectPlugins.Add(Ref.Name);

            bool bEnabled = Ref.bEnabled;
            if (bEnabled)
            {
                bEnabled = Ref.IsEnabledForPlatform(TargetPlatform) &&
                           Ref.IsEnabledForTargetConfiguration(TargetConfigEnum) &&
                           Ref.IsEnabledForTarget(EBuildTargetType::Game) &&
                           Ref.IsSupportedTargetPlatform(TargetPlatform);
            }

            if (bEnabled)
            {
                ++EnabledCount;
            }

            OutEnabled.Add(Ref.Name, bEnabled);
        }
        Report.Validation.Add(TEXT("enabled_plugin_count"), FString::FromInt(EnabledCount));

        for (const FString& PluginName : PolicyPlugins)
        {
            if (UProjectPlugins.Contains(PluginName))
            {
                continue;
            }

            FPluginDescriptor Desc;
            FString DescPath;
            if (ResolvePluginDescriptor(ProjectPath, EnginePluginsDir, PluginName, Desc, DescPath))
            {
                bool bEnabled = (Desc.EnabledByDefault == EPluginEnabledByDefault::Enabled);
                if (bEnabled)
                {
                    bEnabled = Desc.SupportsTargetPlatform(TargetPlatform);
                }
                OutEnabled.Add(PluginName, bEnabled);
            }
        }

        return true;
    }

}

int32 UValidatePluginDependenciesCommandlet::Main(const FString& Params)
{
    namespace Private = UECommandForge::ValidatePluginDepsPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString PolicyPath = ParamsMap.FindRef(TEXT("Policy"));
    FString EnginePluginsDir = ParamsMap.FindRef(TEXT("TestEnginePluginsDir"));
    if (EnginePluginsDir.IsEmpty())
    {
        EnginePluginsDir = FPaths::EnginePluginsDir();
    }

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("ValidatePluginDependencies");
    Report.bDryRun = true;

    if (OutPath.IsEmpty())
    {
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }
    if (PolicyPath.IsEmpty())
    {
        Private::AddError(Report, TEXT("MISSING_ARG"), TEXT("-Policy 인자가 없습니다."), TEXT("Policy"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FString PolicyJson;
    if (!FFileHelper::LoadFileToString(PolicyJson, *PolicyPath))
    {
        Private::AddError(Report, TEXT("POLICY_READ_FAILED"),
            TEXT("정책 파일을 읽을 수 없습니다."), TEXT("Policy"));
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

    Private::FPluginPolicy Policy;
    if (!Private::ParsePolicy(Document, Policy, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    const FString Configuration = ParamsMap.FindRef(TEXT("Configuration"));
    Report.Validation.Add(TEXT("configuration"),
        Configuration.IsEmpty() ? TEXT("Development") : Configuration);

    FString TargetPlatform = ParamsMap.FindRef(TEXT("TargetPlatform"));
    if (TargetPlatform.IsEmpty())
    {
#if PLATFORM_MAC
        TargetPlatform = TEXT("Mac");
#elif PLATFORM_WINDOWS
        TargetPlatform = TEXT("Win64");
#elif PLATFORM_LINUX
        TargetPlatform = TEXT("Linux");
#else
        TargetPlatform = FPlatformProperties::IniPlatformName();
#endif
    }
    Report.Validation.Add(TEXT("target_platform"), TargetPlatform);

    const FString ProjectPath = ParamsMap.FindRef(TEXT("Project"));
    TMap<FString, bool> EnabledPlugins;
    if (!ProjectPath.IsEmpty())
    {
        TArray<FString> PolicyPlugins;
        PolicyPlugins.Append(Policy.Required);
        PolicyPlugins.Append(Policy.ForbiddenInShipping);
        PolicyPlugins.Append(Policy.Optional);
        PolicyPlugins.Append(Policy.AllowedEditorOnly);

        if (!Private::LoadEnabledPlugins(ProjectPath, EnginePluginsDir, PolicyPlugins, TargetPlatform, Configuration, EnabledPlugins, Report))
        {
            Report.bOk = false;
            UECommandForge::FJsonReportWriter::Write(OutPath, Report);
            return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
        }
    }


    for (const FString& Name : Policy.Required)
    {
        const bool* Enabled = EnabledPlugins.Find(Name);
        if (Enabled == nullptr)
        {
            Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_REQUIRED_MISSING"),
                TEXT("Required plugin is missing."),
                FString::Printf(TEXT("required[%s]"), *Name), ProjectPath,
                TEXT("Enable the plugin in the project or remove it from the required policy."));
        }
        else if (!*Enabled)
        {
            Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_REQUIRED_DISABLED"),
                TEXT("Required plugin is present but disabled."),
                FString::Printf(TEXT("required[%s]"), *Name), ProjectPath,
                TEXT("Set Enabled=true for this plugin in the .uproject."));
        }
    }

    const bool bShipping = Configuration.Equals(TEXT("Shipping"), ESearchCase::IgnoreCase);
    if (bShipping)
    {
        for (const FString& Name : Policy.ForbiddenInShipping)
        {
            const bool* Enabled = EnabledPlugins.Find(Name);
            if (Enabled == nullptr || !*Enabled) { continue; }

            FPluginDescriptor Desc;
            FString DescPath;
            FString Msg = TEXT("Plugin is forbidden in Shipping configuration but currently enabled.");

            if (Private::ResolvePluginDescriptor(ProjectPath, EnginePluginsDir, Name, Desc, DescPath))
            {
                if (!Private::DescriptorHasRuntimeModule(Desc))
                {
                    continue; // editor-only → 제외(오탐 방지)
                }
            }
            else
            {
                Msg = TEXT("Plugin is forbidden in Shipping configuration but currently enabled (descriptor 미해석으로 보수적 위반 처리).");
            }

            Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED"),
                Msg,
                FString::Printf(TEXT("Plugins.%s.Enabled"), *Name), ProjectPath,
                FString::Printf(TEXT("Disable %s for Shipping builds."), *Name));
        }
    }

    // Editor 모듈 warning 및 모듈 타입 의심 검사
    if (!ProjectPath.IsEmpty())
    {
        const FString ProjectPluginsDir = FPaths::Combine(FPaths::GetPath(ProjectPath), TEXT("Plugins"));
        TArray<FString> FoundUPlugins;
        IFileManager::Get().FindFilesRecursive(FoundUPlugins, *ProjectPluginsDir, TEXT("*.uplugin"), true, false);

        for (const FString& UPluginPath : FoundUPlugins)
        {
            FPluginDescriptor Desc;
            FText FailReason;
            if (!Desc.Load(UPluginPath, FailReason))
            {
                continue;
            }

            FString PluginName = FPaths::GetBaseFilename(UPluginPath);

            const bool* bEnabledPtr = EnabledPlugins.Find(PluginName);
            if (bEnabledPtr == nullptr || !*bEnabledPtr)
            {
                continue;
            }

            bool bHasRuntimeModule = false;
            bool bHasEditorModule = false;

            for (const FModuleDescriptor& Module : Desc.Modules)
            {
                if (Module.Type == EHostType::Runtime ||
                    Module.Type == EHostType::RuntimeNoCommandlet ||
                    Module.Type == EHostType::RuntimeAndProgram ||
                    Module.Type == EHostType::ClientOnly ||
                    Module.Type == EHostType::ServerOnly ||
                    Module.Type == EHostType::ClientOnlyNoCommandlet ||
                    Module.Type == EHostType::CookedOnly)
                {
                    bHasRuntimeModule = true;
                }

                if (Module.Type == EHostType::Editor ||
                    Module.Type == EHostType::EditorNoCommandlet)
                {
                    bHasEditorModule = true;
                }

                if (Module.Type == EHostType::Developer)
                {
                    Private::AddIssue(Report, TEXT("warning"), TEXT("PLUGIN_MODULE_TYPE_SUSPECT"),
                        FString::Printf(TEXT("Module '%s' uses deprecated 'Developer' host type."), *Module.Name.ToString()),
                        FString::Printf(TEXT("Plugins.%s.Modules.%s.Type"), *PluginName, *Module.Name.ToString()),
                        UPluginPath,
                        TEXT("Change the module host type to 'DeveloperTool' or 'UncookedOnly'."));
                }
            }

            if (bHasRuntimeModule && bHasEditorModule)
            {
                Private::AddIssue(Report, TEXT("warning"), TEXT("PLUGIN_EDITOR_MODULE_IN_RUNTIME_PLUGIN"),
                    FString::Printf(TEXT("Plugin '%s' contains both Runtime and Editor modules. Editor modules in runtime plugins are discouraged."), *PluginName),
                    FString::Printf(TEXT("Plugins.%s.Modules"), *PluginName),
                    UPluginPath,
                    TEXT("Split editor features into a separate editor-only plugin or restructure modules to decouple runtime from editor."));
            }
        }
    }

    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() && !Private::HasErrorIssue(Report.ValidationIssues);
    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote) { return static_cast<int32>(ECommandForgeExitCode::EngineError); }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
