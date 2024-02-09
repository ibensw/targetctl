#pragma once

#include "systemctl.h"
#include <chrono>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class ServiceTree : public SystemCtl
{
  public:
    struct Service {
        std::string name;
        ActiveState state;
        std::chrono::steady_clock::time_point stateChanged;
        unsigned depth;
        std::vector<Service> children;
    };

    ServiceTree(std::string_view name, std::size_t maxDepth = std::numeric_limits<std::size_t>::max());
    ~ServiceTree() = default;

    bool update();
    [[nodiscard]] Service &getParent() { return parent; }

    template <typename T> void forEach(T callback) { forEachImpl(callback, parent); }

  private:
    Service addService(std::set<std::string> &seen, std::string_view service, std::size_t maxDepth, unsigned level = 0);
    template <typename T> void forEachImpl(T callback, Service &service)
    {
        callback(service);
        for (auto &child : service.children) {
            forEachImpl(callback, child);
        }
    }

    Service parent;
};
