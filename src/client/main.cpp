#include "ftxui/dom/elements.hpp"
#include "net.h"
#include <chrono>
#include <cstdlib>
#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <thread>

constexpr uint32_t version = (1 << 16) | (0 << 8) | 0; // 1.0.0

constexpr int rows = 6, cols = 7;

int main(int argc, char* argv[]) {
    int page = 0;
    Net net = Net();
    using namespace ftxui;
    auto screen = ScreenInteractive::TerminalOutput();
 
    std::string ip = "localhost";
    std::string tcp = "50051";
    std::string udp = "50052";
 
    InputOption single;
    single.multiline = false;

    Component ipIn  = Input(&ip, "ip", single);
    Component tcpIn  = Input(&tcp, "tcp port", single);
    Component udpIn  = Input(&udp, "upd port", single);
    Component connect = Button("connect", [&] { 
        page = 1;
        if(!net.init(ip, tcp, udp, version))
            exit(EXIT_FAILURE);
        page = 2;
    });

    auto menuLayout = Container::Horizontal({ipIn, tcpIn, udpIn, connect});
    Component menu = Renderer(menuLayout, [&]{
        return vbox({
            text("connect4") | bold | center,
            separator(),
            hbox(text("ip: "), ipIn->Render()),
            hbox(text("tcp: "), tcpIn->Render()),
            hbox(text("udp: "), udpIn->Render()),
            connect->Render() | center
        }) | border | size(WIDTH, EQUAL, 40) | center;
    });

    // loading doesnt show while single threaded
    Component loading = Renderer({Container::Horizontal({})},[&]{
        return vbox({
            text("loading")
        })| border | size(WIDTH, EQUAL, 40) | center;
    });
    uint8_t current = 0;
    uint8_t opp = 0;
    auto gameView = Renderer([&](bool){
        Elements rowsE;

        Elements select;
        for (int s = 0; s < cols; s++){
            if (s == current){
                select.push_back(text(" # ") | dim | center | borderEmpty);
                continue;;
            }
            if (s == opp){
                select.push_back(text(" # ") | dim | center | borderEmpty | color(Color::Red));
                continue;
            }
            select.push_back(text("   ") | dim | center | borderEmpty);
        }
        rowsE.push_back(hbox(select) | center);

        for(int r = 0; r < rows; r++){
            Elements line;
            for(int c = 0; c < cols; c++){
                line.push_back(text(" . ") | dim | border | center);
            } 
            rowsE.push_back(hbox(line) | center);
        }

        rowsE.push_back(separator());
        return vbox(rowsE)| border | size(WIDTH, EQUAL, 40) | center;
    });

    auto game = CatchEvent(gameView, [&](Event e){
        if(e == Event::ArrowLeft) {
            current = current-1 < rows ? current-1 : rows;
            return true;
        }
        if(e == Event::ArrowRight) {
            current = current+1 <= rows ? current+1 : 0;
            return true;
        }
        return false;
    });

    auto root = Container::Tab({menu, loading, game}, &page);

    Loop loop(&screen, root);

    while (!loop.HasQuitted()) {
        loop.RunOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        net.sendHover(current);
        net.poll(opp);
    }

    return EXIT_SUCCESS;
}