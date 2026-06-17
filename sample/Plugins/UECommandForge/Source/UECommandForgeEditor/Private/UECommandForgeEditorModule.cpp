#include "UECommandForgeEditorModule.h"
#include "Modules/ModuleManager.h"

extern void LinkDiffPlatformConfigCommandletTest();
extern void LinkValidatePluginDependenciesCommandletTest();
extern void LinkPrototypeAutomationCommandletTest();

void FUECommandForgeEditorModule::StartupModule()
{
    // Force link tests to prevent linker dead-code elimination on macOS/Linux
    LinkDiffPlatformConfigCommandletTest();
    LinkValidatePluginDependenciesCommandletTest();
    LinkPrototypeAutomationCommandletTest();
}

void FUECommandForgeEditorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FUECommandForgeEditorModule, UECommandForgeEditor);

