#include "Validators/AIFlowValidator.h"
#include "Components/StateTreeComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Character.h"
#include "StateTree.h"
#include "StateTreeReference.h"

namespace UECommandForge
{
    namespace
    {
        void AddValidatorError(TArray<FCommandForgeError>& OutErrors,
                               const TCHAR* Code,
                               const FString& Message,
                               const TCHAR* Field)
        {
            OutErrors.Add({ Code, Message, Field });
        }

        UBlueprint* LoadValidatorBlueprint(const FString& AssetPath,
                                           const TCHAR* Field,
                                           TArray<FCommandForgeError>& OutErrors)
        {
            UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
            if (!Blueprint || !Blueprint->GeneratedClass)
            {
                AddValidatorError(OutErrors, TEXT("ASSET_NOT_FOUND"),
                    FString::Printf(TEXT("Blueprint 없음: %s"), *AssetPath), Field);
                return nullptr;
            }
            return Blueprint;
        }

        UStateTree* LoadValidatorStateTree(const FString& AssetPath,
                                           TArray<FCommandForgeError>& OutErrors)
        {
            UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *AssetPath);
            if (!StateTree)
            {
                AddValidatorError(OutErrors, TEXT("ASSET_NOT_FOUND"),
                    FString::Printf(TEXT("StateTree 없음: %s"), *AssetPath), TEXT("StateTree.AssetPath"));
                return nullptr;
            }
            return StateTree;
        }

        bool ComponentUsesStateTree(UStateTreeComponent* Component, const UStateTree* ExpectedStateTree)
        {
            if (!Component || !ExpectedStateTree)
            {
                return false;
            }

            FStructProperty* StateTreeRefProperty =
                FindFProperty<FStructProperty>(UStateTreeComponent::StaticClass(), TEXT("StateTreeRef"));
            if (!StateTreeRefProperty || StateTreeRefProperty->Struct != FStateTreeReference::StaticStruct())
            {
                return false;
            }

            const FStateTreeReference* StateTreeRef =
                StateTreeRefProperty->ContainerPtrToValuePtr<FStateTreeReference>(Component);
            return StateTreeRef && StateTreeRef->GetStateTree() == ExpectedStateTree;
        }

        bool HasBoundStateTreeComponent(UBlueprint* CharacterBlueprint, const UStateTree* StateTree)
        {
            if (!CharacterBlueprint || !CharacterBlueprint->SimpleConstructionScript)
            {
                return false;
            }

            for (USCS_Node* Node : CharacterBlueprint->SimpleConstructionScript->GetAllNodes())
            {
                if (!Node)
                {
                    continue;
                }

                UStateTreeComponent* Component = Cast<UStateTreeComponent>(Node->ComponentTemplate);
                if (ComponentUsesStateTree(Component, StateTree))
                {
                    return true;
                }
            }
            return false;
        }

        void AddMismatchErrorIfNeeded(TArray<FCommandForgeError>& OutErrors,
                                      const bool bOk,
                                      const TCHAR* Code,
                                      const TCHAR* Message,
                                      const TCHAR* Field)
        {
            if (!bOk)
            {
                AddValidatorError(OutErrors, Code, Message, Field);
            }
        }
    }

    bool FAIFlowValidator::Validate(const FAIFlowSpec& Spec,
                                    TMap<FString, FString>& OutValidation,
                                    TArray<FCommandForgeError>& OutErrors)
    {
        UBlueprint* CharacterBlueprint = LoadValidatorBlueprint(Spec.Character.AssetPath, TEXT("Character.AssetPath"), OutErrors);
        UBlueprint* ControllerBlueprint = LoadValidatorBlueprint(Spec.AIController.AssetPath, TEXT("AIController.AssetPath"), OutErrors);
        UStateTree* StateTree = LoadValidatorStateTree(Spec.StateTree.AssetPath, OutErrors);
        if (!CharacterBlueprint || !ControllerBlueprint || !StateTree)
        {
            return false;
        }

        ACharacter* CharacterCDO = Cast<ACharacter>(CharacterBlueprint->GeneratedClass->GetDefaultObject());
        if (!CharacterCDO)
        {
            AddValidatorError(OutErrors, TEXT("CDO_CAST_FAILED"),
                TEXT("Character Blueprint CDO를 ACharacter로 캐스트할 수 없습니다."),
                TEXT("Character.AssetPath"));
            return false;
        }

        const bool bControllerOk = CharacterCDO->AIControllerClass == ControllerBlueprint->GeneratedClass;
        const bool bAutoPossessOk =
            CharacterCDO->AutoPossessAI == EAutoPossessAI::PlacedInWorldOrSpawned;
        const bool bStateTreeOk = HasBoundStateTreeComponent(CharacterBlueprint, StateTree);

        OutValidation.Add(TEXT("ai_controller_class"), bControllerOk ? TEXT("ok") : TEXT("mismatch"));
        OutValidation.Add(TEXT("auto_possess_ai"), bAutoPossessOk ? TEXT("ok") : TEXT("mismatch"));
        OutValidation.Add(TEXT("statetree_component"), bStateTreeOk ? TEXT("ok") : TEXT("mismatch"));

        AddMismatchErrorIfNeeded(OutErrors, bControllerOk, TEXT("AI_CONTROLLER_CLASS_MISMATCH"),
            TEXT("Character Blueprint AIControllerClass가 지정된 AIController Blueprint와 일치하지 않습니다."),
            TEXT("AIController.AssetPath"));
        AddMismatchErrorIfNeeded(OutErrors, bAutoPossessOk, TEXT("AUTO_POSSESS_AI_MISMATCH"),
            TEXT("Character Blueprint AutoPossessAI가 PlacedInWorldOrSpawned가 아닙니다."),
            TEXT("Character.AssetPath"));
        AddMismatchErrorIfNeeded(OutErrors, bStateTreeOk, TEXT("STATETREE_COMPONENT_MISMATCH"),
            TEXT("Character Blueprint에 지정된 StateTree를 사용하는 StateTreeComponent가 없습니다."),
            TEXT("StateTree.AssetPath"));

        return bControllerOk && bAutoPossessOk && bStateTreeOk;
    }
}
