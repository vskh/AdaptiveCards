#include "pch.h"
#include "AdaptiveRequirement.h"

namespace winrt::AdaptiveCards::ObjectModel::WinUI3::implementation
{
    AdaptiveRequirement::AdaptiveRequirement(hstring const& requirementName, hstring const& requirementversion) :
        Name{requirementName}, Version{requirementversion}
    {
    }
    AdaptiveRequirement::AdaptiveRequirement(std::pair<const std::string, ::AdaptiveCards::SemanticVersion> const& shared) :
        Name{UTF8ToHString(shared.first)}, Version{UTF8ToHString(static_cast<std::string>(shared.second))}
    {
    }
}
