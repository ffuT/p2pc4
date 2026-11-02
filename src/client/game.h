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
    Net m_net;
    std::string m_ip = "localhost", m_tcp = "50051", m_udp = "50052";
    int m_page = 0;
    uint8_t m_currentPos = 0, m_oppPos = 0;
    std::optional<ftxui::App> m_screen;
    std::optional<ftxui::Loop> m_loop;
};