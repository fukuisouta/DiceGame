// -------------------------------------------------------------------
// 【定数バッファ】C++側から毎フレーム送られてくる行列データ
// -------------------------------------------------------------------
cbuffer SkyboxBuffer : register(b0)
{
    matrix WorldViewProjection; // ワールド変換 × ビュー変換 × 射影変換の合成行列
};

// -------------------------------------------------------------------
// 【リソース】キューブマップ画像（6面ぶんの空画像）とサンプラー（貼り付け方）
// -------------------------------------------------------------------
TextureCube SkyboxTexture : register(t0); // 3D用のキューブマップテクスチャ
SamplerState SamplerLinear : register(s0); // 画像を滑らかに拡大縮小する設定

// 頂点シェーダーに入力されるデータ（立方体の8つの角の座標）
struct VS_INPUT
{
    float3 position : POSITION;
};

// ピクセルシェーダーへ引き渡すデータ
struct PS_INPUT
{
    float4 position : SV_POSITION; // 画面上のピクセル位置
    float3 texCoord : TEXCOORD0; // キューブマップを参照するための方向ベクトル
};

// -------------------------------------------------------------------
// 【頂点シェーダー (VS)】各頂点の位置を計算する
// -------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    // 1. 行列を使って頂点座標を計算
    output.position = mul(float4(input.position, 1.0f), WorldViewProjection);

    // 2. ★超重要★ 奥行き(Z/W)を強制的に「1.0（一番奥）」にするテクニック
    // z成分をw成分と同じにすると、画面に反映される時に Z/W = 1.0 になり、絶対に画面の最遠に描画されます。
    output.position = output.position.xyww;

    // 3. 立方体の原点からのベクトルをそのままテクスチャの方向キーとして使う
    output.texCoord = input.position;

    return output;
}

// -------------------------------------------------------------------
// 【ピクセルシェーダー (PS)】画面上の1ピクセルごとに色を塗る
// -------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_TARGET
{
    // 方向ベクトル（texCoord）を使ってキューブマップから色を取得して返す
    return SkyboxTexture.Sample(SamplerLinear, input.texCoord);
}