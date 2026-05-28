#include "Commandlets/GenerateCppClassCommandlet.h"
#include "BuildCs/BuildCsDependencyUtils.h"
#include "CommandForgeTypes.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Reports/JsonReportWriter.h"
#include "Specs/CommandForgePolicyParser.h"
#include "Specs/CppClassSpec.h"

UGenerateCppClassCommandlet::UGenerateCppClassCommandlet()
{
    IsClient     = false;
    IsServer     = false;
    IsEditor     = true;
    LogToConsole = true;
}

namespace UECommandForge::GenerateCppClassPrivate
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

    FString NormalizeRelativePath(const FString& RawPath)
    {
        return RawPath.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/"));
    }

    bool ContainsWildcard(const FString& Path)
    {
        return Path.Contains(TEXT("*")) || Path.Contains(TEXT("?"));
    }

    bool IsValidIdentifier(const FString& Value)
    {
        if (Value.IsEmpty() || !(FChar::IsAlpha(Value[0]) || Value[0] == TEXT('_')))
        {
            return false;
        }

        for (const TCHAR Character : Value)
        {
            if (!(FChar::IsAlnum(Character) || Character == TEXT('_')))
            {
                return false;
            }
        }
        return true;
    }

    bool ContainsAnyChar(const FString& Value, const FString& Characters)
    {
        for (const TCHAR Character : Value)
        {
            int32 Index = INDEX_NONE;
            if (Characters.FindChar(Character, Index))
            {
                return true;
            }
        }
        return false;
    }

    bool IsSafeIncludePath(const FString& Value)
    {
        if (Value.IsEmpty() || Value.Contains(TEXT("..")) || FPaths::IsRelative(Value) == false)
        {
            return false;
        }
        return !ContainsAnyChar(Value, TEXT("\\\r\n\"'<>;{}#"));
    }

    bool IsSafeCppTypeToken(const FString& Value)
    {
        return !Value.TrimStartAndEnd().IsEmpty() && !ContainsAnyChar(Value, TEXT("\r\n\"'`;{}#"));
    }

    bool IsSafeMetadataValue(const FString& Value)
    {
        return !ContainsAnyChar(Value, TEXT("\r\n\"\\"));
    }

    FString ModuleApiMacro(const FString& ModuleName)
    {
        FString Api = ModuleName.ToUpper();
        Api.ReplaceInline(TEXT("-"), TEXT("_"));
        return Api + TEXT("_API");
    }

    FString EnsureUnrealClassSymbol(const FString& ClassName, const FString& Type)
    {
        if (ClassName.StartsWith(TEXT("U")) || ClassName.StartsWith(TEXT("A")) || ClassName.StartsWith(TEXT("I")))
        {
            return ClassName;
        }

        if (Type.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
        {
            return TEXT("A") + ClassName;
        }
        if (Type.Equals(TEXT("Interface"), ESearchCase::IgnoreCase))
        {
            return TEXT("I") + ClassName;
        }
        return TEXT("U") + ClassName;
    }

    FString DefaultBaseClass(const FString& Type)
    {
        if (Type.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
        {
            return TEXT("AActor");
        }
        if (Type.Equals(TEXT("ActorComponent"), ESearchCase::IgnoreCase))
        {
            return TEXT("UActorComponent");
        }
        if (Type.Equals(TEXT("DataAsset"), ESearchCase::IgnoreCase))
        {
            return TEXT("UDataAsset");
        }
        if (Type.Equals(TEXT("GameInstanceSubsystem"), ESearchCase::IgnoreCase))
        {
            return TEXT("UGameInstanceSubsystem");
        }
        if (Type.Equals(TEXT("WorldSubsystem"), ESearchCase::IgnoreCase))
        {
            return TEXT("UWorldSubsystem");
        }
        if (Type.Equals(TEXT("DeveloperSettings"), ESearchCase::IgnoreCase))
        {
            return TEXT("UDeveloperSettings");
        }
        return TEXT("UObject");
    }

    FString DefaultIncludeForType(const FString& Type)
    {
        if (Type.Equals(TEXT("Actor"), ESearchCase::IgnoreCase))
        {
            return TEXT("GameFramework/Actor.h");
        }
        if (Type.Equals(TEXT("ActorComponent"), ESearchCase::IgnoreCase))
        {
            return TEXT("Components/ActorComponent.h");
        }
        if (Type.Equals(TEXT("DataAsset"), ESearchCase::IgnoreCase))
        {
            return TEXT("Engine/DataAsset.h");
        }
        if (Type.Equals(TEXT("GameInstanceSubsystem"), ESearchCase::IgnoreCase))
        {
            return TEXT("Subsystems/GameInstanceSubsystem.h");
        }
        if (Type.Equals(TEXT("WorldSubsystem"), ESearchCase::IgnoreCase))
        {
            return TEXT("Subsystems/WorldSubsystem.h");
        }
        if (Type.Equals(TEXT("DeveloperSettings"), ESearchCase::IgnoreCase))
        {
            return TEXT("Engine/DeveloperSettings.h");
        }
        return TEXT("UObject/Object.h");
    }

    bool IsPathInsideDirectory(const FString& CandidatePath, const FString& Directory)
    {
        const FString NormalizedCandidate = FPaths::ConvertRelativePathToFull(CandidatePath).Replace(TEXT("\\"), TEXT("/"));
        FString NormalizedDirectory = FPaths::ConvertRelativePathToFull(Directory).Replace(TEXT("\\"), TEXT("/"));
        NormalizedDirectory.RemoveFromEnd(TEXT("/"));
        return NormalizedCandidate.StartsWith(NormalizedDirectory + TEXT("/"));
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
            OutValues.Add(StringValue);
        }
        return true;
    }

    FString JsonValueAsString(const TSharedPtr<FJsonValue>& Value)
    {
        FString StringValue;
        if (Value->TryGetString(StringValue))
        {
            return StringValue;
        }

        bool bBoolValue = false;
        if (Value->TryGetBool(bBoolValue))
        {
            return bBoolValue ? TEXT("true") : TEXT("false");
        }

        double NumberValue = 0.0;
        if (Value->TryGetNumber(NumberValue))
        {
            return FString::SanitizeFloat(NumberValue);
        }
        return TEXT("");
    }

    FCommandForgeCppMetadata ParseMetadata(const TSharedPtr<FJsonObject>& Object)
    {
        FCommandForgeCppMetadata Metadata;
        if (!Object.IsValid())
        {
            return Metadata;
        }

        for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Object->Values)
        {
            Metadata.Values.Add(Entry.Key, JsonValueAsString(Entry.Value));
        }
        return Metadata;
    }

    bool ParseCppClassSpec(const FCommandForgePolicyDocument& Document,
        FCommandForgeCppClassSpec& OutSpec, FCommandForgeReport& Report)
    {
        if (!Document.Kind.Equals(TEXT("cpp_class"), ESearchCase::IgnoreCase))
        {
            AddError(Report, TEXT("INVALID_KIND"),
                TEXT("cpp_class spec만 사용할 수 있습니다."), TEXT("kind"));
            return false;
        }

        OutSpec.Version = Document.Version;
        OutSpec.Kind = Document.Kind;
        Document.RootObject->TryGetStringField(TEXT("transaction_id"), OutSpec.TransactionId);

        const TSharedPtr<FJsonObject>* ClassObject = nullptr;
        if (!Document.RootObject->TryGetObjectField(TEXT("class"), ClassObject) || ClassObject == nullptr)
        {
            AddError(Report, TEXT("REQUIRED_FIELD"), TEXT("'class' object는 필수입니다."), TEXT("class"));
            return false;
        }

        (*ClassObject)->TryGetStringField(TEXT("name"), OutSpec.ClassInfo.Name);
        (*ClassObject)->TryGetStringField(TEXT("type"), OutSpec.ClassInfo.Type);
        (*ClassObject)->TryGetStringField(TEXT("module"), OutSpec.ClassInfo.Module);
        (*ClassObject)->TryGetStringField(TEXT("output_path"), OutSpec.ClassInfo.OutputPath);
        (*ClassObject)->TryGetStringField(TEXT("base_class"), OutSpec.ClassInfo.BaseClass);
        (*ClassObject)->TryGetStringField(TEXT("generated_header_include"), OutSpec.ClassInfo.GeneratedHeaderInclude);
        (*ClassObject)->TryGetBoolField(TEXT("blueprint_type"), OutSpec.ClassInfo.bBlueprintType);
        (*ClassObject)->TryGetBoolField(TEXT("tick"), OutSpec.ClassInfo.bTick);

        if (!TryGetStringArray(Document.RootObject, TEXT("includes"), OutSpec.Includes))
        {
            AddError(Report, TEXT("INVALID_INCLUDES"), TEXT("'includes'는 string 배열이어야 합니다."), TEXT("includes"));
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>* PropertyValues = nullptr;
        if (Document.RootObject->TryGetArrayField(TEXT("properties"), PropertyValues) && PropertyValues != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& PropertyValue : *PropertyValues)
            {
                const TSharedPtr<FJsonObject>* PropertyObject = nullptr;
                if (!PropertyValue->TryGetObject(PropertyObject) || PropertyObject == nullptr)
                {
                    AddError(Report, TEXT("INVALID_PROPERTY"), TEXT("properties 항목은 object여야 합니다."), TEXT("properties"));
                    return false;
                }

                FCommandForgeCppPropertySpec Property;
                (*PropertyObject)->TryGetStringField(TEXT("name"), Property.Name);
                (*PropertyObject)->TryGetStringField(TEXT("type"), Property.Type);
                (*PropertyObject)->TryGetStringField(TEXT("default"), Property.DefaultValue);

                const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
                if ((*PropertyObject)->TryGetObjectField(TEXT("metadata"), MetadataObject) && MetadataObject != nullptr)
                {
                    Property.Metadata = ParseMetadata(*MetadataObject);
                }
                OutSpec.Properties.Add(Property);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* FunctionValues = nullptr;
        if (Document.RootObject->TryGetArrayField(TEXT("functions"), FunctionValues) && FunctionValues != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& FunctionValue : *FunctionValues)
            {
                const TSharedPtr<FJsonObject>* FunctionObject = nullptr;
                if (!FunctionValue->TryGetObject(FunctionObject) || FunctionObject == nullptr)
                {
                    AddError(Report, TEXT("INVALID_FUNCTION"), TEXT("functions 항목은 object여야 합니다."), TEXT("functions"));
                    return false;
                }

                FCommandForgeCppFunctionSpec Function;
                (*FunctionObject)->TryGetStringField(TEXT("name"), Function.Name);
                (*FunctionObject)->TryGetStringField(TEXT("return_type"), Function.ReturnType);

                const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
                if ((*FunctionObject)->TryGetObjectField(TEXT("metadata"), MetadataObject) && MetadataObject != nullptr)
                {
                    Function.Metadata = ParseMetadata(*MetadataObject);
                }

                const TArray<TSharedPtr<FJsonValue>>* ParamValues = nullptr;
                if ((*FunctionObject)->TryGetArrayField(TEXT("params"), ParamValues) && ParamValues != nullptr)
                {
                    for (const TSharedPtr<FJsonValue>& ParamValue : *ParamValues)
                    {
                        const TSharedPtr<FJsonObject>* ParamObject = nullptr;
                        if (!ParamValue->TryGetObject(ParamObject) || ParamObject == nullptr)
                        {
                            AddError(Report, TEXT("INVALID_PARAM"), TEXT("params 항목은 object여야 합니다."), TEXT("functions.params"));
                            return false;
                        }

                        FCommandForgeCppParamSpec Param;
                        (*ParamObject)->TryGetStringField(TEXT("name"), Param.Name);
                        (*ParamObject)->TryGetStringField(TEXT("type"), Param.Type);
                        Function.Params.Add(Param);
                    }
                }
                OutSpec.Functions.Add(Function);
            }
        }

        const TSharedPtr<FJsonObject>* BuildDependenciesObject = nullptr;
        if (Document.RootObject->TryGetObjectField(TEXT("build_dependencies"), BuildDependenciesObject) &&
            BuildDependenciesObject != nullptr)
        {
            if (!TryGetStringArray(*BuildDependenciesObject, TEXT("public"),
                OutSpec.BuildDependencies.PublicDependencies))
            {
                AddError(Report, TEXT("INVALID_BUILD_DEPENDENCY_ARRAY"),
                    TEXT("build_dependencies.public은 string 배열이어야 합니다."),
                    TEXT("build_dependencies.public"));
            }
            if (!TryGetStringArray(*BuildDependenciesObject, TEXT("private"),
                OutSpec.BuildDependencies.PrivateDependencies))
            {
                AddError(Report, TEXT("INVALID_BUILD_DEPENDENCY_ARRAY"),
                    TEXT("build_dependencies.private는 string 배열이어야 합니다."),
                    TEXT("build_dependencies.private"));
            }
        }
        return Report.Errors.IsEmpty();
    }

    void AddMetadataMacroPart(const FCommandForgeCppMetadata& Metadata, const FString& Key,
        TArray<FString>& Parts, TSet<FString>& SeenKeys)
    {
        const FString* Value = Metadata.Values.Find(Key);
        if (Value == nullptr)
        {
            return;
        }

        SeenKeys.Add(Key);
        if (Value->Equals(TEXT("true"), ESearchCase::IgnoreCase))
        {
            Parts.Add(Key);
        }
        else if (!Value->Equals(TEXT("false"), ESearchCase::IgnoreCase))
        {
            Parts.Add(FString::Printf(TEXT("%s=\"%s\""), *Key, **Value));
        }
    }

    FString MetadataMacroArgs(const FCommandForgeCppMetadata& Metadata)
    {
        TArray<FString> Parts;
        TSet<FString> SeenKeys;
        const TArray<FString> PreferredOrder = {
            TEXT("EditAnywhere"),
            TEXT("EditDefaultsOnly"),
            TEXT("VisibleAnywhere"),
            TEXT("BlueprintReadOnly"),
            TEXT("BlueprintReadWrite"),
            TEXT("BlueprintCallable"),
            TEXT("BlueprintPure"),
            TEXT("Category"),
            TEXT("Config")
        };

        for (const FString& Key : PreferredOrder)
        {
            AddMetadataMacroPart(Metadata, Key, Parts, SeenKeys);
        }

        TArray<FString> RemainingKeys;
        for (const TPair<FString, FString>& Entry : Metadata.Values)
        {
            if (!SeenKeys.Contains(Entry.Key))
            {
                RemainingKeys.Add(Entry.Key);
            }
        }
        RemainingKeys.Sort();

        for (const FString& Key : RemainingKeys)
        {
            AddMetadataMacroPart(Metadata, Key, Parts, SeenKeys);
        }
        return FString::Join(Parts, TEXT(", "));
    }

    void ValidateMetadataTokens(const FCommandForgeCppMetadata& Metadata, FCommandForgeReport& Report,
        const FString& FieldPrefix)
    {
        for (const TPair<FString, FString>& Entry : Metadata.Values)
        {
            if (!IsValidIdentifier(Entry.Key))
            {
                AddIssue(Report, TEXT("INVALID_METADATA_KEY"),
                    TEXT("metadata key는 C++ macro identifier여야 합니다."),
                    FieldPrefix + TEXT(".metadata"), Entry.Key,
                    TEXT("예: BlueprintReadOnly, Category"));
            }
            if (!IsSafeMetadataValue(Entry.Value))
            {
                AddIssue(Report, TEXT("INVALID_METADATA_VALUE"),
                    TEXT("metadata value에는 따옴표, 역슬래시, 개행을 사용할 수 없습니다."),
                    FieldPrefix + TEXT(".metadata"), Entry.Value,
                    TEXT("단순 문자열 값만 사용하세요."));
            }
        }
    }

    void ValidateSpecTokens(const FCommandForgeCppClassSpec& Spec, FCommandForgeReport& Report)
    {
        if (!IsValidIdentifier(Spec.ClassInfo.Module))
        {
            AddIssue(Report, TEXT("INVALID_MODULE_NAME"), TEXT("module은 C++ identifier여야 합니다."),
                TEXT("class.module"), Spec.ClassInfo.Module,
                TEXT("예: UECommandForgeRuntime"));
        }
        if (!IsSafeCppTypeToken(Spec.ClassInfo.BaseClass))
        {
            AddIssue(Report, TEXT("INVALID_BASE_CLASS"), TEXT("base_class에 안전하지 않은 C++ 토큰이 포함됐습니다."),
                TEXT("class.base_class"), Spec.ClassInfo.BaseClass,
                TEXT("예: UActorComponent"));
        }
        for (const FString& Include : Spec.Includes)
        {
            if (!IsSafeIncludePath(Include))
            {
                AddIssue(Report, TEXT("INVALID_INCLUDE"),
                    TEXT("include path는 module include 상대 경로이며 따옴표, 개행, 상위 경로를 포함할 수 없습니다."),
                    TEXT("includes"), Include,
                    TEXT("예: Components/ActorComponent.h"));
            }
        }
        for (const FString& Dependency : Spec.BuildDependencies.PublicDependencies)
        {
            if (!IsValidIdentifier(Dependency))
            {
                AddIssue(Report, TEXT("INVALID_BUILD_DEPENDENCY"),
                    TEXT("Build dependency는 module identifier여야 합니다."),
                    TEXT("build_dependencies.public"), Dependency,
                    TEXT("예: Engine"));
            }
        }
        for (const FString& Dependency : Spec.BuildDependencies.PrivateDependencies)
        {
            if (!IsValidIdentifier(Dependency))
            {
                AddIssue(Report, TEXT("INVALID_BUILD_DEPENDENCY"),
                    TEXT("Build dependency는 module identifier여야 합니다."),
                    TEXT("build_dependencies.private"), Dependency,
                    TEXT("예: Slate"));
            }
        }
        for (const FCommandForgeCppPropertySpec& Property : Spec.Properties)
        {
            if (!IsValidIdentifier(Property.Name))
            {
                AddIssue(Report, TEXT("INVALID_PROPERTY_NAME"), TEXT("property name은 C++ identifier여야 합니다."),
                    TEXT("properties.name"), Property.Name,
                    TEXT("예: MaxHealth"));
            }
            if (!IsSafeCppTypeToken(Property.Type))
            {
                AddIssue(Report, TEXT("INVALID_PROPERTY_TYPE"),
                    TEXT("property type에 안전하지 않은 C++ 토큰이 포함됐습니다."),
                    TEXT("properties.type"), Property.Type,
                    TEXT("예: float 또는 TObjectPtr<UObject>"));
            }
            if (!Property.DefaultValue.IsEmpty() && ContainsAnyChar(Property.DefaultValue, TEXT("\r\n`;{}#")))
            {
                AddIssue(Report, TEXT("INVALID_PROPERTY_DEFAULT"),
                    TEXT("property default 값에 안전하지 않은 C++ 토큰이 포함됐습니다."),
                    TEXT("properties.default"), Property.DefaultValue,
                    TEXT("단일 표현식만 사용하세요."));
            }
            ValidateMetadataTokens(Property.Metadata, Report, TEXT("properties"));
        }
        for (const FCommandForgeCppFunctionSpec& Function : Spec.Functions)
        {
            if (!IsValidIdentifier(Function.Name))
            {
                AddIssue(Report, TEXT("INVALID_FUNCTION_NAME"), TEXT("function name은 C++ identifier여야 합니다."),
                    TEXT("functions.name"), Function.Name,
                    TEXT("예: ApplyDamage"));
            }
            if (!IsSafeCppTypeToken(Function.ReturnType))
            {
                AddIssue(Report, TEXT("INVALID_FUNCTION_RETURN_TYPE"),
                    TEXT("return_type에 안전하지 않은 C++ 토큰이 포함됐습니다."),
                    TEXT("functions.return_type"), Function.ReturnType,
                    TEXT("예: void 또는 float"));
            }
            ValidateMetadataTokens(Function.Metadata, Report, TEXT("functions"));
            for (const FCommandForgeCppParamSpec& Param : Function.Params)
            {
                if (!IsValidIdentifier(Param.Name))
                {
                    AddIssue(Report, TEXT("INVALID_PARAM_NAME"), TEXT("param name은 C++ identifier여야 합니다."),
                        TEXT("functions.params.name"), Param.Name,
                        TEXT("예: DamageAmount"));
                }
                if (!IsSafeCppTypeToken(Param.Type))
                {
                    AddIssue(Report, TEXT("INVALID_PARAM_TYPE"),
                        TEXT("param type에 안전하지 않은 C++ 토큰이 포함됐습니다."),
                        TEXT("functions.params.type"), Param.Type,
                        TEXT("예: float 또는 const FString&"));
                }
            }
        }
    }

    FString ParamDeclarationList(const TArray<FCommandForgeCppParamSpec>& Params)
    {
        TArray<FString> Parts;
        for (const FCommandForgeCppParamSpec& Param : Params)
        {
            Parts.Add(Param.Type + TEXT(" ") + Param.Name);
        }
        return FString::Join(Parts, TEXT(", "));
    }

    FString Sha1ForString(const FString& Content)
    {
        FTCHARToUTF8 Converted(*Content);
        FSHA1 Sha;
        Sha.Update(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
        Sha.Final();

        uint8 Digest[20];
        Sha.GetHash(Digest);

        FString Result;
        for (const uint8 Byte : Digest)
        {
            Result += FString::Printf(TEXT("%02x"), Byte);
        }
        return Result;
    }

    FString BuildHeader(const FCommandForgeCppClassSpec& Spec, const FString& ClassSymbol,
        const FString& HeaderFileStem)
    {
        TArray<FString> Lines;
        Lines.Add(TEXT("#pragma once"));
        Lines.Add(TEXT(""));

        TArray<FString> Includes;
        for (const FString& Include : Spec.Includes)
        {
            if (!Includes.Contains(Include))
            {
                Includes.Add(Include);
            }
        }
        const FString TypeInclude = DefaultIncludeForType(Spec.ClassInfo.Type);
        if (!Includes.Contains(TypeInclude))
        {
            Includes.Add(TypeInclude);
        }
        for (const FString& Include : Includes)
        {
            Lines.Add(FString::Printf(TEXT("#include \"%s\""), *Include));
        }
        Lines.Add(FString::Printf(TEXT("#include \"%s.generated.h\""), *HeaderFileStem));
        Lines.Add(TEXT(""));

        TArray<FString> UClassArgs;
        if (Spec.ClassInfo.bBlueprintType)
        {
            UClassArgs.Add(TEXT("BlueprintType"));
        }
        if (Spec.ClassInfo.Type.Equals(TEXT("ActorComponent"), ESearchCase::IgnoreCase))
        {
            UClassArgs.Add(TEXT("ClassGroup=(Custom)"));
            UClassArgs.Add(TEXT("meta=(BlueprintSpawnableComponent)"));
        }
        Lines.Add(FString::Printf(TEXT("UCLASS(%s)"), *FString::Join(UClassArgs, TEXT(", "))));
        Lines.Add(FString::Printf(TEXT("class %s %s : public %s"),
            *ModuleApiMacro(Spec.ClassInfo.Module), *ClassSymbol, *Spec.ClassInfo.BaseClass));
        Lines.Add(TEXT("{"));
        Lines.Add(TEXT("    GENERATED_BODY()"));
        Lines.Add(TEXT(""));
        Lines.Add(TEXT("public:"));
        Lines.Add(FString::Printf(TEXT("    %s();"), *ClassSymbol));

        if (!Spec.Properties.IsEmpty())
        {
            Lines.Add(TEXT(""));
            for (const FCommandForgeCppPropertySpec& Property : Spec.Properties)
            {
                Lines.Add(FString::Printf(TEXT("    UPROPERTY(%s)"), *MetadataMacroArgs(Property.Metadata)));
                Lines.Add(FString::Printf(TEXT("    %s %s;"), *Property.Type, *Property.Name));
                Lines.Add(TEXT(""));
            }
            Lines.Pop();
        }

        if (!Spec.Functions.IsEmpty())
        {
            Lines.Add(TEXT(""));
            for (const FCommandForgeCppFunctionSpec& Function : Spec.Functions)
            {
                Lines.Add(FString::Printf(TEXT("    UFUNCTION(%s)"), *MetadataMacroArgs(Function.Metadata)));
                Lines.Add(FString::Printf(TEXT("    %s %s(%s);"),
                    *Function.ReturnType, *Function.Name, *ParamDeclarationList(Function.Params)));
                Lines.Add(TEXT(""));
            }
            Lines.Pop();
        }

        Lines.Add(TEXT("};"));
        Lines.Add(TEXT(""));
        return FString::Join(Lines, TEXT("\n"));
    }

    FString BuildSource(const FCommandForgeCppClassSpec& Spec, const FString& ClassSymbol,
        const FString& IncludePath)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("#include \"%s\""), *IncludePath));
        Lines.Add(TEXT(""));
        Lines.Add(FString::Printf(TEXT("%s::%s()"), *ClassSymbol, *ClassSymbol));
        Lines.Add(TEXT("{"));
        if (Spec.ClassInfo.Type.Equals(TEXT("ActorComponent"), ESearchCase::IgnoreCase))
        {
            Lines.Add(FString::Printf(TEXT("    PrimaryComponentTick.bCanEverTick = %s;"),
                Spec.ClassInfo.bTick ? TEXT("true") : TEXT("false")));
        }
        for (const FCommandForgeCppPropertySpec& Property : Spec.Properties)
        {
            if (!Property.DefaultValue.IsEmpty())
            {
                Lines.Add(FString::Printf(TEXT("    %s = %s;"), *Property.Name, *Property.DefaultValue));
            }
        }
        Lines.Add(TEXT("}"));

        for (const FCommandForgeCppFunctionSpec& Function : Spec.Functions)
        {
            Lines.Add(TEXT(""));
            Lines.Add(FString::Printf(TEXT("%s %s::%s(%s)"),
                *Function.ReturnType, *ClassSymbol, *Function.Name, *ParamDeclarationList(Function.Params)));
            Lines.Add(TEXT("{"));
            if (!Function.ReturnType.Equals(TEXT("void"), ESearchCase::IgnoreCase))
            {
                Lines.Add(FString::Printf(TEXT("    return %s();"), *Function.ReturnType));
            }
            Lines.Add(TEXT("}"));
        }

        Lines.Add(TEXT(""));
        return FString::Join(Lines, TEXT("\n"));
    }
}

int32 UGenerateCppClassCommandlet::Main(const FString& Params)
{
    using namespace UECommandForge::GenerateCppClassPrivate;

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutPath = ParamsMap.FindRef(TEXT("Output"));
    const FString SpecPath = ParamsMap.FindRef(TEXT("Spec"));
    const bool bApply = Switches.Contains(TEXT("Apply"));
    const bool bApplyBuildCs = ParamsMap.FindRef(TEXT("ApplyBuildCs")).Equals(TEXT("true"), ESearchCase::IgnoreCase);

    FCommandForgeReport Report;
    Report.Commandlet = TEXT("GenerateCppClass");
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
        AddError(Report, TEXT("SPEC_READ_FAILED"), TEXT("C++ class spec 파일을 읽을 수 없습니다."), TEXT("Spec"));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgePolicyDocument Document;
    TArray<FCommandForgeError> ParseErrors;
    if (!UECommandForge::FCommandForgePolicyParser::ParseJson(SpecJson, Document, ParseErrors))
    {
        Report.Errors.Append(ParseErrors);
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    FCommandForgeCppClassSpec Spec;
    if (!ParseCppClassSpec(Document, Spec, Report))
    {
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::SpecParseFailed);
    }

    Spec.TransactionId = Spec.TransactionId.TrimStartAndEnd();
    Report.TransactionId = Spec.TransactionId.IsEmpty()
        ? FString::Printf(TEXT("tx-%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")))
        : Spec.TransactionId;

    Spec.ClassInfo.Name = Spec.ClassInfo.Name.TrimStartAndEnd();
    Spec.ClassInfo.Type = Spec.ClassInfo.Type.TrimStartAndEnd();
    Spec.ClassInfo.Module = Spec.ClassInfo.Module.TrimStartAndEnd();
    Spec.ClassInfo.OutputPath = NormalizeRelativePath(Spec.ClassInfo.OutputPath);
    Spec.ClassInfo.BaseClass = Spec.ClassInfo.BaseClass.TrimStartAndEnd().IsEmpty()
        ? DefaultBaseClass(Spec.ClassInfo.Type)
        : Spec.ClassInfo.BaseClass.TrimStartAndEnd();

    if (!IsValidIdentifier(Spec.ClassInfo.Name))
    {
        AddIssue(Report, TEXT("INVALID_CLASS_NAME"), TEXT("class name은 C++ identifier여야 합니다."),
            TEXT("class.name"), TEXT(""), TEXT("예: HealthComponent"));
    }
    if (Spec.ClassInfo.Module.IsEmpty())
    {
        AddIssue(Report, TEXT("REQUIRED_MODULE"), TEXT("module은 필수입니다."),
            TEXT("class.module"), TEXT(""), TEXT("대상 UE module 이름을 지정하세요."));
    }
    if (Spec.ClassInfo.OutputPath.IsEmpty() || FPaths::IsRelative(Spec.ClassInfo.OutputPath) == false ||
        Spec.ClassInfo.OutputPath.Contains(TEXT("..")) || ContainsWildcard(Spec.ClassInfo.OutputPath))
    {
        AddIssue(Report, TEXT("OUTPUT_PATH_OUTSIDE_MODULE"),
            TEXT("output_path는 module root 내부의 상대 경로여야 합니다."),
            TEXT("class.output_path"), Spec.ClassInfo.OutputPath,
            TEXT("예: Components 또는 Public 하위가 아닌 기능 폴더 이름을 사용하세요."));
    }
    ValidateSpecTokens(Spec, Report);

    const FString ModuleRoot = UECommandForge::BuildCs::ResolveModuleRoot(Spec.ClassInfo.Module);
    if (ModuleRoot.IsEmpty())
    {
        AddIssue(Report, TEXT("MODULE_NOT_FOUND"), TEXT("대상 module root를 찾을 수 없습니다."),
            TEXT("class.module"), Spec.ClassInfo.Module,
            TEXT("sample Source 또는 plugin Source 아래의 module 이름을 사용하세요."));
    }

    const FString HeaderFileStem = Spec.ClassInfo.Name;
    const FString ClassSymbol = EnsureUnrealClassSymbol(Spec.ClassInfo.Name, Spec.ClassInfo.Type);
    const FString HeaderPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        ModuleRoot, TEXT("Public"), Spec.ClassInfo.OutputPath, HeaderFileStem + TEXT(".h")));
    const FString SourcePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        ModuleRoot, TEXT("Private"), Spec.ClassInfo.OutputPath, HeaderFileStem + TEXT(".cpp")));

    if (!ModuleRoot.IsEmpty() &&
        (!IsPathInsideDirectory(HeaderPath, ModuleRoot) || !IsPathInsideDirectory(SourcePath, ModuleRoot)))
    {
        AddIssue(Report, TEXT("OUTPUT_PATH_OUTSIDE_MODULE"),
            TEXT("생성 대상 파일이 module root 밖으로 나갑니다."),
            TEXT("class.output_path"), Spec.ClassInfo.OutputPath,
            TEXT("상대 경로에서 '..'와 절대 경로를 제거하세요."));
    }

    if (FPaths::FileExists(HeaderPath) || FPaths::FileExists(SourcePath))
    {
        AddIssue(Report, TEXT("CPP_CLASS_FILE_EXISTS"),
            TEXT("생성 대상 .h/.cpp 파일이 이미 존재합니다."),
            TEXT("class.name"), HeaderPath,
            TEXT("다른 class name 또는 output_path를 사용하세요."));
    }

    const UECommandForge::BuildCs::FBuildCsDependencyCheck BuildCsCheck =
        UECommandForge::BuildCs::CheckDependencies(Spec.ClassInfo.Module,
            Spec.BuildDependencies.PublicDependencies, Spec.BuildDependencies.PrivateDependencies);

    const FString HeaderIncludePath = Spec.ClassInfo.OutputPath / (HeaderFileStem + TEXT(".h"));
    const FString HeaderPreview = BuildHeader(Spec, ClassSymbol, HeaderFileStem);
    const FString SourcePreview = BuildSource(Spec, ClassSymbol, HeaderIncludePath);

    Report.Validation.Add(TEXT("spec"), SpecPath);
    Report.Validation.Add(TEXT("module"), Spec.ClassInfo.Module);
    Report.Validation.Add(TEXT("class_name"), Spec.ClassInfo.Name);
    Report.Validation.Add(TEXT("class_symbol"), ClassSymbol);
    Report.Validation.Add(TEXT("header_path"), HeaderPath);
    Report.Validation.Add(TEXT("source_path"), SourcePath);
    Report.Validation.Add(TEXT("header_preview"), HeaderPreview);
    Report.Validation.Add(TEXT("source_preview"), SourcePreview);
    Report.Validation.Add(TEXT("header_sha1"), Sha1ForString(HeaderPreview));
    Report.Validation.Add(TEXT("source_sha1"), Sha1ForString(SourcePreview));
    Report.Validation.Add(TEXT("buildcs_path"), BuildCsCheck.BuildCsPath);
    Report.Validation.Add(TEXT("buildcs_apply_requested"), bApplyBuildCs ? TEXT("true") : TEXT("false"));
    Report.Validation.Add(TEXT("buildcs_changed"), BuildCsCheck.bChanged ? TEXT("true") : TEXT("false"));
    Report.Validation.Add(TEXT("buildcs_missing_public"),
        FString::Join(BuildCsCheck.MissingPublicDependencies, TEXT(",")));
    Report.Validation.Add(TEXT("buildcs_missing_private"),
        FString::Join(BuildCsCheck.MissingPrivateDependencies, TEXT(",")));

    if (!Spec.BuildDependencies.PublicDependencies.IsEmpty())
    {
        Report.Validation.Add(TEXT("public_dependencies"),
            FString::Join(Spec.BuildDependencies.PublicDependencies, TEXT(",")));
    }
    if (!Spec.BuildDependencies.PrivateDependencies.IsEmpty())
    {
        Report.Validation.Add(TEXT("private_dependencies"),
            FString::Join(Spec.BuildDependencies.PrivateDependencies, TEXT(",")));
    }

    if (bApplyBuildCs && !BuildCsCheck.bBuildCsExists)
    {
        AddIssue(Report, TEXT("BUILDCS_NOT_FOUND"),
            TEXT("module Build.cs 파일을 찾을 수 없습니다."),
            TEXT("class.module"), Spec.ClassInfo.Module,
            TEXT("module 이름과 plugin/project Source 경로를 확인하세요."));
    }

    if (HasErrorIssue(Report.ValidationIssues))
    {
        Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
        Report.bOk = false;
        UECommandForge::FJsonReportWriter::Write(OutPath, Report);
        return static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
    }

    if (bApply)
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        bool bBuildCsWritten = false;

        if (bApplyBuildCs && BuildCsCheck.bChanged)
        {
            if (UECommandForge::BuildCs::WriteUpdatedBuildCs(BuildCsCheck))
            {
                Report.ChangedFiles.Add(BuildCsCheck.BuildCsPath);
                bBuildCsWritten = true;
            }
            else
            {
                AddIssue(Report, TEXT("BUILDCS_WRITE_FAILED"),
                    TEXT("Build.cs dependency update를 저장할 수 없습니다."),
                    TEXT("build_dependencies"), BuildCsCheck.BuildCsPath,
                    TEXT("Build.cs 경로와 파일 권한을 확인하세요."));
            }
        }

        if (!HasErrorIssue(Report.ValidationIssues))
        {
            PlatformFile.CreateDirectoryTree(*FPaths::GetPath(HeaderPath));
            PlatformFile.CreateDirectoryTree(*FPaths::GetPath(SourcePath));

            const bool bHeaderWritten = FFileHelper::SaveStringToFile(
                HeaderPreview, *HeaderPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            const bool bSourceWritten = FFileHelper::SaveStringToFile(
                SourcePreview, *SourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

            if (!bHeaderWritten || !bSourceWritten)
            {
                IFileManager::Get().Delete(*HeaderPath);
                IFileManager::Get().Delete(*SourcePath);
                if (bBuildCsWritten)
                {
                    FFileHelper::SaveStringToFile(BuildCsCheck.OriginalContent, *BuildCsCheck.BuildCsPath,
                        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
                    Report.ChangedFiles.Remove(BuildCsCheck.BuildCsPath);
                }
                AddIssue(Report, TEXT("CPP_CLASS_WRITE_FAILED"),
                    TEXT("생성된 C++ 파일을 저장할 수 없습니다."),
                    TEXT("class.output_path"), Spec.ClassInfo.OutputPath,
                    TEXT("디스크 권한과 module Source 경로를 확인하세요."));
            }
            else
            {
                Report.ChangedFiles.Add(HeaderPath);
                Report.ChangedFiles.Add(SourcePath);
            }
        }
    }

    Report.Validation.Add(TEXT("issue_count"), FString::FromInt(Report.ValidationIssues.Num()));
    Report.bApplied = bApply && !Report.ChangedFiles.IsEmpty();
    Report.bOk = Report.Errors.IsEmpty() && !HasErrorIssue(Report.ValidationIssues);

    const bool bWrote = UECommandForge::FJsonReportWriter::Write(OutPath, Report);
    if (!bWrote)
    {
        return static_cast<int32>(ECommandForgeExitCode::EngineError);
    }
    return Report.bOk ? 0 : static_cast<int32>(ECommandForgeExitCode::ValidationFailed);
}
