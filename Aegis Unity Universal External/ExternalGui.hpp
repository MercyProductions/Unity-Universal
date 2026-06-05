#pragma once

#include <string>

namespace Aegis::UnityExternal
{
    int RunExternalGui(const std::wstring& initialTarget = {});
    int RunExternalObjectCacheDiagnostic(
        const std::wstring& target,
        const std::string& primaryComponent,
        const std::string& fallbackComponent = {});
}
