#include "Builders/AIFlowBinder.h"
#include "Components/StateTreeComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Character.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StateTree.h"
#include "StateTreeReference.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Validators/AIFlowValidator.h"

namespace UECommandForge
{
    namespace
    {
        constexpr const TCHAR* StateTreeComponentName = TEXT("CommandForgeStateTree");

        void AddBinderError(TArray<FCommandForgeError>& OutErrors,
                            const TCHAR* Code,
                            const FString& Message,
                            const TCHAR* Field)
        {
            OutErrors.Add({ Code, Message, Field });
        }

        UBlueprint* LoadBinderBlueprint(const FString& AssetPath,
                                        const TCHAR* Field,
                                        TArray<FCommandForgeError>& OutErrors)
        {
            UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
            if (!Blueprint || !Blueprint->GeneratedClass)
            {
                AddBinderError(OutErrors, TEXT("ASSET_NOT_FOUND"),
                    FString::Printf(TEXT("Blueprint 없음: %s"), *AssetPath), Field);
                return nullptr;
            }
            return Blueprint;
        }

        UStateTree* LoadBinderStateTree(const FString& AssetPath,
                                        TArray<FCommandForgeError>& OutErrors)
        {
            UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *AssetPath);
            if (!StateTree)
            {
                AddBinderError(OutErrors, TEXT("ASSET_NOT_FOUND"),
                    FString::Printf(TEXT("StateTree 없음: %s"), *AssetPath), TEXT("StateTree.AssetPath"));
                return nullptr;
            }
            return StateTree;
        }

        UStateTreeComponent* FindOrCreateStateTreeComponent(UBlueprint* CharacterBlueprint,
                                                            TArray<FCommandForgeError>& OutErrors)
        {
            if (!CharacterBlueprint->SimpleConstructionScript)
            {
                AddBinderError(OutErrors, TEXT("SCS_MISSING"),
                    TEXT("Character Blueprint SimpleConstructionScript가 없습니다."),
                    TEXT("Character.AssetPath"));
                return nullptr;
            }

            USimpleConstructionScript* SCS = CharacterBlueprint->SimpleConstructionScript;
            for (USCS_Node* Node : SCS->GetAllNodes())
            {
                if (Node && Node->GetVariableName() == StateTreeComponentName)
                {
                    UStateTreeComponent* ExistingComponent = Cast<UStateTreeComponent>(Node->ComponentTemplate);
                    if (!ExistingComponent)
                    {
                        AddBinderError(OutErrors, TEXT("STATETREE_COMPONENT_TYPE_MISMATCH"),
                            TEXT("CommandForgeStateTree SCS 노드가 StateTreeComponent가 아닙니다."),
                            TEXT("StateTree.AssetPath"));
                    }
                    return ExistingComponent;
                }
            }

            USCS_Node* NewNode = SCS->CreateNode(UStateTreeComponent::StaticClass(), StateTreeComponentName);
            if (!NewNode)
            {
                AddBinderError(OutErrors, TEXT("STATETREE_COMPONENT_CREATE_FAILED"),
                    TEXT("StateTreeComponent SCS 노드 생성 실패"),
                    TEXT("StateTree.AssetPath"));
                return nullptr;
            }

            SCS->AddNode(NewNode);
            UStateTreeComponent* NewComponent = Cast<UStateTreeComponent>(NewNode->ComponentTemplate);
            if (!NewComponent)
            {
                AddBinderError(OutErrors, TEXT("STATETREE_COMPONENT_TEMPLATE_INVALID"),
                    TEXT("생성된 CommandForgeStateTree 컴포넌트 템플릿이 StateTreeComponent가 아닙니다."),
                    TEXT("StateTree.AssetPath"));
                return nullptr;
            }
            return NewComponent;
        }

        bool AssignStateTreeReference(UStateTreeComponent* Component,
                                      UStateTree* StateTree,
                                      TArray<FCommandForgeError>& OutErrors)
        {
            FStructProperty* StateTreeRefProperty =
                FindFProperty<FStructProperty>(UStateTreeComponent::StaticClass(), TEXT("StateTreeRef"));
            if (!StateTreeRefProperty || StateTreeRefProperty->Struct != FStateTreeReference::StaticStruct())
            {
                AddBinderError(OutErrors, TEXT("STATETREE_REF_PROPERTY_MISSING"),
                    TEXT("StateTreeComponent StateTreeRef 속성을 찾을 수 없습니다."),
                    TEXT("StateTree.AssetPath"));
                return false;
            }

            FStateTreeReference* StateTreeRef =
                StateTreeRefProperty->ContainerPtrToValuePtr<FStateTreeReference>(Component);
            if (!StateTreeRef)
            {
                AddBinderError(OutErrors, TEXT("STATETREE_REF_ACCESS_FAILED"),
                    TEXT("StateTreeComponent StateTreeRef 접근 실패"),
                    TEXT("StateTree.AssetPath"));
                return false;
            }

            StateTreeRef->SetStateTree(StateTree);
            return true;
        }

        bool SaveBlueprint(UBlueprint* Blueprint,
                           const FString& AssetPath,
                           TArray<FCommandForgeError>& OutErrors)
        {
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            if (Blueprint->Status == BS_Error)
            {
                AddBinderError(OutErrors, TEXT("BP_COMPILE_FAILED"),
                    FString::Printf(TEXT("Blueprint 컴파일 실패: %s"), *AssetPath),
                    TEXT("Character.AssetPath"));
                return false;
            }

            UPackage* Package = Blueprint->GetOutermost();
            Package->SetDirtyFlag(true);
            const FString FileName = FPaths::ConvertRelativePathToFull(
                FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension()));

            FSavePackageArgs SaveArgs;
            SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
            if (!UPackage::SavePackage(Package, Blueprint, *FileName, SaveArgs))
            {
                AddBinderError(OutErrors, TEXT("BP_SAVE_FAILED"),
                    FString::Printf(TEXT("Blueprint 저장 실패: %s"), *FileName),
                    TEXT("Character.AssetPath"));
                return false;
            }
            return true;
        }
    }

    bool FAIFlowBinder::Bind(const FAIFlowSpec& Spec,
                             TArray<FCommandForgeError>& OutErrors,
                             TMap<FString, FString>& OutValidation)
    {
        UBlueprint* CharacterBlueprint = LoadBinderBlueprint(Spec.Character.AssetPath, TEXT("Character.AssetPath"), OutErrors);
        UBlueprint* ControllerBlueprint = LoadBinderBlueprint(Spec.AIController.AssetPath, TEXT("AIController.AssetPath"), OutErrors);
        UStateTree* StateTree = LoadBinderStateTree(Spec.StateTree.AssetPath, OutErrors);
        if (!CharacterBlueprint || !ControllerBlueprint || !StateTree)
        {
            return false;
        }

        ACharacter* CharacterCDO = Cast<ACharacter>(CharacterBlueprint->GeneratedClass->GetDefaultObject());
        if (!CharacterCDO)
        {
            AddBinderError(OutErrors, TEXT("CDO_CAST_FAILED"),
                TEXT("Character Blueprint CDO를 ACharacter로 캐스트할 수 없습니다."),
                TEXT("Character.AssetPath"));
            return false;
        }

        CharacterBlueprint->Modify();
        CharacterCDO->Modify();
        CharacterCDO->AIControllerClass = ControllerBlueprint->GeneratedClass;
        CharacterCDO->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

        UStateTreeComponent* StateTreeComponent = FindOrCreateStateTreeComponent(CharacterBlueprint, OutErrors);
        if (!StateTreeComponent)
        {
            return false;
        }

        StateTreeComponent->Modify();
        if (!AssignStateTreeReference(StateTreeComponent, StateTree, OutErrors))
        {
            return false;
        }
        StateTreeComponent->SetStartLogicAutomatically(true);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(CharacterBlueprint);
        if (!SaveBlueprint(CharacterBlueprint, Spec.Character.AssetPath, OutErrors))
        {
            return false;
        }

        TArray<FCommandForgeError> ValidationErrors;
        const bool bValid = FAIFlowValidator::Validate(Spec, OutValidation, ValidationErrors);
        if (!bValid)
        {
            OutErrors.Append(ValidationErrors);
            return false;
        }

        return true;
    }
}
