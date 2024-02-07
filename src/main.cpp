#include "systemctl.h"
#include <fmt/format.h>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

using namespace ftxui;

class ServiceEntry
{
  public:
    std::string name;
    ActiveState state;
    bool selected;
};

MenuEntryOption DecorateMenuEntry(const ServiceEntry &service)
{
    MenuEntryOption option;
    option.transform = [&](EntryState state) {
        state.label = (service.selected ? "[*] " : "[ ] ") + state.label;
        Element e = text(state.label);
        switch (service.state) {
            case ActiveState::Active:
                e |= color(Color::Green);
                break;
            case ActiveState::Inactive:
                e |= color(Color::GrayDark);
                break;
            case ActiveState::Activating:
            case ActiveState::Deactivating:
            case ActiveState::Reloading:
                e |= color(Color::Yellow);
                break;
            case ActiveState::Failed:
                e |= color(Color::Red);
        }
        if (state.focused)
            e = e | inverted;
        return e;
    };
    return option;
}

int main(int argc, char *argv[])
{
    auto screen = ScreenInteractive::TerminalOutput();
    SystemCtl ctl;
    std::string_view target = argv[1];
    auto childs = ctl.getDependants(target);
    std::vector<ServiceEntry> services;
    services.reserve(childs.size());
    for (auto &child : childs) {
        auto state = ctl.getStatus(child);
        ServiceEntry entry{child, state, false};
        services.push_back(entry);
    }

    int selected = 0;

    auto menu = Container::Vertical(
        [&]() {
            Components menuentries;
            menuentries.reserve(services.size());
            for (const auto &service : services) {
                menuentries.push_back(MenuEntry(service.name, DecorateMenuEntry(service)));
            }
            return menuentries;
        }(),
        &selected);

    auto selectAll = [&]() {
        bool setValue = true;
        if (std::all_of(services.cbegin(), services.cend(), [](const auto &s) { return s.selected; })) {
            setValue = false;
        }
        std::for_each(services.begin(), services.end(), [setValue](auto &s) { s.selected = setValue; });
    };

    auto forEachSelected = [&](std::function<void(std::string_view)> f) {
        return [&services, &f]() {
            std::for_each(services.cbegin(), services.cend(), [&f](const auto &s) {
                if (s.selected)
                    f(s.name);
            });
        };
    };

    auto wind = Container::Vertical({
        menu | vscroll_indicator | frame | size(HEIGHT, EQUAL, Dimension::Full().dimy - 3),
        Container::Horizontal({
            Button("Select all", selectAll, ButtonOption::Ascii()),
            Button("Start", forEachSelected([&](std::string_view s) { ctl.start(s); }), ButtonOption::Ascii()),
            Button("Stop", forEachSelected([&](std::string_view s) { ctl.stop(s); }), ButtonOption::Ascii()),
            Button("Restart", forEachSelected([&](std::string_view s) { ctl.restart(s); }), ButtonOption::Ascii()),
            Button("Reload", forEachSelected([&](std::string_view s) { ctl.reload(s); }), ButtonOption::Ascii()),
        }),
    });

    wind |= CatchEvent([&](Event e) {
        if (menu->Focused()) {
            if (e == Event::Return || e == Event::Character(' ')) {
                services[selected].selected = !services[selected].selected;
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    });

    auto renderer = Renderer(wind, [&] {
        return window(text(fmt::format(" {} ", target)), wind->Render()) | size(HEIGHT, EQUAL, Dimension::Full().dimy);
    });

    std::atomic<bool> exited = false;
    std::thread updater([&]() {
        static const std::set<ActiveState> quickStates = {
            ActiveState::Activating,
            ActiveState::Deactivating,
            ActiveState::Reloading,
        };
        while (!exited) {
            bool modified = false;
            std::chrono::milliseconds updateInterval{250};
            for (auto &service : services) {
                auto newState = ctl.getStatus(service.name);
                if (service.state != newState) {
                    service.state = newState;
                    modified = true;
                }
                if (quickStates.count(service.state)) {
                    updateInterval = std::chrono::milliseconds{50};
                }
            }
            if (modified) {
                screen.RequestAnimationFrame();
            }
            std::this_thread::sleep_for(updateInterval);
        }
    });
    screen.Loop(renderer);
    exited = true;
    updater.join();
}