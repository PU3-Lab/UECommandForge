#include "Builders/BlueprintCDOPropertyApplier.h"

#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Class.h"

namespace UECommandForge
{
    bool FBlueprintCDOPropertyApplier::Apply(UBlueprint* Blueprint,
                                             const TArray<FBlueprintCDOPropertyAssignment>& Assignments,
                                             const FString& FieldPrefix,
                                             bool bCompileBeforeApply,
                                             TArray<FCommandForgeError>& OutErrors,
                                             TMap<FString, FString>& OutValidation)
    {
        if (Assignments.IsEmpty())
        {
            OutValidation.Add(TEXT("cdo_property_count"), TEXT("0"));
            OutValidation.Add(TEXT("cdo_changed_count"), TEXT("0"));
            return true;
        }

        if (!Blueprint)
        {
            OutErrors.Add({ TEXT("BLUEPRINT_NULL"),
                TEXT("CDO 기본값을 적용할 Blueprint 객체가 없습니다."),
                FieldPrefix });
            return false;
        }

        if (bCompileBeforeApply)
        {
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
        }
        if (bCompileBeforeApply && Blueprint->Status != BS_UpToDate)
        {
            OutErrors.Add({ TEXT("BP_COMPILE_FAILED"),
                TEXT("CDO 기본값 적용 전 Blueprint 컴파일에 실패했습니다."),
                FieldPrefix });
            return false;
        }
        if (!Blueprint->GeneratedClass)
        {
            OutErrors.Add({ TEXT("BLUEPRINT_GENERATED_CLASS_MISSING"),
                TEXT("Blueprint GeneratedClass가 없어 CDO 기본값을 적용할 수 없습니다."),
                FieldPrefix });
            return false;
        }

        UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
        if (!CDO)
        {
            OutErrors.Add({ TEXT("BLUEPRINT_CDO_MISSING"),
                TEXT("Blueprint CDO를 찾을 수 없습니다."),
                FieldPrefix });
            return false;
        }

        Blueprint->Modify();
        CDO->Modify();

        int32 AppliedCount = 0;
        int32 ChangedCount = 0;
        for (const FBlueprintCDOPropertyAssignment& Assignment : Assignments)
        {
            const FString Field = FString::Printf(TEXT("%s.defaults.%s"),
                FieldPrefix.IsEmpty() ? TEXT("Blueprint") : *FieldPrefix,
                *Assignment.PropertyName);
            FProperty* Property = Blueprint->GeneratedClass->FindPropertyByName(FName(*Assignment.PropertyName));
            if (!Property)
            {
                OutErrors.Add({ TEXT("CDO_PROPERTY_NOT_FOUND"),
                    FString::Printf(TEXT("CDO 프로퍼티를 찾을 수 없습니다: %s"), *Assignment.PropertyName),
                    Field });
                return false;
            }

            void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);
            FString BeforeValue;
            Property->ExportTextItem_Direct(BeforeValue, ValuePtr, nullptr, CDO, PPF_None);
            const TCHAR* Result = Property->ImportText_Direct(*Assignment.Value, ValuePtr, CDO, PPF_None);
            if (Result == nullptr)
            {
                OutErrors.Add({ TEXT("CDO_PROPERTY_IMPORT_FAILED"),
                    FString::Printf(TEXT("CDO 프로퍼티 값 변환 실패: %s=%s"),
                        *Assignment.PropertyName, *Assignment.Value),
                    Field });
                return false;
            }

            FString AfterValue;
            Property->ExportTextItem_Direct(AfterValue, ValuePtr, nullptr, CDO, PPF_None);
            if (BeforeValue != AfterValue)
            {
                ++ChangedCount;
            }
            ++AppliedCount;
            OutValidation.Add(FString::Printf(TEXT("cdo.%s"), *Assignment.PropertyName), Assignment.Value);
        }

        if (ChangedCount > 0)
        {
            Blueprint->MarkPackageDirty();
        }
        OutValidation.Add(TEXT("cdo_property_count"), FString::FromInt(AppliedCount));
        OutValidation.Add(TEXT("cdo_changed_count"), FString::FromInt(ChangedCount));
        return true;
    }
}
