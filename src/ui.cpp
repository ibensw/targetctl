#include "ui.h"

#include <chrono>
#include <fmt/core.h>
#include <fmt/format.h>
#include <tuilight/terminal.h>

using namespace wibens::tuilight;

Color stateColor(ActiveState state)
{
    switch (state) {
        case ActiveState::Active:
            return Color::Green;
        case ActiveState::Inactive:
            return Color::Gray;
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
    auto statusFailedText = Text("");
    auto statusActiveText = Text("");

    auto fillStatusBar = [=, this](BaseElement e, const View &) {
        if (selectionCount == 0) {
            statusSelectionText->text = "";
        } else {
            statusSelectionText->text = fmt::format("{} selected ", selectionCount);
        }
        unsigned failedCount = 0;
        unsigned activeCount = 0;
        unsigned total = 0;
        services.forEach([&](ServiceTree::Service &service) {
            if (service.state == ActiveState::Failed) {
                failedCount++;
            }
            total++;
            if (service.state == ActiveState::Active) {
                activeCount++;
            }
        });
        statusFailedText->text = fmt::format("{} failed ", failedCount);
        statusActiveText->text = fmt::format("{}/{}", activeCount, total);
    };

    auto statusBar =
        HContainer(statusMessage | HStretch(), statusSelectionText, statusFailedText | ForegroundColor(Color::Red),
                   statusActiveText | ForegroundColor(Color::Green));

    auto actionBar = HContainer(Button("Select All", [this] { selectAllNone(); }),
                                Button("Start", [this] { selectedDo(&ServiceTree::start); }),
                                Button("Stop", [this] { selectedDo(&ServiceTree::stop); }),
                                Button("Restart", [this] { selectedDo(&ServiceTree::restart); }),
                                Button("Reload", [this] { selectedDo(&ServiceTree::reload); }) | Stretch(), statusBar);

    ui = VContainer(serviceMenu | Fit, actionBar) | PreRender(fillStatusBar);
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
