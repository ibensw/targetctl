#include "systemctl.h"
#include <fmt/format.h>
#include <functional>
#include <iostream>
#include <memory>
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
        state.label = (service.selected ? "[X] " : "[ ] ") + state.label;
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

    auto renderer = Renderer(menu, [&] {
        return window(text(fmt::format(" {} ", target)), menu->Render() | vscroll_indicator | frame) |
               size(HEIGHT, EQUAL, Dimension::Full().dimy);
    });

    renderer |= CatchEvent([&](Event e) {
        if (e == Event::Return || e == Event::Character(' ')) {
            services[selected].selected = !services[selected].selected;
            return true;
        } else {
            return false;
        }
    });

    std::atomic<bool> exited = false;
    std::thread updater([&]() {
        while (!exited) {
            bool modified = false;
            for (auto &service : services) {
                auto newState = ctl.getStatus(service.name);
                if (service.state != newState) {
                    service.state = newState;
                    modified = true;
                }
            }
            if (modified) {
                screen.RequestAnimationFrame();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
    });
    screen.Loop(renderer);
    exited = true;
    updater.join();
}