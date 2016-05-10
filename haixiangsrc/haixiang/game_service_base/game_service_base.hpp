#pragma once
#include "game_service_base.h"

#include "platform_7158.h"
#include "platform_17173.h"
#include "platform_t58.h"
#include "platform_koko.h"

#include "msg_server_common.h"
#include "msg_client_common.h"
#include "telfee_match.h"

#include "error.h"

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
msg_base<boost::shared_ptr<world_socket>>::msg_ptr game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::create_world_msg(unsigned short cmd)
{
	msg_base<boost::shared_ptr<world_socket>>::msg_ptr ret_ptr;
	switch (cmd) {
		REGISTER_CLS_CREATE(msg_match_end);
		REGISTER_CLS_CREATE(msg_player_distribute);
		REGISTER_CLS_CREATE(msg_user_login_delegate_ret);
		REGISTER_CLS_CREATE(msg_koko_trade_inout_ret);
		REGISTER_CLS_CREATE(msg_common_reply_srv);
	}
	return ret_ptr;
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
int game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::run()
{
	world_connector_.reset(new world_socket(net_.ios_));
	the_config_.load_from_file("niuniu.xml");
	gamedb_.reset(new utf8_data_base(the_config_.db_host_, the_config_.db_user_, the_config_.db_pwd_, the_config_.db_name_, 0, the_config_.db_port_));

	if (!gamedb_->grabdb())
		return SYS_ERR_WORLDDB_CONN_FAIL;

#ifdef ALERT_EMAILS
	email_db_.reset(new utf8_data_base(the_config_.accdb_host_, the_config_.accdb_user_, the_config_.accdb_pwd_, the_config_.accdb_name_, 0, the_config_.accdb_port_));

	if (!email_db_->grabdb())
		return SYS_ERR_WORLDDB_CONN_FAIL;
#endif

	cache_helper_.account_cache_.ip_ = the_config_.cache_ip_;
	cache_helper_.account_cache_.port_ = the_config_.cache_port_;

	if (cache_helper_.account_cache_.reconnect() != ERROR_SUCCESS_0) {
		glb_log.write_log("warning: cache server is not valid!! %s, %d", the_config_.cache_ip_.c_str(),  the_config_.cache_port_ );
		return SYS_ERR_WORLDDB_CONN_FAIL;
	}
	boost::this_thread::sleep(boost::posix_time::milliseconds(500));
	on_run();

	std::string sql = "SELECT current_stock FROM server_parameters_stock_control";
	Query q(*gamedb_);
	if (q.get_result(sql) && q.fetch_row()){
		longlong curstock = q.getbigint();
		the_service.add_stock(curstock, true);
	}
	q.free_result();

	if (the_config_.world_ip_ != ""){
		int r = world_connector_->connect(the_config_.world_ip_, the_config_.world_port_, 20000);
		if (r != ERROR_SUCCESS_0) {
			return SYS_ERR_NET_INVALID;
		}
		else {
			msg_register_to_world msg;
			msg.gameid_ = the_config_.world_id_;
			send_msg(world_connector_, msg);
			glb_log.write_log("world server connected! %s:%d", the_config_.world_ip_.c_str(), the_config_.world_port_);
		}
	}

	net_.add_acceptor(the_config_.client_port_);
	system::error_code ec;
	if (!net_.run()) {
		return SYS_ERR_NET_INVALID;
	}
	else {
		glb_log.write_log("net service started at port %d.", the_config_.client_port_);
	}
	stoped_ = false;


	if (!the_config_.check_valid()) {
		glb_log.write_log("the_config_.check_valid() == false!");
		return SYS_ERR_UNKOWN_CMD;
	}

	sub_thread_.reset(new boost::thread(boost::bind(&this_type::sub_thread_update, this)));

	ptask.reset(new tast_on_5sec(timer_sv_));
	task_ptr task(ptask);
	ptask->schedule(5000);
	ptask->routine();
	return ERROR_SUCCESS_0;
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
int game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::cheat_win()
{
	int rate = 0;
	//glb_log.write_log("## stock_ =%lld stock_lowcap_=%lld stock_upcap_start_=%lld", stock_,the_service.the_new_config_.stock_lowcap_,
	//	the_service.the_new_config_.stock_upcap_start_);
	if (stock_ < the_service.the_new_config_.stock_lowcap_) {
		longlong		area = the_service.the_new_config_.stock_lowcap_ - the_service.the_new_config_.stock_lowcap_max_;
		if (area == 0) {
			area = 10000000;
		}
		longlong		n = the_service.the_new_config_.stock_lowcap_ - stock_;
		rate = int((n / float(area)) * 10000);
		if ((int)rand_r(10000) < rate)
			return 1;
	}
	else if (stock_ > the_service.the_new_config_.stock_upcap_start_) {
		longlong		area = the_service.the_new_config_.stock_upcap_max_ - the_service.the_new_config_.stock_upcap_start_;
		if (area == 0) {
			area = 100000000;
		}
		longlong		n = stock_ - the_service.the_new_config_.stock_upcap_start_;
		rate = int((n / float(area)) * 10000);
		if ((int)rand_r(10000) < rate)
			return -1;
	}
	return 0;
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
int game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::stop()
{
	stoped_ = true;
	if (sub_thread_.get())
		sub_thread_->join();

	auto cpy_l = task_info::glb_running_task_;
	auto it = cpy_l.begin();
	while (it != cpy_l.end()) {
		auto task = it->second; it++;
		auto ptask = task.lock();
		if (ptask.get()) {
			ptask->cancel();
		}
	}

	if (ptask.get()) {
		ptask->cancel();
		ptask.reset();
	}
	task_info::glb_running_task_.clear();
	world_connector_->close();
	world_connector_.reset();
	net_.stop();

	on_stop();

	for (unsigned int i = 0; i < the_golden_games_.size(); i++)
	{
		logic_t plogic = the_golden_games_[i];
		plogic->stop_logic();
	}
	the_golden_games_.clear();
#ifdef HAS_TELFEE_MATCH
	auto it_m = the_matches_.begin();
	while (it_m != the_matches_.end()) {
		match_t pmatch = it_m->second; it_m++;
		for (unsigned int i = 0; i < pmatch->vgames_.size(); i++)
		{
			pmatch->vgames_[i]->stop_logic();
		}
		pmatch->vgames_.clear();
	}
	the_matches_.clear();
#endif
	players_.clear();
	gamedb_.reset();
	cache_helper_.account_cache_.close();
	glb_http_log.stop_log();

	http_sv_.stop();
	timer_sv_.stop();

	timer_sv_.reset();
	timer_sv_.poll();

	http_sv_.reset();
	http_sv_.stop();
	return ERROR_SUCCESS_0;
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
void game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::main_thread_update()
{
	gamedb_->grabdb();
	boost::posix_time::ptime  plast_check = boost::posix_time::second_clock::local_time();
	boost::posix_time::ptime	pnow;
	time_t time_costs[10] = { 0 };
	while (!stoped_)
	{
		memset(time_costs, 0, 10 * sizeof(int));

		//一切必须建立在cache连接正常上
		boost::system::error_code ec;
		cache_helper_.account_cache_.ios_.reset();
		cache_helper_.account_cache_.ios_.poll(ec);
		if (cache_helper_.account_cache_.s_.get() && !cache_helper_.account_cache_.s_->is_connected_) {
			cache_helper_.account_cache_.keep_alive();
		}
		net_.ios_.reset();
		net_.ios_.poll(ec);

		{
			pnow = boost::posix_time::microsec_clock::local_time();
			std::vector<int> handled;
			process_msg(handled);
			time_costs[0] = (boost::posix_time::microsec_clock::local_time() - pnow).total_milliseconds();
			if (time_costs[0] > 500) {
				std::string aa;
				if (handled.size() < 20) {
					aa = combin_str(handled);
				}
				char* s = (char*)aa.c_str();
				glb_log.write_log("dispatch_msg cost %d ms!, msg_handled = %d, msgs:%s",
					(int)time_costs[0],
					(int)handled.size(),
					s);
			}
			if (ptask)
				ptask->msg_handled += handled.size();
		}
		{
			pnow = boost::posix_time::microsec_clock::local_time();
			update_palyers();
			time_costs[2] = (boost::posix_time::microsec_clock::local_time() - pnow).total_milliseconds();
			if (time_costs[2] > 500) {
				glb_log.write_log("update_palyers cost %d ms!", time_costs[2]);
			}
			if (ptask && (int)ptask->msg_recved < players_.size()) {
				ptask->msg_recved = players_.size();
			}
		}
		{

			http_sv_.reset();
			http_sv_.poll(ec);

			pnow = boost::posix_time::microsec_clock::local_time();

			bool all_game_waiting = true;
			if (wait_for_config_update_) {
				for (unsigned int i = 0; i < the_golden_games_.size(); i++)
				{
					if (!the_golden_games_[i]->is_waiting_config_) {
						all_game_waiting = false;
						break;
					}
				}
				if (all_game_waiting) {
					the_config_ = the_new_config_;
					if (!the_config_.shut_down_)
						wait_for_config_update_ = false;
				}
			}
#ifdef HAS_TELFEE_MATCH
			for (auto mat : the_matches_)
			{
				mat.second->update();
			}
#endif

			if (the_config_.shut_down_) {
				if (the_golden_games_.empty()) {
					stoped_ = true;
				}
			}

			on_main_thread_update();

			timer_sv_.reset();
			int n = timer_sv_.poll(ec);

			time_costs[3] = (boost::posix_time::microsec_clock::local_time() - pnow).total_milliseconds();
			if (time_costs[3] > 500) {
				glb_log.write_log("on_main_thread_update cost %d ms!", time_costs[3]);
			}
		}

		{
			pnow = boost::posix_time::microsec_clock::local_time();
			auto sec = pnow - plast_check;
			if (sec.total_seconds() > 5) {
				update_every_5sec();
				plast_check = pnow;
			}
			time_costs[4] = (boost::posix_time::microsec_clock::local_time() - pnow).total_milliseconds();
		}

		cache_helper_.handle_no_sync_msg();

		boost::posix_time::milliseconds ms(20);
		this_thread::sleep(ms);
	}
}

class		will_trade_out : public task_info
{
public:
	will_trade_out(asio::io_service& ios):task_info(ios)
	{

	}
	player_ptr pp;
	int		routine()
	{
		time_t t = the_service.cache_helper_.get_var<time_t>(pp->uid_ + KEY_ONLINE_LAST);
		if (::time(nullptr) - t > 2)	{
			the_service.auto_trade_out(pp);
			return task_info::routine_ret_break;
		}
		return task_info::routine_ret_continue;
	}
};

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
void game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::on_connection_lost(player_ptr p)
{
	glb_log.write_log("## on_connection_lost ");
	//玩家退出游戏就准备开始兑出货币
	//调用这个函数时,玩家已经从游戏中退出了,不用考虑玩家在游戏中的情况.
	will_trade_out*  out = new will_trade_out(the_service.timer_sv_);
	task_ptr task(out);
	out->pp = p;
	out->schedule(1000);
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
void game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::auto_trade_out(player_ptr p)
{
	if (p->platform_id_ == "7158") {
		platform_7158<this_type> plat(*this);
		plat.auto_trade_out(p);
	}
	else if (p->platform_id_ == "t58") {
		platform_t58<this_type> plat(*this);
		plat.auto_trade_out(p);
	}
	else if (p->platform_id_ == "koko"){
		platform_koko<this_type> plat(*this);
		plat.auto_trade_out(p);
	}
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
void game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::auto_trade_in(player_ptr p)
{
	if (p->platform_id_ == "7158") {
		platform_7158<this_type> plat(*this);
		plat.auto_trade_in(p);
	}
	else if (p->platform_id_ == "t58") {
		platform_t58<this_type> plat(*this);
		plat.auto_trade_in(p);
	}
	else if (p->platform_id_ == "koko"){
		platform_koko<this_type> plat(*this);
		plat.auto_trade_in(p);
	}
	else {
		msg_currency_change msg;
		msg.credits_ = (double)p->credits_;
		msg.why_ = msg_currency_change::why_trade_complete;
		auto psock = p->from_socket_.lock();
		if (psock.get()){
			send_msg(psock, msg);
		}
	}
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
void game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::broadcast_msg(msg_base<none_socket>& msg)
{
	auto it = players_.begin();
	while (it != players_.end())
	{
		player_ptr pp = it->second; it++;
		pp->send_msg_to_client(msg);
	}
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
void game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::update_every_5sec()
{
	the_config_.refresh();
	gamedb_->keep_alive();
	if (the_config_.shut_down_) {
		glb_log.write_log("there are %d games still running!", the_golden_games_.size());
	}

	longlong outc = 0;
	auto it_player = players_.begin();
	while (it_player != players_.end())
	{
		player_ptr pp = it_player->second;
		cache_helper_.give_currency(pp->uid_, 0, outc);
		it_player++;
	}

	//更新禁用的设备
	std::string sql = "select uid from forbidden";
	Query q(*gamedb_);
	std::string handled_uids;
	if (q.get_result(sql)) {
		while (q.fetch_row())
		{
			std::string fuid = q.getstr();
			player_ptr pfb = get_player(fuid);
			if (pfb.get()) {
				//保存禁止设备
				longlong ret = cache_helper_.set_var<longlong>(pfb->device_id_, -1, 1);
				auto itp = players_.begin();
				while (itp != players_.end())
				{
					player_ptr pp = itp->second;
					if (pp->device_id_ == pfb->device_id_) {
						//T出账玩家
						auto cnn = pp->from_socket_.lock();
						if (cnn.get()) {
							msg_logout msg;
							send_msg(cnn, msg, true);
							glb_log.write_log("forbidden device login,uid = %s, kickout!", pp->uid_.c_str());
						}
					}
					itp++;
				}

				if (handled_uids == "") {
					handled_uids += "'" + fuid + "'";
				}
				else {
					handled_uids += ",'" + fuid + "'";
				}
			}
		}
	}
	q.free_result();

	if (!handled_uids.empty()) {
		sql = "delete from forbidden where uid in(" + handled_uids + ")";
		q.execute(sql);
	}

	longlong current_stock = add_stock(0);
	if (current_stock){
		if (current_stock > the_config_.stock_lowcap_)	{
			add_stock(-the_config_.stock_decay_ / 1800);
		}
	}
	{
		BEGIN_UPDATE_TABLE("server_parameters_stock_control");
		SET_FINAL_VALUE("current_stock", current_stock);
		EXECUTE_NOREPLACE_DELAYED("", delay_db_game_);
	}

	if (!world_connector_->is_connected_){
		world_connector_.reset(new world_socket(net_.ios_));
		int r = world_connector_->connect(the_config_.world_ip_, the_config_.world_port_, 20000);
		if (r == ERROR_SUCCESS_0){
			msg_register_to_world msg;
			msg.gameid_ = the_config_.world_id_;
			send_msg(world_connector_, msg);
		}
	}
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
void game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::sub_thread_update()
{
	gamedb_->grabdb();
	delay_db_game_.set_db_ptr(gamedb_);
	boost::posix_time::ptime  plast_check = boost::posix_time::second_clock::local_time();
	while (!stoped_)
	{
		boost::posix_time::ptime  pnow = boost::posix_time::second_clock::local_time();
		on_sub_thread_update();
		delay_db_game_.pop_and_execute();
		auto sec = pnow - plast_check;
		if (sec.total_seconds() > 10) {
			gamedb_->keep_alive();
			plast_check = pnow;
		}
		boost::posix_time::milliseconds ms(50);
		this_thread::sleep(ms);
	}
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
int game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::update_palyers()
{
	int		busy = 0;
	auto it_player = players_.begin();
	while (it_player != players_.end())
	{
		player_ptr p = it_player->second;
		remote_socket_ptr pconn = p->from_socket_.lock();
		if (!pconn.get()) {
			if (!p->is_connection_lost_) {
				p->on_connection_lost();
			}
			if (p->is_connection_lost_) {
				bool is_timeout = false;//是否判断超时
#if(GAME_DDZ)

				boost::posix_time::ptime before = p->conn_lost_tick_;
				boost::posix_time::ptime now    = boost::posix_time::microsec_clock::local_time();

				auto delta = now - before;
				is_timeout = delta > boost::posix_time::seconds(10);
#endif
				//1.游戏logic已经删除
				//2.掉线超时（复制该玩家并设置为机器人到相对应的位置）TODO：还没有处理掉线之后钱该怎么扣
				if (!p->the_game_.lock().get() || is_timeout ) {
					on_connection_lost(p);
#if(GAME_DDZ)
					boost::shared_ptr<ddz_logic> plogic = p->the_game_.lock();
					if(plogic){
						player_ptr new_player = p->clone();
						new_player->is_bot_ = true;
						auto it = std::find_if(plogic->is_playing_,plogic->is_playing_+MAX_SEATS,[&](player_ptr p)->bool{
							return p != NULL && p->uid_ == new_player->uid_;
						});
						if(it != plogic->is_playing_+MAX_SEATS){
							*it = new_player;
						}
					}
#endif
					it_player = players_.erase(it_player); //丢失连接不应马上删除玩家,30秒内不重连则删除
					continue;
				}
			}
		}
		else if (pconn.get()) {
			if (!pconn->is_authorized() &&
				(boost::posix_time::microsec_clock::local_time() - pconn->security_check_).total_seconds() > 2) {
				pconn->close();
			}
		}
		it_player++;
	}
	return busy;
}

template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
int game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::process_msg(std::vector<int>& handled)
{
	std::vector<remote_socket_ptr> remotes = net_.get_remotes();
	for (auto it = remotes.begin(); it < remotes.end(); it++)
	{
		remote_socket_ptr rmt_sock = *it;

		char a[24] = { 0 };
		rmt_sock->pickup_data(a, 7, false);
		if (strcmp(a, "<policy") == 0)
		{
			rmt_sock->pickup_data(a, 23, true);
			char* policy_define = new char[1024];
			boost::shared_array<char> buf(policy_define);
			memset(policy_define, 0, 1024);
			sprintf_s(policy_define, 1024, "%s",
				"<?xml version=\"1.0\"?><cross-domain-policy><site-control permitted-cross-domain-policies=\"all\"/><allow-access-from domain=\"*\" to-ports=\"*\"/></cross-domain-policy>\0");
			//(int)the_config_.client_port_,
			//"\" /></cross-domain-policy>\0");
			rmt_sock->add_to_send_queue(stream_buffer(buf, strlen(policy_define) + 1, strlen(policy_define) + 1), true);
			continue;
		}
		msg_ptr msg = pickup_msg_from_socket(rmt_sock, boost::bind(&this_type::create_msg, this, _1));
		unsigned short n = 0, handled_count = 0;
		while (msg.get() && handled_count < 50)
		{
			handled_count++;
			//如果发过来未经授权的请求，
			msg_authrized<player_ptr> * pauth = dynamic_cast<msg_authrized<player_ptr>*>(msg.get());
			if (pauth && !rmt_sock->is_authorized()) {
				glb_log.write_log("msg is not authorized, cmd = %d", msg->head_.cmd_);
				rmt_sock->close();
				break;
			}
			int	r = handle_msg(msg, rmt_sock);
			if (r != ERROR_SUCCESS_0) {
				msg_common_reply<none_socket> msg_rpl;
				msg_rpl.rp_cmd_ = msg->head_.cmd_;
				msg_rpl.err_ = r;
				send_msg(rmt_sock, msg_rpl);
			}
			handled.push_back(msg->head_.cmd_);
			msg = pickup_msg_from_socket(rmt_sock, boost::bind(&game_service_base::create_msg, this, _1));
		}
	}

	auto msg = pickup_msg_from_socket(world_connector_, boost::bind(&game_service_base::create_world_msg, this, _1));
	while (msg.get())
	{
		int	r = msg->handle_this();
		msg = pickup_msg_from_socket(world_connector_, boost::bind(&game_service_base::create_world_msg, this, _1));
	}
	return ERROR_SUCCESS_0;
}
template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
boost::shared_ptr<player_t> game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::get_player_in_game( string uid )
{
	for (unsigned int i = 0 ; i < the_golden_games_.size(); i++)
	{
		for (int j = 0; j < MAX_SEATS; j++)
		{
			if (!the_golden_games_[i]->is_playing_[j].get()) continue;
			if(the_golden_games_[i]->is_playing_[j]->uid_ == uid){
				return the_golden_games_[i]->is_playing_[j];
			}
		}
	}
	return boost::shared_ptr<player_t>();
}
template<class config_t, class player_t, class socket_t, class logic_t, class match_t, class sceneset_t>
match_ptr		game_service_base<config_t, player_t, socket_t, logic_t, match_t, sceneset_t>::create_match(int tpid) {return match_ptr();}