#pragma once
#include <vector>

enum EMKOFunctionArgumentDefinition_Type {
	EMKOFAD_Integer,
	EMKOFAD_Float,
	EMKOFAD_Short,
	EMKOFAD_UInt,
	EMKOFAD_String
};

struct MKOFunctionArgumentDefinition {
	EMKOFunctionArgumentDefinition_Type type;
};

struct MKOFunctionDefinition {
	char name[256] = {};
	int functionID;
	int num_arguments;
	std::vector<EMKOFunctionArgumentDefinition_Type> args;
};


class MKODict {
public:
	static std::vector<MKOFunctionDefinition> ms_vFunctions;
	static void InitDict();

	static const char* GetInternalName(int functionID);

	static bool IsDefinitionAvailable(int functionID);
	static MKOFunctionDefinition GetDefinition(int functionID);
};