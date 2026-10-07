#include "pch.h"
#include "DiceBattleGame.h"
#include <algorithm>
#include <cmath>
#include <random>

using namespace DirectX;
using namespace DirectX::SimpleMath;

Quaternion SnapToNearest90Degrees(const Quaternion& q)
{
    Vector3 angles = q.ToEuler();
    const float halfPi = XM_PIDIV2;

    float rx = std::round(angles.x / halfPi) * halfPi;
    float ry = std::round(angles.y / halfPi) * halfPi;
    float rz = std::round(angles.z / halfPi) * halfPi;

    return Quaternion::CreateFromYawPitchRoll(ry, rx, rz);
}

int GetFaceFromRotation(const Quaternion& rot)
{
    struct FaceVector
    {
        int number;
        Vector3 localDir;
    };

    static const FaceVector faces[] = {
        {3, Vector3(0.0f, 1.0f, 0.0f)},  {4, Vector3(0.0f, -1.0f, 0.0f)}, {2, Vector3(0.0f, 0.0f, 1.0f)},
        {5, Vector3(0.0f, 0.0f, -1.0f)}, {1, Vector3(1.0f, 0.0f, 0.0f)},  {6, Vector3(-1.0f, 0.0f, 0.0f)},
    };

    Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);
    Quaternion invRot;
    rot.Inverse(invRot);
    Vector3 localUp = Vector3::Transform(worldUp, invRot);

    float maxDot = -100.0f;
    int bestFace = 1;

    for (const auto& face : faces)
    {
        float dot = face.localDir.Dot(localUp);
        if (dot > maxDot)
        {
            maxDot = dot;
            bestFace = face.number;
        }
    }
    return bestFace;
}

void DiceBattleGame::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_device = device;
    m_context = context;

    m_pieces.clear();
    m_currentTurn = PlayerType::Player1;
    m_isGameOver = false;
    m_isDraw = false;

    m_isWaitingForRoll = false;
    m_remainingSteps = 0;
    m_rollAnimTimer = 0.0f;

    m_isShielded[PlayerType::Player1] = false;
    m_isShielded[PlayerType::Player2] = false;

    float cellSize = 2.0f;
    float leftEdgeX = -(BOARD_WIDTH * cellSize * 0.5f) - 3.0f;
    m_stepDice.Initialize(device, context, "Resources/Models/hDice.cmo");
    m_stepDice.SetPosition(Vector3(leftEdgeX, -10.0f, -8.0f));

    auto AddPiece = [&](PlayerType owner, PieceType type, int gx, int gy)
    {
        BoardPiece piece;
        piece.owner = owner;
        piece.type = type;
        piece.gridX = gx;
        piece.gridY = gy;
        piece.initialX = gx;
        piece.initialY = gy;
        piece.isActive = true;
        piece.rotProgress = 1.0f;

        piece.diceInstance = std::make_shared<Dice>();

        std::string modelPath;
        if (owner == PlayerType::Player1)
        {
            modelPath = (type == PieceType::King) ? "Resources/Models/KDice.cmo" : "Resources/Models/BlueDice.cmo";
        }
        else
        {
            modelPath = (type == PieceType::King) ? "Resources/Models/EKDice.cmo" : "Resources/Models/RedDice.cmo";
        }

        piece.diceInstance->Initialize(device, context, modelPath);
        Vector3 initialPos = GridToWorldPosition(gx, gy);
        piece.diceInstance->SetPosition(initialPos);
        piece.startPos = initialPos;
        piece.targetPos = initialPos;

        m_pieces.push_back(piece);
    };

    AddPiece(PlayerType::Player2, PieceType::Pawn, 0, 0);
    AddPiece(PlayerType::Player2, PieceType::Pawn, 2, 0);
    AddPiece(PlayerType::Player2, PieceType::King, 4, 0);
    AddPiece(PlayerType::Player2, PieceType::Pawn, 6, 0);
    AddPiece(PlayerType::Player2, PieceType::Pawn, 8, 0);

    AddPiece(PlayerType::Player1, PieceType::Pawn, 0, 7);
    AddPiece(PlayerType::Player1, PieceType::Pawn, 2, 7);
    AddPiece(PlayerType::Player1, PieceType::King, 4, 7);
    AddPiece(PlayerType::Player1, PieceType::Pawn, 6, 7);
    AddPiece(PlayerType::Player1, PieceType::Pawn, 8, 7);
}

Vector3 DiceBattleGame::GridToWorldPosition(int gridX, int gridY) const
{
    float cellSize = 2.0f;
    float offsetX = -(BOARD_WIDTH - 1) * cellSize * 0.5f;
    float offsetZ = (BOARD_HEIGHT - 1) * cellSize * 0.5f;

    float x = offsetX + gridX * cellSize;
    float z = offsetZ - gridY * cellSize;

    return Vector3(x, 1.0f, z);
}

void DiceBattleGame::StartTurn()
{
    if (m_isGameOver || m_isWaitingForRoll)
        return;

    m_isWaitingForRoll = true;
    m_rollAnimTimer = ROLL_ANIM_DURATION;
    m_remainingSteps = 0;

    float cellSize = 2.0f;
    float leftEdgeX = -(BOARD_WIDTH * cellSize * 0.5f) - 3.0f;
    float rightEdgeX = (BOARD_WIDTH * cellSize * 0.5f) + 3.0f;

    if (m_currentTurn == PlayerType::Player1)
    {
        m_throwStartPos = Vector3(leftEdgeX, 1.0f, -8.0f);
        m_throwTargetPos = Vector3(leftEdgeX, 1.0f, 0.0f);
    }
    else
    {
        m_throwStartPos = Vector3(rightEdgeX, 1.0f, 8.0f);
        m_throwTargetPos = Vector3(rightEdgeX, 1.0f, 0.0f);
    }

    m_stepDice.SetPosition(m_throwStartPos);
}

void DiceBattleGame::Update(float dt)
{
    if (m_isWaitingForRoll)
    {
        m_rollAnimTimer -= dt;

        if (m_rollAnimTimer > 0.0f)
        {
            float t = 1.0f - (m_rollAnimTimer / ROLL_ANIM_DURATION);
            Vector3 currentPos = Vector3::Lerp(m_throwStartPos, m_throwTargetPos, t);

            float arcHeight = 3.5f;
            currentPos.y += std::sin(t * XM_PI) * arcHeight;

            m_stepDice.SetPosition(currentPos);

            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(-XM_PI, XM_PI);
            Quaternion spinRot = Quaternion::CreateFromYawPitchRoll(dist(rng), dist(rng), dist(rng));
            m_stepDice.SetRotation(spinRot);
        }
        else
        {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<int> dist(0, 3);

            float rx = dist(rng) * XM_PIDIV2;
            float ry = dist(rng) * XM_PIDIV2;
            float rz = dist(rng) * XM_PIDIV2;
            Quaternion finalRot = Quaternion::CreateFromYawPitchRoll(ry, rx, rz);

            m_stepDice.SetPosition(m_throwTargetPos);
            m_stepDice.SetRotation(finalRot);

            m_remainingSteps = m_stepDice.GetStepValue();
            m_isWaitingForRoll = false;
        }
    }

    static float pulseTimer = 0.0f;
    pulseTimer += dt * 6.0f;
    float pulseIntensity = (std::sin(pulseTimer) * 0.5f + 0.5f) * 0.8f + 0.2f;

    for (auto& piece : m_pieces)
    {
        if (!piece.isActive)
            continue;

        if (piece.diceInstance)
        {
            if (piece.rotProgress < 1.0f)
            {
                piece.rotProgress += dt * ROTATION_SPEED;
                if (piece.rotProgress >= 1.0f)
                {
                    piece.rotProgress = 1.0f;
                    piece.diceInstance->SetRotation(piece.targetRot);
                    piece.diceInstance->SetPosition(piece.targetPos);
                }
                else
                {
                    Quaternion currentRot = Quaternion::Slerp(piece.startRot, piece.targetRot, piece.rotProgress);
                    piece.diceInstance->SetRotation(currentRot);

                    Vector3 currentPos = Vector3::Lerp(piece.startPos, piece.targetPos, piece.rotProgress);
                    piece.diceInstance->SetPosition(currentPos);
                }
            }

            bool isSelected = (piece.gridX == m_cursorX && piece.gridY == m_cursorY && piece.owner == m_currentTurn);

            if (isSelected)
            {
                Vector3 emissive = Vector3(1.0f, 0.9f, 0.3f) * pulseIntensity;
                piece.diceInstance->SetEmissiveColor(emissive);
            }
            else
            {
                piece.diceInstance->SetEmissiveColor(Vector3::Zero);
            }

            piece.diceInstance->Update(dt);
        }
    }
}

void DiceBattleGame::Render(ID3D11DeviceContext* context, CommonStates& states, const Matrix& view, const Matrix& proj)
{
    if (m_isWaitingForRoll || m_remainingSteps > 0)
    {
        m_stepDice.Render(context, states, view, proj);
    }

    for (auto& piece : m_pieces)
    {
        if (piece.isActive && piece.diceInstance)
        {
            piece.diceInstance->Render(context, states, view, proj);
        }
    }
}

const BoardPiece* DiceBattleGame::GetPieceAt(int x, int y) const
{
    for (const auto& piece : m_pieces)
    {
        if (piece.isActive && piece.gridX == x && piece.gridY == y)
            return &piece;
    }
    return nullptr;
}

BoardPiece* DiceBattleGame::GetPieceAt(int x, int y)
{
    for (auto& piece : m_pieces)
    {
        if (piece.isActive && piece.gridX == x && piece.gridY == y)
            return &piece;
    }
    return nullptr;
}

bool DiceBattleGame::MovePiece(int fromX, int fromY, int toX, int toY)
{
    if (m_isGameOver || m_isWaitingForRoll || m_remainingSteps <= 0)
        return false;

    BoardPiece* attacker = GetPieceAt(fromX, fromY);
    if (!attacker || attacker->owner != m_currentTurn)
        return false;

    if (attacker->rotProgress < 1.0f)
        return false;

    if (toX < 0 || toX >= BOARD_WIDTH || toY < 0 || toY >= BOARD_HEIGHT)
        return false;

    int dx = toX - fromX;
    int dy = toY - fromY;
    if ((std::abs(dx) + std::abs(dy)) != 1)
        return false;

    BoardPiece* targetPiece = GetPieceAt(toX, toY);

    if (targetPiece && targetPiece->owner == m_currentTurn)
    {
        return false;
    }

    if (attacker->diceInstance)
    {
        Quaternion currentRot = attacker->diceInstance->GetRotation();
        Quaternion deltaRot = Quaternion::Identity;

        if (dx == 1)
            deltaRot = Quaternion::CreateFromAxisAngle(Vector3::UnitZ, -XM_PIDIV2);
        else if (dx == -1)
            deltaRot = Quaternion::CreateFromAxisAngle(Vector3::UnitZ, XM_PIDIV2);
        else if (dy == 1)
            deltaRot = Quaternion::CreateFromAxisAngle(Vector3::UnitX, -XM_PIDIV2);
        else if (dy == -1)
            deltaRot = Quaternion::CreateFromAxisAngle(Vector3::UnitX, XM_PIDIV2);

        attacker->startRot = currentRot;
        attacker->targetRot = SnapToNearest90Degrees(currentRot * deltaRot);

        attacker->startPos = GridToWorldPosition(fromX, fromY);
        attacker->targetPos = GridToWorldPosition(toX, toY);

        attacker->rotProgress = 0.0f;
    }

    if (targetPiece && targetPiece->owner != m_currentTurn)
    {
        ResolveBattle(*attacker, *targetPiece, toX, toY);
    }
    else
    {
        attacker->gridX = toX;
        attacker->gridY = toY;
    }

    m_remainingSteps--;

    if (m_remainingSteps <= 0)
    {
        m_currentTurn = (m_currentTurn == PlayerType::Player1) ? PlayerType::Player2 : PlayerType::Player1;
    }

    CheckGameEndConditions();
    return true;
}

void DiceBattleGame::ResolveBattle(BoardPiece& attacker, BoardPiece& defender, int targetX, int targetY)
{
    int attackerX = attacker.gridX;
    int attackerY = attacker.gridY;

    if (IsPlayerShielded(defender.owner))
    {
        RemovePieceAt(attackerX, attackerY);
        return;
    }

    if (attacker.type == PieceType::King && defender.type == PieceType::King)
    {
        RemovePieceAt(attackerX, attackerY);
        RemovePieceAt(targetX, targetY);
        return;
    }

    if (attacker.type == PieceType::King)
    {
        int defFace = defender.diceInstance ? GetFaceFromRotation(defender.targetRot) : 1;
        if (defFace == 1)
        {
            RemovePieceAt(attackerX, attackerY);
        }
        else
        {
            RemovePieceAt(targetX, targetY);
            attacker.gridX = targetX;
            attacker.gridY = targetY;
        }
        return;
    }

    if (defender.type == PieceType::King)
    {
        int atkFace = attacker.diceInstance ? GetFaceFromRotation(attacker.targetRot) : 1;
        if (atkFace == 1)
        {
            RemovePieceAt(targetX, targetY);
            attacker.gridX = targetX;
            attacker.gridY = targetY;
        }
        else
        {
            RemovePieceAt(attackerX, attackerY);
        }
        return;
    }

    int atkFace = attacker.diceInstance ? GetFaceFromRotation(attacker.targetRot) : 1;
    int defFace = defender.diceInstance ? GetFaceFromRotation(defender.targetRot) : 1;

    if (atkFace > defFace)
    {
        RemovePieceAt(targetX, targetY);
        attacker.gridX = targetX;
        attacker.gridY = targetY;
    }
    else if (atkFace < defFace)
    {
        RemovePieceAt(attackerX, attackerY);
    }
    else
    {
        RemovePieceAt(attackerX, attackerY);
        RemovePieceAt(targetX, targetY);
    }
}

void DiceBattleGame::CheckGameEndConditions()
{
    bool p1KingAlive = false;
    bool p2KingAlive = false;

    for (const auto& piece : m_pieces)
    {
        if (piece.isActive && piece.type == PieceType::King)
        {
            if (piece.owner == PlayerType::Player1)
                p1KingAlive = true;
            if (piece.owner == PlayerType::Player2)
                p2KingAlive = true;
        }
    }

    if (!p1KingAlive || !p2KingAlive)
    {
        m_isGameOver = true;
        if (!p1KingAlive && !p2KingAlive)
        {
            m_isDraw = true;
        }
        else if (p1KingAlive)
        {
            m_isDraw = false;
            m_winner = PlayerType::Player1;
        }
        else if (p2KingAlive)
        {
            m_isDraw = false;
            m_winner = PlayerType::Player2;
        }
    }
}

void DiceBattleGame::RemovePieceAt(int x, int y)
{
    for (auto& piece : m_pieces)
    {
        if (piece.isActive && piece.gridX == x && piece.gridY == y)
        {
            piece.isActive = false;
            break;
        }
    }
}

void DiceBattleGame::AddPieceAt(int x, int y, PlayerType owner, PieceType type, int topValue)
{
    BoardPiece piece;
    piece.owner = owner;
    piece.type = type;
    piece.gridX = x;
    piece.gridY = y;
    piece.initialX = x;
    piece.initialY = y;
    piece.isActive = true;
    piece.rotProgress = 1.0f;

    piece.diceInstance = std::make_shared<Dice>();

    std::string modelPath;
    if (owner == PlayerType::Player1)
    {
        modelPath = (type == PieceType::King) ? "Resources/Models/KDice.cmo" : "Resources/Models/BlueDice.cmo";
    }
    else
    {
        modelPath = (type == PieceType::King) ? "Resources/Models/EKDice.cmo" : "Resources/Models/RedDice.cmo";
    }

    piece.diceInstance->Initialize(m_device, m_context, modelPath);
    Vector3 initialPos = GridToWorldPosition(x, y);
    piece.diceInstance->SetPosition(initialPos);
    piece.startPos = initialPos;
    piece.targetPos = initialPos;

    m_pieces.push_back(piece);
}

bool DiceBattleGame::HasDeadPiece(PlayerType player) const
{
    for (const auto& piece : m_pieces)
    {
        if (!piece.isActive && piece.owner == player && piece.type != PieceType::King)
        {
            return true;
        }
    }
    return false;
}

bool DiceBattleGame::RevivePieceAt(PlayerType player, int targetX, int targetY)
{
    if (GetPieceAt(targetX, targetY) != nullptr)
        return false;

    for (auto& piece : m_pieces)
    {
        if (!piece.isActive && piece.owner == player && piece.type != PieceType::King)
        {
            piece.isActive = true;
            piece.gridX = targetX;
            piece.gridY = targetY;

            Vector3 pos = GridToWorldPosition(targetX, targetY);
            piece.startPos = pos;
            piece.targetPos = pos;
            piece.rotProgress = 1.0f;

            if (piece.diceInstance)
            {
                piece.diceInstance->SetPosition(pos);
            }
            return true;
        }
    }
    return false;
}