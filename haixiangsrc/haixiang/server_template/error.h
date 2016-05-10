#pragma once
#include "error_define.h"
enum
{

	GAME_ERR_BANKER_CANT_BET = ERROR_USER_DEF_BEGIN,//庄家不能压注 100000
	GAME_ERR_CANT_BET_NOW = 100001,				//现在不能压注
	GAME_ERR_RESTORE_FAILED = 100002,			//恢复记录失败
	GAME_ERR_DELETE_LOCAL_FAILED = 100003,	//删除本地记录失败
	GAME_ERR_NOT_BANKER = 100004,					//找不到庄家
	GAME_ERR_CANT_FIND_GAME = 100005,			//找不到游戏
	GAME_ERR_CANT_FIND_CHIP_SET = 100006,	//找不到筹码设置
	GAME_ERR_WAITING_UPDATE = 100007,			//正在等待配置更新
	GAME_ERR_CONFIG_WRONG = 100008,				//配置错误
	GAME_ERR_BANKER_TURN_LESS = 100009,		//做庄次数不足
	GAME_ERR_LOSE_CAP_EXCEED = 100010,		//超出今日输上限
	GAME_ERR_WIN_CAP_EXCEED = 100011,			//超出今日赢上限
	GAME_ERR_BET_CAP_EXCEED = 100012,			//庄注上限
	GAME_ERR_CANT_BET_MORE = 100013,			//不能再押了
	GAME_ERR_CANT_SIT = 100014,		//位置上有人，不能坐下
	GAME_ERR_WRONG_IID = 100015,		//iid不对

};