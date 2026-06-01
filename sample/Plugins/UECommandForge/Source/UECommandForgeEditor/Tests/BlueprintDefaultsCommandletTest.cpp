#include "Misc/AutomationTest.h"

#include "Builders/GenericBlueprintBuilder.h"
#include "CommandForgeTypes.h"
#include "Commandlets/SetBlueprintDefaultsCommandlet.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Specs/BlueprintSpec.h"
#include "UObject/GarbageCollection.h"

namespace
{
    void CleanupBlueprintAsset(const FString& AssetPath)
    {
        const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
        if (UPackage* Package = FindPackage(nullptr, *AssetPath))
        {
            Package->FullyLoad();
            if (UObject* Asset = FindObject<UObject>(Package, *AssetName))
            {
                TArray<UObject*> ObjectsToDelete;
                ObjectsToDelete.Add(Asset);
                ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
                CollectGarbage(RF_NoFlags);
            }
        }

        const FString FileName = FPaths::ConvertRelativePathToFull(
            FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension()));
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (PlatformFile.FileExists(*FileName))
        {
            PlatformFile.DeleteFile(*FileName);
        }
    }

    void DeleteFileIfExists(const FString& FilePath)
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (PlatformFile.FileExists(*FilePath))
        {
            PlatformFile.DeleteFile(*FilePath);
        }
    }

    struct FScopedBlueprintAssetCleanup
    {
        explicit FScopedBlueprintAssetCleanup(const FString& InAssetPath)
            : AssetPath(InAssetPath)
        {
        }

        ~FScopedBlueprintAssetCleanup()
        {
            CleanupBlueprintAsset(AssetPath);
        }

        FString AssetPath;
    };

    struct FScopedFileCleanup
    {
        explicit FScopedFileCleanup(const FString& InFilePath)
            : FilePath(InFilePath)
        {
        }

        ~FScopedFileCleanup()
        {
            DeleteFileIfExists(FilePath);
        }

        FString FilePath;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGenericBlueprintBuilderCreatesActorTest,
    "UECommandForge.Builders.GenericBlueprintBuilder.CreatesActorBlueprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGenericBlueprintBuilderCreatesActorTest::RunTest(const FString& Parameters)
{
    FBlueprintSpec Spec;
    Spec.AssetPath = TEXT("/Game/Tests/BP_GenericActor");
    Spec.ParentClass = TEXT("Actor");
    Spec.DisplayName = TEXT("GenericActor");
    FScopedBlueprintAssetCleanup Cleanup(Spec.AssetPath);

    UECommandForge::FGenericBlueprintBuildOptions Options;
    Options.FieldPrefix = TEXT("Blueprint");
    Options.ParentClassModules = { TEXT("Engine") };

    TArray<FCommandForgeError> Errors;
    TMap<FString, FString> Validation;
    const bool bOk = UECommandForge::FGenericBlueprintBuilder::Build(Spec, Options, Errors, Validation);

    TestTrue(TEXT("generic blueprint build ok"), bOk);
    TestTrue(TEXT("generic blueprint errors empty"), Errors.IsEmpty());
    TestEqual(TEXT("compile status"), Validation.FindRef(TEXT("compile_status")), FString(TEXT("ok")));
    TestEqual(TEXT("asset on disk"), Validation.FindRef(TEXT("asset_on_disk")), FString(TEXT("true")));
    TestTrue(TEXT("parent class recorded"), Validation.Contains(TEXT("parent_class")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSetBlueprintDefaultsCommandletTest,
    "UECommandForge.Commandlets.SetBlueprintDefaults.AppliesDefaultsAndComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSetBlueprintDefaultsCommandletTest::RunTest(const FString& Parameters)
{
    FBlueprintSpec BlueprintSpec;
    BlueprintSpec.AssetPath = TEXT("/Game/Tests/BP_DefaultsActor");
    BlueprintSpec.ParentClass = TEXT("Actor");
    BlueprintSpec.DisplayName = TEXT("DefaultsActor");
    FScopedBlueprintAssetCleanup Cleanup(BlueprintSpec.AssetPath);

    UECommandForge::FGenericBlueprintBuildOptions Options;
    Options.FieldPrefix = TEXT("Blueprint");
    Options.ParentClassModules = { TEXT("Engine") };

    TArray<FCommandForgeError> BuildErrors;
    TMap<FString, FString> BuildValidation;
    TestTrue(TEXT("fixture blueprint created"),
        UECommandForge::FGenericBlueprintBuilder::Build(BlueprintSpec, Options, BuildErrors, BuildValidation));

    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("set_blueprint_defaults_spec.json"));
    const FString ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("set_blueprint_defaults_report.json"));
    FScopedFileCleanup SpecFileCleanup(SpecPath);
    FScopedFileCleanup ReportFileCleanup(ReportPath);
    const FString Json = FString::Printf(TEXT(R"JSON(
{
  "version": "1",
  "kind": "blueprint_defaults",
  "transaction_id": "tx-set-blueprint-defaults-test",
  "blueprint": {
    "asset_path": "%s",
    "compile": true,
    "save": true
  },
  "defaults": [
    { "property": "bHidden", "value": "True" }
  ],
  "components": [
    { "name": "CodexSceneComponent", "class": "SceneComponent" }
  ]
}
)JSON"), *BlueprintSpec.AssetPath);

    TestTrue(TEXT("spec write"),
        FFileHelper::SaveStringToFile(Json, *SpecPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    USetBlueprintDefaultsCommandlet* Commandlet = NewObject<USetBlueprintDefaultsCommandlet>();
    const int32 ExitCode = Commandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\""), *SpecPath, *ReportPath));

    TestEqual(TEXT("commandlet exit ok"), ExitCode, 0);
    TestTrue(TEXT("report exists"), FPaths::FileExists(ReportPath));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintSpec.AssetPath);
    TestNotNull(TEXT("blueprint reload"), Blueprint);
    if (!Blueprint || !Blueprint->GeneratedClass)
    {
        return false;
    }

    UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
    FProperty* HiddenProperty = Blueprint->GeneratedClass->FindPropertyByName(TEXT("bHidden"));
    TestNotNull(TEXT("bHidden property"), HiddenProperty);
    if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(HiddenProperty))
    {
        TestTrue(TEXT("bHidden applied"), BoolProperty->GetPropertyValue_InContainer(CDO));
    }

    USetBlueprintDefaultsCommandlet* IdempotentCommandlet = NewObject<USetBlueprintDefaultsCommandlet>();
    const int32 IdempotentExitCode = IdempotentCommandlet->Main(FString::Printf(
        TEXT("-Spec=\"%s\" -Output=\"%s\""), *SpecPath, *ReportPath));
    TestEqual(TEXT("idempotent commandlet exit ok"), IdempotentExitCode, 0);

    FString ReportJson;
    TestTrue(TEXT("idempotent report read"), FFileHelper::LoadFileToString(ReportJson, *ReportPath));
    TSharedPtr<FJsonObject> ReportObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReportJson);
    TestTrue(TEXT("idempotent report parse"), FJsonSerializer::Deserialize(Reader, ReportObject));
    if (ReportObject.IsValid())
    {
        TestFalse(TEXT("idempotent run not applied"), ReportObject->GetBoolField(TEXT("applied")));
        const TSharedPtr<FJsonObject>* ValidationObject = nullptr;
        if (ReportObject->TryGetObjectField(TEXT("validation"), ValidationObject) &&
            ValidationObject && ValidationObject->IsValid())
        {
            TestEqual(TEXT("idempotent run unchanged"),
                (*ValidationObject)->GetStringField(TEXT("changed")), FString(TEXT("false")));
        }
    }
    return true;
}
