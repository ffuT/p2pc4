#include "game.h"
#include "ftxui/dom/elements.hpp"
#include "net.h"
#include <chrono>
#include <cstdint>
#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/color.hpp>
#include <stop_token>
#include <thread>

Game::Game() {
}

Game::~Game(){
}

void Game::start() {
    using namespace ftxui;
    m_screen.emplace(ftxui::ScreenInteractive::TerminalOutput());
 
    InputOption single;
    single.multiline = false;

    Component ipIn  = Input(&m_ip, "127.0.0.1", single);
    Component tcpIn  = Input(&m_tcp, "50051", single);
    Component udpIn  = Input(&m_udp, "50052", single);
    Component roomIn = Input(&m_room, "room", single);
    Component connect = Button("connect", [&] { 
        m_page = 1;
        // TODO give another st to stop on Esc
        std::jthread t([&](std::stop_token st) { 
            bool turn;
            bool ok = m_net.init(Net::InitInfo{m_ip, m_tcp, m_udp, m_room, version}, turn);
            if(ok){
                m_page = 2;
                // set colors and turn
                m_color = turn ? Color::Red : Color::Yellow;
                m_oppColor = !turn ? Color::Red : Color::Yellow;
                m_state = turn ? state::turnNoMove : state::notTurn;
            }
            else
                m_page = 0;
            m_screen->PostEvent(Event::Custom);
        });
        t.detach();
    });

    auto menuLayout = Container::Horizontal({ipIn, tcpIn, udpIn, roomIn, connect});
    Component menu = Renderer(menuLayout, [&]{
        return vbox({
            text("connect4") | bold | center,
            separator(),
            hbox(text("ip: "), ipIn->Render()),
            hbox(text("tcp: "), tcpIn->Render()),
            hbox(text("udp: "), udpIn->Render()),
            hbox(text("room: "), roomIn->Render()),
            connect->Render() | center
        }) | border | size(WIDTH, EQUAL, 40) | center;
    });
    Component loading = Renderer([&] {
        return vbox({
            text("loading")
        })| border | size(WIDTH, EQUAL, 40) | center;
    });
    auto gameView = Renderer([&](bool){
        Elements rowsE;
        Elements select;
        for (int s = 0; s < cols; s++) {
            if((m_state != state::notTurn) && s == m_currentPos) { // own pos
                select.push_back(text(" @ ") | dim | center | borderEmpty | color(m_color));
                continue;
            } else if (m_state == state::notTurn && s == m_oppPos) { // op pos
                select.push_back(text(" @ ") | dim | center | borderEmpty | color(m_oppColor));
                continue;
            }
            select.push_back(text("   ") | dim | center | borderEmpty);
        }
        rowsE.push_back(hbox(select) | center);

        for(int r = 0; r < rows; r++){
            Elements line;
            for(int c = 0; c < cols; c++){
                uint8_t spot = r * cols + c;
                if(m_board[spot] == 0)
                    line.push_back(text(" . ") | dim | border | center);
                else if (m_board[spot] == 1)
                    line.push_back(text(" @ ") | color(m_color) | border | center );
                else
                    line.push_back(text(" @ ") | color(m_oppColor) |  border | center );
            } 
            rowsE.push_back(hbox(line) | center);
        }

        rowsE.push_back(separator());
        return vbox(rowsE)| border | size(WIDTH, EQUAL, 40) | center;
    });
    auto game = CatchEvent(gameView, [&](Event e){
        if (m_state == state::turnNoMove){
            if(e == Event::ArrowLeft) {
                m_currentPos = (uint8_t)(m_currentPos-1) < cols ? m_currentPos-1 : cols-1;
                return true;
            }
            if(e == Event::ArrowRight) {
                m_currentPos = m_currentPos+1 < cols ? m_currentPos+1 : 0;
                return true;
            }
        }
        if(e == Event::Escape) {
            m_page = 0;
            // TODO announce leave + confirm before leaving
            return true;
        }
        if(e == Event::Return) {
            if(m_state == state::turnNoMove){
                m_sendMove = true;
                m_state = state::turnMadeMove;
                makeMove(true, m_currentPos);
            }
            return true;
        }
        return false;
    });

    auto root = Container::Tab({menu, loading, game}, &m_page);
    m_loop.emplace(&m_screen.value(), root);
    bool send = true;
    while (!m_loop->HasQuitted()) {
        if(m_page == 2) {
            switch (m_state) {
                case state::turnMadeMove:
                    if(m_sendMove)
                        m_net.sendMove(m_currentPos);
                    else
                        m_net.sendHover(m_currentPos);
                break;
                default: // send hovers as keep alive
                    m_net.sendHover(m_currentPos);
                break;
            }
            Poll ret = m_net.poll(m_oppPos);
            switch (ret) {
                case Poll::oppMoved:
                    m_state = state::turnNoMove;
                    makeMove(false, m_oppPos);
                break;
                case Poll::ack:
                    m_state = state::notTurn;
                    m_sendMove = false;
                case Poll::empty:
                break;
                case Poll::err:
                case Poll::NONE:
                    m_page = 0; // back to lobby on error for now
                break;
            }
        }       
        send = !send;
        m_loop->RunOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

bool Game::checkWin(){
    return false;
}

void Game::makeMove(bool myMove, uint8_t move) {
    uint8_t spot = cols * (rows-1) + move;
    while (m_board[spot] != 0) {
        spot -= cols;
    }
    m_board[spot] = myMove ? 1 : 2;
}