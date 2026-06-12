#include "Misc/AutomationTest.h"
#include "Commandlets/CreateAIFlowCommandlet.h"
#include "CommandForgeTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Specs/AIFlowSpec.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateAIFlowCommandletTest,
    "UECommandForge.Commandlets.CreateAIFlow.ExecutesFullWorkflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCreateAIFlowCommandletTest::RunTest(const FString& Parameters)
{
    const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("test_create_ai_flow.json"));
    IFileManager::Get().Delete(*OutPath);

    FAIFlowSpec Spec;
    Spec.Character.AssetPath = TEXT("/Game/Tests/Workflow/BP_Guard");
    Spec.Character.ParentClass = TEXT("Character");
    Spec.Character.DisplayName = TEXT("GuardCharacter");
    Spec.AIController.AssetPath = TEXT("/Game/Tests/Workflow/BP_GuardCtrl");
    Spec.AIController.ParentClass = TEXT("AIController");
    Spec.AIController.DisplayName = TEXT("GuardController");
    Spec.StateTree.AssetPath = TEXT("/Game/Tests/Workflow/ST_Guard");
    Spec.StateTree.Tasks.Add({ TEXT("Wait"), TEXT("Idle") });
    Spec.StateTree.Tasks.Add({ TEXT("MoveTo"), TEXT("Patrol") });
    Spec.Placement.MapPath = TEXT("/Game/Tests/Workflow/TestMap");
    Spec.Placement.BlueprintPath = TEXT("/Game/Tests/Workflow/BP_Guard");

    FCommandForgeReport Report;
    const bool bOk = UECommandForge::FCreateAIFlowWorkflow::Execute(Spec, OutPath, Report);

    TestTrue(TEXT("전체 플로우 성공"), bOk);
    TestEqual(TEXT("steps 5개"), Report.Steps.Num(), 5);
    TestFalse(TEXT("rollback 불필요"), Report.bRollbackAvailable);
    TestTrue(TEXT("생성 에셋 기록"), Report.CreatedAssets.Contains(Spec.Character.AssetPath)
        && Report.CreatedAssets.Contains(Spec.AIController.AssetPath)
        && Report.CreatedAssets.Contains(Spec.StateTree.AssetPath));
    TestTrue(TEXT("수정 맵 기록"), Report.ModifiedAssets.Contains(Spec.Placement.MapPath));

    FString Content;
    TestTrue(TEXT("파일 존재"), FFileHelper::LoadFileToString(Content, *OutPath));
    TestTrue(TEXT("actor_placed ok"), Content.Contains(TEXT("\"actor_placed\": \"ok\"")));
    TestTrue(TEXT("rollback_available false"), Content.Contains(TEXT("\"rollback_available\": false")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateAIFlowCommandletWriteFailureTest,
    "UECommandForge.Commandlets.CreateAIFlow.ReportsWriteFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCreateAIFlowCommandletWriteFailureTest::RunTest(const FString& Parameters)
{
    const FString BlockingPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("create_ai_flow_report_parent_file"));
    IFileManager::Get().DeleteDirectory(*BlockingPath, false, true);
    IFileManager::Get().Delete(*BlockingPath);
    TestTrue(TEXT("blocking file 쓰기"), FFileHelper::SaveStringToFile(TEXT("block"), *BlockingPath));
    const FString OutPath = FPaths::Combine(BlockingPath, TEXT("test_create_ai_flow.json"));

    FAIFlowSpec Spec;
    Spec.Character.AssetPath = TEXT("/Game/Tests/Workflow/BP_GuardWriteFailure");
    Spec.Character.ParentClass = TEXT("Character");
    Spec.Character.DisplayName = TEXT("GuardCharacterWriteFailure");
    Spec.AIController.AssetPath = TEXT("/Game/Tests/Workflow/BP_GuardCtrlWriteFailure");
    Spec.AIController.ParentClass = TEXT("AIController");
    Spec.AIController.DisplayName = TEXT("GuardControllerWriteFailure");
    Spec.StateTree.AssetPath = TEXT("/Game/Tests/Workflow/ST_GuardWriteFailure");
    Spec.StateTree.Tasks.Add({ TEXT("Wait"), TEXT("Idle") });
    Spec.Placement.MapPath = TEXT("/Game/Tests/Workflow/TestMapWriteFailure");
    Spec.Placement.BlueprintPath = TEXT("/Game/Tests/Workflow/BP_GuardWriteFailure");

    FCommandForgeReport Report;
    const bool bOk = UECommandForge::FCreateAIFlowWorkflow::Execute(Spec, OutPath, Report);

    TestFalse(TEXT("리포트 저장 실패는 플로우 실패"), bOk);
    TestFalse(TEXT("저장 실패 시 report ok false"), Report.bOk);
    TestTrue(TEXT("저장 실패 에러 기록"), Report.Errors.ContainsByPredicate(
        [](const FCommandForgeError& Error)
        {
            return Error.Code == TEXT("REPORT_WRITE_FAILED");
        }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateAIFlowCommandletMainValidationTest,
    "UECommandForge.Commandlets.CreateAIFlow.MainWritesValidationReport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCreateAIFlowCommandletMainValidationTest::RunTest(const FString& Parameters)
{
    const FString SpecPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("invalid_create_ai_flow_spec.json"));
    const FString OutPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("UECommandForge"), TEXT("Reports"), TEXT("invalid_create_ai_flow_report.json"));
    IFileManager::Get().Delete(*SpecPath);
    IFileManager::Get().Delete(*OutPath);
    TestTrue(TEXT("invalid spec 쓰기"), FFileHelper::SaveStringToFile(TEXT("{}"), *SpecPath));

    UCreateAIFlowCommandlet* Commandlet = NewObject<UCreateAIFlowCommandlet>();
    const int32 ExitCode = Commandlet->Main(
        FString::Printf(TEXT("-SpecFile=\"%s\" -Output=\"%s\""), *SpecPath, *OutPath));

    TestEqual(TEXT("validation 실패 exit code"), ExitCode,
        static_cast<int32>(ECommandForgeExitCode::ValidationFailed));

    FString Content;
    TestTrue(TEXT("validation report 파일 존재"), FFileHelper::LoadFileToString(Content, *OutPath));
    TestTrue(TEXT("validation report ok false"), Content.Contains(TEXT("\"ok\": false")));
    TestTrue(TEXT("validation error 포함"), Content.Contains(TEXT("\"REQUIRED_FIELD\"")));
    return true;
}
