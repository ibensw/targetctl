#include "servicetree.h"
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

struct ServiceMenuEntry {
    ServiceTree::Service &service;
    bool selected = false;
};

ftxui::Color stateColor(ActiveState state)
{
    switch (state) {
        case ActiveState::Active:
            return Color::Green;
        case ActiveState::Inactive:
            return Color::GrayDark;
        case ActiveState::Activating:
        case ActiveState::Deactivating:
        case ActiveState::Reloading:
            return Color::Yellow;
        case ActiveState::Failed:
            return Color::Red;
            break;
    }
    return Color::Black;
}

std::string formatDuration(std::chrono::seconds duration)
{
    auto totalSeconds = duration.count();
    auto seconds = totalSeconds % 60;
    auto minutes = totalSeconds / 60 % 60;
    auto hours = totalSeconds / 60 / 60 % 24;
    auto days = totalSeconds / 60 / 60 / 24;
    std::string result;
    bool started = false;
    if (days > 0) {
        result += fmt::format("{:02d}d ", days);
        started = true;
    }
    if (hours > 0 || started) {
        result += fmt::format("{:02d}h ", hours);
        started = true;
    }
    if (minutes > 0 || started) {
        result += fmt::format("{:02d}m ", minutes);
        started = true;
    }
    result += fmt::format("{:02d}s ", seconds);
    return result;
}

MenuEntryOption DecorateMenuEntry(const ServiceMenuEntry &service)
{
    MenuEntryOption option;
    option.transform = [&](EntryState state) {
        using namespace std::chrono;
        std::string indentName((service.service.depth) * 2, ' ');
        indentName += service.service.name;
        auto uptime = duration_cast<seconds>(steady_clock::now() - service.service.stateChanged);
        state.label = fmt::format("[{}] {:50}{:>20}", service.selected ? '*' : ' ', indentName, formatDuration(uptime));
        Element e = text(state.label) | color(stateColor(service.service.state));
        if (state.focused)
            e = e | inverted;
        return e;
    };
    return option;
}

int main(int argc, char *argv[])
{
    auto screen = ScreenInteractive::TerminalOutput();
    std::string_view target = argv[1];

    ServiceTree services(target, 1);
    std::vector<ServiceMenuEntry> entries;

    services.forEach([&entries](ServiceTree::Service &service) {
        if (service.depth > 0)
            entries.push_back({service});
    });

    int selected = 0;
    auto menu = Container::Vertical(
        [&]() {
            Components menuentries;
            menuentries.reserve(entries.size());
            for (const auto &service : entries) {
                menuentries.push_back(MenuEntry("", DecorateMenuEntry(service)));
            }
            return menuentries;
        }(),
        &selected);

    auto selectAll = [&]() {
        bool setValue = true;
        if (std::all_of(entries.cbegin(), entries.cend(), [](const auto &s) { return s.selected; })) {
            setValue = false;
        }
        std::for_each(entries.begin(), entries.end(), [setValue](auto &s) { s.selected = setValue; });
    };

    auto forEachSelected = [&](std::function<void(std::string_view)> f) {
        return [&entries, &f]() {
            std::for_each(entries.cbegin(), entries.cend(), [&f](const auto &s) {
                if (s.selected)
                    f(s.service.name);
            });
        };
    };

    auto buttons = Container::Horizontal({
        Button("Select all", selectAll, ButtonOption::Ascii()),
        Button("Start", forEachSelected([&](std::string_view s) { services.start(s); }), ButtonOption::Ascii()),
        Button("Stop", forEachSelected([&](std::string_view s) { services.stop(s); }), ButtonOption::Ascii()),
        Button("Restart", forEachSelected([&](std::string_view s) { services.restart(s); }), ButtonOption::Ascii()),
        Button("Reload", forEachSelected([&](std::string_view s) { services.reload(s); }), ButtonOption::Ascii()),
        Button(
            "View journal", []() {}, ButtonOption::Ascii()),
    });

    auto wind = Container::Vertical({
        menu | vscroll_indicator | frame | size(HEIGHT, EQUAL, Dimension::Full().dimy - 3),
        buttons,
    });

    wind |= CatchEvent([&](Event e) {
        if (menu->Focused()) {
            if (e == Event::Return || e == Event::Character(' ')) {
                entries[selected].selected = !entries[selected].selected;
                return true;
            } else if (e == Event::Tab) {
                buttons->TakeFocus();
                return true;
            }
            return false;
        } else if (buttons->Focused()) {
            if (e == Event::TabReverse) {
                menu->TakeFocus();
                return true;
            }
        }
        return false;
    });

    auto renderer = Renderer(wind, [&] {
        auto &parent = services.getParent();
        return window(text(fmt::format(" {} ", parent.name)) | color(stateColor(parent.state)), wind->Render()) |
               size(HEIGHT, EQUAL, Dimension::Full().dimy);
    });

    std::atomic<bool> exited = false;
    std::thread updater([&]() {
        using namespace std::chrono;
        static const std::set<ActiveState> quickStates = {
            ActiveState::Activating,
            ActiveState::Deactivating,
            ActiveState::Reloading,
        };
        auto lastSecs = time_point_cast<seconds>(steady_clock::now());
        while (!exited) {
            bool modified = services.update();
            auto currentSecs = time_point_cast<seconds>(steady_clock::now());
            if (modified || lastSecs != currentSecs) {
                lastSecs = currentSecs;
                screen.RequestAnimationFrame();
                std::this_thread::sleep_for(std::chrono::milliseconds{50});
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds{1000});
            }
        }
    });
    screen.Loop(renderer);
    exited = true;
    updater.join();
}
