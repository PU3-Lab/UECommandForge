#include "Specs/AIFlowSpecValidator.h"
#include "Misc/PackageName.h"

namespace UECommandForge
{
    static const TSet<FString> AllowedTaskTypes = { TEXT("Wait"), TEXT("MoveTo"), TEXT("PlayMontage") };

    void FAIFlowSpecValidator::CheckRequiredPath(const FString& Value, const FString& Field,
                                                  TArray<FCommandForgeError>& OutErrors)
    {
        if (Value.IsEmpty())
        {
            OutErrors.Add({ TEXT("REQUIRED_FIELD"), FString::Printf(TEXT("'%s' 필드는 필수입니다."), *Field), Field });
        }
    }

    void FAIFlowSpecValidator::CheckGamePath(const FString& Value, const FString& Field,
                                             TArray<FCommandForgeError>& OutErrors)
    {
        if (!Value.IsEmpty() && !Value.StartsWith(TEXT("/Game/")))
        {
            OutErrors.Add({ TEXT("INVALID_PATH"), FString::Printf(TEXT("'%s'는 /Game/ 으로 시작해야 합니다."), *Field), Field });
            return;
        }

        FText Reason;
        if (!Value.IsEmpty() && !FPackageName::IsValidLongPackageName(Value, false, &Reason))
        {
            OutErrors.Add({ TEXT("INVALID_PATH"),
                FString::Printf(TEXT("'%s'는 유효한 Unreal 패키지 경로여야 합니다: %s"), *Field, *Reason.ToString()),
                Field });
        }
    }

    bool FAIFlowSpecValidator::Validate(const FAIFlowSpec& Spec, TArray<FCommandForgeError>& OutErrors)
    {
        OutErrors.Empty();

        CheckRequiredPath(Spec.Character.AssetPath,     TEXT("Character.AssetPath"),     OutErrors);
        CheckRequiredPath(Spec.Character.ParentClass,   TEXT("Character.ParentClass"),   OutErrors);
        CheckRequiredPath(Spec.AIController.AssetPath,  TEXT("AIController.AssetPath"),  OutErrors);
        CheckRequiredPath(Spec.StateTree.AssetPath,     TEXT("StateTree.AssetPath"),     OutErrors);
        CheckRequiredPath(Spec.Placement.MapPath,       TEXT("Placement.MapPath"),       OutErrors);
        CheckRequiredPath(Spec.Placement.BlueprintPath, TEXT("Placement.BlueprintPath"), OutErrors);

        CheckGamePath(Spec.Character.AssetPath,     TEXT("Character.AssetPath"),     OutErrors);
        CheckGamePath(Spec.AIController.AssetPath,  TEXT("AIController.AssetPath"),  OutErrors);
        CheckGamePath(Spec.StateTree.AssetPath,     TEXT("StateTree.AssetPath"),     OutErrors);
        CheckGamePath(Spec.Placement.MapPath,       TEXT("Placement.MapPath"),       OutErrors);
        CheckGamePath(Spec.Placement.BlueprintPath, TEXT("Placement.BlueprintPath"), OutErrors);

        for (const FStateTreeTaskSpec& Task : Spec.StateTree.Tasks)
        {
            if (!AllowedTaskTypes.Contains(Task.TaskType))
            {
                OutErrors.Add({ TEXT("INVALID_TASK_TYPE"),
                    FString::Printf(TEXT("허용되지 않는 TaskType: '%s'. 허용 목록: Wait, MoveTo, PlayMontage"), *Task.TaskType),
                    TEXT("StateTree.Tasks[].TaskType") });
            }
        }

        return OutErrors.IsEmpty();
    }
}
