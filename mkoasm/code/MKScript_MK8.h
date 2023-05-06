#pragma once

struct mko_header_mk8 {
	int functions;
	int static_variables;
	int externs;
	int assets;
	int field36;
	int field40;
	int field44;
	int string_size;
	int field52;
	int field56;

};

struct mko_function_header_mk8 {
	int nameOffset;
	int argTypeOffset;
	int field8;
	int size;
	int field16;
	// probably stack size?
	int unkSize;
	int field24;
	int field28;
	int id;
	int numUnk;
	int field40;
};

struct mko_function_header_mk8_unk {
	int field0;
	int field4;
	int field8;
	int field12;
	int field16;
};


struct mko_extern_mk8 {
	int nameOffset;
	int importName;
	unsigned int field8;
	int field12;
	int field16;
	int field20;
};


struct mko_asset_mk8 {
	int archiveNameOffset;
	int nameOffset;
	int field8;
};


struct mko_variable_header_mk8 {
	int name_offset;
	int size;
	int elemSize;
	int offset;
};

enum mko_command_mk8_type {
	MK8_CT_INTERNAL = 0,
	MK8_CT_STEP = 0x4000,
	MK8_CT_EXTERN = 0x8000,
};

struct mko_command_mk8_bytecode {
	int field0;
	int field4;
	int data; // first half - numData, second half - unknown, but seems to be same as previous mks
	int commandData; // first half - func id, second is type;

	// var data
};
struct mko_command_mk8 {
	int field0;
	int field4;
	int numData;
	int functionID;
	int functionType;
	bool is_pad;
};

struct mko_command_mk10 {
	int64 type;
	short subType;
	short field10;
	short numVars;
	short field14;
	bool is_pad;
};