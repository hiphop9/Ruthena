// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#pragma warning(disable:4800)
#include "login.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

#include <common/cli.hpp>
#include <common/core.hpp>
#include <common/malloc.hpp>
#include <common/md5calc.hpp>
#include <common/mmo.hpp>
#include <common/msg_conf.hpp>
#include <common/random.hpp>
#include <common/showmsg.hpp>
#include <common/socket.hpp> //ip2str
#include <common/strlib.hpp>
#include <common/timer.hpp>
#include <common/utilities.hpp>
#include <common/utils.hpp>
#include <config/core.hpp>
#include <config/custom.hpp>

#include "account.hpp"
#include "ipban.hpp"
#include "loginchrif.hpp"
#include "loginclif.hpp"
#include "logincnslif.hpp"
#include "loginlog.hpp"

using namespace rathena;
using namespace rathena::server_login;

#define LOGIN_MAX_MSG 30				/// Max number predefined in msg_conf
static char* msg_table[LOGIN_MAX_MSG];	/// Login Server messages_conf

//definition of exported var declared in header
struct mmo_char_server ch_server[MAX_SERVERS];	/// char server data
struct Login_Config login_config;				/// Configuration of login-serv
std::unordered_map<uint32,struct online_login_data> online_db;
std::unordered_map<uint32,struct auth_node> auth_db;

// account database
AccountDB* accounts = nullptr;
// Advanced subnet check [LuzZza]
struct s_subnet {
	uint32 mask;
	uint32 char_ip;
	uint32 map_ip;
} subnet[16];
int subnet_count = 0; //number of subnet config

int login_fd; // login server file descriptor socket

//early declaration
bool login_check_password( struct login_session_data& sd, struct mmo_account& acc );

///Accessors
AccountDB* login_get_accounts_db(void){
	return accounts;
}

// Console Command Parser [Wizputer]
//FIXME to be remove (moved to cnslif / will be done once map/char/login, all have their cnslif interface ready)
int parse_console(const char* buf){
	return cnslif_parse(buf);
}

struct online_login_data* login_get_online_user( uint32 account_id ){
	return util::umap_find( online_db, account_id );
}

/**
 * Receive info from char-serv that this user is online
 * This function will start a timer to recheck if that user still online
 * @param char_server : Serv id where account_id is connected
 * @param account_id : aid connected
 * @return the new online_login_data for that user
 */
struct online_login_data* login_add_online_user(int char_server, uint32 account_id){
	struct online_login_data* p = login_get_online_user( account_id );

	if( p == nullptr ){
		// Allocate the player
		p = &online_db[account_id];

		p->account_id = account_id;
		p->char_server = char_server;
		p->waiting_disconnect = INVALID_TIMER;
	}else{
		p->char_server = char_server;

		if( p->waiting_disconnect != INVALID_TIMER ){
			delete_timer( p->waiting_disconnect, login_waiting_disconnect_timer );
			p->waiting_disconnect = INVALID_TIMER;
		}
	}

	accounts->enable_webtoken( accounts, account_id );

	return p;
}

/**
 * Received info from char serv that the account_id is now offline
 * remove the user from online_db
 *  Checking if user was already scheduled for deletion, and remove that timer if found.
 * @param account_id : aid to remove from db
 */
void login_remove_online_user(uint32 account_id) {
	struct online_login_data* p = login_get_online_user( account_id );

	if( p == nullptr ){
		return;
	}

	if( p->waiting_disconnect != INVALID_TIMER ){
		delete_timer( p->waiting_disconnect, login_waiting_disconnect_timer );
	}

	accounts->disable_webtoken( accounts, account_id );

	online_db.erase( account_id );
}

struct auth_node* login_get_auth_node( uint32 account_id ){
	return util::umap_find( auth_db, account_id );
}

struct auth_node* login_add_auth_node( struct login_session_data* sd, uint32 ip ){
	struct auth_node* node = &auth_db[sd->account_id];

	node->account_id = sd->account_id;
	node->login_id1 = sd->login_id1;
	node->login_id2 = sd->login_id2;
	node->sex = sd->sex;
	node->ip = ip;
	node->clienttype = sd->clienttype;

	return node;
}

void login_remove_auth_node( uint32 account_id ){
	auth_db.erase( account_id );
}

/**
 * Timered function to disconnect a user from login.
 *  This is done either after auth_ok or kicked by char-server.
 *  Removing user from auth_db and online_db.
 *  Delay is AUTH_TIMEOUT by default.
 * @param tid: timer id
 * @param tick: tick of execution
 * @param id: user account id
 * @param data: unused
 * @return :0
 */
TIMER_FUNC(login_waiting_disconnect_timer){
	struct online_login_data* p = login_get_online_user( id );

	if( p != nullptr && p->waiting_disconnect == tid && p->account_id == id ){
		p->waiting_disconnect = INVALID_TIMER;
		login_remove_online_user(id);
		login_remove_auth_node(id);
	}

	return 0;
}

void login_online_db_setoffline( int char_server ){
	for( std::pair<uint32,struct online_login_data> pair : online_db ){
		if( char_server == -1 ){
			pair.second.char_server = -1;

			if( pair.second.waiting_disconnect != INVALID_TIMER ){
				delete_timer( pair.second.waiting_disconnect, login_waiting_disconnect_timer );
				pair.second.waiting_disconnect = INVALID_TIMER;
			}
		}else if( pair.second.char_server == char_server ){
			// Char server disconnected.
			pair.second.char_server = -2;
		}
	}
}

/**
 * Timered function to check if user is still connected.
 *  Launches every 600s by default.
 * @param tid: timer id
 * @param tick: tick of execution
 * @param id: unused
 * @param data: unused
 * @return : 0
 */
static TIMER_FUNC(login_online_data_cleanup){
	for( std::pair<uint32,struct online_login_data> pair : online_db  ){
		// Unknown server.. set them offline
		if( pair.second.char_server == -2 ){
			login_remove_online_user( pair.first );
		}
	}

	return 0;
}

/**
 * Create a new account and save it in db/sql.
 * @param userid: string for user login
 * @param pass: string for user pass
 * @param sex: should be M|F|S (todo make an enum ?)
 * @param last_ip:
 * @return :
 *	-1: success
 *	0: unregistered id (wrong sex fail to create in db);
 *	1: incorrect pass or userid (userid|pass too short or already exist);
 *	3: registration limit exceeded;
 */
int login_mmo_auth_new(const char* userid, const char* pass, const char sex, const char* last_ip) {
	static int num_regs = 0; // registration counter
	static t_tick new_reg_tick = 0;
	t_tick tick = gettick();
	struct mmo_account acc;

	//Account Registration Flood Protection by [Kevin]
	if( new_reg_tick == 0 )
		new_reg_tick = gettick();
	if( DIFF_TICK(tick, new_reg_tick) < 0 && num_regs >= login_config.allowed_regs ) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowNotice("계정등록이 거부되었습니다. (등록제한 초과)\n");
		else
			ShowNotice("Account registration denied (registration limit exceeded)\n");
		return 3;
	}

	if( strlen(userid) < login_config.acc_name_min_length || strlen(pass) < login_config.password_min_length)
		return 1;

	// check for invalid inputs
	if( sex != 'M' && sex != 'F' )
		return 0; // 0 = Unregistered ID

	// check if the account doesn't exist already
	if( accounts->load_str(accounts, &acc, userid) ) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowNotice("생성하려는 계정이 이미 존재합니다. (계정명: %s, 성별: %c)\n", userid, sex);
		else
			ShowNotice("Attempt of creation of an already existant account (account: %s, sex: %c)\n", userid, sex);
		return 1; // 1 = Incorrect Password
	}

	memset(&acc, '\0', sizeof(acc));
	acc.account_id = -1; // assigned by account db
	safestrncpy(acc.userid, userid, sizeof(acc.userid));
	safestrncpy(acc.pass, pass, sizeof(acc.pass));
	acc.sex = sex;
	safestrncpy(acc.email, "a@a.com", sizeof(acc.email));
	acc.expiration_time = ( login_config.start_limited_time != -1 ) ? time(nullptr) + login_config.start_limited_time : 0;
	safestrncpy(acc.lastlogin, "", sizeof(acc.lastlogin));
	safestrncpy(acc.last_ip, last_ip, sizeof(acc.last_ip));
	safestrncpy(acc.birthdate, "", sizeof(acc.birthdate));
	safestrncpy(acc.pincode, "", sizeof(acc.pincode));
	acc.pincode_change = 0;
	acc.char_slots = MIN_CHARS;
#ifdef VIP_ENABLE
	acc.vip_time = 0;
	acc.old_group = 0;
#endif
	if( !accounts->create(accounts, &acc) )
		return 0;

	if(strcmpi(MessageLang,"KOR") == 0)
		ShowNotice("계정이 생성되었습니다. (계정명: %s(%d), 성별: %c)\n", acc.userid, acc.account_id, acc.sex);
	else
		ShowNotice("Account creation (account %s, id: %d, sex: %c)\n", acc.userid, acc.account_id, acc.sex);

	if( DIFF_TICK(tick, new_reg_tick) > 0 ) {// Update the registration check.
		num_regs = 0;
		new_reg_tick = tick + login_config.time_allowed*1000;
	}
	++num_regs;

	return -1;
}

/**
 * Check/authentication of a connection.
 * @param sd: string (atm:md5key or dbpass)
 * @param isServer: string (atm:md5key or dbpass)
 * @return :
 *	-1: success
 *	0: unregistered id;
 *	1: incorrect pass;
 *	2: expired id
 *	3: blacklisted (or registration limit exceeded if new acc);
 *	5: invalid client_version|hash;
 *	6: banned
 *	x: acc state (TODO document me deeper)
 */
int login_mmo_auth(struct login_session_data* sd, bool isServer) {
	struct mmo_account acc;

	char ip[16];
	ip2str(session[sd->fd]->client_addr, ip);

	// DNS Blacklist check
	if( login_config.use_dnsbl ) {
		char r_ip[16];
		char ip_dnsbl[256];
		char* dnsbl_serv;
		uint8* sin_addr = (uint8*)&session[sd->fd]->client_addr;

		sprintf(r_ip, "%u.%u.%u.%u", sin_addr[0], sin_addr[1], sin_addr[2], sin_addr[3]);

		for( dnsbl_serv = strtok(login_config.dnsbl_servs,","); dnsbl_serv != nullptr; dnsbl_serv = strtok(nullptr,",") ) {
			sprintf(ip_dnsbl, "%s.%s", r_ip, trim(dnsbl_serv));
			if( host2ip(ip_dnsbl) ) {
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowInfo("DNSBL: (%s)은(는) 블랙리스트입니다. 사용자를 추방했습니다.\n", r_ip);
				else		
					ShowInfo("DNSBL: (%s) Blacklisted. User Kicked.\n", r_ip);
				return 3;
			}
		}

	}

	size_t len = strnlen(sd->userid, NAME_LENGTH);

	// Account creation with _M/_F
	if( login_config.new_account_flag ) {
		if( len > 2 && strnlen(sd->passwd, NAME_LENGTH) > 0 && // valid user and password lengths
			sd->userid[len-2] == '_' && memchr("FfMm", sd->userid[len-1], 4) ) // _M/_F suffix
		{
			// Encoded password
			if( sd->passwdenc != 0 ){
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowError( "계정 '%s'은(는) 클라이언트측의 비밀번호 암호화의 활성화로 인하여 생성이 실패 했습니다. \n", sd->userid );
				else
					ShowError( "Account '%s' could not be created because client side password encryption is enabled.\n", sd->userid );
				return 0; // unregistered id
			}

			int result;
			// remove the _M/_F suffix
			len -= 2;
			sd->userid[len] = '\0';

			result = login_mmo_auth_new(sd->userid, sd->passwd, TOUPPER(sd->userid[len+1]), ip);
			if( result != -1 )
				return result;// Failed to make account. [Skotlex].
		}
	}

	if( !accounts->load_str(accounts, &acc, sd->userid) ) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowNotice("알수 없는 계정입니다. (계정명: %s, IP: %s)\n", sd->userid, ip);
		else
			ShowNotice("Unknown account (account: %s, ip: %s)\n", sd->userid, ip);
		return 0; // 0 = Unregistered ID
	}

	if( !isServer && sex_str2num( acc.sex ) == SEX_SERVER ){
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowWarning( "연결이 거부된: ip %s 가 서버 계정에 로그인하려고 했습니다 '%s'\n", ip, sd->userid );
		else
			ShowWarning( "Connection refused: ip %s tried to log into server account '%s'\n", ip, sd->userid );
		return 0; // 0 = Unregistered ID
	}

	if( !login_check_password( *sd, acc ) ) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowNotice("(계정: '%s', ip: %s)잘못된 비밀번호 입니다.\n", sd->userid, ip);
		else
			ShowNotice("Invalid password (account: '%s', ip: %s)\n", sd->userid, ip);
		return 1; // 1 = Incorrect Password
	}

	if( acc.expiration_time != 0 && acc.expiration_time < time(NULL) ) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowNotice("연결이 거부되었습니다. (계정명: %s, 만료된 ID, IP: %s)\n", sd->userid, ip);
		else
			ShowNotice("Connection refused (account: %s, expired ID, ip: %s)\n", sd->userid, ip);
		return 2; // 2 = This ID is expired
	}

	if( acc.unban_time != 0 && acc.unban_time > time(nullptr) ) {
		char tmpstr[24];
		timestamp2string(tmpstr, sizeof(tmpstr), acc.unban_time, login_config.date_format);
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowNotice("연결이 거부되었습니다. (계정명: %s, %s까지 밴(추방), IP: %s)\n", sd->userid, tmpstr, ip);
		else
			ShowNotice("Connection refused (account: %s, banned until %s, ip: %s)\n", sd->userid, tmpstr, ip);
		return 6; // 6 = Your are Prohibited to log in until %s
	}

	if( acc.state != 0 ) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowNotice("연결이 거부되었습니다. (계정명: %s, 상태: %d, IP: %s)\n", sd->userid, acc.state, ip);
		else
			ShowNotice("Connection refused (account: %s, state: %d, ip: %s)\n", sd->userid, acc.state, ip);
		return acc.state - 1;
	}

	if( login_config.client_hash_check && !isServer ) {
		struct client_hash_node *node = nullptr;
		bool match = false;

		for( node = login_config.client_hash_nodes; node; node = node->next ) {
			if( acc.group_id < node->group_id )
				continue;
			if( *node->hash == '\0' // Allowed to login without hash
			 || (sd->has_client_hash && memcmp(node->hash, sd->client_hash, 16) == 0 ) // Correct hash
			) {
				match = true;
				break;
			}
		}

		if( !match ) {
			char smd5[33];
			int i;

			if( !sd->has_client_hash ) {
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowNotice("클라이언트가 클라이언트 해시값을 보내지 않았습니다. (계정명: %s, ip: %s)\n", sd->userid, ip);
				else	
					ShowNotice("Client didn't send client hash (account: %s, ip: %s)\n", sd->userid, ip);
				return 5;
			}

			for( i = 0; i < 16; i++ )
				sprintf(&smd5[i * 2], "%02x", sd->client_hash[i]);

			if(strcmpi(MessageLang,"KOR") == 0)
				ShowNotice("유효하지 않은 클라이언트 해시값 (계정명: %s, 보낸 암호화값(md5): %d, IP: %s)\n", sd->userid, smd5, ip);
			else
				ShowNotice("Invalid client hash (account: %s, sent md5: %s, ip: %s)\n", sd->userid, smd5, ip);
			return 5;
		}
	}

	if(strcmpi(MessageLang,"KOR") == 0)
		ShowNotice("인증을 되었습니다. (계정명: %s(%d), ip: %s)\n", sd->userid, acc.account_id, ip);
	else
		ShowNotice("Authentication accepted (account: %s, id: %d, ip: %s)\n", sd->userid, acc.account_id, ip);

	// update session data
	sd->account_id = acc.account_id;
	sd->login_id1 = rnd_value(1u, UINT32_MAX);
	sd->login_id2 = rnd_value(1u, UINT32_MAX);
	safestrncpy(sd->lastlogin, acc.lastlogin, sizeof(sd->lastlogin));
	sd->sex = acc.sex;
	sd->group_id = acc.group_id;

	// update account data
	timestamp2string(acc.lastlogin, sizeof(acc.lastlogin), time(nullptr), "%Y-%m-%d %H:%M:%S");
	safestrncpy(acc.last_ip, ip, sizeof(acc.last_ip));
	acc.unban_time = 0;
	acc.logincount++;
	accounts->save(accounts, &acc, true);

	if( login_config.use_web_auth_token ){
		safestrncpy( sd->web_auth_token, acc.web_auth_token, WEB_AUTH_TOKEN_LENGTH );
	}

	if( sd->sex != 'S' && sd->account_id < START_ACCOUNT_NUM ) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowWarning("%s 계정이 %d인 계정아이디를 가지려 합니다. 계정이 제대로 작동하려면 계정ID가 %d 이상이어야합니다.\n", sd->userid, sd->account_id, START_ACCOUNT_NUM);
		else
			ShowWarning("Account %s has account id %d! Account IDs must be over %d to work properly!\n", sd->userid, sd->account_id, START_ACCOUNT_NUM);
	}
	return -1; // account OK
}

/**
 * Verify if a password is correct.
 * @param md5key: md5key of client
 * @param passwdenc: encode key of client
 * @param passwd: pass to check
 * @param refpass: pass register in db
 * @return true if matching else false
 */
bool login_check_password( struct login_session_data& sd, struct mmo_account& acc ){
	if( sd.passwdenc == 0 ){
		return 0 == strcmp( sd.passwd, acc.pass );
	}

	// password mode set to 1 -> md5(md5key, refpass) enable with <passwordencrypt></passwordencrypt>
	if( sd.passwdenc & 0x01 ){
		std::string pwd;

		pwd.append( sd.md5key, sd.md5keylen );
		pwd.append( acc.pass );

		char md5str[32 + 1];

		MD5_String( pwd.c_str(), md5str );

		if( 0 == strcmp( sd.passwd, md5str ) ){
			return true;
		}
	}

	// password mode set to 2 -> md5(refpass, md5key) enable with <passwordencrypt2></passwordencrypt2>
	if( sd.passwdenc & 0x02 ){
		std::string pwd;

		pwd.append( acc.pass );
		pwd.append( sd.md5key, sd.md5keylen );

		char md5str[32 + 1];

		MD5_String( pwd.c_str(), md5str );

		if( 0 == strcmp( sd.passwd, md5str ) ){
			return true;
		}
	}

	return false;
}

int login_get_usercount( int users ){
#if PACKETVER >= 20170726
	if( login_config.usercount_disable ){
		return 4; // Removes count and colorization completely
	}else if( users <= login_config.usercount_low ){
		return 0; // Green => Smooth
	}else if( users <= login_config.usercount_medium ){
		return 1; // Yellow => Normal
	}else if( users <= login_config.usercount_high ){
		return 2; // Red => Busy
	}else{
		return 3; // Purple => Crowded
	}
#else
	return users;
#endif
}

/**
 * Test to determine if an IP come from LAN or WAN.
 * @param ip: ip to check if in auth network
 * @return 0 if from wan, or subnet_char_ip if lan
 */
int lan_subnetcheck(uint32 ip) {
	int i;
	ARR_FIND( 0, subnet_count, i, (subnet[i].char_ip & subnet[i].mask) == (ip & subnet[i].mask) );
	return ( i < subnet_count ) ? subnet[i].char_ip : 0;
}




/// Msg_conf tayloring
int login_msg_config_read(char *cfgName){
	return _msg_config_read(cfgName,LOGIN_MAX_MSG,msg_table);
}
const char* login_msg_txt(int msg_number){
	return _msg_txt(msg_number,LOGIN_MAX_MSG,msg_table);
}
void login_do_final_msg(void){
	_do_final_msg(LOGIN_MAX_MSG,msg_table);
}




/// Set and read Configurations

/**
 * Reading Lan Support configuration.
 * @param lancfgName: Name of the lan configuration (could be fullpath)
 * @return 0:success, 1:failure (file not found|readable)
 */
int login_lan_config_read(const char *lancfgName) {
	FILE *fp;
	int line_num = 0, s_subnet=ARRAYLENGTH(subnet);
	char line[1024], w1[64], w2[64], w3[64], w4[64];

	if((fp = fopen(lancfgName, "r")) == NULL) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowWarning("LAN지원 설정파일인 %s을(를) 찾을 수 없습니다.\n", lancfgName);
		else
			ShowWarning("LAN Support configuration file is not found: %s\n", lancfgName);
		return 1;
	}

	while(fgets(line, sizeof(line), fp))
	{
		line_num++;
		if ((line[0] == '/' && line[1] == '/') || line[0] == '\n' || line[1] == '\n')
			continue;

		if(sscanf(line,"%63[^:]: %63[^:]:%63[^:]:%63[^\r\n]", w1, w2, w3, w4) != 4)
		{
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowWarning("설정파일 %s내 %d줄의 구문이 오류입니다.\n", lancfgName, line_num);
			else	
				ShowWarning("Error syntax of configuration file %s in line %d.\n", lancfgName, line_num);
			continue;
		}

		if( strcmpi(w1, "subnet") == 0 ){
			if(subnet_count>=s_subnet) { //We skip instead of break in case we want to add other conf in that file.
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowError("%s: 너무 많은 서브넷이 정의되어 %d줄을 건너 뜁니다...\n", lancfgName, line_num);
				else
					ShowError("%s: Too many subnets defined, skipping line %d...\n", lancfgName, line_num);
				continue;
			}
			subnet[subnet_count].mask = str2ip(w2);
			subnet[subnet_count].char_ip = str2ip(w3);
			subnet[subnet_count].map_ip = str2ip(w4);

			if( (subnet[subnet_count].char_ip & subnet[subnet_count].mask) != (subnet[subnet_count].map_ip & subnet[subnet_count].mask) )
			{
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowError("%s: 설정 오류: 케릭터 서버(%s)와 맵서버(%s)가 서로 다른 서브 네트워크에 있습니다.\n", lancfgName, w3, w4);
				else
					ShowError("%s: Configuration Error: The char server (%s) and map server (%s) belong to different subnetworks!\n", lancfgName, w3, w4);
				continue;
			}

			subnet_count++;
		}
	}

	if( subnet_count > 1 ) { /* only useful if there is more than 1 available */
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowStatus("%d개의 서브 네트워크에 대한 정보를 읽습니다.\n", subnet_count);
		else
			ShowStatus("Read information about %d subnetworks.\n", subnet_count);
	}
	fclose(fp);
	return 0;
}

/**
 * Reading main configuration file.
 * @param cfgName: Name of the configuration (could be fullpath)
 * @param normal: Config read normally when server started
 * @return True:success, Fals:failure (file not found|readable)
 */
bool login_config_read(const char* cfgName, bool normal) {
	char line[1024], w1[32], w2[1024];
	FILE* fp = fopen(cfgName, "r");
	if (fp == NULL) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowError("설정파일 %s을(를) 찾을 수 없습니다.\n", cfgName);
		else
			ShowError("Configuration file (%s) not found.\n", cfgName);
		return false;
	}
	while(fgets(line, sizeof(line), fp)) {
		if (line[0] == '/' && line[1] == '/')
			continue;

		if (sscanf(line, "%31[^:]: %1023[^\r\n]", w1, w2) < 2)
			continue;

		// Config that loaded only when server started, not by reloading config file
		if (normal) {
			if( !strcmpi(w1, "bind_ip") ) {
				login_config.login_ip = host2ip(w2);
				if( login_config.login_ip ) {
					char ip_str[16];
					if(strcmpi(MessageLang,"KOR") == 0)
						ShowStatus("로그인 서버 바인딩 IP 주소 : %s -> %s\n", w2, ip2str(login_config.login_ip, ip_str));
					else		
						ShowStatus("Login server binding IP address : %s -> %s\n", w2, ip2str(login_config.login_ip, ip_str));
				}
			}
			else if( !strcmpi(w1, "login_port") )
				login_config.login_port = (uint16)atoi(w2);
			else if(!strcmpi(w1, "console"))
				login_config.console = (bool)config_switch(w2);
		}

		if(!strcmpi(w1,"timestamp_format"))
			safestrncpy(timestamp_format, w2, 20);
		else if(strcmpi(w1,"db_path")==0)
			safestrncpy(db_path, w2, ARRAYLENGTH(db_path));
		else if(!strcmpi(w1,"stdout_with_ansisequence"))
			stdout_with_ansisequence = config_switch(w2);
		else if(!strcmpi(w1,"console_silent")) {
			msg_silent = atoi(w2);
			if( msg_silent ) { /* only bother if we actually have this enabled */
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowInfo("콘솔 자동 설정: %d\n", atoi(w2));
				else		
					ShowInfo("Console Silent Setting: %d\n", atoi(w2));
			}
		}
		else if (strcmpi(w1, "console_msg_log") == 0)
			console_msg_log = atoi(w2);
		else if  (strcmpi(w1, "console_log_filepath") == 0)
			safestrncpy(console_log_filepath, w2, sizeof(console_log_filepath));
		else if(!strcmpi(w1, "log_login"))
			login_config.log_login = (bool)config_switch(w2);
		else if(!strcmpi(w1, "new_account"))
			login_config.new_account_flag = (bool)config_switch(w2);
		else if(!strcmpi(w1, "acc_name_min_length"))
			login_config.acc_name_min_length = cap_value(atoi(w2), 0, NAME_LENGTH - 1);
		else if(!strcmpi(w1, "password_min_length"))
			login_config.password_min_length = cap_value(atoi(w2), 0, PASSWD_LENGTH - 1);
		else if(!strcmpi(w1, "start_limited_time"))
			login_config.start_limited_time = atoi(w2);
		else if(!strcmpi(w1, "use_MD5_passwords"))
			login_config.use_md5_passwds = (bool)config_switch(w2);
		else if(!strcmpi(w1, "group_id_to_connect"))
			login_config.group_id_to_connect = atoi(w2);
		else if(!strcmpi(w1, "min_group_id_to_connect"))
			login_config.min_group_id_to_connect = atoi(w2);
		else if(!strcmpi(w1, "date_format"))
			safestrncpy(login_config.date_format, w2, sizeof(login_config.date_format));
		else if(!strcmpi(w1, "allowed_regs")) //account flood protection system
			login_config.allowed_regs = atoi(w2);
		else if(!strcmpi(w1, "time_allowed"))
			login_config.time_allowed = atoi(w2);
		else if(!strcmpi(w1, "use_dnsbl"))
			login_config.use_dnsbl = (bool)config_switch(w2);
		else if(!strcmpi(w1, "dnsbl_servers"))
			safestrncpy(login_config.dnsbl_servs, w2, sizeof(login_config.dnsbl_servs));
		else if(!strcmpi(w1, "ipban_cleanup_interval"))
			login_config.ipban_cleanup_interval = (unsigned int)atoi(w2);
		else if(!strcmpi(w1, "ip_sync_interval"))
			login_config.ip_sync_interval = (unsigned int)1000*60*atoi(w2); //w2 comes in minutes.
		else if(!strcmpi(w1, "client_hash_check"))
			login_config.client_hash_check = config_switch(w2);
		else if(!strcmpi(w1, "use_web_auth_token"))
			login_config.use_web_auth_token = config_switch(w2);
		else if (!strcmpi(w1, "disable_webtoken_delay"))
			login_config.disable_webtoken_delay = cap_value(atoi(w2), 0, INT_MAX);
		else if(!strcmpi(w1, "client_hash")) {
			int group = 0;
			char md5[33];

			if (sscanf(w2, "%3d, %32s", &group, md5) == 2) {
				struct client_hash_node *nnode;
				CREATE(nnode, struct client_hash_node, 1);
				if (strcmpi(md5, "disabled") == 0) {
					nnode->hash[0] = '\0';
				} else {
					int i;
					for (i = 0; i < 32; i += 2) {
						char buf[3];
						unsigned int byte;

						memcpy(buf, &md5[i], 2);
						buf[2] = 0;

						sscanf(buf, "%2x", &byte);
						nnode->hash[i / 2] = (uint8)(byte & 0xFF);
					}
				}
				nnode->group_id = group;
				nnode->next = login_config.client_hash_nodes;
				login_config.client_hash_nodes = nnode;
			}
		} else if (!strcmpi(w1, "usercount_disable"))
			login_config.usercount_disable = config_switch(w2);
		else if (!strcmpi(w1, "usercount_low"))
			login_config.usercount_low = atoi(w2);
		else if (!strcmpi(w1, "usercount_medium"))
			login_config.usercount_medium = atoi(w2);
		else if (!strcmpi(w1, "usercount_high"))
			login_config.usercount_high = atoi(w2);
		else if(strcmpi(w1, "chars_per_account") == 0) { //maxchars per account [Sirius]
			login_config.char_per_account = atoi(w2);
			if( login_config.char_per_account > MAX_CHARS ) {
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowWarning("계정당 최대 케릭터수(%d) 제한을 초과 했습니다. '%d'으(로) 재설정합니다.\n", login_config.char_per_account, MAX_CHARS);
				else
					ShowWarning("Exceeded limit of max chars per account '%d'. Capping to '%d'.\n", login_config.char_per_account, MAX_CHARS);
				login_config.char_per_account = MAX_CHARS;
			}else if( login_config.char_per_account < 0 ){
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowWarning("계정(%d)당 최대 케릭터수가 0보다 작습니다. '%d'으(로) 재설정합니다.\n", login_config.char_per_account, MAX_CHARS);
				else
					ShowWarning("Max chars per account '%d' is negative. Capping to '%d'.\n", login_config.char_per_account, MIN_CHARS);
				login_config.char_per_account = MIN_CHARS;
			}
		}
#ifdef VIP_ENABLE
		else if(strcmpi(w1,"vip_group")==0)
			login_config.vip_sys.group = cap_value(atoi(w2),0,99);
		else if(strcmpi(w1,"vip_char_increase")==0) {
			if (atoi(w2) == -1)
				login_config.vip_sys.char_increase = MAX_CHAR_VIP;
			else
				login_config.vip_sys.char_increase = atoi(w2);
			if (login_config.vip_sys.char_increase > (unsigned int) MAX_CHARS-login_config.char_per_account) {
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowWarning("vip_char_increase가 너무 많습니다. char_per_account 설정값 %d에 따라 %d까지만 이동할 수 있습니다.\n",
						MAX_CHARS-login_config.char_per_account,login_config.char_per_account);
				else		
					ShowWarning("vip_char_increase too high, can only go up to %d, according to your char_per_account config %d\n",
						MAX_CHARS-login_config.char_per_account,login_config.char_per_account);
				login_config.vip_sys.char_increase = MAX_CHARS-login_config.char_per_account;
			}
		}
#endif
		else if(!strcmpi(w1, "import"))
			login_config_read(w2, normal);
		else {// try the account engines
			if (!normal)
				continue;
			if (accounts && accounts->set_property(accounts, w1, w2))
				continue;
			// try others
			ipban_config_read(w1, w2);
			loginlog_config_read(w1, w2);
		}
	}
	fclose(fp);
	if(strcmpi(MessageLang,"KOR") == 0)
		ShowInfo("%s 읽기 완료.\n", cfgName);
	else
		ShowInfo("Finished reading %s.\n", cfgName);
	return true;
}

/**
 * Init login-serv default configuration.
 */
void login_set_defaults() {
	login_config.login_ip = INADDR_ANY;
	login_config.login_port = 6900;
	login_config.ipban_cleanup_interval = 60;
	login_config.ip_sync_interval = 0;
	login_config.log_login = true;
	safestrncpy(login_config.date_format, "%Y-%m-%d %H:%M:%S", sizeof(login_config.date_format));
	login_config.console = false;
	login_config.new_account_flag = true;
#if PACKETVER >= 20181114
	login_config.acc_name_min_length = 6;
	login_config.password_min_length = 6;
#else
	login_config.acc_name_min_length = 4;
	login_config.password_min_length = 4;
#endif
	login_config.use_md5_passwds = false;
	login_config.group_id_to_connect = -1;
	login_config.min_group_id_to_connect = -1;

	login_config.ipban = true;
	login_config.dynamic_pass_failure_ban = true;
	login_config.dynamic_pass_failure_ban_interval = 5;
	login_config.dynamic_pass_failure_ban_limit = 7;
	login_config.dynamic_pass_failure_ban_duration = 5;
	login_config.use_dnsbl = false;
	safestrncpy(login_config.dnsbl_servs, "", sizeof(login_config.dnsbl_servs));
	login_config.allowed_regs = 1;
	login_config.time_allowed = 10; //in second

	login_config.client_hash_check = 0;
	login_config.client_hash_nodes = nullptr;
	login_config.usercount_disable = false;
	login_config.usercount_low = 200;
	login_config.usercount_medium = 500;
	login_config.usercount_high = 1000;
	login_config.char_per_account = MAX_CHARS - MAX_CHAR_VIP - MAX_CHAR_BILLING;
#ifdef VIP_ENABLE
	login_config.vip_sys.char_increase = MAX_CHAR_VIP;
	login_config.vip_sys.group = 5;
#endif
	login_config.use_web_auth_token = true;
	login_config.disable_webtoken_delay = 10000;

	//other default conf
	safestrncpy(login_config.loginconf_name, "conf/login_athena.conf", sizeof(login_config.loginconf_name));
	safestrncpy(login_config.lanconf_name, "conf/subnet_athena.conf", sizeof(login_config.lanconf_name));
	safestrncpy(login_config.msgconf_name, "conf/msg_conf/login_msg.conf", sizeof(login_config.msgconf_name));
}




/// Constructor destructor and signal handlers

/**
 * Login-serv destructor
 *  dealloc..., function called at exit of the login-serv
 */
void LoginServer::finalize(){
	struct client_hash_node *hn = login_config.client_hash_nodes;
	AccountDB* db = accounts;

	while (hn)
	{
		struct client_hash_node *tmp = hn;
		hn = hn->next;
		aFree(tmp);
	}

	login_log(0, "login server", 100, "login server shutdown");
	if(strcmpi(MessageLang,"KOR") == 0)
		ShowStatus("종료중...\n");
	else
		ShowStatus("Terminating...\n");

	if( login_config.log_login )
		loginlog_final();

	do_final_msg();
	ipban_final();
	do_final_loginclif();
	do_final_logincnslif();

	if (db) { // destroy account engine
		db->destroy(db);
		db = nullptr;
	}

	accounts = nullptr; // destroyed in account_engine
	online_db.clear();
	auth_db.clear();

	do_final_loginchrif();

	if( login_fd != -1 )
	{
		do_close(login_fd);
		login_fd = -1;
	}

	if(strcmpi(MessageLang,"KOR") == 0)
		ShowStatus("완료되었습니다.\n");
	else
		ShowStatus("Finished.\n");
}

void LoginServer::handle_shutdown(){
	if(strcmpi(MessageLang,"KOR") == 0)
		ShowStatus("종료중...\n");
	else
		ShowStatus("Shutting down...\n");
	// TODO proper shutdown procedure; kick all characters, wait for acks, ...  [FlavioJS]
	do_shutdown_loginchrif();
	flush_fifos();
}

bool LoginServer::initialize( int argc, char* argv[] ){
	// Init default value
	safestrncpy(console_log_filepath, "./log/login-msg_log.log", sizeof(console_log_filepath));

	// initialize engine
	accounts = account_db_sql();

	// read login-server configuration
	login_set_defaults();
	logcnslif_get_options(argc,argv);

	login_config_read(login_config.loginconf_name, true);
	msg_config_read(login_config.msgconf_name);
	login_lan_config_read(login_config.lanconf_name);
	//end config

	do_init_loginclif();
	do_init_loginchrif();

	// initialize logging
	if( login_config.log_login )
		loginlog_init();

	// initialize static and dynamic ipban system
	ipban_init();

	add_timer_func_list(login_waiting_disconnect_timer, "waiting_disconnect_timer");

	// set default parser as parse_login function
	set_defaultparse(logclif_parse);

	// every 10 minutes cleanup online account db.
	add_timer_func_list(login_online_data_cleanup, "online_data_cleanup");
	add_timer_interval(gettick() + 600*1000, login_online_data_cleanup, 0, 0, 600*1000);

	// Account database init
	if( accounts == NULL ) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowFatalError("do_init: 계정엔진을 찾을 수 없습니다.\n");
		else
			ShowFatalError("do_init: account engine not found.\n");
		return false;
	} else {
		if(!accounts->init(accounts)) {
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowFatalError("do_init: 계정엔진을 초기화 하는데 실패 했습니다.\n");
			else	
				ShowFatalError("do_init: Failed to initialize account engine.\n");
			return false;
		}
	}

	// server port open & binding
	if( (login_fd = make_listen_bind(login_config.login_ip,login_config.login_port)) == -1 ) {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowFatalError("'" CL_WHITE "%d" CL_RESET "'번 포트에 바인드 하지 못했습니다.\n",login_config.login_port);
		else
			ShowFatalError("Failed to bind to port '" CL_WHITE "%d" CL_RESET "'\n",login_config.login_port);
		return false;
	}

	do_init_logincnslif();

	if(strcmpi(MessageLang,"KOR") == 0)
		ShowStatus("로그인 서버가 " CL_GREEN "준비" CL_RESET "되었습니다. (서버가 포트번호 %u에서 수신 대기중입니다.)\n\n", login_config.login_port);
	else
		ShowStatus("The login-server is " CL_GREEN "ready" CL_RESET " (Server is listening on the port %u).\n\n", login_config.login_port);
	login_log(0, "login server", 100, "login server started");

	return true;
}

int main( int argc, char *argv[] ){
	return main_core<LoginServer>( argc, argv );
}
