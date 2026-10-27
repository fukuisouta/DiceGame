#include "pch.h"
#include "Dice.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

// ダイスモデルのロードおよび初期パラメータのセットアップ
void Dice::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& modelPath)
{
    // CMOモデルエフェクトの読み込みパスを設定
    EffectFactory fx(device);
    fx.SetDirectory(L"Resources/Models");

    std::wstring wPath(modelPath.begin(), modelPath.end());
    try
    {
        // CMO形式の3Dダイスモデルを作成
        m_model = Model::CreateFromCMO(device, wPath.c_str(), fx);
    }
    catch (...)
    {
        // 読み込み失敗時の例外ハンドリング（必要に応じて代替処理）
    }

    // 初期トランスフォームと運動パラメータのリセット
    m_position = Vector3(0.0f, 1.0f, 0.0f);
    m_rotation = Quaternion::Identity;
    m_scale = Vector3(1.0f, 1.0f, 1.0f);
    m_collisionScale = Vector3(1.0f, 1.0f, 1.0f);
    m_velocity = Vector3::Zero;
    m_angularVelocity = Vector3::Zero;
    m_isSettled = false;
    m_emissiveColor = Vector3::Zero;
}

// ダイスを投擲（ロール）し、初速とランダムな回転運動を与える処理
void Dice::Roll(const Vector3& initialImpulse)
{
    // 投擲インパルスを適用（上方への跳ね上がりも加算）
    m_velocity = initialImpulse * 2.5f;
    m_velocity.y += 5.0f;

    // 各軸に対してランダムなランダム角速度を設定（バラバラに回るようにする）
    m_angularVelocity = Vector3(static_cast<float>(rand() % 31 - 15), static_cast<float>(rand() % 31 - 15),
                                static_cast<float>(rand() % 31 - 15));

    m_isSettled = false;

    // サイコロを振った瞬間の効果音再生
    SoundManager::GetInstance().PlaySE(SEType::DiceRoll);
}

// フレームごとの物理演算・位置姿勢更新
void Dice::Update(float dt)
{
    // 既に静止している場合は計算をスキップ
    if (m_isSettled)
    {
        return;
    }

    // 重力加速度の適用 (Y方向を下向きに減速)
    m_velocity.y -= 9.8f * dt;
    Vector3 nextPos = m_position + m_velocity * dt;
    float floorY = 1.0f; // 床面の高さ制限

    // 床面との衝突・バウンド処理
    if (nextPos.y <= floorY)
    {
        nextPos.y = floorY;

        // 床にめり込んでいる場合、反発係数をかけて上昇力へ変換
        if (m_velocity.y < 0.0f)
        {
            // ある程度のスピードでバウンドした時にも効果音を鳴らす
            if (std::abs(m_velocity.y) > 1.0f)
            {
                SoundManager::GetInstance().PlaySE(SEType::DiceRoll, 0.6f);
            }

            m_velocity.y = -m_velocity.y * 0.2f;
            if (std::abs(m_velocity.y) < 0.2f)
            {
                m_velocity.y = 0.0f; // 微小な跳ね返りは停止
            }
        }

        // 床面接触時の摩擦抵抗（水平移動速度および回転角速度の減衰）
        m_velocity.x *= 0.85f;
        m_velocity.z *= 0.85f;
        m_angularVelocity *= 0.85f;
    }

    m_position = nextPos;

    // 角速度による回転状態（クォータニオン）の更新
    if (m_angularVelocity.LengthSquared() > 0.001f)
    {
        Quaternion deltaRot = Quaternion::CreateFromYawPitchRoll(m_angularVelocity.y * dt, m_angularVelocity.x * dt,
                                                                 m_angularVelocity.z * dt);
        m_rotation = m_rotation * deltaRot;
        m_rotation.Normalize();
    }

    // 速度・角速度が十分に小さくなり、着地したと判定された場合の完全静止処理
    if (m_position.y <= floorY + 0.05f && m_velocity.LengthSquared() < 0.2f && m_angularVelocity.LengthSquared() < 0.2f)
    {
        m_velocity = Vector3::Zero;
        m_angularVelocity = Vector3::Zero;
        m_position.y = floorY;
        m_isSettled = true; // 完全停止状態に移行
    }
}

// 自身のトランスフォーム情報をもとにワールド行列を生成して描画
void Dice::Render(ID3D11DeviceContext* context, DX11::CommonStates& states, const Matrix& view,
                  const Matrix& proj) const
{
    Render(context, states, GetWorldMatrix(), view, proj);
}

// 指定されたワールド行列を用いてモデルを描画（自発光色の適用含む）
void Dice::Render(ID3D11DeviceContext* context, DX11::CommonStates& states, const Matrix& world, const Matrix& view,
                  const Matrix& proj) const
{
    if (!m_model)
        return;

    // モデルのマテリアルに現在の発光カラー（EmissiveColor）を反映
    m_model->UpdateEffects(
        [this](IEffect* effect)
        {
            if (auto basicEffect = dynamic_cast<BasicEffect*>(effect))
            {
                basicEffect->SetEmissiveColor(m_emissiveColor);
            }
        });

    // ジオメトリの描画実行
    m_model->Draw(context, states, world, view, proj);
}

// 現在のダイス姿勢における上面の数値を取得
int Dice::GetUpwardFace() const
{
    return GetTopFaceFromRotation(m_rotation);
}

// 任意の回転（クォータニオン）から、真上（ワールド空間+Y軸）を向いている出目を判定・計算するアルゴリズム
int Dice::GetTopFaceFromRotation(const Quaternion& rot) const
{
    // 各面のローカル方向ベクトルと出目のマッピング定義
    struct FaceVector
    {
        int number;       // 出目の値 (1～6)
        Vector3 localDir; // ダイス中心から各面へのローカル法線ベクトル
    };

    static const FaceVector faces[] = {
        {3, Vector3(0.0f, 1.0f, 0.0f)},  // 上面（+Y方向）
        {4, Vector3(0.0f, -1.0f, 0.0f)}, // 下面（-Y方向）
        {2, Vector3(0.0f, 0.0f, 1.0f)},  // 前面（+Z方向）
        {5, Vector3(0.0f, 0.0f, -1.0f)}, // 後面（-Z方向）
        {1, Vector3(1.0f, 0.0f, 0.0f)},  // 右面（+X方向）
        {6, Vector3(-1.0f, 0.0f, 0.0f)}, // 左面（-X方向）
    };

    Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);

    // ダイスの回転の逆クォータニオンを用いて、ワールド空間の「上方向(+Y)」をダイスのローカル空間座標へ逆変換
    Quaternion invRot;
    rot.Inverse(invRot);
    Vector3 localUp = Vector3::Transform(worldUp, invRot);

    float maxDot = -100.0f;
    int bestFace = 1;

    // ローカル上方向ベクトルと各面の法線ベクトルの内積（Dot Product）を計算し、最も一致度が高い面（内積最大）を選択
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