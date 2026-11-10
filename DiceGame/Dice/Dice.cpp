#include "pch.h"
#include "Dice.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 3Dモデルのロードおよび各種物理パラメータの初期化
void Dice::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& modelPath)
{
    EffectFactory fx(device);
    fx.SetDirectory(L"Resources/Models");

    std::wstring wPath(modelPath.begin(), modelPath.end());
    try
    {
        m_model = Model::CreateFromCMO(device, wPath.c_str(), fx);
    }
    catch (...)
    {
        // CMOモデルの読み込み失敗時のハンドリング
    }

    m_position = Vector3(0.0f, DiceConstants::DEFAULT_POS_Y, 0.0f);
    m_rotation = Quaternion::Identity;
    m_scale = Vector3::One;
    m_collisionScale = Vector3::One;
    m_velocity = Vector3::Zero;
    m_angularVelocity = Vector3::Zero;
    m_isSettled = false;
    m_emissiveColor = Vector3::Zero;
}

// 初速インパルスおよび各軸ランダム角速度の適用によるロール開始処理
void Dice::Roll(const Vector3& initialImpulse)
{
    // 投擲インパルスの適用（水平ベクトル拡大 ＋ 上方への跳ね上がり力）
    m_velocity = initialImpulse * DiceConstants::ROLL_IMPULSE_SCALE;
    m_velocity.y += DiceConstants::ROLL_UPWARD_IMPULSE;

    // 各軸に対してランダムな角速度を設定
    m_angularVelocity = Vector3(
        static_cast<float>(rand() % DiceConstants::ROLL_ANGULAR_VEL_RANGE - DiceConstants::ROLL_ANGULAR_VEL_OFFSET),
        static_cast<float>(rand() % DiceConstants::ROLL_ANGULAR_VEL_RANGE - DiceConstants::ROLL_ANGULAR_VEL_OFFSET),
        static_cast<float>(rand() % DiceConstants::ROLL_ANGULAR_VEL_RANGE - DiceConstants::ROLL_ANGULAR_VEL_OFFSET));

    m_isSettled = false;

    // 投擲効果音の再生
    SoundManager::GetInstance().PlaySE(SEType::DiceRoll);
}

// フレーム毎の重力落下、床面衝突バウンド、摩擦減衰、完全停止判定の実行
void Dice::Update(float dt)
{
    if (m_isSettled)
    {
        return;
    }

    // 重力加速度の適用と仮位置計算
    m_velocity.y -= DiceConstants::GRAVITY * dt;
    Vector3 nextPos = m_position + m_velocity * dt;

    // 床面との判定・反発処理
    if (nextPos.y <= DiceConstants::FLOOR_Y)
    {
        nextPos.y = DiceConstants::FLOOR_Y;

        if (m_velocity.y < 0.0f)
        {
            // 一定以上のスピードで激しくバウンドした場合にSEを鳴らす
            if (std::abs(m_velocity.y) > DiceConstants::MIN_BOUNCE_SE_SPEED)
            {
                SoundManager::GetInstance().PlaySE(SEType::DiceRoll, 0.6f);
            }

            // 反発係数による上昇力への変換
            m_velocity.y = -m_velocity.y * DiceConstants::BOUNCE_FACTOR;
            if (std::abs(m_velocity.y) < DiceConstants::BOUNCE_STOP_THRESHOLD)
            {
                m_velocity.y = 0.0f;
            }
        }

        // 床面接触時の摩擦減衰
        m_velocity.x *= DiceConstants::FRICTION_DAMPING;
        m_velocity.z *= DiceConstants::FRICTION_DAMPING;
        m_angularVelocity *= DiceConstants::FRICTION_DAMPING;
    }

    m_position = nextPos;

    // 角速度に応じた回転姿勢（クォータニオン）の更新
    if (m_angularVelocity.LengthSquared() > DiceConstants::ANGULAR_VELOCITY_THRESHOLD)
    {
        Quaternion deltaRot = Quaternion::CreateFromYawPitchRoll(m_angularVelocity.y * dt, m_angularVelocity.x * dt,
                                                                 m_angularVelocity.z * dt);
        m_rotation = m_rotation * deltaRot;
        m_rotation.Normalize();
    }

    // 速度および角速度が閾値以下に低下した際の完全停止処理
    if (m_position.y <= DiceConstants::FLOOR_Y + DiceConstants::SETTLE_HEIGHT_MARGIN &&
        m_velocity.LengthSquared() < DiceConstants::SETTLE_VELOCITY_THRESHOLD_SQ &&
        m_angularVelocity.LengthSquared() < DiceConstants::SETTLE_VELOCITY_THRESHOLD_SQ)
    {
        m_velocity = Vector3::Zero;
        m_angularVelocity = Vector3::Zero;
        m_position.y = DiceConstants::FLOOR_Y;
        m_isSettled = true;
    }
}

// 内部保持の変換行列を使用した描画処理の呼び出し
void Dice::Render(ID3D11DeviceContext* context, DX11::CommonStates& states, const Matrix& view,
                  const Matrix& proj) const
{
    Render(context, states, GetWorldMatrix(), view, proj);
}

// 外部指定のワールド行列を用いた3Dモデルのレンダリング
void Dice::Render(ID3D11DeviceContext* context, DX11::CommonStates& states, const Matrix& world, const Matrix& view,
                  const Matrix& proj) const
{
    if (!m_model)
        return;

    // マテリアルの発光色（EmissiveColor）を反映
    m_model->UpdateEffects(
        [this](IEffect* effect)
        {
            if (auto basicEffect = dynamic_cast<BasicEffect*>(effect))
            {
                basicEffect->SetEmissiveColor(m_emissiveColor);
            }
        });

    m_model->Draw(context, states, world, view, proj);
}

// 現在のダイスの回転姿勢から上面出目を取得
int Dice::GetUpwardFace() const
{
    return GetTopFaceFromRotation(m_rotation);
}

// クォータニオン逆変換と内積演算による上面出目算出アルゴリズム
int Dice::GetTopFaceFromRotation(const Quaternion& rot) const
{
    struct FaceInfo
    {
        int number;
        Vector3 localDir;
    };

    static const FaceInfo faces[] = {
        {3, Vector3(0.0f, 1.0f, 0.0f)},  // +Y (上面)
        {4, Vector3(0.0f, -1.0f, 0.0f)}, // -Y (下面)
        {2, Vector3(0.0f, 0.0f, 1.0f)},  // +Z (前面)
        {5, Vector3(0.0f, 0.0f, -1.0f)}, // -Z (後面)
        {1, Vector3(1.0f, 0.0f, 0.0f)},  // +X (右面)
        {6, Vector3(-1.0f, 0.0f, 0.0f)}  // -X (左面)
    };

    const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);

    // ワールド上方向をダイスのローカル座標系へ逆変換
    Quaternion invRot;
    rot.Inverse(invRot);
    Vector3 localUp = Vector3::Transform(worldUp, invRot);

    float maxDot = DiceConstants::DOT_PRODUCT_INIT_MIN;
    int bestFace = 1;

    // 各面のローカル法線とローカル上方向の内積を計算し最密着面を特定
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