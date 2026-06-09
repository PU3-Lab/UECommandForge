#include "Specs/CppClassBatchSpecParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace UECommandForge
{
    namespace CppClassBatchSpecParserPrivate
    {
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
    }

    bool FCppClassBatchSpecParser::Parse(const FString& Json, FCppClassBatchSpec& OutSpec,
                                         TArray<FCommandForgeError>& OutErrors)
    {
        using namespace CppClassBatchSpecParserPrivate;

        const int32 InitialErrorCount = OutErrors.Num();

        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            FCommandForgeError Err;
            Err.Code    = TEXT("PARSE_FAILED");
            Err.Message = TEXT("JSON 파싱 실패");
            OutErrors.Add(Err);
            return false;
        }

        Root->TryGetStringField(TEXT("version"), OutSpec.Version);
        Root->TryGetStringField(TEXT("kind"), OutSpec.Kind);
        Root->TryGetStringField(TEXT("transaction_id"), OutSpec.TransactionId);

        if (OutSpec.Version.IsEmpty())
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("version은 필수 필드입니다."), TEXT("version") });
        }
        else if (OutSpec.Version != TEXT("1"))
        {
            OutErrors.Add({ TEXT("PLAN_VERSION_MISMATCH"), TEXT("계획 version은 '1'이어야 합니다."), TEXT("version") });
        }

        if (OutSpec.Kind != TEXT("cpp_class_batch"))
        {
            OutErrors.Add({ TEXT("PLAN_KIND_MISMATCH"), TEXT("계획 kind는 'cpp_class_batch'이어야 합니다."), TEXT("kind") });
        }

        if (OutErrors.Num() > InitialErrorCount)
        {
            return false;
        }

        // 1. classes 파싱
        const TArray<TSharedPtr<FJsonValue>>* ClassesArray = nullptr;
        if (!Root->TryGetArrayField(TEXT("classes"), ClassesArray) || ClassesArray == nullptr)
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("'classes' 배열은 필수입니다."), TEXT("classes") });
            return false;
        }

        if (ClassesArray->Num() == 0)
        {
            OutErrors.Add({ TEXT("EMPTY_CLASSES"), TEXT("'classes' 배열이 비어있습니다. 최소 하나의 클래스 사양이 필요합니다."), TEXT("classes") });
            return false;
        }

        for (int32 Index = 0; Index < ClassesArray->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject>* ClassObject = nullptr;
            if (!(*ClassesArray)[Index]->TryGetObject(ClassObject) || ClassObject == nullptr || !ClassObject->IsValid())
            {
                OutErrors.Add({ TEXT("INVALID_CLASS"), TEXT("classes 항목은 object여야 합니다."), FString::Printf(TEXT("classes[%d]"), Index) });
                continue;
            }

            FCommandForgeCppClassInfoSpec ClassInfo;
            if (!(*ClassObject)->TryGetStringField(TEXT("name"), ClassInfo.Name) || ClassInfo.Name.IsEmpty())
            {
                OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("name은 필수 필드입니다."), FString::Printf(TEXT("classes[%d].name"), Index) });
            }
            if (!(*ClassObject)->TryGetStringField(TEXT("type"), ClassInfo.Type) || ClassInfo.Type.IsEmpty())
            {
                OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("type은 필수 필드입니다."), FString::Printf(TEXT("classes[%d].type"), Index) });
            }
            if (!(*ClassObject)->TryGetStringField(TEXT("module"), ClassInfo.Module) || ClassInfo.Module.IsEmpty())
            {
                OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("module은 필수 필드입니다."), FString::Printf(TEXT("classes[%d].module"), Index) });
            }
            if (!(*ClassObject)->TryGetStringField(TEXT("output_path"), ClassInfo.OutputPath) || ClassInfo.OutputPath.IsEmpty())
            {
                OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("output_path는 필수 필드입니다."), FString::Printf(TEXT("classes[%d].output_path"), Index) });
            }

            (*ClassObject)->TryGetStringField(TEXT("base_class"), ClassInfo.BaseClass);
            (*ClassObject)->TryGetStringField(TEXT("generated_header_include"), ClassInfo.GeneratedHeaderInclude);
            (*ClassObject)->TryGetBoolField(TEXT("blueprint_type"), ClassInfo.bBlueprintType);
            (*ClassObject)->TryGetBoolField(TEXT("tick"), ClassInfo.bTick);

            OutSpec.Classes.Add(ClassInfo);
        }

        // 2. 공통 includes 파싱
        if (!TryGetStringArray(Root, TEXT("includes"), OutSpec.Includes))
        {
            OutErrors.Add({ TEXT("INVALID_INCLUDES"), TEXT("'includes'는 string 배열이어야 합니다."), TEXT("includes") });
        }

        // 3. 공통 properties 파싱
        const TArray<TSharedPtr<FJsonValue>>* PropertyValues = nullptr;
        if (Root->TryGetArrayField(TEXT("properties"), PropertyValues) && PropertyValues != nullptr)
        {
            for (int32 Index = 0; Index < PropertyValues->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject>* PropertyObject = nullptr;
                if (!(*PropertyValues)[Index]->TryGetObject(PropertyObject) || PropertyObject == nullptr || !PropertyObject->IsValid())
                {
                    OutErrors.Add({ TEXT("INVALID_PROPERTY"), TEXT("properties 항목은 object여야 합니다."), TEXT("properties") });
                    continue;
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

        // 4. 공통 functions 파싱
        const TArray<TSharedPtr<FJsonValue>>* FunctionValues = nullptr;
        if (Root->TryGetArrayField(TEXT("functions"), FunctionValues) && FunctionValues != nullptr)
        {
            for (int32 Index = 0; Index < FunctionValues->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject>* FunctionObject = nullptr;
                if (!(*FunctionValues)[Index]->TryGetObject(FunctionObject) || FunctionObject == nullptr || !FunctionObject->IsValid())
                {
                    OutErrors.Add({ TEXT("INVALID_FUNCTION"), TEXT("functions 항목은 object여야 합니다."), TEXT("functions") });
                    continue;
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
                    for (int32 ParamIndex = 0; ParamIndex < ParamValues->Num(); ++ParamIndex)
                    {
                        const TSharedPtr<FJsonObject>* ParamObject = nullptr;
                        if (!(*ParamValues)[ParamIndex]->TryGetObject(ParamObject) || ParamObject == nullptr || !ParamObject->IsValid())
                        {
                            OutErrors.Add({ TEXT("INVALID_PARAM"), TEXT("params 항목은 object여야 합니다."), FString::Printf(TEXT("functions[%d].params[%d]"), Index, ParamIndex) });
                            continue;
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

        // 5. 공통 build_dependencies 파싱
        const TSharedPtr<FJsonObject>* BuildDependenciesObject = nullptr;
        if (Root->TryGetObjectField(TEXT("build_dependencies"), BuildDependenciesObject) && BuildDependenciesObject != nullptr)
        {
            if (!TryGetStringArray(*BuildDependenciesObject, TEXT("public"), OutSpec.BuildDependencies.PublicDependencies))
            {
                OutErrors.Add({ TEXT("INVALID_BUILD_DEPENDENCY_ARRAY"), TEXT("build_dependencies.public은 string 배열이어야 합니다."), TEXT("build_dependencies.public") });
            }
            if (!TryGetStringArray(*BuildDependenciesObject, TEXT("private"), OutSpec.BuildDependencies.PrivateDependencies))
            {
                OutErrors.Add({ TEXT("INVALID_BUILD_DEPENDENCY_ARRAY"), TEXT("build_dependencies.private는 string 배열이어야 합니다."), TEXT("build_dependencies.private") });
            }
        }

        return OutErrors.Num() == InitialErrorCount;
    }
}
