#include "Builders/AIFlowBinder.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "GameFramework/Character.h"
#include "Components/StateTreeComponent.h"
#include "StateTree.h"
#include "StateTreeReference.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"

namespace UECommandForge
{
    bool FAIFlowBinder::Bind(const FAIFlowSpec& Spec, TArray<FCommandForgeError>& OutErrors)
    {
        UBlueprint* CharBP = LoadObject<UBlueprint>(nullptr, *Spec.Character.AssetPath);
        if (!CharBP)
        {
            OutErrors.Add({ TEXT("ASSET_NOT_FOUND"),
                FString::Printf(TEXT("Character BP 없음: %s"), *Spec.Character.AssetPath),
                TEXT("Character.AssetPath") });
            return false;
        }

        UBlueprint* CtrlBP = LoadObject<UBlueprint>(nullptr, *Spec.AIController.AssetPath);
        if (!CtrlBP)
        {
            OutErrors.Add({ TEXT("ASSET_NOT_FOUND"),
                FString::Printf(TEXT("AIController BP 없음: %s"), *Spec.AIController.AssetPath),
                TEXT("AIController.AssetPath") });
            return false;
        }

        UStateTree* ST = LoadObject<UStateTree>(nullptr, *Spec.StateTree.AssetPath);
        if (!ST)
        {
            OutErrors.Add({ TEXT("ASSET_NOT_FOUND"),
                FString::Printf(TEXT("StateTree 없음: %s"), *Spec.StateTree.AssetPath),
                TEXT("StateTree.AssetPath") });
            return false;
        }

        if (!SetAIControllerClass(CharBP, CtrlBP, OutErrors)) { return false; }
        if (!SetAutoPossessAI(CharBP, OutErrors))              { return false; }
        if (!AttachStateTree(CharBP, ST, OutErrors))           { return false; }

        // CDO 변경 2-pass: 설정 → 컴파일 → 저장
        FKismetEditorUtilities::CompileBlueprint(CharBP);

        UPackage* Package = CharBP->GetOutermost();
        Package->SetDirtyFlag(true);
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString FileName = FPackageName::LongPackageNameToFilename(
            Spec.Character.AssetPath, FPackageName::GetAssetPackageExtension());
        UPackage::SavePackage(Package, CharBP, *FileName, SaveArgs);

        return true;
    }

    bool FAIFlowBinder::SetAIControllerClass(UBlueprint* CharBP, UBlueprint* CtrlBP,
                                               TArray<FCommandForgeError>& OutErrors)
    {
        UClass* CDOClass = CharBP->GeneratedClass;
        if (!CDOClass)
        {
            OutErrors.Add({ TEXT("CDO_CLASS_NULL"),
                FString::Printf(TEXT("GeneratedClass 없음: %s"), *CharBP->GetName()),
                TEXT("Character.AssetPath") });
            return false;
        }

        ACharacter* CDO = Cast<ACharacter>(CDOClass->GetDefaultObject());
        if (!CDO)
        {
            OutErrors.Add({ TEXT("CDO_CAST_FAILED"),
                FString::Printf(TEXT("ACharacter 캐스트 실패: %s"), *CharBP->GetName()),
                TEXT("Character.AssetPath") });
            return false;
        }

        CDO->AIControllerClass = CtrlBP->GeneratedClass;
        return true;
    }

    bool FAIFlowBinder::SetAutoPossessAI(UBlueprint* CharBP,
                                           TArray<FCommandForgeError>& OutErrors)
    {
        UClass* CDOClass = CharBP->GeneratedClass;
        if (!CDOClass)
        {
            OutErrors.Add({ TEXT("CDO_CLASS_NULL"),
                FString::Printf(TEXT("GeneratedClass 없음: %s"), *CharBP->GetName()),
                TEXT("Character.AssetPath") });
            return false;
        }

        APawn* CDO = Cast<APawn>(CDOClass->GetDefaultObject());
        if (!CDO)
        {
            OutErrors.Add({ TEXT("CDO_CAST_FAILED"),
                FString::Printf(TEXT("APawn 캐스트 실패: %s"), *CharBP->GetName()),
                TEXT("Character.AssetPath") });
            return false;
        }

        CDO->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
        return true;
    }

    bool FAIFlowBinder::AttachStateTree(UBlueprint* CharBP, UStateTree* ST,
                                         TArray<FCommandForgeError>& OutErrors)
    {
        // StateTreeComponent가 이미 있으면 재사용, 없으면 새로 추가
        USCS_Node* ExistingNode = nullptr;
        if (CharBP->SimpleConstructionScript)
        {
            for (USCS_Node* Node : CharBP->SimpleConstructionScript->GetAllNodes())
            {
                if (Node && Node->ComponentClass &&
                    Node->ComponentClass->IsChildOf(UStateTreeComponent::StaticClass()))
                {
                    ExistingNode = Node;
                    break;
                }
            }
        }

        UStateTreeComponent* STComp = nullptr;
        if (ExistingNode)
        {
            STComp = Cast<UStateTreeComponent>(ExistingNode->ComponentTemplate);
        }
        else if (CharBP->SimpleConstructionScript)
        {
            USCS_Node* NewNode = CharBP->SimpleConstructionScript->CreateNode(
                UStateTreeComponent::StaticClass(), TEXT("StateTreeComponent"));
            if (!NewNode)
            {
                OutErrors.Add({ TEXT("SCS_NODE_FAILED"),
                    TEXT("StateTreeComponent SCS 노드 생성 실패"),
                    TEXT("StateTree.AssetPath") });
                return false;
            }
            CharBP->SimpleConstructionScript->AddNode(NewNode);
            STComp = Cast<UStateTreeComponent>(NewNode->ComponentTemplate);
        }

        if (STComp)
        {
            // UStateTreeComponent::SetStateTree() calls ValidateStateTreeReference() which
            // requires GetOwner() != null. ComponentTemplate has no owning Actor → crash.
            // Use reflection to access the protected StateTreeRef and call FStateTreeReference::SetStateTree.
            if (FStructProperty* STRefProp = FindFProperty<FStructProperty>(
                UStateTreeComponent::StaticClass(), TEXT("StateTreeRef")))
            {
                if (FStateTreeReference* STRef =
                    STRefProp->ContainerPtrToValuePtr<FStateTreeReference>(STComp))
                {
                    STRef->SetStateTree(ST);
                }
            }
        }

        return true;
    }
}
