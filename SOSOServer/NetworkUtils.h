#pragma once

#include<string>
#include<vector>


//ネットワーク関連のユーティリティ関数群をまとめたクラス
class NetworkUtils
{
public:
	NetworkUtils() = delete;//インスタンス化を禁止（静的ユーティリティ）

	//~NetworkUtils();

	//ローカルマシンのIpv4アドレスリストを取得する関数
	//戻り値：取得されたIpアドレス文字列の配列
	static std::vector<std::string> GetLocalIPAddresses();

	//接続に利用するメインのローカルIPアドレスを一つ取得する関数
	// 戻り値：メインのIPアドレス文字列
	static std::string GetPrimaryLocalIP();

};

