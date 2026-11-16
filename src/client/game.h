#pragma once

#include "net.h"
#include <cstdint>
#include <ftxui/component/app.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/screen/color.hpp>
#include <thread>

constexpr uint32_t version = (1 << 16) | (0 << 8) | 0; // 1.0.0
constexpr int8_t rows = 6, cols = 7;

enum class state : int8_t{
    turnNoMove,
    turnMadeMove,
    notTurn,
};

enum class player : int8_t{
    me = 1,
    opp = 2,
    NONE = 0,
};

class Game {
public:
    Game();
    ~Game();

    void start();

private:
    uint8_t checkWin();
    void resetGame();
    void makeMove(bool myMove, uint8_t move);
    
    // net
    bool poll();
    void send();
    void tryConnect();

    // ui
    bool HandleEvent(ftxui::Event e);
    ftxui::Component buildMenu();
    ftxui::Component buildLoad();
    ftxui::Component buildGame();
    // ftxui::Component buildWin();
    // ftxui::Component buildLose();
    // ftxui::Component buildDraw();

    // game 
    state m_state = state::notTurn;
    player m_board[rows*cols]{player::NONE};
    ftxui::Color m_color;
    ftxui::Color m_oppColor;
    uint8_t m_moveCount = 0;
    bool m_sendMove = false;

    // core
    Net m_net;
    std::jthread m_netInitThread;
    std::string m_ip, m_tcp, m_udp, m_room;
    int m_page = 0;
    uint8_t m_currentPos = cols/2, m_oppPos = cols/2;
    std::optional<ftxui::App> m_screen;
    std::optional<ftxui::Loop> m_loop;
};