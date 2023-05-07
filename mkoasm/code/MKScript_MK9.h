#pragma once

struct mko_header_mk9 {
	int functions;
	int static_variables;
	int externs;
	int externVariables;
	int assets;
	int soundAssets;
	int field24;
	int field28;
	int bytecodeSize;
	int string_size;
	int stack_size;
	int fixups;
	int tweakVarsOffset;
	int tweakVarsSize;

};

struct mko_function_header_mk9 {
	int nameOffset;
	unsigned int functionHash;
	int functionOffset;
	int size;
	int field16;
	int field20;
	int field24;
	int field28;
	int function_index;
	int local_fixup_count;
	int field40;
	unsigned int paramsHash;
};

struct mko_function_header_mk9_unk {
	int field0;
	//int data[field0];
};


struct mko_extern_mk9 {
	unsigned int nameHash;
	int importName;
	unsigned int field8;
	int field12;
	int field16;
	int field20;
	int field24;
};


struct mko_extern_variable_mk9 {
	int name_offset;
	int field4;
	int field8;
	int field12;
};


struct mko_asset_mk9 {
	int archiveNameOffset;
	int nameOffset;
	int field8;
	int field12;
};

struct mko_sound_asset_mk9 {
	int archiveNameOffset;
	int nameOffset;
	int field8;
	int field12;
	int field16;
};


struct mko_variable_header_mk9 {
	unsigned int name_hash;
	int size;
	int elemSize;
	int offset;
};


struct mko_fixup_mk9 {
	int field0;
	int offset; // ?
	int name_offset;
	int field12;
};
