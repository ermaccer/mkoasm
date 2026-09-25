#pragma once
#include "..\MKOReader.h"
#include <fstream>

enum EColorType {
	CT_Warning,
	CT_Error,
	CT_Good,
	CT_Reset,
	CT_Info
};

struct MKOGeneratedFixup {
	int type;
	int data;
	int offset;
};

class MKOCompiler {
public:
	static void ParseFunctionLineMK9(char* line, bool& error, std::ofstream& file, std::vector<MKOGeneratedFixup>& fixups, std::vector<MKOVariableEntry>& variables);
	static void ParseFunctionLine(char* line, bool& error, std::ofstream& file, EGameMode game = Game_Deception);
	static void CompileFile(const char* file, EGameMode game = Game_Deception, const char* variablesFile = nullptr);

	static void SetColor(EColorType ct);
};