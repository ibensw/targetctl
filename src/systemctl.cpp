#include "systemctl.h"
#include "msgreader.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

const char *const SERVICE_NAME = "org.freedesktop.systemd1";
const char *const OBJECT_PATH = "/org/freedesktop/systemd1";
const char *const INTERFACE_MANAGER = "org.freedesktop.systemd1.Manager";
const char *const INTERFACE_UNIT = "org.freedesktop.systemd1.Unit";

namespace Methods
{
const char *START = "StartUnit";
const char *STOP = "StopUnit";
const char *RESTART = "RestartUnit";
const char *RELOAD = "ReloadUnit";
const char *ENABLE = "EnableUnitFiles";
const char *DISABLE = "DisableUnitFiles";
const char *IS_ENABLED = "IsUnitEnabled";
const char *IS_ACTIVE = "IsUnitActive";
const char *GET_UNIT = "GetUnit";
}; // namespace Methods

SystemCtl::SystemCtl()
{
    auto ret = sd_bus_open_system(&bus);
    if (ret < 0) {
        throw std::runtime_error(strerror(errno));
    }
}

SystemCtl::~SystemCtl() { sd_bus_close(bus); }

void SystemCtl::doAction(std::string_view name, const char *action)
{
    DBusMessage reply;
    auto ret = sd_bus_call_method(bus, SERVICE_NAME, OBJECT_PATH, INTERFACE_MANAGER, action, &reply.err(), &reply.msg(),
                                  "ss", name.data(), "replace");
    if (ret < 0) {
        throw std::runtime_error(reply.err().message);
    }
}

void SystemCtl::start(std::string_view name) { doAction(name, Methods::START); }
void SystemCtl::stop(std::string_view name) { doAction(name, Methods::STOP); }
void SystemCtl::restart(std::string_view name) { doAction(name, Methods::RESTART); }
void SystemCtl::reload(std::string_view name) { doAction(name, Methods::RELOAD); }

std::string SystemCtl::getUnitObjectPath(std::string_view name)
{
    DBusMessage reply;
    auto r = sd_bus_call_method(bus, SERVICE_NAME, OBJECT_PATH, INTERFACE_MANAGER, "GetUnit", &reply.err(),
                                &reply.msg(), "s", name.data());
    if (r < 0) {
        throw std::runtime_error(reply.err().message);
    }
    const char *ans;
    r = sd_bus_message_read(reply.msg(), "o", &ans);
    if (r < 0) {
        throw std::runtime_error("Failed to read reply");
    }
    return ans;
}

std::vector<std::string> SystemCtl::getDependants(std::string_view name, RelationType relation)
{
    static constexpr const std::array<std::pair<RelationType, const char *>, 6> relationMap{{
        {RelationType::RequiredBy, "RequiredBy"},
        {RelationType::Requires, "Requires"},
        {RelationType::Wants, "Wants"},
        {RelationType::WantedBy, "WantedBy"},
        {RelationType::ConsistsOf, "ConsistsOf"},
        {RelationType::PartOf, "PartOf"},
    }};

    auto query = std::find_if(relationMap.cbegin(), relationMap.cend(), [relation](const auto &rel) {
                     return rel.first == relation;
                 })->second;

    DBusMessage reply;
    auto ret = sd_bus_get_property(bus, SERVICE_NAME, getUnitObjectPath(name).c_str(), INTERFACE_UNIT, query,
                                   &reply.err(), &reply.msg(), "as");
    if (ret < 0) {
        throw std::runtime_error(strerror(-ret));
    }

    return *DBusMessageReader<std::vector<std::string>>::read(reply);
}

ActiveState SystemCtl::getStatus(std::string_view name)
{
    DBusMessage reply;
    auto ret = sd_bus_get_property(bus, SERVICE_NAME, getUnitObjectPath(name).c_str(), INTERFACE_UNIT, "ActiveState",
                                   &reply.err(), &reply.msg(), "s");
    if (ret < 0) {
        throw std::runtime_error(strerror(-ret));
    }

    static constexpr const std::array<std::pair<std::string_view, ActiveState>, 6> stateMap{{
        {"active", ActiveState::Active},
        {"reloading", ActiveState::Reloading},
        {"inactive", ActiveState::Inactive},
        {"failed", ActiveState::Failed},
        {"activating", ActiveState::Activating},
        {"deactivating", ActiveState::Deactivating},
    }};

    auto state = *DBusMessageReader<std::string>::read(reply);
    return std::find_if(stateMap.cbegin(), stateMap.cend(), [&](const auto &entry) { return entry.first == state; })
        ->second;
}

std::chrono::steady_clock::time_point SystemCtl::getStateChange(std::string_view name)
{
    DBusMessage reply;
    auto ret = sd_bus_get_property(bus, SERVICE_NAME, getUnitObjectPath(name).c_str(), INTERFACE_UNIT,
                                   "StateChangeTimestampMonotonic", &reply.err(), &reply.msg(), "t");
    if (ret < 0) {
        throw std::runtime_error(strerror(-ret));
    }

    auto timestamp = *DBusMessageReader<uint64_t>::read(reply);
    return std::chrono::steady_clock::time_point(std::chrono::microseconds(timestamp));
}
