#include "pch.h"
#include "DiceBattleGame.h"
#include "Framework/SoundManager.h"
#include <algorithm>
#include <cmath>
#include <random>

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 歩数（1〜3）に応じた標準ダイス回転姿態（クォータニオン）の取得
static Quaternion GetStepDiceQuaternion(int stepValue)
{
    switch (stepValue)
    {
        case 1:
            return Quaternion::CreateFromAxisAngle(Vector3::UnitZ, -XM_PIDIV2);
        case 2:
            return Quaternion::CreateFromAxisAngle(Vector3::UnitX, -XM_PIDIV2);
        case 3:
            return Quaternion::Identity;
        default:
            return Quaternion::Identity;
    }
}

// 任意回転を最も近い90度単位にスナップ補正
Quaternion SnapToNearest90Degrees(const Quaternion& q)
{
    Vector3 angles = q.ToEuler();
    const float halfPi = XM_PIDIV2;

    float rx = std::round(angles.x / halfPi) * halfPi;
    float ry = std::round(angles.y / halfPi) * halfPi;
    float rz = std::round(angles.z / halfPi) * halfPi;

    return Quaternion::CreateFromYawPitchRoll(ry, rx, rz);
}

// スナップ姿勢から現在の上面出目を算出
int GetFaceFromRotation(const Quaternion& rot)
{
    struct FaceVector
    {
        int number;
        Vector3 localDir;
    };

    static const FaceVector faces[] = {
        {3, Vector3(0.0f, 1.0f, 0.0f)},  // +Y
        {4, Vector3(0.0f, -1.0f, 0.0f)}, // -Y
        {2, Vector3(0.0f, 0.0f, 1.0f)},  // +Z
        {5, Vector3(0.0f, 0.0f, -1.0f)}, // -Z
        {1, Vector3(1.0f, 0.0f, 0.0f)},  // +X
        {6, Vector3(-1.0f, 0.0f, 0.0f)}, // -X
    };

    Quaternion normalizedRot = rot;
    normalizedRot.Normalize();
    Quaternion snappedRot = SnapToNearest90Degrees(normalizedRot);

    Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);
    Quaternion invRot;
    snappedRot.Inverse(invRot);
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

// 初期化（コマの配置・歩数ダイスのセットアップ）
void DiceBattleGame::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_device = device;
    m_context = context;

    m_pieces.clear();
    m_currentTurn = PlayerType::Player1;
    m_isGameOver = false;
    m_isDraw = false;

    m_isWaitingForRoll = false;
    m_hasRolledThisTurn = false;
    m_remainingSteps = 0;
    m_targetFixedSteps = 0;
    m_justLandedSteps = 0;
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

        piece.startRot = Quaternion::Identity;
        piece.targetRot = Quaternion::Identity;
        piece.diceInstance->SetRotation(Quaternion::Identity);

        piece.startPos = initialPos;
        piece.targetPos = initialPos;

        m_pieces.push_back(piece);
    };

    // プレイヤー2（敵）陣地コマの初期配置
    AddPiece(PlayerType::Player2, PieceType::Pawn, 0, 0);
    AddPiece(PlayerType::Player2, PieceType::Pawn, 2, 0);
    AddPiece(PlayerType::Player2, PieceType::King, 4, 0);
    AddPiece(PlayerType::Player2, PieceType::Pawn, 6, 0);
    AddPiece(PlayerType::Player2, PieceType::Pawn, 8, 0);

    // プレイヤー1（自分）陣地コマの初期配置
    AddPiece(PlayerType::Player1, PieceType::Pawn, 0, 7);
    AddPiece(PlayerType::Player1, PieceType::Pawn, 2, 7);
    AddPiece(PlayerType::Player1, PieceType::King, 4, 7);
    AddPiece(PlayerType::Player1, PieceType::Pawn, 6, 7);
    AddPiece(PlayerType::Player1, PieceType::Pawn, 8, 7);
}

// 全オブジェクト姿勢の同期パケット構築（ホスト側送信機能）
void DiceBattleGame::BuildWorldSyncPacket(MsgWorldSync& outPacket) const
{
    outPacket.header.size = sizeof(MsgWorldSync);
    outPacket.header.type = PacketType::SC_WorldSync;

    Vector3 sPos = m_stepDice.GetPosition();
    Quaternion sRot = m_stepDice.GetRotation();
    outPacket.stepDiceTransform = {sPos.x, sPos.y, sPos.z, sRot.x, sRot.y, sRot.z, sRot.w};

    outPacket.pieceCount = static_cast<uint8_t>(m_pieces.size());
    for (size_t i = 0; i < m_pieces.size() && i < 10; ++i)
    {
        const auto& p = m_pieces[i];
        outPacket.pieces[i].owner = static_cast<uint8_t>(p.owner);
        outPacket.pieces[i].type = static_cast<uint8_t>(p.type);
        outPacket.pieces[i].gridX = static_cast<int8_t>(p.gridX);
        outPacket.pieces[i].gridY = static_cast<int8_t>(p.gridY);
        outPacket.pieces[i].isActive = p.isActive;

        if (p.diceInstance)
        {
            Vector3 pos = p.diceInstance->GetPosition();
            Quaternion rot = p.diceInstance->GetRotation();
            outPacket.pieces[i].transform = {pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, rot.w};
        }
    }
}

// ワールド姿勢の適用（クライアント側受信機能）
void DiceBattleGame::ApplyWorldSyncFromHost(const MsgWorldSync& syncData)
{
    Vector3 sPos(syncData.stepDiceTransform.posX, syncData.stepDiceTransform.posY, syncData.stepDiceTransform.posZ);
    Quaternion sRot(syncData.stepDiceTransform.rotX, syncData.stepDiceTransform.rotY, syncData.stepDiceTransform.rotZ,
                    syncData.stepDiceTransform.rotW);
    m_stepDice.SetPosition(sPos);
    m_stepDice.SetRotation(sRot);

    for (size_t i = 0; i < syncData.pieceCount && i < m_pieces.size(); ++i)
    {
        auto& p = m_pieces[i];
        const auto& src = syncData.pieces[i];

        p.gridX = src.gridX;
        p.gridY = src.gridY;
        p.isActive = src.isActive;

        if (p.diceInstance)
        {
            Vector3 pos(src.transform.posX, src.transform.posY, src.transform.posZ);
            Quaternion rot(src.transform.rotX, src.transform.rotY, src.transform.rotZ, src.transform.rotW);
            p.diceInstance->SetPosition(pos);
            p.diceInstance->SetRotation(rot);
            p.targetRot = rot;
        }
    }
}

// マス目（GridX, GridY）から3Dワールド座標への変換
Vector3 DiceBattleGame::GridToWorldPosition(int gridX, int gridY) const
{
    float cellSize = 2.0f;
    float offsetX = -(BOARD_WIDTH - 1) * cellSize * 0.5f;
    float offsetZ = (BOARD_HEIGHT - 1) * cellSize * 0.5f;

    float x = offsetX + gridX * cellSize;
    float z = offsetZ - gridY * cellSize;

    return Vector3(x, 1.0f, z);
}

// ターン開始・ダイス回転アニメーションの開始
void DiceBattleGame::StartTurn()
{
    if (m_isGameOver || m_isWaitingForRoll || m_hasRolledThisTurn || m_remainingSteps > 0)
        return;

    m_isWaitingForRoll = true;
    m_hasRolledThisTurn = true; // 振り開始フラグをマーク
    m_rollAnimTimer = ROLL_ANIM_DURATION;
    m_remainingSteps = 0;
    m_justLandedSteps = 0;

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
    SoundManager::GetInstance().PlaySE(SEType::DiceRoll);
}

// 固定歩数でのターン開始
void DiceBattleGame::StartTurnWithSteps(int fixedSteps)
{
    if (m_isWaitingForRoll || m_hasRolledThisTurn || m_remainingSteps > 0)
        return;

    m_targetFixedSteps = fixedSteps;
    StartTurn();
}

// 出目をアニメーションなしで即時決定
void DiceBattleGame::SetDiceResultImmediately(int steps)
{
    m_isWaitingForRoll = false;
    m_hasRolledThisTurn = true;
    m_remainingSteps = steps;
    m_targetFixedSteps = 0;

    m_stepDice.SetRotation(GetStepDiceQuaternion(steps));

    float cellSize = 2.0f;
    float edgeX = (m_currentTurn == PlayerType::Player1) ? (-(BOARD_WIDTH * cellSize * 0.5f) - 3.0f)
                                                         : ((BOARD_WIDTH * cellSize * 0.5f) + 3.0f);

    m_stepDice.SetPosition(Vector3(edgeX, 1.0f, 0.0f));
}

// 毎フレームのダイスアニメーションおよびコマ回転アニメーション更新
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
            int targetVal = 0;

            if (m_targetFixedSteps > 0)
            {
                targetVal = m_targetFixedSteps;
                m_remainingSteps = targetVal;
                m_targetFixedSteps = 0;
            }
            else
            {
                static std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<int> dist(1, 3);
                targetVal = dist(rng);
                m_remainingSteps = targetVal;
            }

            m_stepDice.SetRotation(GetStepDiceQuaternion(targetVal));
            m_stepDice.SetPosition(m_throwTargetPos);
            m_isWaitingForRoll = false;

            SoundManager::GetInstance().PlaySE(SEType::DiceRoll, 5.0f);

            m_justLandedSteps = targetVal;
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

                    SoundManager::GetInstance().PlaySE(SEType::DiceRoll, 2.0f);


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

// ゲームモデルの描画
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

// 座標指定でのコマ取得
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

// コマの移動実行および戦闘勝敗・ターン遷移
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

        SoundManager::GetInstance().PlaySE(SEType::DiceRoll, 2.0f);


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

    CheckGameEndConditions();

    if (m_isGameOver)
    {
        return true;
    }

    m_remainingSteps--;

    if (m_remainingSteps <= 0)
    {
        m_currentTurn = (m_currentTurn == PlayerType::Player1) ? PlayerType::Player2 : PlayerType::Player1;
        m_hasRolledThisTurn = false; // ターン切り替え時に振り済みフラグをリセット
    }

    return true;
}

// 対戦バトル結果の勝敗解決ロジック
void DiceBattleGame::ResolveBattle(BoardPiece& attacker, BoardPiece& defender, int targetX, int targetY)
{
    int attackerX = attacker.gridX;
    int attackerY = attacker.gridY;

    if (IsPlayerShielded(defender.owner))
    {
        RemovePieceAt(attackerX, attackerY);
        return;
    }

    // 王（King）同士の相討ち
    if (attacker.type == PieceType::King && defender.type == PieceType::King)
    {
        RemovePieceAt(attackerX, attackerY);
        RemovePieceAt(targetX, targetY);

        m_isGameOver = true;
        m_isDraw = true;
        return;
    }

    // 攻撃側が王の場合（相手の出目1なら相討ち/撃破される）
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

    // 防御側が王の場合（攻撃出目1なら王を撃破）
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

    // 通常コマ同士の出目数値比較
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
        // 同数の場合は相討ち
        RemovePieceAt(attackerX, attackerY);
        RemovePieceAt(targetX, targetY);
    }
}

// ゲーム全滅・終了条件チェック
void DiceBattleGame::CheckGameEndConditions()
{
    if (m_isGameOver)
        return;

    bool p1KingAlive = false;
    bool p2KingAlive = false;

    int p1NonKingCount = 0;
    int p2NonKingCount = 0;

    for (const auto& piece : m_pieces)
    {
        if (!piece.isActive)
            continue;

        if (piece.owner == PlayerType::Player1)
        {
            if (piece.type == PieceType::King)
                p1KingAlive = true;
            else
                p1NonKingCount++;
        }
        else if (piece.owner == PlayerType::Player2)
        {
            if (piece.type == PieceType::King)
                p2KingAlive = true;
            else
                p2NonKingCount++;
        }
    }

    // 両者王非生存、または双方通常コマ全滅時の引き分け判定
    if ((!p1KingAlive && !p2KingAlive) || (p1KingAlive && p2KingAlive && p1NonKingCount == 0 && p2NonKingCount == 0))
    {
        m_isGameOver = true;
        m_isDraw = true;
        return;
    }

    if (!p1KingAlive || p1NonKingCount == 0)
    {
        m_isGameOver = true;
        m_isDraw = false;
        m_winner = PlayerType::Player2;
        return;
    }

    if (!p2KingAlive || p2NonKingCount == 0)
    {
        m_isGameOver = true;
        m_isDraw = false;
        m_winner = PlayerType::Player1;
        return;
    }
}

// 盤面コマの削除
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

// 指定位置へのコマ動的生成・追加
void DiceBattleGame::AddPieceAt(int x, int y, PlayerType owner, PieceType type, int topValue)
{
    (void)topValue;
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

    piece.startRot = Quaternion::Identity;
    piece.targetRot = Quaternion::Identity;
    piece.diceInstance->SetRotation(Quaternion::Identity);

    piece.startPos = initialPos;
    piece.targetPos = initialPos;

    m_pieces.push_back(piece);
}

// 蘇生対象となる撃破済みコマの存在判定
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

// スキル用：コマの蘇生処理
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