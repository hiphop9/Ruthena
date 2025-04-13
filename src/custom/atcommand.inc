// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

/**
 * Custom Atcommands
 * Place the body of custom atcommands in this file.
 * Format:
 *	ACMD_FUNC(command_name)
 *	{
 *		<code>;
 *	}
 **/

//ACMD_FUNC(newcommand)
//{
//	clif_displaymessage(fd, "It works!");
//	clif_specialeffect(&sd->bl, EF_HEARTCASTING, AREA);
//	return 0;
//}
 ACMD_FUNC(charkami)
 {

	int64 color=(int)-1;
	int litem=0;
	char output[200];
	std::shared_ptr<item_data> idata;
	int64 SetCColor=(int)0xffccff;
	int64 glv=10;

	nullpo_retr(-1, sd);

	if(sd->charkamiset > 0) {
                clif_displaymessage(fd,"전체채팅이 꺼져있습니다..");
                return -1;
        }
	 if(charkami_flag > 0) {
                clif_displaymessage(fd,"GM님께서 전체채팅을 끄셨습니다…");
                return -1;
        }

	// 배틀컨피그로부터값읽어오기
	
	memset(atcmd_output, '\0', sizeof(atcmd_output));

	sscanf(message, "%199[^\n]", atcmd_output);

	if(!message || !*message) {
		sprintf(output,"메시지를 입력해주세요. (사용법: %s <메시지>)",command);
		clif_displaymessage(fd,output);
		return -1;
	}
	 SetCColor = pc_readreg2(sd,KAMI_COLORS);
        if(SetCColor <= 0) SetCColor = pc_readaccountreg(sd,add_str(KAMI_ACC_COLORS));


	if(sscanf(message, "%199[^\n]",atcmd_output) < 1) {
//		if (!atcmd_output || !*atcmd_output) {
		clif_displaymessage(fd,"메시지를 입력해 주십시오.");
		return -1;
	}
			
		// 컬러값미입력시흰색으로출력
	if(SetCColor <= 0) {
		 if (sd->status.sex == 1)
	         {
	            color=(int)0x00FFF6;
		 }
	         else color=(int)0xFF99FF;	

	}
	else {
		color = SetCColor;
	}


	
	// 레벨제한채크
	if(battle_config.charkami_usegrantlevel>sd->status.base_level) {
		sprintf(output,"사용가능한 레벨은 %d이상부터입니다.",battle_config.charkami_usegrantlevel);
		clif_displaymessage(fd,output);
		return -1;
	}

	// 돈사용여부채크
	if(battle_config.charkami_usemoney > sd->status.zeny) {
		sprintf(output,"소지금이 부족합니다. 1회 사용료는 %d 입니다.",battle_config.charkami_usemoney);
		clif_displaymessage(fd,output);
		return -1;
	}

	// 아이템사용여부채크
	if(battle_config.charkami_useitem==1) {
		// 아이템사용이니아이템코드채크
		if((idata=item_db.find(battle_config.charkami_useitemcode)) == nullptr) {
			ShowError("[CharKami]: Unknown item code. ( set: %d )\n",battle_config.charkami_useitemcode);
			battle_config.charkami_useitem=0;
		}
		
		// 아이템소지여부채크
		if(battle_config.charkami_useitem==1) {
			litem = pc_search_inventory(sd,battle_config.charkami_useitemcode);

			if(litem<0) {
				sprintf(output,"필요아이템이 없습니다. 기능사용을 위해서는 %d개의 %s가 필요합니다.",battle_config.charkami_useitemamount,idata->ename.c_str());
				clif_displaymessage(fd,output);
				return -1;
			}

			if(sd->inventory.u.items_inventory[litem].amount < battle_config.charkami_useitemamount) {
				sprintf(output,"필요아이템이 필요수량보다 적습니다. 기능사용을 위해서는 %d개의 %s가 필요합니다.",battle_config.charkami_useitemamount,idata->name.c_str());
				clif_displaymessage(fd,output);
				return -1;
			}

		}	

	}

		// 컬러값미입력시흰색으로출력
		if(SetCColor <= 0) {
			 if (sd->status.sex == 1)
		         {
		            color=(int)0x00FFF6;
			 }
		         else color=(int)0xFF99FF;	
	
		}
		else {
			color = SetCColor;
		}

	// 아이템삭제및제니삭제
	
	// 아이템
	if(battle_config.charkami_useitem==1) {
		if(pc_delitem(sd,litem,battle_config.charkami_useitemamount,0,0,LOG_TYPE_COMMAND)) {
			clif_displaymessage(fd,"필요아이템이 없습니다.");
			return -1;
		}
	}


	// 제니삭제
	if((battle_config.charkami_usemoney > 0)) {
		if(pc_payzeny(sd,battle_config.charkami_usemoney,LOG_TYPE_COMMAND) == 1) {
			clif_displaymessage(fd,"소지금이 부족합니다.");
			return -1;
		}
	}

	// 정상.
	sprintf(output,"[%s]: %s",sd->status.name,atcmd_output);
	clif_ykami(NULL,output,strlen(output)+1,color);

	return 0;
 }
 ACMD_FUNC(charkami2)
 {
	int64 color=(int)-1;
	int litem=0;
	char output[200];
	std::shared_ptr<item_data> idata;
	int64 SetCColor2=0xffccff;

	nullpo_retr(-1, sd);

	if(sd->charkamiset2 > 0) {
                clif_displaymessage(fd,"전체채팅이 꺼져있습니다..");
                return -1;
        }
	 if(charkami2_flag > 0) {
                clif_displaymessage(fd,"GM님께서 전체채팅을 끄셨습니다…");
                return -1;
        }

	// 배틀컨피그로부터값읽어오기
	
	memset(atcmd_output, '\0', sizeof(atcmd_output));

	sscanf(message, "%199[^\n]", atcmd_output);

	if(!message || !*message) {
		sprintf(output,"메시지를 입력해주세요. (사용법: %s <메시지>)",command);
		clif_displaymessage(fd,output);
		return -1;
	}
	 SetCColor2 = pc_readreg2(sd,KAMI_COLORS2);
        if(SetCColor2 <= 0) SetCColor2 = pc_readaccountreg(sd,add_str(KAMI_ACC_COLORS2));


	if(sscanf(message, "%199[^\n]",atcmd_output) < 1) {
//		if (!atcmd_output || !*atcmd_output) {
		clif_displaymessage(fd,"메시지를 입력해 주십시오.");
		return -1;
	}
		
	// 컬러값미입력시흰색으로출력
	if(SetCColor2 <= 0) {
		 if (sd->status.sex == 1)
	         {
	            color=(int)0x00FFF6;
		 }
	         else color=(int)0xFF99FF;	

	}
	else {
		color = SetCColor2;
	}

	
	// 레벨제한채크
	if(battle_config.charkami2_usegrantlevel>sd->status.base_level) {
		sprintf(output,"사용가능한 레벨은 %d이상부터입니다.",battle_config.charkami2_usegrantlevel);
		clif_displaymessage(fd,output);
		return -1;
	}

	// 돈사용여부채크
	if(battle_config.charkami2_usemoney>sd->status.zeny) {
		sprintf(output,"소지금이 부족합니다. 1회 사용료는 %d 입니다.",battle_config.charkami2_usemoney);
		clif_displaymessage(fd,output);
		return -1;
	}

	// 아이템사용여부채크
	if(battle_config.charkami2_useitem==1) {
		// 아이템사용이니아이템코드채크
		if((idata=item_db.find(battle_config.charkami2_useitemcode)) == nullptr) {
			ShowError("[CharKami]: Unknown item code. ( set: %d )\n",battle_config.charkami2_useitemcode);
			battle_config.charkami2_useitem=0;
		}
		
		// 아이템소지여부채크
		if(battle_config.charkami2_useitem==1) {
			litem = pc_search_inventory(sd,battle_config.charkami2_useitemcode);

			if(litem<0) {
				sprintf(output,"필요아이템이 없습니다. 기능사용을 위해서는 %d개의 %s가 필요합니다.",battle_config.charkami2_useitemamount,idata->name.c_str());
				clif_displaymessage(fd,output);
				return -1;
			}

			if(sd->inventory.u.items_inventory[litem].amount < battle_config.charkami2_useitemamount) {
				sprintf(output,"필요아이템이 필요수량보다 적습니다. 기능사용을 위해서는 %d개의 %s가 필요합니다.",battle_config.charkami2_useitemamount,idata->name.c_str());
				clif_displaymessage(fd,output);
				return -1;
			}

		}	

	}

		// 컬러값미입력시흰색으로출력
		if(SetCColor2 <= 0) {
			 if (sd->status.sex == 1)
		         {
		            color=(int)0x00FFF6;
			 }
		         else color=(int)0xFF99FF;	
	
		}
		else {
			color = SetCColor2;
		}

	// 아이템삭제및제니삭제
	
	// 아이템
	if(battle_config.charkami2_useitem==1) {
		if(pc_delitem(sd,litem,battle_config.charkami2_useitemamount,0,0,LOG_TYPE_COMMAND)) {
			clif_displaymessage(fd,"필요아이템이 없습니다.");
			return -1;
		}
	}


	// 제니삭제
	if((battle_config.charkami2_usemoney > 0)) {
		if(pc_payzeny(sd,battle_config.charkami2_usemoney,LOG_TYPE_COMMAND)) {
			clif_displaymessage(fd,"소지금이 부족합니다.");
			return -1;
		}
	}

	// 정상.
	sprintf(output,"[%s]: %s",sd->status.name,atcmd_output);
	intif_broadcast2(output, strlen(output) + 1, color, 0x190, 12, 0, 0);

	return 0;
 }
ACMD_FUNC(charkami_color)
{

	int64 color=(int)-1;
	unsigned int color_type=3;
	char c_type[200];
	char output[200];
	int64 kami_color=(int)-1;
	nullpo_retr(-1, sd);


	memset(atcmd_output, '\0', sizeof(atcmd_output));

	sscanf(message, "%199[^\n]", atcmd_output);
	
	if(!message || !*message) {
		kami_color = pc_readreg2(sd,KAMI_COLORS);
		if(kami_color <=0) kami_color = pc_readaccountreg(sd,add_str(KAMI_ACC_COLORS));

		if(kami_color <= 0) kami_color=(int) 0xFFFFFF;

		sprintf(output,"색상과 옵션을 입력하세요.. (사용법: %s  <RRGGBB> <옵션: 3(기본값=이케릭터만),2:계정전체>)",command);
		clif_displaymessage(fd,output);
		sprintf(output,"현재 입력되어있는 색깔코드: %lx",kami_color);
		clif_displaymessage(fd,output);
		return -1;
	}

    if(sscanf(message, "%lx %d[^\n]", &kami_color, &color_type) < 1) {
		if (!color) {
			clif_displaymessage(fd,"색상을 입력하세요.");
			return -1;
		}
	}
		
	if(!color_type || color_type > 3 || color_type < 2) color_type=3;
	
	if(color_type == 3) 
		pc_setreg2(sd,KAMI_COLORS,kami_color);
	else
		pc_setaccountreg(sd,add_str(KAMI_ACC_COLORS),kami_color);
	
	if(color_type == 3) sprintf(c_type,"이 케릭터에 한해서");
	if(color_type == 2) sprintf(c_type,"이 케릭터가 있는 계정전체에");
    
	sprintf(output,"%lx 색상으로 %s 지정되었습니다.",kami_color,c_type);
	clif_displaymessage(fd,output);
	return 0;
}
ACMD_FUNC(charkami2_color)
{

	int64 color=(int)-1;
	unsigned int color_type=3;
	char c_type[200];
	char output[200];
	int64 kami_color=(int)-1;
	nullpo_retr(-1, sd);


	memset(atcmd_output, '\0', sizeof(atcmd_output));

	sscanf(message, "%199[^\n]", atcmd_output);
	
	if(!message || !*message) {
		kami_color = pc_readreg2(sd,KAMI_COLORS2);
		if(kami_color <=0) kami_color = pc_readaccountreg(sd,add_str(KAMI_ACC_COLORS2));

		if(kami_color <= 0) kami_color=(int) 0xFFFFFF;

		sprintf(output,"색상과 옵션을 입력하세요.. (사용법: %s  <RRGGBB> <옵션: 3(기본값=이케릭터만),2:계정전체>)",command);
		clif_displaymessage(fd,output);
		sprintf(output,"현재 입력되어있는 색깔코드: %lx",kami_color);
		clif_displaymessage(fd,output);
		return -1;
	}

    if(sscanf(message, "%lx %d[^\n]", &kami_color, &color_type) < 1) {
		if (!color) {
			clif_displaymessage(fd,"색상을 입력하세요.");
			return -1;
		}
	}
		
	if(!color_type || color_type > 3 || color_type < 2) color_type=3;

	if(color_type == 3) 
		pc_setreg2(sd,KAMI_COLORS2,kami_color);
	else
		pc_setaccountreg(sd,add_str(KAMI_ACC_COLORS2),kami_color);
	
	if(color_type == 3) sprintf(c_type,"이 케릭터에 한해서");
	if(color_type == 2) sprintf(c_type,"이 케릭터가 있는 계정전체에");
    
	sprintf(output,"%lx 색상으로 %s 지정되었습니다.",kami_color,c_type);
	clif_displaymessage(fd,output);
	return 0;
}

ACMD_FUNC(charkamisetup)
{
	if(sd->charkamiset <= 0) {

		sd->charkamiset = 1;	
		clif_displaymessage(fd,"[전체채팅 OFF]");
		clif_displaymessage(fd,"지금부터 전체채팅을 보실수 없으며 또한 전체채팅도 하실수 없습니다..");
		return 0;
	}

		sd->charkamiset = 0;
		clif_displaymessage(fd,"[전체채팅 ON]");
		clif_displaymessage(fd,"지금부터 전체채팅을 보실수 있으며 @잡을 이용하여 말도 할수 있습니다..");
		return 0;
}	
ACMD_FUNC(charkamisetup2)
{
	if(sd->charkamiset2 <= 0) {

		sd->charkamiset2 = 1;	
		clif_displaymessage(fd,"[전체말 OFF]");
		clif_displaymessage(fd,"지금부터 전체말을 보실수 없으며 또한 전체말도 하실수 없습니다..");
		return 0;
	}

		sd->charkamiset2 = 0;
		clif_displaymessage(fd,"[전체말 ON]");
		clif_displaymessage(fd,"지금부터 전체말을 보실수 있으며 @말을 이용하여 말도 할수 있습니다..");
		return 0;
}	
ACMD_FUNC(charkami_allon) 
{
	charkami_flag = 0;
	clif_displaymessage(fd,"[전체채팅 ON]");
	return 0;
}
ACMD_FUNC(charkami_alloff) 
{
	charkami_flag = 1;
	clif_displaymessage(fd,"[전체채팅 OFF]");
	return 0;
}
ACMD_FUNC(charkami2_allon) 
{
	charkami2_flag = 0;
	clif_displaymessage(fd,"[전체말 ON]");
	return 0;
}
ACMD_FUNC(charkami2_alloff) 
{
	charkami2_flag = 1;
	clif_displaymessage(fd,"[전체말 OFF]");
	return 0;
}
/*===================================
 * 모두감정
 *-----------------------------------*/

ACMD_FUNC(all_identify)
{

	int c;
	int i;
	int color;
	char output[200];

	sprintf(output,"감정중입니다..");
	color=(int)0xbe0000;
	clif_messagecolor(&sd->bl, color, output, true, SELF);		// [달빛정원]

//	clif_disp_onlyself(sd,output,(int)strlen(output)+1);

	for(i=c=0;i<MAX_INVENTORY;i++){
		if(sd->inventory.u.items_inventory[i].nameid > 0 && !sd->inventory.u.items_inventory[i].identify){
			
			sd->inventory.u.items_inventory[i].identify=1;
			clif_item_identified(*sd, i, 0);
			c++;
		}
	}

	sprintf(output,"모두 감정되었습니다..");
	color=(int)0x007dbe;
	clif_messagecolor(&sd->bl, color, output, true, SELF);		// [달빛정원]
	return 0;

}
