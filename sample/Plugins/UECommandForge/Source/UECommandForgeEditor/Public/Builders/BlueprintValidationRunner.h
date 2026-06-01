#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"

class UBlueprint;

namespace UECommandForge
{
    class FBlueprintValidationRunner
    {
    public:
        static bool Compile(UBlueprint* Blueprint,
                            const FString& AssetPath,
                            const FString& Field,
                            TArray<FCommandForgeError>& OutErrors,
                            TMap<FString, FString>& OutValidation);

        static bool Save(UBlueprint* Blueprint,
                         const FString& AssetPath,
                         const FString& Field,
                         TArray<FCommandForgeError>& OutErrors,
                         TMap<FString, FString>& OutValidation);

        static bool ValidateRegistry(const FString& AssetPath,
                                     const FString& FileName,
                                     TMap<FString, FString>& OutValidation);

        static bool CompileSaveAndValidate(UBlueprint* Blueprint,
                                           const FString& AssetPath,
                                           const FString& Field,
                                           TArray<FCommandForgeError>& OutErrors,
                                           TMap<FString, FString>& OutValidation);
    };
}
