#include "Builders/CharacterBlueprintBuilder.h"
#include "Builders/GenericBlueprintBuilder.h"

namespace UECommandForge
{
    bool FCharacterBlueprintBuilder::Build(const FBlueprintSpec& Spec,
                                            TArray<FCommandForgeError>& OutErrors,
                                            TMap<FString, FString>& OutValidation)
    {
        FGenericBlueprintBuildOptions Options;
        Options.FieldPrefix = TEXT("Character");
        Options.ParentClassModules = { TEXT("Engine") };
        Options.bAllowReplace = true;
        return FGenericBlueprintBuilder::Build(Spec, Options, OutErrors, OutValidation);
    }
}
