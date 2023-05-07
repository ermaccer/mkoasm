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
	int functionSet;
	int num_arguments;
	std::vector<EMKOFunctionArgumentDefinition_Type> args;
};


struct HashEntry {
	unsigned int hash;
	char name[128] = {};
};

unsigned int _hash(const char* input);

class MKODict {
public:
	static EGameMode ms_gameMode;

	static std::vector<MKOFunctionDefinition> ms_vFunctions;
	static std::vector<HashEntry> ms_vHashes;
	static void InitDict(EGameMode game);
	static void InitHashTable();

	static void hash2txt();
	static void txt2hash();

	static const char* GetInternalName(int functionID);
	static int GetInternalID(const char* name);
	static bool IsFunctionInternal(const char* name);

	static bool IsDefinitionAvailable(int functionID, int functionSet = 0);
	static bool IsDefinitionAvailable(const char* name);
	static MKOFunctionDefinition GetDefinition(int functionID, int functionSet = 0);
	static MKOFunctionDefinition GetDefinition(const char* name);
	static std::string GetHashString(unsigned int hash);
	static bool IsHashAvailable(unsigned int hash);
};