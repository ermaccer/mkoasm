#pragma once
#include <vector>
#include <string>
#include "..\enums.h"

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


struct MidwayHashEntry {
	unsigned int hash;
	char name[256] = {};
};


class MKODict {
public:
	static std::vector<MKOFunctionDefinition> ms_vFunctions;
	static std::vector<MidwayHashEntry> ms_vHashes;
	static void InitDict(EGameMode game);
	static void InitHashTable();

	static const char* GetInternalName(int functionID);

	static bool IsDefinitionAvailable(int functionID);
	static MKOFunctionDefinition GetDefinition(int functionID);
	static std::string GetHashString(unsigned int hash);
	static bool IsHashAvailable(unsigned int hash);
};