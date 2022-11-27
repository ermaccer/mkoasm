#pragma once
#include "MKScriptTypes.h"

struct mko_header {
	int functions;
	int last_function_offset;
	int script_string_size;
	int string_data_size;

	int unknown_data_size;		// post mkda
	int field20;
	int static_variables;
	int last_variable_offset;
};


struct mko_function_header {
	int	name_offset;
	int offset;
	int unknown; // post mkda
};

struct mko_variable_header {
	int name_offset;
	int unknown;
	int offset;
	int numElems;
};


struct mko_command {
	short functionID;
	short isInternal;
	short numVariables;
	short unk2;
};
