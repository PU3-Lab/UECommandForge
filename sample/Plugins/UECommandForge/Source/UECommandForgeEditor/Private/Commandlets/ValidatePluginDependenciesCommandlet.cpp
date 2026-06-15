#include "Commandlets/ValidatePluginDependenciesCommandlet.h"

#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
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

    bool LoadEnabledPlugins(const FString& ProjectPath, const FString& TargetPlatform, const FString& TargetConfig, TMap<FString, bool>& OutEnabled,
        FCommandForgeReport& Report)
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

        TMap<FString, TSharedRef<IPlugin>> DiscoveredMap;
        for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
        {
            DiscoveredMap.Add(Plugin->GetName(), Plugin);
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

        for (const auto& Pair : DiscoveredMap)
        {
            const FString& PluginName = Pair.Key;
            if (UProjectPlugins.Contains(PluginName))
            {
                continue;
            }

            const TSharedRef<IPlugin>& Plugin = Pair.Value;
            const FPluginDescriptor& Desc = Plugin->GetDescriptor();

            bool bEnabled = (Desc.EnabledByDefault == EPluginEnabledByDefault::Enabled);
            if (bEnabled)
            {
                bEnabled = Desc.SupportsTargetPlatform(TargetPlatform);
            }

            OutEnabled.Add(PluginName, bEnabled);
        }

        return true;
    }

    // True if the plugin (located under the target project's Plugins dir) has a module
    // whose type ships into runtime/cooked builds. If the descriptor can't be found,
    // returns true conservatively (avoid silently passing a real shipping risk).
    bool PluginHasRuntimeModule(const FString& ProjectPath, const FString& PluginName)
    {
        const FString PluginsDir = FPaths::Combine(FPaths::GetPath(ProjectPath), TEXT("Plugins"));
        TArray<FString> Found;
        IFileManager::Get().FindFilesRecursive(Found, *PluginsDir,
            *(PluginName + TEXT(".uplugin")), true, false);
        if (Found.Num() == 0) { return true; }
        FPluginDescriptor Desc;
        FText FailReason;
        if (!Desc.Load(Found[0], FailReason)) { return true; }
        for (const FModuleDescriptor& Module : Desc.Modules)
        {
            switch (Module.Type)
            {
                case EHostType::Runtime:
                case EHostType::RuntimeNoCommandlet:
                case EHostType::RuntimeAndProgram:
                case EHostType::ClientOnly:
                case EHostType::ServerOnly:
                case EHostType::ClientOnlyNoCommandlet:
                case EHostType::CookedOnly:
                    return true;
                default:
                    break;
            }
        }
        return false;
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

    TMap<FString, TSharedRef<IPlugin>> DiscoveredMap;
    for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
    {
        DiscoveredMap.Add(Plugin->GetName(), Plugin);
    }

    const FString ProjectPath = ParamsMap.FindRef(TEXT("Project"));
    TMap<FString, bool> EnabledPlugins;
    if (!ProjectPath.IsEmpty())
    {
        if (!Private::LoadEnabledPlugins(ProjectPath, TargetPlatform, Configuration, EnabledPlugins, Report))
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
            if (Enabled != nullptr && *Enabled &&
                Private::PluginHasRuntimeModule(ProjectPath, Name))
            {
                Private::AddIssue(Report, TEXT("error"), TEXT("PLUGIN_FORBIDDEN_IN_SHIPPING_ENABLED"),
                    TEXT("Plugin is forbidden in Shipping configuration but currently enabled."),
                    FString::Printf(TEXT("Plugins.%s.Enabled"), *Name), ProjectPath,
                    FString::Printf(TEXT("Disable %s for Shipping builds."), *Name));
            }
        }
    }

    // Editor 모듈 warning 및 모듈 타입 의심 검사
    for (const auto& Pair : DiscoveredMap)
    {
        const FString& PluginName = Pair.Key;
        const bool* bEnabledPtr = EnabledPlugins.Find(PluginName);
        if (bEnabledPtr == nullptr || !*bEnabledPtr)
        {
            continue;
        }

        const TSharedRef<IPlugin>& Plugin = Pair.Value;
        if (Plugin->GetLoadedFrom() != EPluginLoadedFrom::Project)
        {
            continue;
        }

        const FPluginDescriptor& Desc = Plugin->GetDescriptor();
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
                    Plugin->GetDescriptorFileName(),
                    TEXT("Change the module host type to 'DeveloperTool' or 'UncookedOnly'."));
            }
        }

        if (bHasRuntimeModule && bHasEditorModule)
        {
            Private::AddIssue(Report, TEXT("warning"), TEXT("PLUGIN_EDITOR_MODULE_IN_RUNTIME_PLUGIN"),
                FString::Printf(TEXT("Plugin '%s' contains both Runtime and Editor modules. Editor modules in runtime plugins are discouraged."), *PluginName),
                FString::Printf(TEXT("Plugins.%s.Modules"), *PluginName),
                Plugin->GetDescriptorFileName(),
                TEXT("Split editor features into a separate editor-only plugin or restructure modules to decouple runtime from editor."));
        }
    }

    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bOk = Report.Errors.IsEmpty() && !Private::HasErrorIssue(Report.ValidationIssues);
    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote) { return static_cast<int32>(ECommandForgeExitCode::EngineError); }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
