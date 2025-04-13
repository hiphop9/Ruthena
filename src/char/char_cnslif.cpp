// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "char_cnslif.hpp"

#include <cstdlib>
#include <cstring>

#include <common/cli.hpp>
#include <common/ers.hpp>
#include <common/showmsg.hpp>
#include <common/socket.hpp>
#include <common/timer.hpp>
#include <config/custom.hpp>

#include "char.hpp"

/*======================================================
 * Login-Server help option info
 *------------------------------------------------------*/
void display_helpscreen(bool do_exit)
{
	if(strcmpi(MessageLang,"KOR") == 0) {
		ShowInfo("사용방법: %s [옵션]\n", SERVER_NAME);
		ShowInfo("\n");
		ShowInfo("옵션들:\n");
		ShowInfo("  -?, -h [--help]\t\t도움말을 봅니다.\n");
		ShowInfo("  -v [--version]\t\t서버의 버전을 봅니다.\n");
		ShowInfo("  --run-once\t\t\t로드한 후 서버를 닫습니다. (테스트용).\n");
		ShowInfo("  --char-config <file>\t\t지정한 설정 파일을 케릭터 서버의 설정파일로 대체합니다.\n");
		ShowInfo("  --lan-config <file>\t\t지정한 파일로 언어를 설정합니다.\n");
		ShowInfo("  --inter-config <file>\t\t지정한 파일로 inter-server 설정파일로 대체합니다.\n");	
		ShowInfo("  --msg-config <file>\t\t지정한 파일로 메시지 설정파일로 대체합니다.\n");
	}
	else {
		ShowInfo("Usage: %s [options]\n", SERVER_NAME);
		ShowInfo("\n");
		ShowInfo("Options:\n");
		ShowInfo("  -?, -h [--help]\t\tDisplays this help screen.\n");
		ShowInfo("  -v [--version]\t\tDisplays the server's version.\n");
		ShowInfo("  --run-once\t\t\tCloses server after loading (testing).\n");
		ShowInfo("  --char-config <file>\t\tAlternative char-server configuration.\n");
		ShowInfo("  --lan-config <file>\t\tAlternative lag configuration.\n");
		ShowInfo("  --inter-config <file>\t\tAlternative inter-server configuration.\n");
		ShowInfo("  --msg-config <file>\t\tAlternative message configuration.\n");
	}
	if( do_exit )
		exit(EXIT_SUCCESS);
}

/**
 * Timered function to check if the console has a new event to be read.
 * @param tid: timer id
 * @param tick: tick of execution
 * @param id: user account id
 * @param data: unused
 * @return 0
 */
TIMER_FUNC(cnslif_console_timer){
	char buf[MAX_CONSOLE_IN]; //max cmd atm is 63+63+63+3+3

	memset(buf,0,MAX_CONSOLE_IN); //clear out buf

	if(cli_hasevent()){
		if(fgets(buf, MAX_CONSOLE_IN, stdin)==nullptr)
			return -1;
		else if(strlen(buf)>MIN_CONSOLE_IN)
			cnslif_parse(buf);
	}
	return 0;
}

// Console Command Parser [Wizputer]
int cnslif_parse(const char* buf)
{
	char type[64];
	char command[64];
	int n=0;

	if( ( n = sscanf(buf, "%63[^:]:%63[^\n]", type, command) ) < 2 ){
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
	if( n == 2 && strcmpi("server", type) == 0 ){
		if( strcmpi("shutdown", command) == 0 || strcmpi("exit", command) == 0 || strcmpi("quit", command) == 0 ){
			global_core->signal_shutdown();
		}
		else if( strcmpi("alive", command) == 0 || strcmpi("status", command) == 0 ) {
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowInfo(CL_CYAN "콘솔: " CL_BOLD "잘 작동하고 있습니다." CL_RESET"\n");
			else
				ShowInfo(CL_CYAN "Console: " CL_BOLD "I'm Alive." CL_RESET "\n");
		} else if( strcmpi("reloadconf", command) == 0 ) {
			if(strcmpi(MessageLang,"KOR") == 0)
				ShowInfo("설정파일 \"%s\"이(가) 다시 로드 되었습니다.\n", CHAR_CONF_NAME);
			else
				ShowInfo("Reloading config file \"%s\"\n", CHAR_CONF_NAME);
			char_config_read(CHAR_CONF_NAME, false);
		}
	}
	else if( strcmpi("ers_report", type) == 0 ){
		ers_report();
	}
	else if( strcmpi("help", type) == 0 ){
		if(strcmpi(MessageLang,"KOR") == 0) {
			ShowInfo("사용가능한 명령어:\n");
			ShowInfo("\t server:shutdown => 서버를 중지합니다..\n");
			ShowInfo("\t server:alive => 서버가 잘 실행되고 있는지 확인합니다.\n");
			ShowInfo("\t server:reloadconf => 설정파일 \"%s\"를 다시 불러옵니다.\n", CHAR_CONF_NAME);
			ShowInfo("\t ers_report => 데이터베이스 사용방법을 봅니다.\n");
		} else {
			ShowInfo("Available commands:\n");
			ShowInfo("\t server:shutdown => Stops the server.\n");
			ShowInfo("\t server:alive => Checks if the server is running.\n");
			ShowInfo("\t server:reloadconf => Reload config file: \"%s\"\n", CHAR_CONF_NAME);
			ShowInfo("\t ers_report => Displays database usage.\n");
		}
	}

	return 0;
}

void do_init_chcnslif(void){
	if( charserv_config.console ){ //start listening
		add_timer_func_list(cnslif_console_timer, "cnslif_console_timer");
		add_timer_interval(gettick()+1000, cnslif_console_timer, 0, 0, 1000); //start in 1s each 1sec
	}
}
