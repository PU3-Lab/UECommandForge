#pragma once

#include "CoreMinimal.h"
#include "CommandForgeTypes.h"
#include "Specs/PlacementSpec.h"

namespace UECommandForge
{
    class FMapActorPlacer
    {
    public:
        static bool Place(const FPlacementSpec& Spec, TArray<FCommandForgeError>& OutErrors);
        static bool ValidatePlacement(const FPlacementSpec& Spec,
                                      TMap<FString, FString>& OutValidation,
                                      TArray<FCommandForgeError>& OutErrors);

    private:
        static UWorld* LoadOrCreateMap(const FString& MapPath,
                                       TArray<FCommandForgeError>& OutErrors);
    };
}
