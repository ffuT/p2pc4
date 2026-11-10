#pragma once

#include "net.h"
#include <cstdint>
#include <ftxui/component/app.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/screen/color.hpp>

constexpr uint32_t version = (1 << 16) | (0 << 8) | 0; // 1.0.0
constexpr int rows = 6, cols = 7;

enum class state{
    turnNoMove,
    turnMadeMove,
    notTurn,
};


class Game {
public:
    Game();
    ~Game();

    void start();

private:
    bool checkWin();
    void makeMove(bool myMove, uint8_t move);
    bool m_sendMove = false;
    state m_state = state::notTurn;
    uint8_t m_board[rows*cols]{};
    ftxui::Color m_color;
    ftxui::Color m_oppColor;

    Net m_net;
    std::string m_ip, m_tcp, m_udp, m_room;
    int m_page = 0;
    uint8_t m_currentPos = 0, m_oppPos = 0;
    std::optional<ftxui::App> m_screen;
    std::optional<ftxui::Loop> m_loop;
};