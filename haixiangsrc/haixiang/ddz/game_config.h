#pragma once
#include "boost/shared_ptr.hpp"
#include "msg.h"

#include "game_player.h"
#include "game_logic.h"
#include "service.h"

typedef		boost::shared_ptr<remote_socket<ddz_player>> remote_socket_ptr;
extern		ddz_service  the_service;

typedef		ddz_player		game_player_type;
typedef		ddz_logic		game_logic_type;
typedef		ddz_service  service_type;
#define		EXTRA_LOGIN_CODE()

#define     MAX_USER 5000
//服务器参数信息
class msg_server_parameter : public msg_base<none_socket>
{
public:
	int					min_guarantee_;	//参加游戏最低货币限制
	int					is_free_;				//使用的是免费的货币
	int					banker_set_;		//抢庄保证金
	int					low_cap_;				//底注
	int					player_tax_;		//手续费
	int					enter_scene_;		//是否进入场次信息。1是，0服务器参数，2， 开始 3服务器参数配置结束
	msg_server_parameter()
	{
		head_.cmd_ = 1107;//GET_CLSID(msg_server_parameter);
		enter_scene_ = 0;
	}

	int			write(char*& data_s, unsigned int l)
	{
		msg_base::write(data_s, l);
		write_data(min_guarantee_, data_s);
		write_data(is_free_, data_s);
		write_data(banker_set_, data_s);
		write_data(low_cap_, data_s);
		write_data(player_tax_, data_s);
		write_data(enter_scene_, data_s);
		return 0;
	}
};

#define	 SEND_SERVER_PARAM()\
for (unsigned int i = 0; i < the_service.scenes_.size(); i++)\
{\
	scene_set& sc = the_service.scenes_[i];\
	msg_server_parameter msg;\
	msg.is_free_ = sc.is_free_;\
	msg.min_guarantee_ = (int)sc.gurantee_set_;\
	msg.banker_set_ = (int)sc.to_banker_set_;\
	msg.low_cap_ = (int)(sc.gurantee_set_ - sc.player_tax_) / 50;\
	msg.player_tax_ = sc.player_tax_;\
	if (i == 0){\
		msg.enter_scene_ = 2;\
	}\
	else if (i == the_service.scenes_.size() - 1){\
		msg.enter_scene_ = 3;\
	}\
	else{\
		msg.enter_scene_ = 0;\
	}\
	send_msg(from_sock_, msg);\
}