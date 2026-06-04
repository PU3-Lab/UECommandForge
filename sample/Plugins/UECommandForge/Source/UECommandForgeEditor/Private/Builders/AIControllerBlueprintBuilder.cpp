#include "Builders/AIControllerBlueprintBuilder.h"
#include "Builders/GenericBlueprintBuilder.h"

namespace UECommandForge
{
    bool FAIControllerBlueprintBuilder::Build(const FBlueprintSpec& Spec,
                                               TArray<FCommandForgeError>& OutErrors,
                                               TMap<FString, FString>& OutValidation)
    {
        FGenericBlueprintBuildOptions Options;
        Options.FieldPrefix = TEXT("AIController");
        Options.ParentClassModules = { TEXT("AIModule"), TEXT("Engine") };
        Options.bAllowReplace = true;
        return FGenericBlueprintBuilder::Build(Spec, Options, OutErrors, OutValidation);
    }
}
