#include "Misc/AutomationTest.h"
#include "Specs/DataSchemaSpec.h"
#include "Specs/DataSchemaSpecParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUECommandForgeDataSchemaSpecParserTest,
    "UECommandForge.Specs.DataSchemaSpecParser.ParsesDataSchema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUECommandForgeDataSchemaSpecParserTest::RunTest(const FString& Parameters)
{
    const FString Json = TEXT(R"({
        "version": "1",
        "kind": "data_schema",
        "key_field": "id",
        "source_type": "json",
        "fields": [
            {
                "name": "id",
                "type": "string",
                "required": true,
                "unique": true,
                "regex": "^[A-Z0-9_]+$",
                "severity": "error"
            },
            {
                "name": "cost",
                "type": "int",
                "required": true,
                "min": 0,
                "max": 999
            },
            {
                "name": "category",
                "type": "enum",
                "allowed_values": ["weapon", "armor"],
                "enum_source": "/Script/UECommandForgeSample.EItemCategory"
            },
            {
                "name": "asset",
                "type": "soft_object_path"
            },
            {
                "name": "tag",
                "type": "gameplay_tag",
                "gameplay_tag_source": "ProjectTags"
            }
        ],
        "exceptions": [
            { "field": "id", "value": "LEGACY_001", "reason": "legacy migration" }
        ]
    })");

    FCommandForgeDataSchemaSpec Spec;
    TArray<FCommandForgeError> Errors;
    const bool bOk = UECommandForge::FDataSchemaSpecParser::ParseJson(Json, Spec, Errors);

    TestTrue(TEXT("data schema parse succeeded"), bOk);
    TestEqual(TEXT("schema version"), Spec.Version, TEXT("1"));
    TestEqual(TEXT("schema kind"), Spec.Kind, TEXT("data_schema"));
    TestEqual(TEXT("schema key field"), Spec.KeyField, TEXT("id"));
    TestEqual(TEXT("field count"), Spec.Fields.Num(), 5);
    TestEqual(TEXT("first field name"), Spec.Fields[0].Name, TEXT("id"));
    TestEqual(TEXT("first field type"), Spec.Fields[0].Type, TEXT("string"));
    TestTrue(TEXT("required policy parsed"), Spec.Fields[0].bRequired);
    TestTrue(TEXT("unique policy parsed"), Spec.Fields[0].bUnique);
    TestEqual(TEXT("regex policy parsed"), Spec.Fields[0].Regex, TEXT("^[A-Z0-9_]+$"));
    TestTrue(TEXT("min policy present"), Spec.Fields[1].bHasMin);
    TestEqual(TEXT("min policy parsed"), Spec.Fields[1].Min, 0.0);
    TestTrue(TEXT("max policy present"), Spec.Fields[1].bHasMax);
    TestEqual(TEXT("max policy parsed"), Spec.Fields[1].Max, 999.0);
    TestEqual(TEXT("allowed values parsed"), Spec.Fields[2].AllowedValues.Num(), 2);
    TestEqual(TEXT("enum source parsed"), Spec.Fields[2].EnumSource, TEXT("/Script/UECommandForgeSample.EItemCategory"));
    TestEqual(TEXT("gameplay tag source parsed"), Spec.Fields[4].GameplayTagSource, TEXT("ProjectTags"));
    TestEqual(TEXT("exception parsed"), Spec.Exceptions[0].Value, TEXT("LEGACY_001"));
    TestEqual(TEXT("parse errors empty"), Errors.Num(), 0);

    FCommandForgeDataSchemaSpec InvalidSpec;
    TArray<FCommandForgeError> InvalidErrors;
    const bool bInvalidOk = UECommandForge::FDataSchemaSpecParser::ParseJson(
        TEXT(R"({"version":"1","kind":"data_schema","fields":[{"name":"id","type":"vector"}]})"),
        InvalidSpec,
        InvalidErrors);

    TestFalse(TEXT("unsupported field type rejected"), bInvalidOk);
    TestTrue(TEXT("unsupported type error emitted"), InvalidErrors.Num() > 0);
    return true;
}
