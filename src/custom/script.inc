// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

/**
 * Custom Script Commands
 * Place the body of custom script commands in this file.
 * Format:
 *	BUILDIN_FUNC(command_name)
 *	{
 *		<code>;
 *	}
 **/

//BUILDIN_FUNC(example)
//{
//	ShowInfo("buildin_example: It works!\n");
//	script_pushint(st,1);
//	return 0;
//}

BUILDIN_FUNC(specialeffect3)
{
	int type = script_getnum(st, 2);
	enum send_target target = script_hasdata(st, 3) ? (send_target)script_getnum(st, 3) : AREA;

	struct block_list* bl;

	if (!script_rid2bl(4, bl))
		return SCRIPT_CMD_SUCCESS;

	if( type <= EF_NONE || type >= EF_MAX ){
		ShowError( "buildin_specialeffect3: unsupported effect id %d.\n", type );
		return SCRIPT_CMD_FAILURE;
	}

	clif_specialeffect(bl, type, target);

	return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(unitisforcewalk)
{
	struct block_list *bl = nullptr;
	bool force_walk = false;

	if (script_getnum(st, 2))
		bl = map_id2bl(script_getnum(st,2));
	else
		bl = map_id2bl(st->rid);

	if (bl) {
		struct unit_data *ud = unit_bl2ud(bl);

		if (ud)
			force_walk = (ud->walktimer == INVALID_TIMER && ud->state.force_walk);
	}
	script_pushint(st, force_walk);

	return SCRIPT_CMD_SUCCESS;
}

BUILDIN_FUNC(numisstr)
{

			int i=0;
			int j=0;
			int cnt=0;
			int getcash=0;
			char temp[200]="";
			char str[200]="";
			char sstr[200]="";

			struct script_data *data;

			data = script_getdata(st,2);
	
			get_val(st,data);
			getcash = conv_num(st,data);
	
			if(getcash <= 0) {

				//script_pushstr(st,"0");
				script_pushconststr(st,"0");
				return 0;
			}
			sprintf(str,"%d",getcash);
			
			i = strlen(str)-1;

			//4자리마다 컴파 침
			 while(i >= 0 ){
                if(cnt%3 == 0 && cnt != 0){
                temp[j]=',';
                j++;
                }
                temp[j] = str[i];
                j++;
                i--;
                cnt ++;

	        }

			for(i = strlen(temp)-1; i>=0; i--) {
				sprintf(sstr,"%s%c", sstr,temp[i]);
				
			}


			script_pushstrcopy(st,sstr);
			return 0;

}