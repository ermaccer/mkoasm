#pragma once
struct mko_header_dcf {
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
	int functionsOffset;
	int unknowns;
	int tweakVarsOffset;
	int tweakVarsSize;
	// new in dcf
	int field56;

};

struct mko_asset_dcf {
	int archiveNameOffset;
	int field4;
	int nameOffset;
	int field12;
	int field16;
};
