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
