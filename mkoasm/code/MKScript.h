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


struct mko_function_header_mka {
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
	// new in mka
	short functionSet;

	// padding for jumps
	bool is_pad;
};


enum EMKOAICommandButton {
	EAICommandButton_Attack1,
	EAICommandButton_Attack1Up,
	EAICommandButton_Attack1Down,
	EAICommandButton_Attack1Towards,
	EAICommandButton_Attack1Away,

	EAICommandButton_Attack2,
	EAICommandButton_Attack2Up,
	EAICommandButton_Attack2Down,
	EAICommandButton_Attack2Towards,
	EAICommandButton_Attack2Away,

	EAICommandButton_Attack3,
	EAICommandButton_Attack3Up,
	EAICommandButton_Attack3Down,
	EAICommandButton_Attack3Towards,
	EAICommandButton_Attack3Away,

	EAICommandButton_Attack4,
	EAICommandButton_Attack4Up,
	EAICommandButton_Attack4Down,
	EAICommandButton_Attack4Towards,
	EAICommandButton_Attack4Away,

	EAICommandButton_Attack5,
	EAICommandButton_Attack5Up,
	EAICommandButton_Attack5Down,
	EAICommandButton_Attack5Towards,
	EAICommandButton_Attack5Away,
};

enum EMKOAICommandType {
	EAICommandType_Button,
	EAICommandType_ChangeStyle,
	EAICommandType_SpecialMove = 4
};