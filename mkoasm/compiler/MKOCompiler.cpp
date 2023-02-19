#include "MKOCompiler.h"
#include "..\code\MKODict.h"

#include <iostream>
#include <string>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

void MKOCompiler::ParseFunctionLine(char* line, bool& error, std::ofstream& file)
{
	char buff[512] = {};
	char args[1024] = {};
	sprintf(buff, line);

	// name
	
	char* pLine = strtok(buff, "(");

	std::string functionName = pLine;
	std::string fullLine = line;
	
	int funcID = -1;
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
		expectedArgs = def.args.size();
	}
	else if (MKODict::IsFunctionInternal(functionName.c_str()))
	{
		funcID = MKODict::GetInternalID(functionName.c_str());
		isInternal = true;
	}
	else
	{
		SetColor(CT_Warning);
		printf("WARNING: Definition for %s is not available! Detailed information is not available.\n", functionName.c_str());
		if (size_t pos = functionName.find("function_") != std::string::npos)
		{
			std::string number = functionName.substr(functionName.find_last_of("function_") + 1);
			std::stringstream ss(number);
			ss >> funcID;
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


	int a1 = MAKELONG(funcID + 1, isInternal ? 0xFF00 : 0);
	file.write((char*)&a1, sizeof(int));
	int a2 = MAKELONG(numArgs, -1);
	file.write((char*)&a2, sizeof(int));

	for (int i = 0; i < argsData.size(); i++)
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

void MKOCompiler::CompileFile(const char* file)
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
		SetColor(CT_Info);
		printf("Processing file: %s\n", file);
		SetColor(CT_Reset);
		while (fgets(szLine, sizeof(szLine), pFile))
		{
			line++;
			if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
				continue;


			MKOCodeEntry c;
			ParseFunctionLine(szLine, error, oFile);

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
