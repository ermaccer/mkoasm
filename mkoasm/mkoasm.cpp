// mkoasm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <memory>

#include "code/MKScript.h"
#include "MKOReader.h"
#include "compiler/MKOCompiler.h"


int main(int argc, char* argv[])
{
	bool _d_switch = false;
	bool _b_switch = false;
	bool _v_switch = false;
	bool _e_switch = false;
	bool _g_switch = false;
	bool _c_switch = false;
	bool _p_switch = false;
	std::string m_param;
	std::string a_param;

	if (argc == 1) {
		std::cout << "Usage: mkoasm <optional params> <file>\n"
			<< "    -b  Build hash database.\n"
			<< "    -d	Adds offset and size to every decompiled instruction.\n"
			<< "    -v  Only prints MKO information.\n"
			<< "    -g  Gamecube/Wii file.\n"
			<< "    -e  Extracts data only.\n"
			<< "    -c  Compile specified file.\n"
			<< "    -p  Pack variable.\n"
			<< "    -a <arg> Argument for variable packer.\n"
			<< "    -m <mode>  Set mode: mku, mkd, mka, mkda, mkvsdc, mk9, mk9_vita, inj\n"
#ifdef _M_X64
			<< "	X64 modes: mkx (mk10), i2 (dcf2), mk11, mk12\n"
#endif // _M_X64
			;

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
		case 'b': _b_switch = true;
			break;
		case 'd': _d_switch = true;
			break;
		case 'v': _v_switch = true;
			break;
		case 'e': _e_switch = true;
			break;
		case 'g': _g_switch = true;
			break;
		case 'c': _c_switch = true;
			break;
		case 'p': _p_switch = true;
			break;
		case 'm':
			i++;
			m_param = argv[i];
			break;
		case 'a':
			i++;
			a_param = argv[i];
			break;
		default:
			std::cout << "ERROR: Param does not exist: " << argv[i] << std::endl;
			return 0;
			break;
		}
	}

	if (_b_switch)
	{
		MKODict::txt2hash();
		return 0;
	}

	EGameMode game = Game_Unchained;

	if (m_param == "mkd") game = Game_Deception;
	if (m_param == "mku") game = Game_Unchained;
	if (m_param == "mka") game = Game_Armageddon;
	if (m_param == "mkda") game = Game_DeadlyAlliance;
	if (m_param == "mkvsdc") game = Game_MKVSDC;
	if (m_param == "mk9") game = Game_MK9;
	if (m_param == "mk9_vita") game = Game_MK9_Vita;
	if (m_param == "inj") game = Game_Injustice;
	if (m_param == "mkx" || m_param == "mk10") game = Game_MK10;
	if (m_param == "dcf2" || m_param == "i2") game = Game_Injustice2;
	if (m_param == "mk11") game = Game_MK11;
	if (m_param == "mk12") game = Game_MK12;

	if (game >= Game_MK10 && !MKOReader::Is64BitSupported())
	{
		std::cout << "ERROR: MK10+ is only supported in 64bit version of mkoasm!" << std::endl;
		return 0;
	}


	const char* path = nullptr;
	if (argc > 2)
		path = argv[argc - 1];
	else if (argc == 2)
		path = argv[1];

	if (!path)
		return 0;


	MKODict::InitDict(game);
	if (game > Game_MKVSDC)
		MKODict::InitHashTable();


	if (_p_switch)
	{
		MKOReader::Pack(path, a_param, game);
		return 0;
	}

	if (_c_switch)
	{
		MKOCompiler::CompileFile(path);
		return 0;
	}

	MKOReader mko(path, _g_switch, game);

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