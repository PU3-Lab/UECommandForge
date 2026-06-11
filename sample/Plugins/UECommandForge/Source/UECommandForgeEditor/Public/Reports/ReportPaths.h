#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace UECommandForge
{
    /** 프로젝트 Saved 아래 UECommandForge 리포트 서브경로. 모든 commandlet이 이 상수만 참조한다. */
    inline const TCHAR* ReportsSubDir()
    {
        return TEXT("UECommandForge/Reports");
    }

    /** 정규화된 절대 리포트 디렉터리. (= <Project>/Saved/UECommandForge/Reports) */
    inline FString GetReportsDir()
    {
        FString ReportsDir = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), ReportsSubDir()));
        FPaths::NormalizeDirectoryName(ReportsDir);
        FPaths::CollapseRelativeDirectories(ReportsDir);
        return ReportsDir;
    }
}
