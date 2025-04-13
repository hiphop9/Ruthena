// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "logincnslif.hpp"

#include <cstdlib>
#include <cstring>

#include <common/cli.hpp>
#include <common/md5calc.hpp>
#include <common/mmo.hpp> //cbasetype + NAME_LENGTH
#include <common/showmsg.hpp> //show notice
#include <common/strlib.hpp>
#include <common/timer.hpp>
#include <config/custom.hpp>

#include "login.hpp"

/**
 * Login-server console help: starting option info.
 *  Do not rename function used as extern.
 * @param do_exit: terminate program execution ?
 */
void display_helpscreen(bool do_exit) {
	if(strcmpi(MessageLang,"KOR") == 0) {
		ShowInfo("사용방법: %s [옵션]\n", SERVER_NAME);
		ShowInfo("\n");
		ShowInfo("옵션들:\n");
		ShowInfo("  -?, -h [--help]\t\t도움말을 봅니다.\n");
		ShowInfo("  -v [--version]\t\t서버의 버전을 봅니다.\n");
		ShowInfo("  --run-once\t\t\t로드한 후 서버를 닫습니다. (테스트용).\n");
		ShowInfo("  --login-config <file>\t\t지정한 설정 파일을 로그인 서버의 설정파일로 대체합니다.\n");
		ShowInfo("  --lan-config <file>\t\t지정한 파일로 언어를 설정합니다.\n");
		ShowInfo("  --msg-config <file>\t\t지정한 파일로 메시지 설정파일로 대체합니다.\n");
	}
	else {
		ShowInfo("Usage: %s [options]\n", SERVER_NAME);
		ShowInfo("\n");
		ShowInfo("Options:\n");
		ShowInfo("  -?, -h [--help]\t\tDisplays this help screen.\n");
		ShowInfo("  -v [--version]\t\tDisplays the server's version.\n");
		ShowInfo("  --run-once\t\t\tCloses server after loading (testing).\n");
		ShowInfo("  --login-config <file>\t\tAlternative login-server configuration.\n");
		ShowInfo("  --lan-config <file>\t\tAlternative lan configuration.\n");
		ShowInfo("  --msg-config <file>\t\tAlternative message configuration.\n");
	}
	if( do_exit )
		exit(EXIT_SUCCESS);
}

/**
 * Read the option specified in command line
 *  and assign the confs used by the different server.
 * @param argc:
 * @param argv:
 * @return true or Exit on failure.
 */
int logcnslif_get_options(int argc, char ** argv) {
	int i = 0;
	for (i = 1; i < argc; i++) {
		const char* arg = argv[i];

		if (arg[0] != '-' && (arg[0] != '/' || arg[1] == '-')) {// -, -- and /
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowError("알수 없는 옵션: '%s'.\n", argv[i]);
			else	
				ShowError("Unknown option '%s'.\n", argv[i]);
			exit(EXIT_FAILURE);
		} else if ((++arg)[0] == '-') {// long option
			arg++;

			if (strcmp(arg, "help") == 0) {
				display_helpscreen(true);
			} else if (strcmp(arg, "version") == 0) {
				display_versionscreen(true);
			} else if (strcmp(arg, "run-once") == 0){ // close the map-server as soon as its done.. for testing [Celest]
				global_core->set_run_once( true );
			} else if (strcmp(arg, "lan-config") == 0) {
				if (opt_has_next_value(arg, i, argc)) safestrncpy(login_config.lanconf_name, argv[++i], sizeof(login_config.lanconf_name));
			} else if (strcmp(arg, "login-config") == 0) {
				if (opt_has_next_value(arg, i, argc)) safestrncpy(login_config.loginconf_name, argv[++i], sizeof(login_config.loginconf_name));
			} else if (strcmp(arg, "msg-config") == 0) {
				if (opt_has_next_value(arg, i, argc)) safestrncpy(login_config.msgconf_name, argv[++i], sizeof(login_config.msgconf_name));
			} else {
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowError("'%s'은(는) 알 수 없는 옵션입니다.\n", argv[i]);
				else
					ShowError("Unknown option '%s'.\n", argv[i]);
				exit(EXIT_FAILURE);
			}
		} else switch (arg[0]) {// short option
			case '?':
			case 'h':
				display_helpscreen(true);
				break;
			case 'v':
				display_versionscreen(true);
				break;
			default:
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowError("알 수 없는 옵션: '%s'.\n", argv[i]);
				else
					ShowError("Unknown option '%s'.\n", argv[i]);
				exit(EXIT_FAILURE);
		}
	}
	return 1;
}

/**
 * Console Command Parser
 * Transmited from command cli.cpp
 * note common name for all serv do not rename (extern in cli)
 * @author [Wizputer]
 * @param buf: buffer to parse, (from console)
 * @return 1=success
 */
int cnslif_parse(const char* buf){
	char type[64];
	char command[64];
	int n=0;

	if( ( n = sscanf(buf, "%127[^:]:%255[^\n\r]", type, command) ) < 2 ){
		if((n = sscanf(buf, "%63[^\n]", type))<1) return -1; //nothing to do no arg
	}
	if( n != 2 ){ //end string
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowNotice("타입: '%s'\n",type);
		else
			ShowNotice("Type: '%s'\n",type);
		command[0] = '\0';
	}
	else {
		if(strcmpi(MessageLang,"KOR") == 0)
			ShowNotice("명령어의 타입: '%s' || 명령어: '%s'\n",type,command);
		else
			ShowNotice("Type of command: '%s' || Command: '%s'\n",type,command);
	}
	if( n == 2 ){
		if(strcmpi("server", type) == 0 ){
			if( strcmpi("shutdown", command) == 0 || strcmpi("exit", command) == 0 || strcmpi("quit", command) == 0 ){
				global_core->signal_shutdown();
			}
			else if( strcmpi("alive", command) == 0 || strcmpi("status", command) == 0 ) {
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowInfo(CL_CYAN "콘솔: " CL_BOLD "잘 작동중입니다.." CL_RESET "\n");
				else
					ShowInfo(CL_CYAN "Console: " CL_BOLD "I'm Alive." CL_RESET"\n");
			} else if( strcmpi("reloadconf", command) == 0 ) {
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowInfo("설정파일 \"%s\"이(가) 다시 로드 되었습니다.\n", login_config.loginconf_name);
				else
					ShowInfo("Reloading config file \"%s\"\n", login_config.loginconf_name);
				login_config_read(login_config.loginconf_name, false);
			}
		}
		if( strcmpi("create",type) == 0 )
		{
			char username[NAME_LENGTH], password[NAME_LENGTH], md5password[32+1], sex; //23+1 plaintext 32+1 md5
			bool md5 = 0;
			if( sscanf(command, "%23s %23s %c", username, password, &sex) < 3 || strnlen(username, sizeof(username)) < 4 || strnlen(password, sizeof(password)) < 1 ){
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowWarning("콘솔: '%s'에 대한 매개변수(파라메터)가 잘못되었습니다. 사용법: %s <아이디> <비밀번호> <성별:F/M>\n", type, type);
				else
					ShowWarning("Console: Invalid parameters for '%s'. Usage: %s <username> <password> <sex:F/M>\n", type, type);
				return 0;
			}
			if( login_config.use_md5_passwds ){
				MD5_String(password,md5password);
				md5 = 1;
			}
			if( login_mmo_auth_new(username,(md5?md5password:password), TOUPPER(sex), "0.0.0.0") != -1 ){
				if(strcmpi(MessageLang,"KOR") == 0)
					ShowError("콘솔: 계정 생성에 실패 했습니다.\n");
				else
					ShowError("Console: Account creation failed.\n");
				return 0;
			}
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowStatus("콘솔: '%s' 계정이 성공적으로 생성되었습니다.\n", username);
			else
				ShowStatus("Console: Account '%s' created successfully.\n", username);
		}
	}
	else if( strcmpi("help", type) == 0 ){
		if(strcmpi(MessageLang,"KOR") == 0) {
			ShowInfo("사용가능한 명령어:\n");
			ShowInfo("\t server:shutdown => 서버를 중지합니다.\n");
			ShowInfo("\t server:alive => 서버가 잘 실행중인지 체크합니다.\n");
			ShowInfo("\t server:reloadconf => 설정파일 \"%s\"를 다시 불러옵니다.\n", login_config.loginconf_name);
			ShowInfo("\t ers_report => 데이터베이스 사용방법을 봅니다.\n");
			ShowInfo("\t create:<username> <password> <sex:M|F> => 새로운 계정을 생성합니다.\n");
		} else {
			ShowInfo("Available commands:\n");
			ShowInfo("\t server:shutdown => Stops the server.\n");
			ShowInfo("\t server:alive => Checks if the server is running.\n");
			ShowInfo("\t server:reloadconf => Reload config file: \"%s\"\n", login_config.loginconf_name);
			ShowInfo("\t create:<username> <password> <sex:M|F> => Creates a new account.\n");
		}
	}
	return 1;
}

/**
 * Initialize the module.
 * Launched at login-serv start, create db or other long scope variable here.
 */
void do_init_logincnslif(void){
	if( login_config.console ) {
		add_timer_func_list(parse_console_timer, "parse_console_timer");
		add_timer_interval(gettick()+1000, parse_console_timer, 0, 0, 1000); //start in 1s each 1sec
	}
}

/**
 * Handler to cleanup module, called when login-server stops.
 */
void do_final_logincnslif(void){
	return;
}
