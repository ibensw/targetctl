#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <systemd/sd-bus.h>
#include <vector>

class DBusMessage
{
  public:
    DBusMessage() = default;
    inline ~DBusMessage()
    {
        if (message != nullptr) {
            sd_bus_message_unref(message);
        }
        if (error._need_free) {
            sd_bus_error_free(&error);
        }
    }
    inline sd_bus_message *&msg() { return message; }

    inline sd_bus_error &err() { return error; }

  private:
    sd_bus_message *message = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
};

enum class ActiveState {
    Active,
    Reloading,
    Inactive,
    Failed,
    Activating,
    Deactivating,
};

enum class RelationType {
    RequiredBy,
    Requires,
    Wants,
    WantedBy,
    ConsistsOf,
    PartOf,
};

class SystemCtl
{
  public:
    SystemCtl();
    ~SystemCtl();

    void start(std::string_view name);
    void stop(std::string_view name);
    void restart(std::string_view name);
    void reload(std::string_view name);
    ActiveState getStatus(std::string_view name);
    std::chrono::steady_clock::time_point getStateChange(std::string_view name);
    std::vector<std::string> getDependants(std::string_view name, RelationType relation);

  private:
    void doAction(std::string_view name, const char *action);
    std::string getUnitObjectPath(std::string_view name);
    std::vector<std::string> readA(std::string_view name, std::string_view property);
    sd_bus *bus = nullptr;
};
