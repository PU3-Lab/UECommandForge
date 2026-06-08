#include "Specs/BlueprintBatchSpecParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace UECommandForge
{
    bool FBlueprintBatchSpecParser::Parse(const FString& Json, FBlueprintBatchSpec& OutSpec,
                                          TArray<FCommandForgeError>& OutErrors)
    {
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
        Root->TryGetBoolField(TEXT("compile"), OutSpec.bCompile);
        Root->TryGetBoolField(TEXT("save"), OutSpec.bSave);

        if (OutSpec.Version.IsEmpty())
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("version은 필수 필드입니다."), TEXT("version") });
        }
        else if (OutSpec.Version != TEXT("1"))
        {
            OutErrors.Add({ TEXT("PLAN_VERSION_MISMATCH"), TEXT("계획 version은 '1'이어야 합니다."), TEXT("version") });
        }

        if (OutSpec.Kind != TEXT("blueprint_batch"))
        {
            OutErrors.Add({ TEXT("PLAN_KIND_MISMATCH"), TEXT("계획 kind는 'blueprint_batch'이어야 합니다."), TEXT("kind") });
        }

        if (OutErrors.Num() > InitialErrorCount)
        {
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
        if (!Root->TryGetArrayField(TEXT("items"), ItemsArray) || ItemsArray == nullptr)
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("'items' 배열은 필수입니다."), TEXT("items") });
            return false;
        }

        for (int32 Index = 0; Index < ItemsArray->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject>* ItemObject = nullptr;
            if (!(*ItemsArray)[Index]->TryGetObject(ItemObject) || ItemObject == nullptr || !ItemObject->IsValid())
            {
                OutErrors.Add({ TEXT("INVALID_ITEM"), TEXT("items 항목은 object여야 합니다."), FString::Printf(TEXT("items[%d]"), Index) });
                continue;
            }

            FBlueprintBatchItemSpec ItemSpec;
            (*ItemObject)->TryGetBoolField(TEXT("allow_replace"), ItemSpec.bAllowReplace);

            if (!(*ItemObject)->TryGetStringField(TEXT("asset_path"), ItemSpec.AssetPath) || ItemSpec.AssetPath.IsEmpty())
            {
                OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("asset_path는 필수 필드입니다."), FString::Printf(TEXT("items[%d].asset_path"), Index) });
            }
            if (!(*ItemObject)->TryGetStringField(TEXT("parent_class"), ItemSpec.ParentClass) || ItemSpec.ParentClass.IsEmpty())
            {
                OutErrors.Add({ TEXT("REQUIRED_FIELD"), TEXT("parent_class는 필수 필드입니다."), FString::Printf(TEXT("items[%d].parent_class"), Index) });
            }

            // 컴포넌트들 파싱
            const TArray<TSharedPtr<FJsonValue>>* ComponentsArray = nullptr;
            if ((*ItemObject)->TryGetArrayField(TEXT("components"), ComponentsArray) && ComponentsArray != nullptr)
            {
                for (int32 CmpIndex = 0; CmpIndex < ComponentsArray->Num(); ++CmpIndex)
                {
                    const TSharedPtr<FJsonObject>* CmpObject = nullptr;
                    if (!(*ComponentsArray)[CmpIndex]->TryGetObject(CmpObject) || CmpObject == nullptr || !CmpObject->IsValid())
                    {
                        OutErrors.Add({ TEXT("INVALID_COMPONENT"), TEXT("components 항목은 object여야 합니다."), FString::Printf(TEXT("items[%d].components[%d]"), Index, CmpIndex) });
                        continue;
                    }

                    FBlueprintComponentSpec CmpSpec;
                    (*CmpObject)->TryGetStringField(TEXT("name"), CmpSpec.Name);
                    (*CmpObject)->TryGetStringField(TEXT("class"), CmpSpec.Class);
                    ItemSpec.Components.Add(CmpSpec);
                }
            }

            // 디폴트값 프로퍼티들 파싱
            const TArray<TSharedPtr<FJsonValue>>* DefaultsArray = nullptr;
            if ((*ItemObject)->TryGetArrayField(TEXT("defaults"), DefaultsArray) && DefaultsArray != nullptr)
            {
                for (int32 DftIndex = 0; DftIndex < DefaultsArray->Num(); ++DftIndex)
                {
                    const TSharedPtr<FJsonObject>* DftObject = nullptr;
                    if (!(*DefaultsArray)[DftIndex]->TryGetObject(DftObject) || DftObject == nullptr || !DftObject->IsValid())
                    {
                        OutErrors.Add({ TEXT("INVALID_DEFAULT"), TEXT("defaults 항목은 object여야 합니다."), FString::Printf(TEXT("items[%d].defaults[%d]"), Index, DftIndex) });
                        continue;
                    }

                    FBlueprintCDOPropertyAssignment DftSpec;
                    (*DftObject)->TryGetStringField(TEXT("property"), DftSpec.PropertyName);
                    (*DftObject)->TryGetStringField(TEXT("value"), DftSpec.Value);
                    ItemSpec.Defaults.Add(DftSpec);
                }
            }

            OutSpec.Items.Add(ItemSpec);
        }

        return OutErrors.Num() == InitialErrorCount;
    }
}
