#include "notifier.h"
#include "servicetree.h"
#include "ui.h"
#include <fmt/format.h>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>

#include <argparse/argparse.hpp>

using namespace wibens::tuilight;

int main(int argc, char *argv[])
{
    argparse::ArgumentParser argParse("targetctl");
    argParse.add_description("And interactive systemd controller.\nhttps://github.com/ibensw/targetctl");
    argParse.add_argument("target").help("The systemd target to observe").default_value("-.slice");
    argParse.add_argument("-t", "--tree").help("Enable recursive scanning").flag();

    auto &typeGroup = argParse.add_mutually_exclusive_group();
    RelationType type{RelationType::RequiredBy};
    typeGroup.add_argument("-r", "--required-by").flag().action([&](const auto &) { type = RelationType::RequiredBy; });
    typeGroup.add_argument("-R", "--requires").flag().action([&](const auto &) { type = RelationType::Requires; });
    typeGroup.add_argument("-w", "--wanted-by").flag().action([&](const auto &) { type = RelationType::WantedBy; });
    typeGroup.add_argument("-W", "--wants").flag().action([&](const auto &) { type = RelationType::Wants; });
    typeGroup.add_argument("-c", "--consists-of").flag().action([&](const auto &) { type = RelationType::ConsistsOf; });
    typeGroup.add_argument("-C", "--part-of").flag().action([&](const auto &) { type = RelationType::PartOf; });

    try {
        argParse.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << argParse;
        return 1;
    }
    auto target = argParse.get<std::string>("target");
    if (target.find('.') == target.npos) {
        target += ".target";
    }
    ServiceTree services(target, type, argParse.get<bool>("-t") ? 100 : 1);
    services.update();

    TargetCtlUI ui(services);

    Terminal terminal;
    std::atomic<bool> exited = false;
    Notifier stopSignal;

    std::thread updater([&]() {
        while (!exited) {
            terminal.post([&](Terminal &, BaseElement) { services.update(); });
            stopSignal.wait_for(std::chrono::milliseconds{1000});
        }
    });

    auto exitHandler = [&](KeyEvent event, BaseElement e) {
        if (e->handleEvent(event)) {
            return true;
        }
        if (event == KeyEvent::ESCAPE || event == ansi::CharEvent('q')) {
            terminal.stop();
            return true;
        }
        return false;
    };

    try {
        while (!exited) {
            try {
                terminal.runInteractive(KeyHander(exitHandler)(ui));
                exited = true;
            } catch (const std::runtime_error &e) {
                ui.setStatus(e.what());
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Uncaught exception: " << e.what() << std::endl;
    }

    stopSignal.notify();
    terminal.clear();
    updater.join();

    return 0;
}
