#pragma once

struct mko_header_mk9 {
	int functions;
	int static_variables;
	int externs;
	int externVariables;
	int assets;
	int soundAssets;
	int variablesSize;
	int globalObjects;
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
	int stackOffset;
	int stackSize;
	int numArgs;
	int field28; // always 0
	int function_index;
	int local_fixup_count;
	int globalObjects;
	unsigned int paramsHash;
};

struct mko_function_header_mk9_unk {
	int field0;
	//int data[field0];
};


struct mko_extern_mk9 {
	unsigned int nameHash;
	int importName;
	unsigned int paramsHash;
	int externHash;
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
	int type;
	int offset;
	int data;
	int pad;
};

struct mko_global_object_mk9 {
	int offset;
	int data;
};


enum mko_fixup_mk9_types {
	// stackData at offset = data
	MK9_Fixup_StringVar,
	// bytecode at offset = data
	MK9_Fixup_String,
	// local only
	MK9_Fixup_String2,
	// set stuff at stackData at offsetm, local only
	MK9_Fixup_Register,
	// stackData at offset = asset ptr
	MK9_Fixup_AssetVar,
	// bytecode at offset = asset ptr
	MK9_Fixup_Asset,
	// stack at offset = this script ptr
	MK9_Fixup_This,
	// unused?
	MK9_Fixup_9,
	// bytecode at offset = new function call
	MK9_Fixup_Function,
	// stackData at offset = sound ptr
	MK9_Fixup_SoundVar,
	// bytecode at offset = sound ptr
	MK9_Fixup_Sound
};

