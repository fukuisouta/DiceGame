#pragma once

#include <cstddef>
#include <cstdint>

//サーバー通信に関する設定パラメータ構造体
struct ServerNetworkConfig
{
	uint16_t port = 46860;			//サーバー待ち受けポート番号
	uint32_t targetFps = 60;		//サーバーの目標FPS（1秒間の更新回数）
	size_t maxBufferCapacity = 8192;//受信バッファの最大保持サイズ（バイト）
};

//ゲームのルール及び盤面に関する設定パラメータ構造体
struct BoardConfig
{
	int width = 9;		  // 盤面の横幅マス数
	int height = 8;		  // 盤面の縦幅マス数
	int minDiceValue = 1; //歩数ダイスの最小値
	int maxDiceValue = 3; //歩数ダイスの最大値
};