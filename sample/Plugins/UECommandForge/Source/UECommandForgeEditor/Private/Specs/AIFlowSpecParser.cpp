#include "Specs/AIFlowSpecParser.h"
#include "JsonObjectConverter.h"

namespace UECommandForge
{
    bool FAIFlowSpecParser::Parse(const FString& Json, FAIFlowSpec& OutSpec,
                                   TArray<FCommandForgeError>& OutErrors)
    {
        if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutSpec, 0, 0))
        {
            FCommandForgeError Err;
            Err.Code    = TEXT("PARSE_FAILED");
            Err.Message = TEXT("JSON을 FAIFlowSpec으로 변환하지 못했습니다.");
            OutErrors.Add(Err);
            return false;
        }
        return true;
    }
}
