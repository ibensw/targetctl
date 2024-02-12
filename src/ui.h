#pragma once

#include "servicetree.h"
#include <chrono>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/task.hpp>
#include <ftxui/screen/color.hpp>
#include <functional>
#include <string>
#include <vector>

class TargetCtlUI
{
  public:
    TargetCtlUI(ServiceTree &stree, ftxui::Closure exitCb);
    TargetCtlUI(const TargetCtlUI &) = delete;
    TargetCtlUI(TargetCtlUI &&) = delete;

    operator ftxui::Component();

    void selectAllNone();
    using actionFn = void (ServiceTree::*)(std::string_view);
    void selectedDo(actionFn action);
    inline void setStatus(const std::string &message) { statusText = message; }

  private:
    struct ServiceMenuEntry {
        const ServiceTree::Service &service;
        bool selected = false;
    };
    ServiceTree &services;
    std::vector<ServiceMenuEntry> menuEntries{};
    ServiceTree::Service &parent;
    ftxui::Closure exit;
    int menuSelectedIndex{};
    ftxui::Element statusBar();
    std::string statusText;
    unsigned nSelected;

    static ftxui::MenuEntryOption DecorateMenuEntry(const ServiceMenuEntry &service);
    static std::string formatDuration(std::chrono::seconds duration);
    static ftxui::Color stateColor(ActiveState state);

    ftxui::Component menu;
    ftxui::Component buttons;
    ftxui::Component mainWindow;
};
