// mkoasm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <memory>

#include "code/MKScript.h"
#include "MKOReader.h"

int main(int argc, char* argv[])
{
	bool _d_switch = false;
	bool _v_switch = false;
	bool _e_switch = false;
	bool _g_switch = false;
	std::string m_param;

	if (argc == 1) {
		std::cout << "Usage: mkoasm <optional params> <file>\n"
			<< "    -d	Adds offset and size to every decompiled instruction.\n"
			<< "    -v  Only prints MKO information.\n"
			<< "    -g  Gamecube/Wii file.\n"
			<< "    -e  Extracts data only.\n"
			<< "    -m <mode>  Set mode: mku, mkd, mka, mkda, mkvsdc.\n";
		return 1;
	}

	// params
	for (int i = 1; i < argc - 1; i++)
	{
		if (argv[i][0] != '-' || strlen(argv[i]) != 2) {
			return 1;
		}
		switch (argv[i][1])
		{
		case 'd': _d_switch = true;
			break;
		case 'v': _v_switch = true;
			break;
		case 'e': _e_switch = true;
			break;
		case 'g': _g_switch = true;
			break;
		case 'm':
			i++;
			m_param = argv[i];
			break;
		default:
			std::cout << "ERROR: Param does not exist: " << argv[i] << std::endl;
			return 0;
			break;
		}
	}


	const char* path = nullptr;
	if (argc > 2)
		path = argv[argc - 1];
	else if (argc == 2)
		path = argv[1];

	if (!path)
		return 0;

	EGameMode game = Game_Deception;

	if (m_param == "mkd") game = Game_Deception;
	if (m_param == "mku") game = Game_Deception;
	if (m_param == "mka") game = Game_Armageddon;
	if (m_param == "mkda") game = Game_DeadlyAlliance;
	if (m_param == "mkvsdc") game = Game_MKVSDC;

    MKOReader mko(path, _g_switch, game);

	if (game == Game_MKVSDC)
		return 0;

	MKODict::InitDict(game);

	if (mko && !mko.m_bBuildMode)
	{
		mko.m_bDebugMKO = _d_switch;
		mko.m_bExtractOnly = _e_switch;
		if (!_v_switch)
			mko.ExtractData();
		else
			mko.PrintInfo();
	}

   
    return 0;
}