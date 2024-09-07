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

class MKOCompiler {
public:

	static void ParseFunctionLine(char* line, bool& error, std::ofstream& file, EGameMode game = Game_Deception);
	static void CompileFile(const char* file, EGameMode game = Game_Deception);

	static void SetColor(EColorType ct);
};