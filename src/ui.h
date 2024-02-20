#pragma once

#include "journal.h"
#include "servicetree.h"
#include <chrono>
#include <functional>
#include <string>
#include <tuilight/terminal.h>
#include <vector>

struct ServiceEntry : wibens::tuilight::detail::HContainer {
    ServiceEntry(ServiceTree::Service *pservice, unsigned *selCount);
    bool handleEvent(wibens::tuilight::KeyEvent event) override;
    void setFocus(bool focus) override { wibens::tuilight::BaseElementImpl::setFocus(focus); };
    [[nodiscard]] bool focusable() const override { return true; }
    static std::string formatDuration(std::chrono::seconds duration);
    void render(wibens::tuilight::View &view) override;

    ServiceTree::Service &service;
    unsigned *selCount;
    wibens::tuilight::Element<wibens::tuilight::detail::Text> selectedText;
    wibens::tuilight::Element<wibens::tuilight::detail::Text> stateTime;
    bool selected = false;
};

class TargetCtlUI
{
  public:
    TargetCtlUI(ServiceTree &stree);
    TargetCtlUI(const TargetCtlUI &) = delete;
    TargetCtlUI(TargetCtlUI &&) = delete;

    operator wibens::tuilight::BaseElement() const { return ui; };

    inline void setStatus(const std::string &message) { statusMessage->text = message + " | "; }

  private:
    void selectAllNone();
    using actionFn = void (ServiceTree::*)(std::string_view);
    void selectedDo(actionFn action);

    ServiceTree &services;
    wibens::tuilight::Element<wibens::tuilight::detail::Text> statusMessage{""};
    wibens::tuilight::BaseElement ui{};
    std::vector<wibens::tuilight::Element<ServiceEntry>> serviceMenuEntries;
    unsigned selectionCount{};
};
