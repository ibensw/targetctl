#include "servicetree.h"
#include <algorithm>
#include <fmt/format.h>

ServiceTree::Service ServiceTree::addService(std::set<std::string> &seen, std::string_view service,
                                             std::size_t maxDepth, unsigned level)
{
    seen.insert(std::string(service));
    Service serviceObj{std::string(service), {}, {}, level, {}};

    auto childNames = getDependants(service);
    std::sort(childNames.begin(), childNames.end());
    serviceObj.children.reserve(childNames.size());
    if (maxDepth > 0) {
        for (auto childName : childNames) {
            if (seen.count(childName)) {
                fmt::print("Ignored duplicate in dependency chain: {}\n", childName);
            } else {
                serviceObj.children.push_back(addService(seen, childName, maxDepth - 1, level + 1));
            }
        }
    }
    return serviceObj;
};

ServiceTree::ServiceTree(std::string_view name, std::size_t maxDepth)
{
    std::set<std::string> seen;
    parent = addService(seen, name, maxDepth);
    update();
}

bool ServiceTree::update()
{
    bool modified = false;
    auto updateService = [this, &modified](Service &service) {
        service.state = getStatus(service.name);
        auto lastChange = getStateChange(service.name);
        if (lastChange != service.stateChanged) {
            service.stateChanged = getStateChange(service.name);
            modified = true;
        }
    };
    std::for_each(parent.children.begin(), parent.children.end(), updateService);
    return modified;
}
