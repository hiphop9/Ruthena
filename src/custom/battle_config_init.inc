// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

/**
 * Custom battle config initializations
 * Place the definition of custom battle configuration initializations in this file.
 *
 * Do not forget to add a comma at the end of the line, if you want to add another config
 *
 * Format:
 *	 { "name", &battle_config.<variable name>, <default value>, <minimum value>, <maximum value> },
 **/

	//Ä¿½ºÅÒ¼Ò½º
	{ "charkami_useitem",					&battle_config.charkami_useitem,				0,		0,		1,				},
	{ "charkami_usemoney",					&battle_config.charkami_usemoney,				0,		0,		MAX_ZENY,		},
	{ "charkami_usegrantlevel",				&battle_config.charkami_usegrantlevel,				0,		0,		MAX_LEVEL,		},
	{ "charkami_useitemcode",				&battle_config.charkami_useitemcode,			12221,	501,	INT_MAX,		},
	{ "charkami_useitemamount",				&battle_config.charkami_useitemamount,			0,		0,		INT_MAX,		},
	{ "charkami2_useitem",					&battle_config.charkami2_useitem,				0,		0,		1,				},
	{ "charkami2_usemoney",					&battle_config.charkami2_usemoney,				0,		0,		MAX_ZENY,		},
	{ "charkami2_usegrantlevel",				&battle_config.charkami2_usegrantlevel,				0,		0,		MAX_LEVEL,		},
	{ "charkami2_useitemcode",				&battle_config.charkami2_useitemcode,			12221,	501,	INT_MAX,		},
	{ "charkami2_useitemamount",				&battle_config.charkami2_useitemamount,			0,		0,		INT_MAX,		},
