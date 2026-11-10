#include "pch.h"
#include "DiceBattleAI.h"
#include "DiceBattleGame.h"
#include <algorithm>
#include <climits>
#include <cmath>

namespace
{
    // 評価値の最大・最小安全バウンダリ
    constexpr int SCORE_INF = 1000000;

    // AI評価重み定数定義
    constexpr int SCORE_WIN = 100000;            // 勝利決定時の評価値
    constexpr int SCORE_LOSE = -100000;          // 敗北決定時の評価値
    constexpr int KING_BASE_VALUE = 8000;        // 王の基本評価価値
    constexpr int PAWN_BASE_VALUE = 100;         // 兵の基本評価価値
    constexpr int PAWN_FACE_MULTIPLIER = 30;     // 出目1あたりの評価倍率
    constexpr int ADVANCE_DISTANCE_WEIGHT = 5;   // 盤面進行度（Y座標）の評価重み
    constexpr int KING_ATTACK_BONUS_P2 = 1500;   // P1の王を狙える位置のボーナス
    constexpr int KING_DANGER_PENALTY_P2 = 4000; // P2の王が危険な位置のペナルティ
    constexpr int SHIELD_EVAL_SCORE = 200;       // 守護効果の評価点

    constexpr int MOVE_SCORE_ATTACK_SHIELD_KING = -50000; // 守護状態の王への無効攻撃ペナルティ
    constexpr int MOVE_SCORE_ATTACK_KING = 5000;          // 王攻撃成功時のボーナス
    constexpr int MOVE_SCORE_ATTACK_FACE_ONE = -10000;    // 出目1のコマを攻撃する際のリスク
    constexpr int MOVE_SCORE_ATTACK_PAWN = 800;           // 通常コマ攻撃成功ボーナス
    constexpr int MOVE_SCORE_ATTACK_SHIELD = -3000;       // 守護状態コマ攻撃ペナルティ
    constexpr int MOVE_SCORE_KING_ACE_KILL = 25000;       // 出目1で王を狙う最強手ボーナス
    constexpr int MOVE_SCORE_KING_ATTACK_FAIL = -2000;    // 王攻撃失敗のリスクペナルティ
    constexpr int MOVE_SCORE_ADVANCE_STEP = 30;           // 前進移動の標準加算スコア

    constexpr int ROLL_MAP_COLS = 4; // サイコロ回転テーブルの方向カラム数
    constexpr int ROLL_MAP_ROWS = 7; // サイコロ回転テーブルの面数

    // サイコロの転がり方向から移動後の上面の出目を予測計算する関数
    int PredictFaceFromCurrent(int currentFace, int dx, int dy)
    {
        if (currentFace < 1 || currentFace > 6)
            return currentFace;

        static const int rollMap[ROLL_MAP_ROWS][ROLL_MAP_COLS] = {
            {0, 0, 0, 0}, {3, 4, 2, 5}, // 1: [右, 左, 上, 下]
            {3, 4, 6, 1},               // 2
            {6, 1, 2, 5},               // 3
            {1, 6, 2, 5},               // 4
            {5, 4, 1, 6},               // 5
            {4, 3, 2, 5}                // 6
        };

        int dirIndex = -1;
        if (dx == 1)
            dirIndex = 0;
        else if (dx == -1)
            dirIndex = 1;
        else if (dy == 1)
            dirIndex = 2;
        else if (dy == -1)
            dirIndex = 3;

        if (dirIndex == -1)
            return currentFace;
        return rollMap[currentFace][dirIndex];
    }
}

// AI思考および行動のメイン更新ループ
void DiceBattleAI::Update(DiceBattleGame& game, float dt)
{
    if (game.IsGameOver() || game.GetCurrentTurn() != PlayerType::Player2)
    {
        m_thinkTimer = 0.0f;
        return;
    }

    if (game.IsWaitingForRoll() || game.IsAnyPieceMoving())
        return;

    m_thinkTimer += dt;
    if (m_thinkTimer < m_thinkDelay)
        return;

    m_thinkTimer = 0.0f;

    if (game.GetRemainingSteps() <= 0)
    {
        game.StartTurn();
        return;
    }

    if (!ExecuteMove(game))
    {
        game.ForceEndTurn();
    }
}

// 盤面評価値の計算
int DiceBattleAI::EvaluateBoard(const SimBoard& board)
{
    if (board.isGameOver)
    {
        if (board.isDraw)
            return 0;

        if (board.winner == PlayerType::Player2)
            return SCORE_WIN;
        if (board.winner == PlayerType::Player1)
            return SCORE_LOSE;
    }

    if (m_ruleMode == GameRuleMode::Original)
    {
        return EvaluateOriginal(board);
    }
    else
    {
        return EvaluateAdvance(board);
    }
}

// オリジナルルールの評価ロジック
int DiceBattleAI::EvaluateOriginal(const SimBoard& board)
{
    int score = 0;
    const SimPiece* p1King = nullptr;
    const SimPiece* p2King = nullptr;

    for (const auto& piece : board.pieces)
    {
        if (!piece.isActive)
            continue;

        int pieceValue =
            (piece.type == PieceType::King) ? KING_BASE_VALUE : (PAWN_BASE_VALUE + piece.face * PAWN_FACE_MULTIPLIER);

        if (piece.owner == PlayerType::Player2)
        {
            score += pieceValue;
            score += piece.gridY * ADVANCE_DISTANCE_WEIGHT;
            if (piece.type == PieceType::King)
                p2King = &piece;
        }
        else
        {
            score -= pieceValue;
            score -= (DiceBattleGame::BOARD_HEIGHT - 1 - piece.gridY) * ADVANCE_DISTANCE_WEIGHT;
            if (piece.type == PieceType::King)
                p1King = &piece;
        }
    }

    if (p1King)
    {
        for (const auto& piece : board.pieces)
        {
            if (piece.isActive && piece.owner == PlayerType::Player2 && piece.type == PieceType::Pawn)
            {
                int dist = std::abs(piece.gridX - p1King->gridX) + std::abs(piece.gridY - p1King->gridY);
                if (dist == 1 && piece.face == 1)
                    score += KING_ATTACK_BONUS_P2;
            }
        }
    }

    if (p2King)
    {
        for (const auto& piece : board.pieces)
        {
            if (piece.isActive && piece.owner == PlayerType::Player1 && piece.type == PieceType::Pawn)
            {
                int dist = std::abs(piece.gridX - p2King->gridX) + std::abs(piece.gridY - p2King->gridY);
                if (dist == 1 && piece.face == 1)
                    score -= KING_DANGER_PENALTY_P2;
            }
        }
    }

    return score;
}

// アドバンスルールの評価
int DiceBattleAI::EvaluateAdvance(const SimBoard& board)
{
    int score = EvaluateOriginal(board);

    if (board.p2Shielded)
        score += SHIELD_EVAL_SCORE;
    if (board.p1Shielded)
        score -= SHIELD_EVAL_SCORE;

    return score;
}

// 移動可能な選択肢を生成
std::vector<DiceBattleAI::MoveOption> DiceBattleAI::GenerateValidMoves(const SimBoard& board, PlayerType player)
{
    std::vector<MoveOption> moves;
    const int dirX[] = {0, 0, -1, 1};
    const int dirY[] = {1, -1, 0, 0};

    bool isTargetShielded = (m_ruleMode == GameRuleMode::Advance)
                                ? ((player == PlayerType::Player2) ? board.p1Shielded : board.p2Shielded)
                                : false;

    int destroyedCount = 0;
    for (const auto& p : board.pieces)
    {
        if (!p.isActive)
            destroyedCount++;
    }
    int allowedRows = std::min(DiceBattleGame::BOARD_HEIGHT, 2 + destroyedCount);

    int kingMinY = 0;
    int kingMaxY = DiceBattleGame::BOARD_HEIGHT - 1;
    if (player == PlayerType::Player1)
    {
        kingMinY = std::max(0, DiceBattleGame::BOARD_HEIGHT - allowedRows);
        kingMaxY = DiceBattleGame::BOARD_HEIGHT - 1;
    }
    else
    {
        kingMinY = 0;
        kingMaxY = std::min(DiceBattleGame::BOARD_HEIGHT - 1, allowedRows - 1);
    }

    for (const auto& piece : board.pieces)
    {
        if (!piece.isActive || piece.owner != player)
            continue;

        if (m_ruleMode == GameRuleMode::Advance && piece.isFrozen)
            continue;

        if (piece.type == PieceType::King)
        {
            for (int ny = kingMinY; ny <= kingMaxY; ++ny)
            {
                for (int nx = 0; nx < DiceBattleGame::BOARD_WIDTH; ++nx)
                {
                    if (nx == piece.gridX && ny == piece.gridY)
                        continue;

                    auto it = std::find_if(board.pieces.begin(), board.pieces.end(), [&](const SimPiece& p)
                                           { return p.isActive && p.gridX == nx && p.gridY == ny; });

                    const SimPiece* targetPiece = (it != board.pieces.end()) ? &(*it) : nullptr;
                    if (targetPiece && targetPiece->owner == player)
                        continue;

                    int moveScore = 0;
                    if (targetPiece && targetPiece->owner != player)
                    {
                        if (m_ruleMode == GameRuleMode::Advance && isTargetShielded)
                        {
                            moveScore += MOVE_SCORE_ATTACK_SHIELD_KING;
                        }
                        else
                        {
                            if (targetPiece->type == PieceType::King)
                                moveScore += MOVE_SCORE_ATTACK_KING;
                            else if (targetPiece->face == 1)
                                moveScore += MOVE_SCORE_ATTACK_FACE_ONE;
                            else
                                moveScore += MOVE_SCORE_ATTACK_PAWN;
                        }
                    }

                    moves.push_back({piece.gridX, piece.gridY, nx, ny, moveScore});
                }
            }
            continue;
        }

        for (int i = 0; i < 4; ++i)
        {
            int dx = dirX[i];
            int dy = dirY[i];
            int targetX = piece.gridX + dx;
            int targetY = piece.gridY + dy;

            if (targetX < 0 || targetX >= DiceBattleGame::BOARD_WIDTH || targetY < 0 ||
                targetY >= DiceBattleGame::BOARD_HEIGHT)
                continue;

            auto it = std::find_if(board.pieces.begin(), board.pieces.end(), [&](const SimPiece& p)
                                   { return p.isActive && p.gridX == targetX && p.gridY == targetY; });

            const SimPiece* targetPiece = (it != board.pieces.end()) ? &(*it) : nullptr;
            if (targetPiece && targetPiece->owner == player)
                continue;

            int moveScore = 0;

            if (targetPiece && targetPiece->owner != player)
            {
                if (m_ruleMode == GameRuleMode::Advance && isTargetShielded)
                {
                    moveScore += MOVE_SCORE_ATTACK_SHIELD;
                }
                else
                {
                    int myFutureFace = PredictFaceFromCurrent(piece.face, dx, dy);
                    int enemyFace = targetPiece->face;

                    if (targetPiece->type == PieceType::King)
                    {
                        if (myFutureFace == 1)
                            moveScore += MOVE_SCORE_KING_ACE_KILL;
                        else
                            moveScore += MOVE_SCORE_KING_ATTACK_FAIL;
                    }
                    else
                    {
                        if (myFutureFace > enemyFace)
                            moveScore += 1000 + (myFutureFace - enemyFace) * 150;
                        else if (myFutureFace == enemyFace)
                            moveScore -= 100;
                        else
                            moveScore -= 3000;
                    }
                }
            }
            else
            {
                if (player == PlayerType::Player2 && dy == 1)
                    moveScore += MOVE_SCORE_ADVANCE_STEP;
                if (player == PlayerType::Player1 && dy == -1)
                    moveScore += MOVE_SCORE_ADVANCE_STEP;
            }

            moves.push_back({piece.gridX, piece.gridY, targetX, targetY, moveScore});
        }
    }

    std::sort(moves.begin(), moves.end(), [](const MoveOption& a, const MoveOption& b) { return a.score > b.score; });

    return moves;
}

// 仮想盤面に指し手を適用
DiceBattleAI::SimBoard DiceBattleAI::ApplyMove(const SimBoard& board, const MoveOption& move, PlayerType player)
{
    SimBoard nextBoard = board;

    auto myIt = std::find_if(nextBoard.pieces.begin(), nextBoard.pieces.end(), [&](const SimPiece& p)
                             { return p.isActive && p.gridX == move.fromX && p.gridY == move.fromY; });

    if (myIt == nextBoard.pieces.end())
        return nextBoard;

    int dx = move.toX - move.fromX;
    int dy = move.toY - move.fromY;

    if (myIt->type != PieceType::King)
    {
        myIt->face = PredictFaceFromCurrent(myIt->face, dx, dy);
    }

    auto enemyIt = std::find_if(nextBoard.pieces.begin(), nextBoard.pieces.end(), [&](const SimPiece& p)
                                { return p.isActive && p.gridX == move.toX && p.gridY == move.toY; });

    if (enemyIt != nextBoard.pieces.end())
    {
        bool isEnemyShielded = (m_ruleMode == GameRuleMode::Advance)
                                   ? ((player == PlayerType::Player2) ? nextBoard.p1Shielded : nextBoard.p2Shielded)
                                   : false;

        if (m_ruleMode == GameRuleMode::Advance && isEnemyShielded)
        {
            myIt->isActive = false;
        }
        else if (myIt->type == PieceType::King && enemyIt->type == PieceType::King)
        {
            myIt->isActive = false;
            enemyIt->isActive = false;
        }
        else if (myIt->type == PieceType::King)
        {
            if (enemyIt->face == 1)
                myIt->isActive = false;
            else
                enemyIt->isActive = false;
        }
        else if (enemyIt->type == PieceType::King)
        {
            if (myIt->face == 1)
                enemyIt->isActive = false;
            else
                myIt->isActive = false;
        }
        else
        {
            if (myIt->face > enemyIt->face)
                enemyIt->isActive = false;
            else if (myIt->face < enemyIt->face)
                myIt->isActive = false;
            else
            {
                myIt->isActive = false;
                enemyIt->isActive = false;
            }
        }
    }

    if (myIt->isActive)
    {
        myIt->gridX = move.toX;
        myIt->gridY = move.toY;
    }

    nextBoard.remainingSteps--;
    if (nextBoard.remainingSteps <= 0)
    {
        nextBoard.currentTurn =
            (nextBoard.currentTurn == PlayerType::Player1) ? PlayerType::Player2 : PlayerType::Player1;
        nextBoard.remainingSteps = 2;
    }

    bool p1KingAlive =
        std::any_of(nextBoard.pieces.begin(), nextBoard.pieces.end(), [](const SimPiece& p)
                    { return p.isActive && p.owner == PlayerType::Player1 && p.type == PieceType::King; });
    bool p2KingAlive =
        std::any_of(nextBoard.pieces.begin(), nextBoard.pieces.end(), [](const SimPiece& p)
                    { return p.isActive && p.owner == PlayerType::Player2 && p.type == PieceType::King; });

    if (!p1KingAlive || !p2KingAlive)
    {
        nextBoard.isGameOver = true;
        if (!p1KingAlive && !p2KingAlive)
        {
            nextBoard.isDraw = true;
        }
        else if (p1KingAlive)
        {
            nextBoard.isDraw = false;
            nextBoard.winner = PlayerType::Player1;
        }
        else if (p2KingAlive)
        {
            nextBoard.isDraw = false;
            nextBoard.winner = PlayerType::Player2;
        }
    }

    return nextBoard;
}

// Alpha-Beta探索アルゴリズム
int DiceBattleAI::AlphaBeta(SimBoard board, int depth, int alpha, int beta, bool isMaximizing)
{
    if (depth == 0 || board.isGameOver)
    {
        return EvaluateBoard(board);
    }

    PlayerType currentPlayer = board.currentTurn;
    auto validMoves = GenerateValidMoves(board, currentPlayer);

    if (validMoves.empty())
    {
        return EvaluateBoard(board);
    }

    if (isMaximizing)
    {
        int maxEval = -SCORE_INF;
        for (const auto& move : validMoves)
        {
            SimBoard nextBoard = ApplyMove(board, move, currentPlayer);
            bool nextIsMax = (nextBoard.currentTurn == PlayerType::Player2);
            int eval = move.score + AlphaBeta(nextBoard, depth - 1, alpha, beta, nextIsMax);
            maxEval = (std::max)(maxEval, eval);
            alpha = (std::max)(alpha, eval);
            if (beta <= alpha)
                break;
        }
        return maxEval;
    }
    else
    {
        int minEval = SCORE_INF;
        for (const auto& move : validMoves)
        {
            SimBoard nextBoard = ApplyMove(board, move, currentPlayer);
            bool nextIsMax = (nextBoard.currentTurn == PlayerType::Player2);
            int eval = -move.score + AlphaBeta(nextBoard, depth - 1, alpha, beta, nextIsMax);
            minEval = (std::min)(minEval, eval);
            beta = (std::min)(beta, eval);
            if (beta <= alpha)
                break;
        }
        return minEval;
    }
}

// AI思考を実行し実際のゲームロジックへ反映
bool DiceBattleAI::ExecuteMove(DiceBattleGame& game)
{
    SimBoard currentBoard;
    const auto& pieces = game.GetPieces();

    for (const auto& piece : pieces)
    {
        if (!piece.isActive)
            continue;

        SimPiece sp;
        sp.gridX = piece.gridX;
        sp.gridY = piece.gridY;
        sp.owner = piece.owner;
        sp.type = piece.type;
        sp.face = (piece.type == PieceType::King) ? 7 : (piece.diceInstance ? piece.diceInstance->GetUpwardFace() : 1);
        sp.isActive = true;

        currentBoard.pieces.push_back(sp);
    }

    currentBoard.currentTurn = PlayerType::Player2;
    currentBoard.remainingSteps = game.GetRemainingSteps();

    if (m_ruleMode == GameRuleMode::Advance)
    {
        currentBoard.p1Shielded = game.IsPlayerShielded(PlayerType::Player1);
        currentBoard.p2Shielded = game.IsPlayerShielded(PlayerType::Player2);
    }

    auto validMoves = GenerateValidMoves(currentBoard, PlayerType::Player2);
    if (validMoves.empty())
        return false;

    int bestScore = -SCORE_INF;
    std::vector<MoveOption> bestMoves;

    for (auto& move : validMoves)
    {
        SimBoard nextBoard = ApplyMove(currentBoard, move, PlayerType::Player2);
        bool nextIsMax = (nextBoard.currentTurn == PlayerType::Player2);

        int totalScore = move.score + AlphaBeta(nextBoard, m_searchDepth - 1, -SCORE_INF, SCORE_INF, nextIsMax);

        if (totalScore > bestScore)
        {
            bestScore = totalScore;
            bestMoves.clear();
            bestMoves.push_back(move);
        }
        else if (totalScore == bestScore)
        {
            bestMoves.push_back(move);
        }
    }

    if (bestMoves.empty())
    {
        bestMoves.push_back(validMoves[0]);
    }

    std::uniform_int_distribution<size_t> dist(0, bestMoves.size() - 1);
    MoveOption chosen = bestMoves[dist(m_rng)];

    bool success = game.MovePiece(chosen.fromX, chosen.fromY, chosen.toX, chosen.toY);

    if (!success)
    {
        for (const auto& fallbackMove : validMoves)
        {
            if (game.MovePiece(fallbackMove.fromX, fallbackMove.fromY, fallbackMove.toX, fallbackMove.toY))
            {
                return true;
            }
        }
    }

    return success;
}