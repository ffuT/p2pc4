#pragma once

#include "net.h"
#include <ftxui/component/app.hpp>
#include <ftxui/component/loop.hpp>

class Game {
public:
    Game();
    ~Game();

    void start();

private:
    bool checkWin();
    void makeMove();
    bool m_myTurn;

    Net m_net;
    std::string m_ip, m_tcp, m_udp, m_room;
    int m_page = 0;
    uint8_t m_currentPos = 0, m_oppPos = 0;
    std::optional<ftxui::App> m_screen;
    std::optional<ftxui::Loop> m_loop;
};