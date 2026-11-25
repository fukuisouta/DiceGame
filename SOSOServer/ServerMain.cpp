#include "SOSOServerEngine.h"

int main()
{
	//SOSOServerEngineのインスタンス生成
	SOSOServerEngine server;

	//通信設定パラメータ
	ServerNetworkConfig config;
	config.port      = 46860;
	config.targetFps = 60;

	//サーバーの初期化及び起動
	if (server.Initialize(config))
	{
		//メインループの開始（プログラム終了まで実行）
		server.Run();
	}

	return 0;
}