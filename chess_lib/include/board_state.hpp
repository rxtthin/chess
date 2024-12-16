#pragma once

#include "piece.hpp"
#include "player.hpp"

#include <optional>

struct BoardState {
	PieceColor turnColor = PieceColor::White;
	std::uint16_t fullMoves = 0;
	std::uint16_t halfMoveClock = 0;
	std::optional<uint8_t> enPassantTarget = std::nullopt;
	Player whitePlayer;
	Player blackPlayer;
};