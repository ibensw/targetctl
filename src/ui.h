#pragma once

#include "journal.h"
#include "servicetree.h"
#include <chrono>
#include <functional>
#include <string>
#include <terminal.h>
#include <vector>

struct ServiceEntry : detail::HContainer {
    ServiceEntry(ServiceTree::Service *pservice, unsigned *selCount);
    bool handleEvent(KeyEvent event) override;
    void setFocus(bool focus) override { BaseElementImpl::setFocus(focus); };
    [[nodiscard]] bool focusable() const override { return true; }
    static std::string formatDuration(std::chrono::seconds duration);
    void render(View &view) override;

    ServiceTree::Service &service;
    unsigned *selCount;
    Element<detail::Text> selectedText;
    Element<detail::Text> stateTime;
    bool selected = false;
};

class TargetCtlUI
{
  public:
    TargetCtlUI(ServiceTree &stree);
    TargetCtlUI(const TargetCtlUI &) = delete;
    TargetCtlUI(TargetCtlUI &&) = delete;

    operator BaseElement() const { return ui; };

    inline void setStatus(const std::string &message) { statusMessage->text = message; }

  private:
    void selectAllNone();
    using actionFn = void (ServiceTree::*)(std::string_view);
    void selectedDo(actionFn action);

    ServiceTree &services;
    Element<detail::Text> statusMessage = Text("");
    BaseElement ui{};
    std::vector<Element<ServiceEntry>> serviceMenuEntries;
    unsigned selectionCount{};
};
