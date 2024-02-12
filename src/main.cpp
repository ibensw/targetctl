#include "notifier.h"
#include "servicetree.h"
#include "ui.h"
#include <fmt/format.h>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unistd.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <argparse/argparse.hpp>

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
        target += ".service";
    }
    ServiceTree services(target, type, argParse.get<bool>("-t") ? 100 : 1);
    services.update();

    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    std::atomic<bool> exited = false;
    Notifier stopSignal;

    std::thread updater([&]() {
        while (!exited) {
            screen.Post([&]() { services.update(); });
            screen.RequestAnimationFrame();
            stopSignal.wait_for(std::chrono::milliseconds{1000});
        }
    });

    TargetCtlUI ui{services, screen.ExitLoopClosure()};

    try {
        while (!exited) {
            try {
                screen.Loop(ui);
                exited = true;
            } catch (const std::runtime_error &e) {
                ui.setStatus(e.what());
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Uncaught exception: " << e.what() << std::endl;
    }
    stopSignal.notify();
    updater.join();

    return 0;
}
