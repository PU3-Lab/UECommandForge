#include "Validators/AIFlowValidator.h"
#include "GameFramework/Character.h"
#include "Engine/Blueprint.h"

namespace UECommandForge
{
    bool FAIFlowValidator::Validate(const FAIFlowSpec& Spec,
                                     TMap<FString, FString>& OutValidation,
                                     TArray<FCommandForgeError>& OutErrors)
    {
        UBlueprint* CharBP = LoadObject<UBlueprint>(nullptr, *Spec.Character.AssetPath);
        if (!CharBP || !CharBP->GeneratedClass)
        {
            OutErrors.Add({ TEXT("ASSET_NOT_FOUND"),
                FString::Printf(TEXT("Character BP 없음: %s"), *Spec.Character.AssetPath),
                TEXT("Character.AssetPath") });
            return false;
        }

        ACharacter* CDO = Cast<ACharacter>(CharBP->GeneratedClass->GetDefaultObject());
        if (!CDO)
        {
            OutErrors.Add({ TEXT("CDO_CAST_FAILED"), TEXT("Character CDO 캐스트 실패"), TEXT("") });
            return false;
        }

        UBlueprint* CtrlBP = LoadObject<UBlueprint>(nullptr, *Spec.AIController.AssetPath);
        const bool bCtrlOk = CtrlBP && CtrlBP->GeneratedClass &&
                             CDO->AIControllerClass == CtrlBP->GeneratedClass;
        OutValidation.Add(TEXT("ai_controller_class"), bCtrlOk ? TEXT("ok") : TEXT("mismatch"));

        APawn* PawnCDO = Cast<APawn>(CDO);
        const bool bPossessOk = PawnCDO &&
            PawnCDO->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned;
        OutValidation.Add(TEXT("auto_possess_ai"), bPossessOk ? TEXT("ok") : TEXT("mismatch"));

        return bCtrlOk && bPossessOk;
    }
}
