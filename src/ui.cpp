#include "ui.h"

#include <chrono>
#include <fmt/format.h>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <fmt/core.h>

using namespace ftxui;

const ButtonOption BlockButton{[](const EntryState &s) {
    Element e = text(fmt::format("[ {} ]", s.label));
    if (s.focused) {
        e = e | inverted;
    }
    return e;
}};

std::string TargetCtlUI::formatDuration(std::chrono::seconds duration)
{
    auto totalSeconds = duration.count();
    auto seconds = totalSeconds % 60;
    auto minutes = totalSeconds / 60 % 60;
    auto hours = totalSeconds / 60 / 60 % 24;
    auto days = totalSeconds / 60 / 60 / 24;
    std::string result;
    bool started = false;
    if (days > 0) {
        result += fmt::format("{:2d}d ", days);
        started = true;
    }
    if (hours > 0 || started) {
        result += fmt::format("{:2d}h ", hours);
        started = true;
    }
    if (minutes > 0 || started) {
        result += fmt::format("{:2d}m ", minutes);
        started = true;
    }
    result += fmt::format("{:2d}s ", seconds);
    return result;
}

ftxui::Color TargetCtlUI::stateColor(ActiveState state)
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

TargetCtlUI::TargetCtlUI(ServiceTree &stree, Closure exitCb)
    : services(stree), parent(services.getParent()), exit(exitCb)
{
    services.forEach([this](ServiceTree::Service &service) {
        if (service.depth > 0)
            this->menuEntries.push_back({service});
    });
}

MenuEntryOption TargetCtlUI::DecorateMenuEntry(const ServiceMenuEntry &service)
{
    MenuEntryOption option;
    option.transform = [&](EntryState state) {
        using namespace std::chrono;
        std::string indentName((service.service.depth) * 2, ' ');
        indentName += service.service.name;
        auto uptime = duration_cast<seconds>(steady_clock::now() - service.service.stateChanged);
        Element e = hbox({
                        text(service.selected ? "[*] " : "[ ] "),
                        text(indentName) | flex,
                        text(formatDuration(uptime)),
                    }) |
                    color(stateColor(service.service.state));
        if (state.focused)
            e = e | inverted;
        return e;
    };
    return option;
}

void TargetCtlUI::selectAllNone()
{
    bool select = nSelected != menuEntries.size();
    std::for_each(menuEntries.begin(), menuEntries.end(), [select](auto &entry) { entry.selected = select; });
}

void TargetCtlUI::selectedDo(actionFn action)
{
    if (nSelected == 0) {
        statusText = "Nothing selected";
    }
    std::for_each(menuEntries.cbegin(), menuEntries.cend(), [this, action](const auto &entry) {
        if (entry.selected) {
            (services.*action)(entry.service.name);
        }
    });
}

Element TargetCtlUI::statusBar()
{
    struct {
        unsigned active{};
        unsigned inactive{};
        unsigned failed{};
    } stateCount;

    nSelected = 0;

    std::for_each(menuEntries.cbegin(), menuEntries.cend(), [&](const auto &s) {
        if (s.selected) {
            nSelected++;
        }
        switch (s.service.state) {
            case ActiveState::Active:
                ++stateCount.active;
                break;
            case ActiveState::Inactive:
                ++stateCount.inactive;
                break;
            case ActiveState::Failed:
                ++stateCount.failed;
                break;
        }
    });

    Elements fields;
    if (!statusText.empty()) {
        fields.push_back(text(statusText));
        fields.push_back(separator());
    }
    if (nSelected) {
        fields.push_back(text(fmt::format("{} selected", nSelected)));
        fields.push_back(separator());
    }
    if (stateCount.failed) {
        fields.push_back(text(fmt::format("{} failed", stateCount.failed)) | color(Color::Red));
        fields.push_back(separator());
    }
    fields.push_back(text(fmt::format("{}/{}", stateCount.active, menuEntries.size())) | color(Color::Green));
    return hbox(fields);
}

TargetCtlUI::operator ftxui::Component()
{
    menu = Container::Vertical(
        [this]() {
            Components menuElems;
            menuElems.reserve(menuEntries.size());
            for (const auto &service : menuEntries) {
                menuElems.push_back(MenuEntry("", DecorateMenuEntry(service)));
            }
            return menuElems;
        }(),
        &menuSelectedIndex);

    using actionFn = void (ServiceTree::*)(std::string_view);

    buttons = Container::Horizontal({
        Button(
            "Select all", [this]() { selectAllNone(); }, BlockButton),
        Button(
            "Start", [this]() { selectedDo(&ServiceTree::start); }, BlockButton),
        Button(
            "Stop", [this]() { selectedDo(&ServiceTree::stop); }, BlockButton),
        Button(
            "Restart", [this]() { selectedDo(&ServiceTree::restart); }, BlockButton),
        Button(
            "Reload", [this]() { selectedDo(&ServiceTree::reload); }, BlockButton),
        // Button("View journal", doJournal, BlockButton),
    });

    buttons |= Renderer([this](Element inner) {
        auto buttons = inner;
        auto status = statusBar();

        buttons->ComputeRequirement();
        status->ComputeRequirement();
        auto width = buttons->requirement().min_x + status->requirement().min_x;

        if (width > Dimension::Full().dimx - 2) {
            return vbox({inner, hbox(separatorEmpty() | flex, status)});
        }

        return hbox({
            inner | flex,
            statusBar(),
        });
    });

    mainWindow = Container::Vertical({
        menu | vscroll_indicator | yframe | flex,
        buttons,
    });

    mainWindow |= CatchEvent([&, this](Event e) {
        statusText = "";
        if (e.character() == "q") {
            exit();
            return true;
        }
        if (menu->Focused()) {
            if (e == Event::Return || e == Event::Character(' ')) {
                menuEntries[menuSelectedIndex].selected = !menuEntries[menuSelectedIndex].selected;
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

    return Renderer(mainWindow, [&, this] {
        return window(text(fmt::format(" {} ", parent.name)) | color(stateColor(parent.state)), mainWindow->Render()) |
               size(HEIGHT, EQUAL, Dimension::Full().dimy);
    });
}
