#pragma once
#include "CoreMinimal.h"
#include "Specs/AIFlowSpec.h"
#include "CommandForgeTypes.h"

class UBlueprint;
class UStateTree;

namespace UECommandForge
{
    class FAIFlowBinder
    {
    public:
        static bool Bind(const FAIFlowSpec& Spec, TArray<FCommandForgeError>& OutErrors);

    private:
        static bool SetAIControllerClass(UBlueprint* CharBP, UBlueprint* CtrlBP,
                                          TArray<FCommandForgeError>& OutErrors);
        static bool SetAutoPossessAI(UBlueprint* CharBP,
                                      TArray<FCommandForgeError>& OutErrors);
        static bool AttachStateTree(UBlueprint* CharBP, UStateTree* ST,
                                     TArray<FCommandForgeError>& OutErrors);
    };
}
