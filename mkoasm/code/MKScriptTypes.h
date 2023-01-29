#pragma once

enum EMovelistTypes {
	EMovelist_Style_One,
	EMovelist_Style_Two,
	EMovelist_Style_Three,
	EMovelist_SpecialMove

};

struct type_movelist {
	int category;
	int name;
	int tip;
	int command;
	int command2;
	int unk;
};

struct type_attributes {
	float activeFrame;
	float field4;
	float field8;
	float animSpeed;
	float movementSpeed;
	short field20;
	short field22;
	short field24;
	short field26;
	short field28;
	short field30;
	int	  field32;
	float field36;
	float field40;
	float damage;
	int   hitLevel;
	short field52;
	short field54;
	float field56;
};

struct type_plrdata_model {
	int archiveNameOffset;
	int headNameOffset;
	int animsNameOffset;
	int field12;
	int field16;
	int field20;
	float field24;
	int startupFunctionID;
	int reloadFunctionID;
};

// MKD/MKU
struct type_player_data {
	int field0;
	type_plrdata_model primary;
	type_plrdata_model alt;
	int apArchiveNameOffset;
	int apHeadNameOffset;
	int apAltArchiveNameOffset;
	int apAltHeadNameOffset;
	int sharedArchiveNameOffset;
	int specialsSbankID;
	int voiceID;
	int field104;
	int winVoiceID;
	int field112;
	int field116;
	int field120;
	int field124;
	int stylesVariableID;
	int fatDataVariableID;
	int field136;
	float vec[3];
	int puzzleAnimNameOffset;
	int field156;
	int particleVariableIDs[2];
	int field168;
	int limbPiecePivotsID;
	int limbAssemblyPivotsID;
	int field180;
	int field184;
	int field188;
	int attackHighFastID;
	int field196;
	int field200;
	int field204;
	int distanceID;
	int field212;
	int field216;
	int field220;
	int field224;
	int field228;
	int field232;
	int field236;
	int field240;
	int field244;
	int field248;
	int field252;
	int field256;
	int field260;
	int field264;
	int field268;
	int field272;
	int field276;
	int throwAttackID;
	int field284;
	int field288;
	int field292;
	int field296;
	int field300;
	int field304;
	int reactionCleanUpFunctionID;
	int victoryFunctionID;
	int throwFunctionID;
	int field320;
};