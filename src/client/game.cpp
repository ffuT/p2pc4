#include "game.h"
#include "ftxui/dom/elements.hpp"
#include "net.h"
#include <chrono>
#include <cstdint>
#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/color.hpp>
#include <stop_token>
#include <thread>

Game::Game(){}
Game::~Game(){}

void Game::start() {
    using namespace ftxui;
    
    m_screen.emplace(ftxui::ScreenInteractive::TerminalOutput());
    auto root = Container::Tab({buildMenu(), buildLoad(), buildGame()}, &m_page);
    auto app = CatchEvent(root, [this](Event e){return HandleEvent(e);});
    m_loop.emplace(&m_screen.value(), app);

    bool shouldSend = true;
    auto now = std::chrono::steady_clock::now();
    auto last = now;
    while (!m_loop->HasQuitted()) {
        last = now;
        now = std::chrono::steady_clock::now();
        auto delta = now - last;
        if(m_page == 2) { // 2 = live game
            if(shouldSend) // send every other frame only 
                send();
            bool chw = poll();
            if(chw) {
                int8_t win = checkWin();
                if(win != 0){
                    m_page = win == 1 ? 0 : 0; // win- : loss-screen
                    resetGame();
                }
            }
        }
        shouldSend = !shouldSend;
        m_loop->RunOnce();
        std::this_thread::sleep_for((std::chrono::microseconds(16000)-delta));
    }
}

void Game::send() {
    switch (m_state) {
    case state::turnMadeMove:
        if(m_sendMove)
            m_net.sendMove(m_currentPos, m_moveCount);
        else
            m_net.sendHover(m_currentPos);
    break;
    default: // send hovers as keep alive
        m_net.sendHover(m_currentPos);
    break;
    }
}

bool Game::poll(){
    bool quedPoll = true;
    bool checkWinner = false;
    while (quedPoll) {
        uint8_t cOppPos = m_oppPos;
        Poll ret = m_net.poll(m_oppPos, m_moveCount);
        if(ret == Poll::empty)
            break;
        m_screen->PostEvent(ftxui::Event::Custom);
        switch (ret) {
            case Poll::oppMoved:
                checkWinner = true;
                m_state = state::turnNoMove;
                makeMove(false, m_oppPos);
            break;
            case Poll::ack:
                checkWinner = true;
                m_state = state::notTurn;
                m_sendMove = false;
            case Poll::update:
            case Poll::empty:
            break;
            case Poll::err:
            case Poll::NONE:
                quedPoll = false;
                m_page = 0; // back to lobby on error for now
            break;
        }
    }
    return checkWinner;
}

bool Game::HandleEvent(ftxui::Event e){
    using namespace ftxui;
    switch (m_page) {
        case 0: // main menu
            if(e == Event::Escape){
                m_screen->Exit(); // close game
                return false;
            }
        case 1: // loading menu
            if(e == Event::Escape){
                m_netInitThread.request_stop(); // back to menu
                return false;
            }
        case 2: // ingame
            if (m_state == state::turnNoMove){
                if(e == Event::ArrowLeft) {
                    m_currentPos = (uint8_t)(m_currentPos-1) < cols ? m_currentPos-1 : cols-1;
                    return true;
                }
                if(e == Event::ArrowRight) {
                    m_currentPos = m_currentPos+1 < cols ? m_currentPos+1 : 0;
                    return true;
                }
                if(e == Event::Return){
                    m_sendMove = true;
                    m_state = state::turnMadeMove;
                    makeMove(true, m_currentPos);
                    return true;
                }
            }
            if(e == Event::Escape){
                m_page = 0;
                // TODO announce leave or sumthn   
                return false;   
            }
    }
    return false;
}

void Game::resetGame() {
    for(int i = 0; i < rows*cols; i ++){
        m_board[i] = player::NONE;
    }

    m_moveCount = 0;
    m_sendMove = 0;
    m_state = state::notTurn;
    m_currentPos = cols/2;
    m_oppPos = cols/2;
}

void Game::makeMove(bool myMove, uint8_t move) {
    m_moveCount++;
    int8_t spot = cols * (rows-1) + move;
    while (m_board[spot] != player::NONE) {
        spot -= cols;
    }
    if(spot < 0){ // illegal moves are never sent
        m_moveCount--;
        m_state = state::turnNoMove;
        m_sendMove = false;
        return;
    }
    m_board[spot] = myMove ? player::me : player::opp;
}

uint8_t Game::checkWin() { // return 0 = none, 1 = client, 2 = opponent
    for(int8_t i = 1; i <= 2; i++) { // run for player 1 and 2
        player p = static_cast<player>(i);
        int8_t count = 0;
        for(int row = 0; row < rows; row++){ // horizontal
            count = 0;
            for(int col = 0; col < cols; col++){
                if(m_board[row*cols + col] == p)
                    count++;
                else
                    count = 0;
                if (count == 4)
                    return i;
            }
        }
        for(int col = 0; col < cols; col++){ // vertical
            count = 0;
            for(int row = rows-1; row >= 0; row--){
                if(m_board[row*cols + col] == p)
                    count++;
                else
                    count = 0;
                if (count == 4)
                    return i;
            }
        }
        for(int row = rows-1; row > 3; row--){ // diagonal asc
            for(int col = 0; col < cols-3; col++){
                if(m_board[row*cols + col] == p && // ugh
                    m_board[(row-1)*cols + (col+1)] == p &&
                    m_board[(row-2)*cols + (col+2)] == p &&
                    m_board[(row-3)*cols + (col+3)] == p) 
                    return i;
            }
        }
        for(int row = 0; row < rows-3; row++){ // diagonal desc
            for(int col = 0; col < cols-3; col++){
                if(m_board[row*cols + col] == p && // ugh
                    m_board[(row+1)*cols + (col+1)] == p &&
                    m_board[(row+2)*cols + (col+2)] == p &&
                    m_board[(row+3)*cols + (col+3)] == p) 
                    return i;  
            }
        }
    }
    return 0; 
}

void Game::tryConnect(){
    m_page = 1;
    m_netInitThread = std::jthread([this](std::stop_token st) {
        bool turn;
        bool ok = false;
        try {
            ok = m_net.init(Net::InitInfo{m_ip, m_tcp, m_udp, m_room, version}, turn, st);
        } catch (const std::exception& e) {
            // print to log or carry back to menu
        }
        if(ok){
            m_page = 2;
            // set colors and turn
            m_color = turn ? ftxui::Color::Red : ftxui::Color::Yellow;
            m_oppColor = !turn ? ftxui::Color::Red : ftxui::Color::Yellow;
            m_state = turn ? state::turnNoMove : state::notTurn;
        }
        else
            m_page = 0;
        m_screen->PostEvent(ftxui::Event::Custom);
        
    });
    m_netInitThread.detach();
}

ftxui::Component Game::buildMenu(){
    using namespace ftxui;
    InputOption single;
    single.multiline = false;

    Component ipIn = Input(&m_ip, "127.0.0.1", single);
    Component tcpIn = Input(&m_tcp, "50051", single);
    Component udpIn = Input(&m_udp, "50052", single);
    Component roomIn = Input(&m_room, "room", single);
    Component connect = Button("connect", [this] {tryConnect();});

    auto menuLayout = Container::Horizontal({ipIn, tcpIn, udpIn, roomIn, connect});
    return Renderer(menuLayout, [ipIn, tcpIn, udpIn, roomIn, connect]{
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
}

ftxui::Component Game::buildLoad(){
    using namespace ftxui;
    return Renderer([&] {
        return vbox({
            text("loading")
        })| border | size(WIDTH, EQUAL, 40) | center;
    });
}

ftxui::Component Game::buildGame(){
    using namespace ftxui;
    return Renderer([&](bool){
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
                if(m_board[spot] == player::NONE)
                    line.push_back(text(" . ") | dim | border | center);
                else if (m_board[spot] == player::me)
                    line.push_back(text(" @ ") | color(m_color) | border | center );
                else
                    line.push_back(text(" @ ") | color(m_oppColor) |  border | center );
            }
            rowsE.push_back(hbox(line) | center);
        }

        rowsE.push_back(separator());
        return vbox(rowsE)| border | size(WIDTH, EQUAL, 40) | center;
    });
}