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

	static void ParseFunctionLine(char* line, bool& error, std::ofstream& file);
	static void CompileFile(const char* file);

	static void SetColor(EColorType ct);
};