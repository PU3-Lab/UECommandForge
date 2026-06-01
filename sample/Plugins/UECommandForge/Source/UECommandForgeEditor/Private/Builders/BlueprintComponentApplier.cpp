#include "Builders/BlueprintComponentApplier.h"

#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace UECommandForge
{
    namespace
    {
        UClass* ResolveComponentClass(const FString& ClassPath)
        {
            if (ClassPath.IsEmpty())
            {
                return nullptr;
            }

            if (ClassPath.StartsWith(TEXT("/Script/")))
            {
                return LoadObject<UClass>(nullptr, *ClassPath);
            }

            if (UClass* EngineClass = LoadObject<UClass>(nullptr,
                    *FString::Printf(TEXT("/Script/Engine.%s"), *ClassPath)))
            {
                return EngineClass;
            }

            if (UClass* GameplayStateTreeClass = LoadObject<UClass>(nullptr,
                    *FString::Printf(TEXT("/Script/GameplayStateTreeModule.%s"), *ClassPath)))
            {
                return GameplayStateTreeClass;
            }

            return LoadObject<UClass>(nullptr, *ClassPath);
        }

        bool HasComponentNode(USimpleConstructionScript* SCS, const FName& Name)
        {
            for (const USCS_Node* Node : SCS->GetAllNodes())
            {
                if (Node && Node->GetVariableName() == Name)
                {
                    return true;
                }
            }
            return false;
        }
    }

    bool FBlueprintComponentApplier::Apply(UBlueprint* Blueprint,
                                           const TArray<FBlueprintComponentSpec>& Components,
                                           const FString& FieldPrefix,
                                           TArray<FCommandForgeError>& OutErrors,
                                           TMap<FString, FString>& OutValidation)
    {
        if (Components.IsEmpty())
        {
            OutValidation.Add(TEXT("component_count"), TEXT("0"));
            OutValidation.Add(TEXT("component_created_count"), TEXT("0"));
            OutValidation.Add(TEXT("component_existing_count"), TEXT("0"));
            return true;
        }

        if (!Blueprint || !Blueprint->SimpleConstructionScript)
        {
            OutErrors.Add({ TEXT("SCS_MISSING"),
                TEXT("Blueprint SimpleConstructionScript가 없습니다."),
                FieldPrefix });
            return false;
        }

        Blueprint->Modify();
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

        int32 CreatedCount = 0;
        int32 ExistingCount = 0;
        for (const FBlueprintComponentSpec& Component : Components)
        {
            const FString Field = FString::Printf(TEXT("%s.components.%s"),
                FieldPrefix.IsEmpty() ? TEXT("Blueprint") : *FieldPrefix,
                *Component.Name);
            if (Component.Name.IsEmpty())
            {
                OutErrors.Add({ TEXT("COMPONENT_NAME_REQUIRED"),
                    TEXT("컴포넌트 이름은 필수입니다."),
                    Field });
                return false;
            }

            const FName ComponentName(*Component.Name);
            if (HasComponentNode(SCS, ComponentName))
            {
                ++ExistingCount;
                OutValidation.Add(FString::Printf(TEXT("component.%s"), *Component.Name), TEXT("existing"));
                continue;
            }

            UClass* ComponentClass = ResolveComponentClass(Component.Class);
            if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
            {
                OutErrors.Add({ TEXT("COMPONENT_CLASS_INVALID"),
                    FString::Printf(TEXT("ActorComponent 클래스를 찾을 수 없습니다: %s"), *Component.Class),
                    Field });
                return false;
            }

            USCS_Node* NewNode = SCS->CreateNode(ComponentClass, ComponentName);
            if (!NewNode)
            {
                OutErrors.Add({ TEXT("COMPONENT_CREATE_FAILED"),
                    FString::Printf(TEXT("컴포넌트 SCS 노드 생성 실패: %s"), *Component.Name),
                    Field });
                return false;
            }

            SCS->AddNode(NewNode);
            ++CreatedCount;
            OutValidation.Add(FString::Printf(TEXT("component.%s"), *Component.Name), TEXT("created"));
        }

        if (CreatedCount > 0)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }
        OutValidation.Add(TEXT("component_count"), FString::FromInt(Components.Num()));
        OutValidation.Add(TEXT("component_created_count"), FString::FromInt(CreatedCount));
        OutValidation.Add(TEXT("component_existing_count"), FString::FromInt(ExistingCount));
        return true;
    }
}
