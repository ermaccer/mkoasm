#pragma once
#include "MKScriptTypes.h"

struct mko_header_mk10 {
	int functions;
	int static_variables;
	int dynamic_variables;
	int externs;
	int extern_variables;
	int assets;
	int global_pointers;
	int total_pointers;
	int bytecodeSize;
	int string_size;
	int stack_size;
	int fixups;
	int tweakvars_size;
	int numSource;
	int tweakvars_stringSize;
	int globalstack_size;
};

#pragma pack(push, 1)
struct mko_function_header_mk10 {
	int64 nameOffset;
	int64 functionOffset;
	int64 field16;
	int64 field24;
	int64 field32;
	unsigned int functionHash;
	int size;
	int field48;
	int field52;
	int num_args;
	int function_index;
	int local_fixup_count;
	int  checked_object_count;
	unsigned int paramsHash;
	int field72;
};
#pragma pack(pop)

struct mko_variable_header_mk10 {
	int stack_offset;
	int data;
	unsigned int nameHash;
	int size;
	int stride;
	int pad;
};


struct mko_extern_mk10 {
	int file_name_offset;
	int file_name;
	int name_offset;
	int name;
	unsigned int nameHash;
	int pad;
};

struct mko_extern_var_mk10 {
	int FName_Index;
	int FName_Flags;
	int64 addr;
	int flags;
	unsigned int name_offset;
};

struct mko_source_header_mk10 {
	int name_offset;
	int pad;
};

struct mko_asset_mk10 {
	int package_name_offset;
	int package_name;
	int item_name_offset;
	int item_name;
	int asset;
	int sound;
	int type;
	int index;
};

struct mko_fixup_mk10 {
	int type;
	int offset;
	int64 data;
};



enum mko_bc_type_mk10 {
	MK10_BC_Type_MKO = 1
};