#pragma once
#include <vector>
#include <string>
#include "..\enums.h"

//#define SORT_FUNCTIONS

enum EMKOFunctionArgumentDefinition_Type {
	EMKOFAD_Integer,
	EMKOFAD_Float,
	EMKOFAD_Short,
	EMKOFAD_UInt,
	EMKOFAD_String,
	EMKOFAD_Hex
};

struct MKOFunctionArgumentDefinition {
	EMKOFunctionArgumentDefinition_Type type;
	union argData {
		float flt;
		int integer;
		unsigned int uint;
		short word;
	}data;
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
	static int GetInternalID(const char* name);
	static bool IsFunctionInternal(const char* name);

	static bool IsDefinitionAvailable(int functionID);
	static bool IsDefinitionAvailable(const char* name);
	static MKOFunctionDefinition GetDefinition(int functionID);
	static MKOFunctionDefinition GetDefinition(const char* name);
	static std::string GetHashString(unsigned int hash);
	static bool IsHashAvailable(unsigned int hash);
};