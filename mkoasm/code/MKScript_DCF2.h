#pragma once
#include "MKScriptTypes.h"

struct mko_header_dcf2 {
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
	int tweakvars; // only difference from mkx
	int numSource;
	int tweakvars_stringSize;
	int globalstack_size;
};

#pragma pack(push, 1)
struct mko_tweakvar_dcf2 {
	int type;
	int offset;
	int size;
	int index;
	int index2;
	int unk;
	int64 unk2;
};
#pragma pack(pop)
