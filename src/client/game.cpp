#include "game.h"
#include "ftxui/dom/elements.hpp"
#include <chrono>
#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <thread>

constexpr uint32_t version = (1 << 16) | (0 << 8) | 0; // 1.0.0
constexpr int rows = 6, cols = 7;

Game::Game() {

}

Game::~Game(){

}

void Game::start() {
    using namespace ftxui;
    m_screen.emplace(ftxui::ScreenInteractive::TerminalOutput());
 
    InputOption single;
    single.multiline = false;

    Component ipIn  = Input(&m_ip, "ip", single);
    Component tcpIn  = Input(&m_tcp, "tcp port", single);
    Component udpIn  = Input(&m_udp, "upd port", single);
    Component connect = Button("connect", [&] { 
        m_page = 1;
        if(!m_net.init(m_ip, m_tcp, m_udp, version))
            printf("failed to init Net!\n");
        m_page = 2;
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
    auto gameView = Renderer([&](bool){
        Elements rowsE;
        Elements select;
        for (int s = 0; s < cols; s++){
            if (s == m_currentPos){ // own pos
                select.push_back(text(" # ") | dim | center | borderEmpty);
                continue;;
            }
            if (s == m_oppPos){ // op pos
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
            m_currentPos = m_currentPos-1 < rows ? m_currentPos-1 : rows;
            return true;
        }
        if(e == Event::ArrowRight) {
            m_currentPos = m_currentPos+1 <= rows ? m_currentPos+1 : 0;
            return true;
        }
        if(e == Event::Escape) {
            m_page = 0;
            // TODO announce leave + confirm before leaving
            return true;
        }
        return false;
    });

    auto root = Container::Tab({menu, loading, game}, &m_page);
    m_loop.emplace(&m_screen.value(), root);
    while (!m_loop->HasQuitted()) {
        m_loop->RunOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        if(m_page == 2) {
            m_net.sendHover(m_currentPos);
            if(!m_net.poll(m_oppPos)){
                m_page = 0;
            }
        }
    }
}
