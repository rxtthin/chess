#include "board.hpp"
#include "rules.hpp"
#include "piece.hpp"

#include <iostream>
#include <algorithm>
#include <sstream>

enum FenRecord {
	PiecePlacement = 0,
	ActiveColor = 1,
	CastlingAvailability = 2,
	EnPassantTargetSquare = 3,
	HalfMoveClock = 4,
	FullMoveNumber = 5
};

Board::Board() {
	reset();
}

Board::Board(const Board &board) : m_pieces(board.m_pieces), m_state(board.m_state) {}

void Board::reset() {
	m_pieces.fill(0);
													
	m_checkResult = (CheckResult){
                .isCheck = false,
	};

        m_status = BoardStatus::Playing;
	m_lastMove = std::nullopt;
	applyFen(DEFAULT_FEN);
}

bool Board::applyMove(const Move &move, const bool isFake, const bool updateCheckResult) {
        if(!isFake) {
                if(std::find(m_legalMoves.begin(), m_legalMoves.end(), move) == m_legalMoves.end()) {
                        std::cerr << "This move (" << move << ") is not legal" << std::endl;
                        std::cerr << "Legal moves are: ";
                        for(const Move &move : m_legalMoves) {
                                std::cerr << move << std::endl;
                        }

                        return false;
                }
        }

	if(move.isKingSideCastling) {
		performCastling(move.piece.getColor(), false);
	} else if(move.isQueenSideCastling) {
		performCastling(move.piece.getColor(), true);
	} else {
		m_pieces[move.toIdx] = move.piece;
		m_pieces[move.fromIdx] = 0;
	}

	handlePawnPromotion(move);
	handleEnPassant(move);

	if(!isFake) {
		handleMove(move);

		m_state.turnColor = m_state.turnColor == White ? Black : White;
		m_lastMove = move;
	}

	if(updateCheckResult) {
		m_checkResult = calculateCheck(m_state.turnColor);
	}

	if(!isFake) {
		updateLegalMoves();
                updateStatus();
	}

        return true;
}

void Board::performCastling(PieceColor color, bool isQueenSide) {
        uint8_t kingStartIdx, rookStartIdx, kingEndIdx, rookEndIdx;
	if(color == Black) {
		if(isQueenSide) {
			kingStartIdx = 4;
			kingEndIdx = 2;
			rookStartIdx = 0;
			rookEndIdx = 3;
		} else {
			kingStartIdx = 4;
			kingEndIdx = 6;
			rookStartIdx = 7;
			rookEndIdx = 5;
		}
	} else {
		if(isQueenSide) {
			kingStartIdx = 60;
			kingEndIdx = 58;
			rookStartIdx = 56;
			rookEndIdx = 59;
		} else {
			kingStartIdx = 60;
			kingEndIdx = 62;
			rookStartIdx = 63;
			rookEndIdx = 61;
		}
	}

	m_pieces[kingStartIdx] = 0;
	m_pieces[rookStartIdx] = 0;

	m_pieces[kingEndIdx] = Piece(color, King);
	m_pieces[rookEndIdx] = Piece(color, Rook);
}

void Board::handleMove(const Move &move) {
	if(move.piece.isColor(Black)) {
		++m_state.fullMoves;
	}

	if(move.piece.isType(Pawn) || move.isCapture) {
		m_state.halfMoveClock = 0;
	} else {
		++m_state.halfMoveClock;
		if(m_state.halfMoveClock == 50) {
			std::cout << "50 half moves without capture or pawn move. Draw!" << std::endl;
			m_status = BoardStatus::Draw;
		}
	}

	Player &player = getPlayer(move.piece.getColor());

	if(move.piece.isType(King)) {
		player.canCastleQueenSide = false;
		player.canCastleKingSide = false;
	} else if(move.piece.isType(Rook)) {
		if(move.fromIdx == 0 || move.fromIdx == 56) {
			player.canCastleQueenSide = false;
		} else {
			player.canCastleKingSide = false;
		}
	}

	std::cout << "Half move clock: " << m_state.halfMoveClock << std::endl;
	std::cout << "Full moves: " << m_state.fullMoves << std::endl;
}

void Board::handlePawnPromotion(const Move &move) {
	if(move.isPawnPromotion)
		m_pieces[move.toIdx] = Piece(move.piece.getColor(), Queen);
}

void Board::handleEnPassant(const Move &move) {
	if(move.piece.getType() == Pawn) {
		// If moved 2 files
		if(move.toIdx == m_state.enPassantTarget) {
			if(move.piece.getColor() == White)
				m_pieces[move.toIdx + 8] = 0;
			else
				m_pieces[move.toIdx - 8] = 0;
		}

		if(std::abs(move.fromIdx - move.toIdx) == 16) {
			if(move.piece.getColor() == White)
				m_state.enPassantTarget = move.toIdx + 8;
			else
				m_state.enPassantTarget = move.toIdx - 8;

			return;
		}
	} 
	m_state.enPassantTarget = std::nullopt;
}

// based on: https://www.chess.com/terms/fen-chess
bool Board::applyFen(const std::string &fen) {
	m_pieces.fill(0);
	m_state = {};

	std::uint8_t file = 0, rank = 0;
	std::istringstream stream(fen);
	std::string record;

	int recordIdx = 0;

	while(stream >> record) {
		switch(recordIdx++) {
			case FenRecord::PiecePlacement:
				for(const char c : record) {
					if(c == '/') {
						file = 0;
						rank++;
					} else {
						if(isdigit(c)) {
							file += c - '0';
						} else {
							const PieceColor color = isupper(c) ? White : Black;

							PieceType type = None;
							switch(tolower(c)) {
							case 'q':
								type = Queen;
								break;
							case 'k':
								type = King;
								break;
							case 'n':
								type = Knight;
								break;
							case 'b':
								type = Bishop;
								break;
							case 'r':
								type = Rook;
								break;
							case 'p':
								type = Pawn;
								break;
							default:
								std::cerr << "\e[1;31mInvalid piece type: " << (int)c << "!\e[0m\n";
																return false;
							}

							m_pieces[rank * 8 + file] = Piece(color, type);
							file++;
						}
					}
				}
				break;

			case FenRecord::ActiveColor:
                                switch(record[0]) {
                                                case 'w': m_state.turnColor = White; break;
                                                case 'b': m_state.turnColor = Black; break;
                                                default: std::cerr << "Invalid active color in fen: " << record[0] << std::endl; return false;
                                }
				break;

			case FenRecord::CastlingAvailability:
				if(record.empty() || record[0] == '-') {
					const auto disableCastling = [&](Player &player) {
						player.canCastleKingSide = false;
						player.canCastleQueenSide = false;
					};

					disableCastling(m_state.whitePlayer);
					disableCastling(m_state.blackPlayer);

					break;
				}

				for(const char c : record) {
					switch(c) {
						case 'K':
							m_state.whitePlayer.canCastleKingSide = true;
							break;
						case 'Q':
							m_state.whitePlayer.canCastleQueenSide = true;
							break;
						case 'k':
							m_state.blackPlayer.canCastleKingSide = true;
							break;
						case 'q':
							m_state.blackPlayer.canCastleQueenSide = true;
							break;
                                                default: return false;
					}
				}
				break;

			case FenRecord::EnPassantTargetSquare:
				if(record.empty() || record[0] == '-') {
					m_state.enPassantTarget = std::nullopt;
					break;
				}

				m_state.enPassantTarget = getSquareIdx(record);
				break;

			case FenRecord::HalfMoveClock:
				m_state.halfMoveClock = std::stoi(record);
				break;

			case FenRecord::FullMoveNumber:
				m_state.fullMoves = std::stoi(record);
				break;
		}
	}

	updateLegalMoves();
        return true;
}

void Board::convertToFen(std::string &fen) const {
	std::ostringstream stream;

	for(std::uint8_t rank = 0; rank < 8; ++rank) {
		uint8_t emptyCount = 0;
		for(std::uint8_t file = 0; file < 8; ++file) {
			const std::uint8_t idx = rank * 8 + file;
			const Piece &piece = getPiece(idx);

			if(piece.isNull()) {
				++emptyCount;
				continue;
			}

			if(emptyCount != 0) {
				stream << emptyCount;
				emptyCount = 0;
			}

			stream << piece.toChar();
		}

		if(emptyCount != 0) {
			stream << emptyCount;
		}

		if(rank != 7) {
			stream << '/';
		}
	}

	stream << ' ';

	bool anyCastle = false;
	if(m_state.whitePlayer.canCastleKingSide) { 
		anyCastle = true;
		stream << 'K';
	}
	if(m_state.whitePlayer.canCastleQueenSide) {
		anyCastle = true;
		stream << 'Q';
	}
	if(m_state.blackPlayer.canCastleKingSide) {
		anyCastle = true;
		stream << 'k';
	}
	if(m_state.blackPlayer.canCastleQueenSide) {
		anyCastle = true;
		stream << 'q';
	}

	if(!anyCastle) {
		stream << '-';
	}

	stream << ' ';

	if(m_state.enPassantTarget.has_value()) {
		stream << Move::positionToString(m_state.enPassantTarget.value());
	} else {
		stream << '-';
	}

	stream << ' ' << m_state.halfMoveClock << ' ' << m_state.fullMoves;

	fen = stream.str();
}

CheckResult Board::calculateCheck(const PieceColor color) {
	std::vector<Move> legalMoves;

	// Get all legal moves of the opponent
	for(std::uint8_t i = 0; i < 64; ++i) {
		const Piece &piece = getPiece(i);

		if(piece.isNull() || piece.isColor(color))
			continue;

		Rules::addLegalMoves(legalMoves, *this, i, true);
	}

	// Check if any of the opponent's moves puts the king in check
	for(const Move &move : legalMoves) {
		if(move.targetPiece.isType(King)) {
			return(CheckResult){.isCheck = true, .kingIdx = move.toIdx, .checkingPieceIdx = move.fromIdx};
		}
	}

	return(CheckResult){.isCheck = false};
}

void Board::updateLegalMoves() {
	m_legalMoves.clear();

	for(std::uint8_t i = 0; i < 64; ++i) {
		const Piece &piece = getPiece(i);
		if(piece.isNull() || piece.getColor() != m_state.turnColor)
			continue;

		Rules::addLegalMoves(m_legalMoves, *this, i);
	}
}

void Board::updateStatus() {
	if(m_checkResult.isCheck) {
		if(m_legalMoves.empty()) {
                        m_status = m_state.turnColor == White ? BoardStatus::BlackWin : BoardStatus::WhiteWin; // Checkmate
		}
	} else {
		if(m_legalMoves.empty()) {
                        m_status = BoardStatus::Draw; // Stalemate
		}
	}
}