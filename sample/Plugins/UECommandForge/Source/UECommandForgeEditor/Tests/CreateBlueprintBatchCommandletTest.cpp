#include "Misc/AutomationTest.h"
#include "Commandlets/CreateBlueprintBatchCommandlet.h"
#include "CommandForgeTypes.h"
#include "Builders/GenericBlueprintBuilder.h"
#include "Engine/Blueprint.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateBlueprintBatchSuccessTest,
    "UECommandForge.Commandlets.CreateBlueprintBatch.ExecutesSuccessfully",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCreateBlueprintBatchSuccessTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_bp_batch_success_spec.json"));
    const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_bp_batch_success_report.json"));

    IFileManager& FileManager = IFileManager::Get();
    FileManager.Delete(*SpecPath);
    FileManager.Delete(*OutPath);

    // 사전에 테스트 에셋이 존재한다면 삭제하여 클린 상태 확보
    const FString AssetPath1 = TEXT("/Game/Tests/BP_BatchSuccessChar");
    const FString AssetPath2 = TEXT("/Game/Tests/BP_BatchSuccessActor");
    TArray<FCommandForgeError> DeleteErrors;
    UECommandForge::FGenericBlueprintBuilder::DeleteExistingBlueprint(AssetPath1, TEXT("Cleanup"), DeleteErrors);
    UECommandForge::FGenericBlueprintBuilder::DeleteExistingBlueprint(AssetPath2, TEXT("Cleanup"), DeleteErrors);

    // 정상 배치 스펙 JSON 구성
    const FString SpecJson = TEXT(R"({
        "version": "1",
        "kind": "blueprint_batch",
        "transaction_id": "tx_success_123",
        "compile": true,
        "save": true,
        "items": [
            {
                "asset_path": "/Game/Tests/BP_BatchSuccessChar",
                "parent_class": "Character",
                "allow_replace": true,
                "components": [],
                "defaults": []
            },
            {
                "asset_path": "/Game/Tests/BP_BatchSuccessActor",
                "parent_class": "Actor",
                "allow_replace": true,
                "components": [],
                "defaults": []
            }
        ]
    })");

    TestTrue(TEXT("Spec 파일 쓰기"), FFileHelper::SaveStringToFile(SpecJson, *SpecPath));

    UCreateBlueprintBatchCommandlet* Commandlet = NewObject<UCreateBlueprintBatchCommandlet>();
    const int32 ExitCode = Commandlet->Main(
        FString::Printf(TEXT("-Spec=\"%s\" -Output=\"%s\""), *SpecPath, *OutPath));

    TestEqual(TEXT("배치 실행 exit code 성공"), ExitCode, 0);

    // 생성 확인
    UBlueprint* BP1 = LoadObject<UBlueprint>(nullptr, *AssetPath1);
    UBlueprint* BP2 = LoadObject<UBlueprint>(nullptr, *AssetPath2);
    TestNotNull(TEXT("첫 번째 블루프린트 생성 확인"), BP1);
    TestNotNull(TEXT("두 번째 블루프린트 생성 확인"), BP2);

    // 테스트 정리
    UECommandForge::FGenericBlueprintBuilder::DeleteExistingBlueprint(AssetPath1, TEXT("Cleanup"), DeleteErrors);
    UECommandForge::FGenericBlueprintBuilder::DeleteExistingBlueprint(AssetPath2, TEXT("Cleanup"), DeleteErrors);
    FileManager.Delete(*SpecPath);
    FileManager.Delete(*OutPath);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateBlueprintBatchRollbackTest,
    "UECommandForge.Commandlets.CreateBlueprintBatch.RollsbackOnFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCreateBlueprintBatchRollbackTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_bp_batch_rollback_spec.json"));
    const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("CodexReports"), TEXT("test_bp_batch_rollback_report.json"));

    IFileManager& FileManager = IFileManager::Get();
    FileManager.Delete(*SpecPath);
    FileManager.Delete(*OutPath);

    const FString OriginalAssetPath = TEXT("/Game/Tests/BP_BatchOriginalActor");
    const FString FailedAssetPath = TEXT("/Game/Tests/BP_BatchFailedActor");

    // 1. 기존 에셋 청소 및 사전 생성
    TArray<FCommandForgeError> TempErrors;
    UECommandForge::FGenericBlueprintBuilder::DeleteExistingBlueprint(OriginalAssetPath, TEXT("Cleanup"), TempErrors);
    UECommandForge::FGenericBlueprintBuilder::DeleteExistingBlueprint(FailedAssetPath, TEXT("Cleanup"), TempErrors);

    // 원본 블루프린트를 미리 정상 빌드하여 저장해 둡니다.
    FBlueprintSpec PreSpec;
    PreSpec.AssetPath = OriginalAssetPath;
    PreSpec.ParentClass = TEXT("Actor");
    PreSpec.bAllowReplace = true;
    PreSpec.bCompile = true;
    PreSpec.bSave = true;
    
    UECommandForge::FGenericBlueprintBuildOptions Options;
    Options.bAllowReplace = true;
    Options.bCompile = true;
    Options.bSave = true;
    Options.ParentClassModules = { TEXT("Engine") };
    TMap<FString, FString> StepVal;
    
    TestTrue(TEXT("사전 원본 에셋 생성"), 
        UECommandForge::FGenericBlueprintBuilder::Build(PreSpec, Options, TempErrors, StepVal));

    UBlueprint* OriginalBP = LoadObject<UBlueprint>(nullptr, *OriginalAssetPath);
    TestNotNull(TEXT("원본 블루프린트 로드 성공 확인"), OriginalBP);

    // 2. 배치 스펙 작성
    // 첫 번째 아이템: 기존 원본 에셋을 수정하려 시도 (allow_replace: true)
    // 두 번째 아이템: 존재하지 않는 무효한 클래스(NonExistentClass)를 부모로 삼아 강제 실패 유도
    const FString SpecJson = TEXT(R"({
        "version": "1",
        "kind": "blueprint_batch",
        "transaction_id": "tx_rollback_123",
        "compile": true,
        "save": true,
        "items": [
            {
                "asset_path": "/Game/Tests/BP_BatchOriginalActor",
                "parent_class": "Character",
                "allow_replace": true,
                "components": [],
                "defaults": []
            },
            {
                "asset_path": "/Game/Tests/BP_BatchFailedActor",
                "parent_class": "NonExistentClass",
                "allow_replace": true,
                "components": [],
                "defaults": []
            }
        ]
    })");

    TestTrue(TEXT("Rollback Spec 파일 쓰기"), FFileHelper::SaveStringToFile(SpecJson, *SpecPath));

    UCreateBlueprintBatchCommandlet* Commandlet = NewObject<UCreateBlueprintBatchCommandlet>();
    const int32 ExitCode = Commandlet->Main(
        FString::Printf(TEXT("-Spec=\"%s\" -Output=\"%s\""), *SpecPath, *OutPath));

    TestNotEqual(TEXT("배치 실행 exit code 실패"), ExitCode, 0);

    // 3. 롤백 검증
    // 3.1. 무효 부모 클래스 때문에 생성을 시도했던 실패 대상 에셋은 디스크/메모리에 없어야 함
    UBlueprint* FailedBP = LoadObject<UBlueprint>(nullptr, *FailedAssetPath);
    TestNull(TEXT("실패 에셋은 생성되어 남아있으면 안 됨"), FailedBP);

    // 3.2. 첫 번째 아이템(수정 시도 대상)은 롤백되어 이전 상태인 Actor 계층을 유지하고 있어야 함
    UBlueprint* RestoredBP = LoadObject<UBlueprint>(nullptr, *OriginalAssetPath);
    TestNotNull(TEXT("복구된 원본 에셋 로드"), RestoredBP);
    if (RestoredBP)
    {
        TestEqual(TEXT("부모 클래스가 원래 Actor 형태여야 함"), 
            RestoredBP->ParentClass->GetName(), TEXT("Actor"));
    }

    // 4. 테스트 청소
    UECommandForge::FGenericBlueprintBuilder::DeleteExistingBlueprint(OriginalAssetPath, TEXT("Cleanup"), TempErrors);
    FileManager.Delete(*SpecPath);
    FileManager.Delete(*OutPath);

    return true;
}
