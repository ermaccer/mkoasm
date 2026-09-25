#include "MKOCompiler.h"
#include "..\code\MKODict.h"
#include "..\code\misc.h"

#include <iostream>
#include <string>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

void MKOCompiler::ParseFunctionLineMK9(char* line, bool& error, std::ofstream& file, std::vector<MKOGeneratedFixup>& fixups, std::vector<MKOVariableEntry>& variables)
{
	char buff[512] = {};
	char args[1024] = {};
	sprintf(buff, line);

	// name

	char* pLine = strtok(buff, "(");

	std::string functionName = pLine;
	std::string fullLine = line;

	if (functionName == "pad")
	{
		int pad = 0;
		file.write((char*)&pad, sizeof(int));
		return;
	}

	int funcType = -1;
	int funcID = 0;
	int funcSet = 0;
	int flags = 0;
	int numArgs = 0;
	int expectedArgs = 0;
	bool isInternal = false;
	std::vector<MKOFunctionArgumentDefinition> argsData;
	std::vector<int> argFixupType;
	std::vector<int> argFixupData;
	MKOFunctionDefinition def;


	std::string baseName = functionName;
	size_t under = functionName.find_last_of("_");
	if (under != std::string::npos && under + 1 < functionName.size())
	{
		bool allDigits = true;
		for (size_t i = under + 1; i < functionName.size(); i++)
		{
			if (!isdigit((unsigned char)functionName[i]))
			{
				allDigits = false;
				break;
			}
		}

		if (allDigits)
		{
			std::string temp = functionName.substr(0, under);
			if (MKODict::IsFunctionInternal(temp.c_str()) || MKODict::IsDefinitionAvailable(temp.c_str()))
			{
				baseName = temp;
				flags = atoi(functionName.c_str() + under + 1);
			}
		}
	}

	bool def_available = MKODict::IsDefinitionAvailable(baseName.c_str());

	if (def_available)
	{
		def = MKODict::GetDefinition(baseName.c_str());
		funcType = def.functionType;
		funcSet = def.functionSet;
		funcID = def.functionID;
		flags = def.functionFlags;
		expectedArgs = def.args.size();
	}
	else if (MKODict::IsFunctionInternal(baseName.c_str()))
	{
		funcType = MKODict::GetInternalID(baseName.c_str()) + 1;
		isInternal = true;
	}
	else
	{
		SetColor(CT_Warning);
		printf("WARNING: Definition for %s is not available! Detailed information is not available.\n", functionName.c_str());

		if (functionName.find("function_") != std::string::npos)
		{
			std::string funcInfo = functionName.substr(strlen("function_"));
			sscanf(funcInfo.c_str(), "%d_%d_%d_%d", &funcType, &funcSet, &funcID, &flags);
			isInternal = (funcType != 65);
		}
		else
		{
			SetColor(CT_Error);
			printf_s("ERROR: Cannot get the function ID of %s.\n", functionName.c_str());
			error = true;
			SetColor(CT_Reset);
			return;
		}
	}
	SetColor(CT_Reset);

	// arguments
	pLine = strtok(NULL, ")");

	if (fullLine.find("();") == std::string::npos && pLine != NULL)
	{
		sprintf(args, pLine);

		char* argLine = strtok(args, ",");
		while (!(argLine == NULL) && !(strcmp(argLine, ";") == 0))
		{
			while (*argLine == ' ' || *argLine == '\t')
				argLine++;

			MKOFunctionArgumentDefinition arg;
			int fixupType = -1;
			int fixupData = 0;

			if (strncmp(argLine, "var:", 4) == 0)
			{
				std::string varName = argLine + 4;
				int varOffset = -1;

				for (unsigned int i = 0; i < variables.size(); i++)
				{
					if (variables[i].name == varName)
					{
						varOffset = variables[i].var_mk9.offset;
						break;
					}
				}

				if (varOffset < 0)
				{
					SetColor(CT_Error);
					printf("ERROR: Unknown variable %s\n", varName.c_str());
					error = true;
					SetColor(CT_Reset);
					return;
				}

				arg.type = EMKOFAD_UInt;
				arg.data.uint = 0x40000000 | varOffset;
			}
			else if (strncmp(argLine, "asset:", 6) == 0)
			{
				fixupType = MK9_Fixup_Asset;
				fixupData = atoi(argLine + 6);
				arg.type = EMKOFAD_UInt;
				arg.data.uint = fixupData + 1;
			}
			else if (strncmp(argLine, "sound:", 6) == 0)
			{
				fixupType = MK9_Fixup_Sound;
				fixupData = atoi(argLine + 6);
				arg.type = EMKOFAD_UInt;
				arg.data.uint = fixupData + 1;
			}
			else if (strncmp(argLine, "str:", 4) == 0)
			{
				fixupType = MK9_Fixup_String;
				fixupData = atoi(argLine + 4);
				arg.type = EMKOFAD_UInt;
				arg.data.uint = fixupData + 1;
			}
			else if (strncmp(argLine, "func:", 5) == 0)
			{
				fixupType = MK9_Fixup_Function;
				fixupData = atoi(argLine + 5);
				arg.type = EMKOFAD_UInt;
				arg.data.uint = fixupData + 1;
			}
			else if (strncmp(argLine, "ext:", 4) == 0)
			{
				arg.type = EMKOFAD_UInt;
				arg.data.uint = 0x80000000 | atoi(argLine + 4);
			}
			else if (strncmp(argLine, "0x", 2) == 0 || strncmp(argLine, "0X", 2) == 0)
			{
				arg.type = EMKOFAD_Hex;
				sscanf(argLine, "%x", &arg.data.uint);
			}
			else if (def_available && numArgs < expectedArgs)
			{
				EMKOFunctionArgumentDefinition_Type type = def.args[numArgs];
				arg.type = type;
				switch (type)
				{
				case EMKOFAD_Integer:
					sscanf(argLine, "%d", &arg.data.integer);
					break;
				case EMKOFAD_Float:
					sscanf(argLine, "%f", &arg.data.flt);
					break;
				case EMKOFAD_Short:
					sscanf(argLine, "%d", &arg.data.word);
					break;
				case EMKOFAD_UInt:
					sscanf(argLine, "%d", &arg.data.uint);
					break;
				case EMKOFAD_String:
					printf("INFO: String arguments are not supported for now.");
					break;
				case EMKOFAD_Hex:
					sscanf(argLine, "%x", &arg.data.uint);
					break;
				default:
					break;
				}
			}
			else
			{
				arg.type = EMKOFAD_Integer;
				sscanf(argLine, "%d", &arg.data.integer);
			}

			argsData.push_back(arg);
			argFixupType.push_back(fixupType);
			argFixupData.push_back(fixupData);

			argLine = strtok(NULL, ",");
			numArgs++;
		}
	}

	if (def_available)
	{
		SetColor(CT_Warning);
		if (numArgs > expectedArgs)
		{
			printf_s("WARNING: Too many arguments for %s! %d required, got %d.\n", baseName.c_str(), expectedArgs, numArgs);
		}

		if (numArgs < expectedArgs)
		{
			printf_s("WARNING: Not enough arguments for %s! %d required, got %d.\n", baseName.c_str(), expectedArgs, numArgs);
		}
	}
	SetColor(CT_Reset);

	SetColor(CT_Good);
	printf_s("INFO: %-35s\t Args: %03d Definition: %s\n", functionName.c_str(), numArgs, def_available ? "yes" : "no");

	int numData = numArgs + (isInternal ? 0 : 1);

	int a1 = funcType;
	int a2 = flags;
	int a3 = MAKELONG(numData, 0);

	file.write((char*)&a1, sizeof(int));
	file.write((char*)&a2, sizeof(int));
	file.write((char*)&a3, sizeof(int));

	if (!isInternal)
	{
		int funcData = MAKELONG(funcID, funcSet);
		file.write((char*)&funcData, sizeof(int));
	}

	for (unsigned int i = 0; i < argsData.size(); i++)
	{
		MKOFunctionArgumentDefinition arg = argsData[i];

		if (argFixupType[i] >= 0)
		{
			MKOGeneratedFixup fx;
			fx.type = argFixupType[i];
			fx.data = argFixupData[i];
			fx.offset = (int)file.tellp();
			fixups.push_back(fx);
		}

		switch (arg.type)
		{
		case EMKOFAD_Integer:
			file.write((char*)&arg.data.integer, sizeof(int));
			break;
		case EMKOFAD_Float:
			file.write((char*)&arg.data.flt, sizeof(float));
			break;
		case EMKOFAD_Short:
			file.write((char*)&arg.data.word, sizeof(int));
			break;
		case EMKOFAD_UInt:
			file.write((char*)&arg.data.uint, sizeof(int));
			break;
		case EMKOFAD_String:
			printf("N/A\n");
			break;
		case EMKOFAD_Hex:
			file.write((char*)&arg.data.uint, sizeof(int));
			break;
		default:
			break;
		}
	}

	SetColor(CT_Reset);
}

void MKOCompiler::ParseFunctionLine(char* line, bool& error, std::ofstream& file, EGameMode game)
{
	char buff[512] = {};
	char args[1024] = {};
	sprintf(buff, line);

	// name
	
	char* pLine = strtok(buff, "(");

	std::string functionName = pLine;
	std::string fullLine = line;

	if (functionName == "pad")
	{
		int pad = 0;
		file.write((char*)&pad, sizeof(int));
		return;
	}
	
	int funcID = -1;
	int funcSet = -1;
	int numArgs = 0;
	int expectedArgs = 0;
	bool isInternal = false;
	std::vector<MKOFunctionArgumentDefinition> argsData;
	MKOFunctionDefinition def;

	bool def_available = MKODict::IsDefinitionAvailable(functionName.c_str());

	if (def_available)
	{
		def = MKODict::GetDefinition(functionName.c_str());
		funcID = def.functionID;

		if (game == Game_Armageddon)
			funcSet = def.functionSet;

		expectedArgs = def.args.size();
	}
	else if (MKODict::IsFunctionInternal(functionName.c_str()))
	{
		funcID = MKODict::GetInternalID(functionName.c_str());
		isInternal = true;
		if (game == Game_Armageddon)
			funcSet = 0;
	}
	else
	{
		SetColor(CT_Warning);
		printf("WARNING: Definition for %s is not available! Detailed information is not available.\n", functionName.c_str());
		if (size_t pos = functionName.find("function_") != std::string::npos)
		{
			if (game == Game_Deception)
			{
				std::string number = functionName.substr(functionName.find_last_of("function_") + 1);
				std::stringstream ss(number);
				ss >> funcID;
			}
		

			if (game == Game_Armageddon)
			{
				std::string combo = functionName.substr(strlen("function_"));
				std::string setStr = combo.substr(0, combo.find_first_of("_"));
				std::string funcStr = combo.substr(combo.find_last_of("_") + 1);

				std::stringstream set(setStr);
				set >> funcSet;

				std::stringstream func(funcStr);
				func >> funcID;

			}
		}
		else
		{
			SetColor(CT_Error);
			printf_s("ERROR: Cannot get the function ID of %s.\n", functionName.c_str());
			error = true;
			return;
		}

	}
	SetColor(CT_Reset);

	// arguments
	pLine = strtok(NULL, ")");
	std::string argStr = pLine;

	if (fullLine.find("();") == std::string::npos)
	{
		sprintf(args, pLine);

		char* argLine = strtok(args, ",");
		while (!(argLine == NULL) && !(strcmp(argLine, ";") == 0))
		{
			MKOFunctionArgumentDefinition arg;
			if (def_available)
			{
				EMKOFunctionArgumentDefinition_Type type = def.args[numArgs];
				arg.type = type;
				switch (type)
				{
				case EMKOFAD_Integer:
					sscanf(argLine, "%d", &arg.data.integer);
					break;
				case EMKOFAD_Float:
					sscanf(argLine, "%f", &arg.data.flt);
					break;
				case EMKOFAD_Short:
					sscanf(argLine, "%d", &arg.data.word);
					break;
				case EMKOFAD_UInt:
					sscanf(argLine, "%d", &arg.data.uint);
					break;
				case EMKOFAD_String:
					printf("INFO: String arguments are not supported for now.");
					break;
				case EMKOFAD_Hex:
					sscanf(argLine, "%x", &arg.data.uint);
					break;
				default:
					break;
				}
				argsData.push_back(arg);
			}
			else
			{
				arg.type = EMKOFAD_Integer;
				sscanf(argLine, "%d", &arg.data.integer);
				argsData.push_back(arg);
			}

			argLine = strtok(NULL, ",");
			numArgs++;
		}
	}
	

	if (def_available)
	{
		SetColor(CT_Warning);
		if (numArgs > expectedArgs)
		{
			printf_s("WARNING: Too many arguments for %s! %d required, got %d.\n", functionName.c_str(), expectedArgs, numArgs);
		}


		if (numArgs < expectedArgs)
		{
			printf_s("WARNING: Not enough arguments for %s! %d required, got %d.\n", functionName.c_str(), expectedArgs, numArgs);
		}


	}
	SetColor(CT_Reset);

	SetColor(CT_Good);
	printf_s("INFO: %-35s\t Args: %03d Definition: %s\n", functionName.c_str(), numArgs, def_available ? "yes" : "no");


	int a1 = 0;
	int a2 = 0;



	if (game == Game_Deception || game == Game_Unchained)
		a1 = MAKELONG(funcID + 1, isInternal ? 0xFF00 : 0);
	else if (game == Game_Armageddon)
	{
		short outSet = funcSet;
		changeEndSHORT(&outSet);
		a1 = MAKELONG(funcID + 1, outSet);
	}



	file.write((char*)&a1, sizeof(int));

	a2 = MAKELONG(numArgs, -1);
	file.write((char*)&a2, sizeof(int));

	for (unsigned int i = 0; i < argsData.size(); i++)
	{
		MKOFunctionArgumentDefinition arg = argsData[i];
		switch (arg.type)
		{
		case EMKOFAD_Integer:
			file.write((char*)&arg.data.integer, sizeof(int));
			break;
		case EMKOFAD_Float:
			file.write((char*)&arg.data.flt, sizeof(float));
			break;
		case EMKOFAD_Short:
			file.write((char*)&arg.data.word, sizeof(int));
			break;
		case EMKOFAD_UInt:
			file.write((char*)&arg.data.integer, sizeof(int));
			break;
		case EMKOFAD_String:
			printf("N/A\n");
			break;
		case EMKOFAD_Hex:
			file.write((char*)&arg.data.integer, sizeof(int));
			break;
		default:
			break;
		}
	}
#ifdef _DEBUG

	for (int i = 0; i < argsData.size(); i++)
	{
		MKOFunctionArgumentDefinition arg = argsData[i];
		printf("Arg %d:", i);
		switch (arg.type)
		{
		case EMKOFAD_Integer:
			printf("%d\n", arg.data.integer);
			break;
		case EMKOFAD_Float:
			printf("%f\n", arg.data.flt);
			break;
		case EMKOFAD_Short:
			printf("%d\n", arg.data.word);
			break;
		case EMKOFAD_UInt:
			printf("%d\n", arg.data.uint);
			break;
		case EMKOFAD_String:
			printf("N/A\n");
			break;
		case EMKOFAD_Hex:
			printf("0x%X\n", arg.data.uint);
			break;
		default:
			break;
		}
	}
#endif // _DEBUG

	SetColor(CT_Reset);
}

void MKOCompiler::CompileFile(const char* file, EGameMode game, const char* variablesFile)
{
	FILE* pFile = fopen(file, "rb");
	if (pFile)
	{
		std::string output = file;
		output = output.substr(0, output.find_last_of("."));

		std::ofstream oFile(output, std::ofstream::binary);

		char szLine[2048] = {};
		bool error = false;
		int line = 0;

		std::vector<MKOVariableEntry> vars;
		std::vector<MKOGeneratedFixup> fixups;
		if (game >= Game_MK9)
		{
			if (!MKOReader::ReadVariablesMK9(variablesFile, vars))
			{
				SetColor(CT_Error);
				printf("MK9+ requires variables list! Failed to open or not provided\n");
				SetColor(CT_Reset);

			}
		}

		SetColor(CT_Info);
		printf("Processing file: %s\n", file);
		SetColor(CT_Reset);

		while (fgets(szLine, sizeof(szLine), pFile))
		{
			line++;
			if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
				continue;


			MKOCodeEntry c;
			if (game == Game_MK9)
				ParseFunctionLineMK9(szLine, error, oFile, fixups, vars);
			else
				ParseFunctionLine(szLine, error, oFile, game);

			SetColor(CT_Reset);

			if (error)
			{
				SetColor(CT_Error);
				printf("An error occured on line %d in \"%s\". Compilation stopped.\n", line, file);
				oFile.close();
				std::remove(output.c_str());
				SetColor(CT_Reset);
				break;
			}

		}
		if (!error)
		{
			if (game == Game_MK9)
			{
				std::string fixupsName = output + "_fixups.cfg";
				std::string funcName = output;
				size_t slash = funcName.find_last_of("/\\");
				if (slash != std::string::npos)
					funcName = funcName.substr(slash + 1);
				FILE* pOut = fopen(fixupsName.c_str(), "wb");
				if (pOut)
				{
					fprintf(pOut, "; fixups dump\n");
					fprintf(pOut, "; format: \n");
					fprintf(pOut, "; type ref base offset data extra\n");
					for (unsigned int i = 0; i < fixups.size(); i++)
					{
						fprintf(pOut, "%d\tcode\t %s\t %d\t %d\t %d\t\n", fixups[i].type, funcName.c_str(), fixups[i].offset, fixups[i].data, 0);
					}

					fclose(pOut);

					SetColor(CT_Info);
					printf("Wrote %d fixups to %s\n", (int)fixups.size(), fixupsName.c_str());
					SetColor(CT_Reset);
				}
			}

			SetColor(CT_Info);
			printf("Build succesful. Output: %s\n", output.c_str());
			SetColor(CT_Reset);
		}

		fclose(pFile);
	}
}

void MKOCompiler::SetColor(EColorType ct)
{
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	switch (ct)
	{
	case CT_Warning:
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		break;
	case CT_Error:
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
		break;
	case CT_Good:
		SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		break;
	case CT_Reset:
		SetConsoleTextAttribute(hConsole, 7);
		break;
	case CT_Info:
		SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		break;
	default:
		break;
	}
#endif // _WIN32

}
