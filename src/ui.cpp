#include "ui.h"

#include <chrono>
#include <fmt/core.h>
#include <fmt/format.h>
#include <terminal.h>

std::string_view stateColor(ActiveState state)
{
    switch (state) {
        case ActiveState::Active:
            return ANSIControlCodes::FG_GREEN;
        case ActiveState::Inactive:
            return "\033[38;5;244m";
        case ActiveState::Activating:
        case ActiveState::Deactivating:
        case ActiveState::Reloading:
            return ANSIControlCodes::FG_YELLOW;
        case ActiveState::Failed:
            return ANSIControlCodes::FG_RED;
            break;
    }
    return ANSIControlCodes::FG_BLACK;
}

ServiceEntry::ServiceEntry(ServiceTree::Service *pservice, unsigned *selCount)
    : HContainer({}), service(*pservice), selCount(selCount), selectedText("[ ]"), stateTime("")
{
    elements.push_back(selectedText);
    std::string indent(service.depth * 2 + 1, ' ');
    elements.push_back(Text(indent + service.name, true) | HStretch());
    elements.push_back(stateTime);
}

bool ServiceEntry::handleEvent(KeyEvent event)
{
    if (event == KeyEvent::RETURN || event == KeyEvent::SPACE) {
        selected = !selected;
        *selCount = *selCount + (selected ? +1 : -1);
        return true;
    }
    return false;
}

std::string ServiceEntry::formatDuration(std::chrono::seconds duration)
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
    result += fmt::format("{:2d}s", seconds);
    return result;
}

void ServiceEntry::render(View &view)
{
    using namespace std::chrono;
    if (isFocused()) {
        view.viewStyle.invert = true;
    }
    view.viewStyle.fgColor = stateColor(service.state);
    selectedText->text = selected ? "[*]" : "[ ]";
    auto uptime = duration_cast<seconds>(steady_clock::now() - service.stateChanged);
    stateTime->text = formatDuration(uptime);
    HContainer::render(view);
}

TargetCtlUI::TargetCtlUI(ServiceTree &stree) : services(stree)
{
    // Make the service list
    services.forEach(
        [this](ServiceTree::Service &service) { serviceMenuEntries.emplace_back(&service, &selectionCount); });
    std::vector<BaseElement> baseServices(serviceMenuEntries.begin(), serviceMenuEntries.end());
    auto serviceMenu = VMenu(baseServices);

    auto statusSelectionText = Text("");
    auto statusSelection = statusSelectionText | PreRender([=](BaseElement e, const View &) {
                               if (selectionCount == 0) {
                                   statusSelectionText->text = "";
                               } else {
                                   statusSelectionText->text = fmt::format("Selected: {} ", selectionCount);
                               }
                           });

    auto statusFailedText = Text("");
    auto statusFailed =
        statusFailedText | Color(ANSIControlCodes::FG_RED) | PreRender([=](BaseElement e, const View &) {
            unsigned failedCount = 0;
            services.forEach([&](ServiceTree::Service &service) {
                if (service.state == ActiveState::Failed) {
                    failedCount++;
                }
            });
            statusFailedText->text = fmt::format("{} failed ", failedCount);
        });
    auto statusActiveText = Text("");
    auto statusActive =
        statusActiveText | Color(ANSIControlCodes::FG_GREEN) | PreRender([=](BaseElement e, const View &) {
            unsigned activeCount = 0;
            unsigned total = 0;
            services.forEach([&](ServiceTree::Service &service) {
                total++;
                if (service.state == ActiveState::Active) {
                    activeCount++;
                }
            });
            statusActiveText->text = fmt::format("{}/{}", activeCount, total);
        });

    auto statusBar = HContainer(statusMessage | HStretch(), statusSelection, statusFailed, statusActive);

    auto actionBar = HContainer(Button("Select All", [this] { selectAllNone(); }),
                                Button("Start", [this] { selectedDo(&ServiceTree::start); }),
                                Button("Stop", [this] { selectedDo(&ServiceTree::stop); }),
                                Button("Restart", [this] { selectedDo(&ServiceTree::restart); }),
                                Button("Reload", [this] { selectedDo(&ServiceTree::reload); }) | Stretch(), statusBar);

    ui = VContainer(serviceMenu | Fit, actionBar);
}

void TargetCtlUI::selectAllNone()
{
    bool select = selectionCount != serviceMenuEntries.size();
    std::for_each(serviceMenuEntries.begin(), serviceMenuEntries.end(),
                  [select](auto &entry) { entry->selected = select; });
    selectionCount = select ? serviceMenuEntries.size() : 0;
}

void TargetCtlUI::selectedDo(actionFn action)
{
    if (selectionCount == 0) {
        setStatus("Nothing selected");
    }
    std::for_each(serviceMenuEntries.cbegin(), serviceMenuEntries.cend(), [this, action](const auto &entry) {
        if (entry->selected) {
            (services.*action)(entry->service.name);
        }
    });
}

#if 0

using namespace ftxui;

const ButtonOption BlockButton = {"test", {}, [](const EntryState &s) {
                                      Element e = text(fmt::format("[ {} ]", s.label));
                                      if (s.focused) {
                                          e = e | inverted;
                                      }
                                      return e;
                                  }};
// const ButtonOption BlockButton{5, {}};

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
        std::string indentName(static_cast<std::size_t>(service.service.depth * 2), ' ');
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
        Button(
            "View journal", [this]() { showJournal(); }, BlockButton),
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

    mainWindow |= CatchEvent([this](Event e) {
        statusText = "";
        if (e.character() == "q" || e == Event::Escape) {
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

    journalWindow = Container::Vertical([this]() {
        Components logLines;
        for (auto &entry : journalEntries) {
            logLines.push_back(MenuEntry(entry.message));
        }
        return logLines;
    }());

    return Renderer(mainWindow, [this] {
        if (showingJournal()) {
            return window(text(fmt::format(" {} ", parent.name)), journalWindow->Render()) |
                   size(HEIGHT, EQUAL, Dimension::Full().dimy);
        } else {
            return window(text(fmt::format(" {} ", parent.name)) | color(stateColor(parent.state)),
                          mainWindow->Render()) |
                   size(HEIGHT, EQUAL, Dimension::Full().dimy);
        }
    });
}
#endif
