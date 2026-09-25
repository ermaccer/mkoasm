#include "MKOReader.h"
#include <iostream>
#include <filesystem>
#include <memory>
#include <string>
#include "code/FileFunctions.h"
#include "IniReader.h"
#include <Windows.h>
#include "code/misc.h"
#include <algorithm>
#include <string>

#ifdef _M_X64
#include "lz4/lz4.h"
#pragma comment(lib, "lz4/liblz4_static.lib")
#endif

MKOReader::MKOReader(const char* file, bool isGameCube, EGameMode _game)
{
    std::string path = file;

    if (checkSlash(path))
    {
        std::string folder = splitString(path,  true);
        m_szInputName = folder;
    }
    else
        m_szInputName = file;

    m_bGameCube = isGameCube;
    game = _game;

    bool build = std::filesystem::is_directory(m_szInputName);

    if (build && !IsBuildingSupported(_game))
    {
        build = false;
        std::cout << "ERROR: Building is not supported for selected game." << std::endl;
        m_bIsValid = false;
        return;
    }

    m_bBuildMode = build;


    if (!build)
    {
        switch (game)
        {
        case Game_Deception:
        case Game_Armageddon:
        case Game_DeadlyAlliance:
        case Game_Unchained:
            m_bIsValid = Read(file);
            break;
        case Game_MKVSDC:
            m_bIsValid = ReadMK8();
            break;
        case Game_MK9:
        case Game_MK9_Vita:
            m_bIsValid = ReadMK9();
            break;
        case Game_Injustice:
            m_bIsValid = ReadDCF();
            break;
        case Game_MK10:
            m_bIsValid = ReadMK10();
            break;
        case Game_Injustice2:
            m_bIsValid = ReadDCF2();
            break;
        case Game_MK11:
            m_bIsValid = ReadMK11();
            break;
        case Game_MK12:
            m_bIsValid = ReadMK12();
            break;
        default:
            break;
        }
    }
    else
    {
        switch (game)
        {
        case Game_Deception:
        case Game_Armageddon:
        case Game_Unchained:
            m_bIsValid = Build();
            break;
        case Game_MK9:
            m_bIsValid = BuildMK9();
            break;
        }
    }


}

bool MKOReader::Read(const char* file)
{
	pFile.open(file, std::ifstream::binary);
	if (pFile.is_open())
	{
        pFile.read((char*)&header, sizeof(mko_header));


        SwapINT(&header.functions);
        SwapINT(&header.last_function_offset);
        SwapINT(&header.script_string_size);
        SwapINT(&header.string_data_size);
        SwapINT(&header.unknown_data_size);
        SwapINT(&header.field20);
        SwapINT(&header.static_variables);
        SwapINT(&header.last_variable_offset);

        if (game == Game_DeadlyAlliance)
        {
            int backOffset = -16;
            pFile.seekg(backOffset, pFile.cur);

            header.unknown_data_size = 0;
            header.field20 = 0;
            header.static_variables = 0;
            header.last_variable_offset = 0;
        }

        if (game == Game_Armageddon)
        {
            int size = header.functions * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < header.functions; i++)
        {
            mko_function_header func;
            pFile.read((char*)&func, sizeof(mko_function_header));



            SwapINT(&func.name_offset);
            SwapINT(&func.offset);
            SwapINT(&func.unknown);

            if (game == Game_DeadlyAlliance)
            {
                func.unknown = 0;
                int backOffset = -4;
                pFile.seekg(backOffset, pFile.cur);
                func.name_offset++;
            }

            if (game == Game_Armageddon)
            {
                std::vector<int> funcLinks;

                int field12, field16;
                pFile.read((char*)&field12, sizeof(int));
                pFile.read((char*)&field16, sizeof(int));
                mka_funcSizes.push_back(func.unknown);

                SwapINT(&field12);
                SwapINT(&field16);

                func.unknown = field12;

                for (int a = 0; a < field16; a++)
                {
                    int funcLink = 0;
                    pFile.read((char*)&funcLink, sizeof(int));
                    SwapINT(&funcLink);
                    funcLinks.push_back(funcLink);
                }
                mka_funcLinks.push_back(funcLinks);
            }

            funcs.push_back(func);
        }

        if (game == Game_Armageddon)
        {
            int size = header.static_variables * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < header.static_variables; i++)
        {
            mko_variable_header var;
            pFile.read((char*)&var, sizeof(mko_variable_header));

            SwapINT(&var.name_offset);
            SwapINT(&var.unknown);
            SwapINT(&var.offset);
            SwapINT(&var.numElems);


            if (game == Game_Armageddon)
            {
                std::vector<int> varLinks;

                int field16;
                pFile.read((char*)&field16, sizeof(int));

                SwapINT(&field16);
                for (int a = 0; a < field16; a++)
                {
                    int varLink = 0;
                    pFile.read((char*)&varLink, sizeof(int));
                    SwapINT(&varLink);
                    varLinks.push_back(varLink);
                }
                mka_varLinks.push_back(varLinks);
            }

            vars.push_back(var);
        }



        for (int i = 0; i < vars.size(); i++)
        {
            int next_size = 0;
            if (i == vars.size() - 1)
                next_size = header.last_variable_offset;
            else
                next_size = vars[i + 1].offset * 4;


            int cur_size = vars[i].offset * 4;
            int newSize = next_size - cur_size - 4;

            if (newSize < 0)
                newSize = 0;

            var_sizes.push_back(newSize);
        }

        for (int i = 0; i < funcs.size(); i++)
        {
            int next_size = 0;
            if (i == funcs.size() - 1)
                next_size = header.last_function_offset;
            else
                next_size = funcs[i + 1].offset * 4;

            int cur_size = funcs[i].offset * 4;
            int off = 4;
            if (game == Game_DeadlyAlliance)
                off = 0;

            int newSize = next_size - cur_size - off;

            func_sizes.push_back(newSize);
        }

        script_names = std::make_unique<char[]>(header.script_string_size);
        pFile.read(script_names.get(), header.script_string_size);
        string_data = std::make_unique<char[]>(header.string_data_size);
        pFile.read(string_data.get(), header.string_data_size);

        unk_data = std::make_unique<char[]>(header.unknown_data_size);
        pFile.read(unk_data.get(), header.unknown_data_size);

        m_pDataStartOffset = (uint32_t)(pFile.tellg());

        if (game == Game_Armageddon)
            m_pFunctionsStartOffset = m_pDataStartOffset + header.last_variable_offset - sizeof(int);
        else
            m_pFunctionsStartOffset = m_pDataStartOffset + GetAllVariablesSize();


        return true;
	}
	return false;
}

bool MKOReader::ReadMK8()
{
    pFile.open(m_szInputName, std::ifstream::binary);
    if (pFile.is_open())
    {
        if (GetExtension() == ".mkscriptbinary")
        {
            std::cout << "This file has metadata, removing" << std::endl;
            int unk[7];
            int mkoNameLen;
            pFile.read((char*)&unk, sizeof(unk));
            pFile.read((char*)&mkoNameLen, sizeof(int));

            SwapINT(&mkoNameLen);

            std::unique_ptr<char[]> name = std::make_unique<char[]>(mkoNameLen);
            pFile.read(name.get(), mkoNameLen);

            std::string fileName = name.get();

            int data[5];
            pFile.read((char*)&data, sizeof(data));

            SwapINT(&data[2]);
            int size = data[2];

            std::cout << "Output: " << fileName << std::endl;

            std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(size);
            pFile.read(dataBuff.get(), size);

            std::ofstream out(fileName, std::ofstream::binary);
            out.write(dataBuff.get(), size);
            return false;
        }



        pFile.read((char*)&mk8_header, sizeof(mko_header_mk8));

        SwapINT(&mk8_header.functions);
        SwapINT(&mk8_header.static_variables);
        SwapINT(&mk8_header.externs);
        SwapINT(&mk8_header.assets);
        SwapINT(&mk8_header.field36);
        SwapINT(&mk8_header.field40);
        SwapINT(&mk8_header.field44);
        SwapINT(&mk8_header.string_size);
        SwapINT(&mk8_header.field52);
        SwapINT(&mk8_header.field56);


        // same as mka
        {
            int size = mk8_header.functions * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }


        for (int i = 0; i < mk8_header.functions; i++)
        {
            mko_function_header_mk8 func;
            pFile.read((char*)&func, sizeof(mko_function_header_mk8));


            SwapINT(&func.nameOffset);
            SwapINT(&func.argTypeOffset);
            SwapINT(&func.field8);
            SwapINT(&func.size);
            SwapINT(&func.field16);
            SwapINT(&func.unkSize);
            SwapINT(&func.field24);
            SwapINT(&func.field28);
            SwapINT(&func.id);
            SwapINT(&func.numUnk);
            SwapINT(&func.field40);

            {
                if (func.numUnk)
                {
                    int size = func.numUnk * sizeof(mko_function_header_mk8_unk);
                    std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
                    pFile.read(unkData.get(), size);
                }
            }


            mk8_funcs.push_back(func);
        }

        {
            int size = mk8_header.static_variables * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk8_header.static_variables; i++)
        {
            mko_variable_header_mk8 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk8));

            SwapINT(&var.name_offset);
            SwapINT(&var.size);
            SwapINT(&var.elemSize);
            SwapINT(&var.offset);

            mk8_vars.push_back(var);
        }


        {
            int size = mk8_header.externs * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        // new in mk8
        for (int i = 0; i < mk8_header.externs; i++)
        {
            mko_extern_mk8 ext;
            pFile.read((char*)&ext, sizeof(mko_extern_mk8));

            SwapINT(&ext.nameOffset);
            SwapINT(&ext.importName);
            SwapINT((int*)&ext.field8);
            SwapINT(&ext.field12);
            SwapINT(&ext.field16);
            SwapINT(&ext.field20);
            mk8_externs.push_back(ext);
        }
       
        {
            int size = mk8_header.assets * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        // new in mk8
        for (int i = 0; i < mk8_header.assets; i++)
        {
            mko_asset_mk8 ass;
            pFile.read((char*)&ass, sizeof(mko_asset_mk8));

            SwapINT(&ass.nameOffset);
            SwapINT(&ass.archiveNameOffset);
            SwapINT(&ass.field8);

            mk8_assets.push_back(ass);
        }


        string_data = std::make_unique<char[]>(mk8_header.string_size);
        pFile.read(string_data.get(), mk8_header.string_size);


        m_pDataStartOffset = (uint32_t)(pFile.tellg());
        m_pFunctionsStartOffset = m_pDataStartOffset + GetAllVariablesSizeMK8() + GetAllFunctionsUnkSizeMK8();
        return true;
    }
    return false;
}

bool MKOReader::ReadMK9()
{
    pFile.open(m_szInputName, std::ifstream::binary);
    if (pFile.is_open())
    {
        if (GetExtension() == ".mkscriptbinary")
        {
            std::cout << "This file has metadata, removing" << std::endl;
            int unk[6];
            int mkoNameLen;
            pFile.read((char*)&unk, sizeof(unk));
            pFile.read((char*)&mkoNameLen, sizeof(int));

            SwapINT(&mkoNameLen);

            std::unique_ptr<char[]> name = std::make_unique<char[]>(mkoNameLen);
            pFile.read(name.get(), mkoNameLen);

            std::string fileName = name.get();

            int data[3];
            pFile.read((char*)&data, sizeof(data));

            SwapINT(&data[2]);
            int size = data[2];

            std::cout << "Output: " << fileName << std::endl;

            std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(size);
            pFile.read(dataBuff.get(), size);

            std::ofstream out(fileName, std::ofstream::binary);
            out.write(dataBuff.get(), size);
            return false;
        }

        int field0;
        unsigned int hash;
        pFile.read((char*)&field0, sizeof(int));
        SwapINT(&field0);
        pFile.read((char*)&hash, sizeof(int));
        SwapINT((int*)&hash);
        headerHash = hash;

        pFile.read((char*)&mk9_header, sizeof(mko_header_mk9));

        SwapINT(&mk9_header.functions);
        SwapINT(&mk9_header.static_variables);
        SwapINT(&mk9_header.externs);
        SwapINT(&mk9_header.externVariables);
        SwapINT(&mk9_header.assets);
        SwapINT(&mk9_header.soundAssets);
        SwapINT(&mk9_header.variablesSize);
        SwapINT(&mk9_header.globalObjects);
        SwapINT(&mk9_header.bytecodeSize);
        SwapINT(&mk9_header.string_size);
        SwapINT(&mk9_header.stack_size);
        SwapINT(&mk9_header.fixups);
        SwapINT(&mk9_header.tweakVarsOffset);
        SwapINT(&mk9_header.tweakVarsSize);


        int vitaData = 0;
        if (game == Game_MK9_Vita)
            pFile.read((char*)&vitaData, sizeof(int));


        // same as mka
        {
            int size = mk9_header.functions * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }


        for (int i = 0; i < mk9_header.functions; i++)
        {
            mko_function_header_mk9 func;
            pFile.read((char*)&func, sizeof(mko_function_header_mk9));


            SwapINT(&func.nameOffset);
            SwapINT((int*)&func.functionHash);
            SwapINT(&func.functionOffset);
            SwapINT(&func.size);
            SwapINT(&func.stackOffset);
            SwapINT(&func.stackSize);
            SwapINT(&func.numArgs);
            SwapINT(&func.field28);
            SwapINT(&func.function_index);
            SwapINT(&func.local_fixup_count);
            SwapINT(&func.globalObjects);
            SwapINT((int*)&func.paramsHash);

            std::vector<mko_fixup_mk9> localFixups;
            for (int a = 0; a < func.local_fixup_count; a++)
            {
                mko_fixup_mk9 fx;
                pFile.read((char*)&fx, sizeof(mko_fixup_mk9));
                SwapINT(&fx.type);
                SwapINT(&fx.offset);
                SwapINT(&fx.data);
                SwapINT(&fx.pad);
                localFixups.push_back(fx);
            }

            mk9_funcs.push_back(func);
            mk9_local_fixups.push_back(localFixups);
        }

        {
            int size = mk9_header.static_variables * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk9_header.static_variables; i++)
        {
            mko_variable_header_mk9 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk9));

            SwapINT((int*)&var.name_hash);
            SwapINT(&var.size);
            SwapINT(&var.elemSize);
            SwapINT(&var.offset);

            mk9_vars.push_back(var);
        }


        {
            int size = mk9_header.externs * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk9_header.externs; i++)
        {
            mko_extern_mk9 ext;
            pFile.read((char*)&ext, sizeof(mko_extern_mk9));

            SwapINT((int*)&ext.nameHash);
            SwapINT(&ext.importName);
            SwapINT((int*)&ext.paramsHash);
            SwapINT((int*)&ext.externHash);
            SwapINT(&ext.field16);
            SwapINT(&ext.field20);
            SwapINT(&ext.field24);
            mk9_externs.push_back(ext);
        }

        for (int i = 0; i < mk9_header.externVariables; i++)
        {
            mko_variable_header_mk9 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk9));

            SwapINT((int*)&var.name_hash); // offset in this case
            SwapINT(&var.size);
            SwapINT(&var.elemSize);
            SwapINT(&var.offset);

            mk9_extern_vars.push_back(var);
        }

        {
            if (vitaData > 0)
            {
                int size = vitaData * sizeof(int);
                std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
                pFile.read(unkData.get(), size);
            }

        }

        for (int i = 0; i < mk9_header.globalObjects; i++)
        {
            mko_global_object_mk9 go;
            pFile.read((char*)&go, sizeof(mko_global_object_mk9));
            SwapINT(&go.data);
            SwapINT(&go.offset);

            mk9_globals.push_back(go);
        }

        {
            int size = mk9_header.assets * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk9_header.assets; i++)
        {
            mko_asset_mk9 ass;
            pFile.read((char*)&ass, sizeof(mko_asset_mk9));

            SwapINT(&ass.nameOffset);
            SwapINT(&ass.archiveNameOffset);
            SwapINT(&ass.field8);
            SwapINT(&ass.field12);
            mk9_assets.push_back(ass);
        }

        {
            int size = mk9_header.soundAssets * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        // new in mk9
        for (int i = 0; i < mk9_header.soundAssets; i++)
        {
            mko_sound_asset_mk9 sass;
            pFile.read((char*)&sass, sizeof(mko_sound_asset_mk9));

            SwapINT(&sass.nameOffset);
            SwapINT(&sass.archiveNameOffset);
            SwapINT(&sass.field8);
            SwapINT(&sass.field12);
            SwapINT(&sass.field16);
            mk9_sounds.push_back(sass);
        }

        string_data = std::make_unique<char[]>(mk9_header.string_size);
        pFile.read(string_data.get(), mk9_header.string_size);


        m_pDataStartOffset = (uint32_t)(pFile.tellg());
        m_pFunctionsStartOffset = m_pDataStartOffset + mk9_header.stack_size;

        pFile.seekg(m_pFunctionsStartOffset + mk9_header.bytecodeSize, pFile.beg);

        for (int i = 0; i < mk9_header.fixups; i++)
        {
            mko_fixup_mk9 unk;
            pFile.read((char*)&unk, sizeof(mko_fixup_mk9));

            SwapINT(&unk.type);
            SwapINT(&unk.offset);
            SwapINT(&unk.data);
            SwapINT(&unk.pad);
            mk9_fixup.push_back(unk);
        }

        return true;
    }
    return false;
}

bool MKOReader::ReadDCF()
{
    pFile.open(m_szInputName, std::ifstream::binary);
    if (pFile.is_open())
    {
        if (GetExtension() == ".mkscriptbinary")
        {
            std::cout << "This file has metadata, removing" << std::endl;
            int unk[6];
            int mkoNameLen;
            pFile.read((char*)&unk, sizeof(unk));
            pFile.read((char*)&mkoNameLen, sizeof(int));

            SwapINT(&mkoNameLen);

            std::unique_ptr<char[]> name = std::make_unique<char[]>(mkoNameLen);
            pFile.read(name.get(), mkoNameLen);

            std::string fileName = name.get();

            int data[5];
            pFile.read((char*)&data, sizeof(data));

            SwapINT(&data[2]);
            int size = data[2];

            std::cout << "Output: " << fileName << std::endl;

            std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(size);
            pFile.read(dataBuff.get(), size);

            std::ofstream out(fileName, std::ofstream::binary);
            out.write(dataBuff.get(), size);
            return false;
        }

        pFile.read((char*)&dcf_header, sizeof(mko_header_dcf));

        SwapINT(&dcf_header.functions);
        SwapINT(&dcf_header.static_variables);
        SwapINT(&dcf_header.externs);
        SwapINT(&dcf_header.externVariables);
        SwapINT(&dcf_header.assets);
        SwapINT(&dcf_header.soundAssets);
        SwapINT(&dcf_header.field24);
        SwapINT(&dcf_header.field28);
        SwapINT(&dcf_header.bytecodeSize);
        SwapINT(&dcf_header.string_size);
        SwapINT(&dcf_header.functionsOffset);
        SwapINT(&dcf_header.unknowns);
        SwapINT(&dcf_header.tweakVarsOffset);
        SwapINT(&dcf_header.tweakVarsSize);
        SwapINT(&dcf_header.field56);


        // dcf adds one more int for some reason
        {
            int size = (dcf_header.functions + 1) * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }


        for (int i = 0; i < dcf_header.functions; i++)
        {
            mko_function_header_mk9 func;
            pFile.read((char*)&func, sizeof(mko_function_header_mk9));


            SwapINT(&func.nameOffset);
            SwapINT((int*)&func.functionHash);
            SwapINT(&func.functionOffset);
            SwapINT(&func.size);
            SwapINT(&func.stackOffset);
            SwapINT(&func.stackSize);
            SwapINT(&func.numArgs);
            SwapINT(&func.field28);
            SwapINT(&func.function_index);
            SwapINT(&func.local_fixup_count);
            SwapINT(&func.globalObjects);
            {
                if (func.local_fixup_count > 0)
                {
                    for (int a = 0; a < func.local_fixup_count; a++)
                    {
                        int count = 0;
                        pFile.read((char*)&count, sizeof(int));
                        int size = count * sizeof(int);
                        std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
                        pFile.read(unkData.get(), size);
                    }

                }
            }


            dcf_funcs.push_back(func);
        }

        {
            int size = dcf_header.static_variables * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < dcf_header.static_variables; i++)
        {
            mko_variable_header_mk9 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk9));

            SwapINT((int*)&var.name_hash);
            SwapINT(&var.size);
            SwapINT(&var.elemSize);
            SwapINT(&var.offset);

            dcf_vars.push_back(var);
        }


        {
            int size = dcf_header.externs * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < dcf_header.externs; i++)
        {
            mko_extern_mk9 ext;
            pFile.read((char*)&ext, sizeof(mko_extern_mk9));

            SwapINT((int*)&ext.nameHash);
            SwapINT(&ext.importName);
            SwapINT((int*)&ext.paramsHash);
            SwapINT((int*)&ext.externHash);
            SwapINT(&ext.field16);
            SwapINT(&ext.field20);
            SwapINT(&ext.field24);
            dcf_externs.push_back(ext);
        }

        for (int i = 0; i < dcf_header.externVariables; i++)
        {
            mko_variable_header_mk9 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk9));

            SwapINT((int*)&var.name_hash); // offset in this case
            SwapINT(&var.size);
            SwapINT(&var.elemSize);
            SwapINT(&var.offset);

            dcf_dyn_vars.push_back(var);
        }

        {
            int size = (dcf_header.field28 * ((sizeof(int) * 2) * 4)) + 4;
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        {
            int size = dcf_header.assets * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < dcf_header.assets; i++)
        {
            mko_asset_dcf ass;
            pFile.read((char*)&ass, sizeof(mko_asset_dcf));

            SwapINT(&ass.nameOffset);
            SwapINT(&ass.archiveNameOffset);
            SwapINT(&ass.field4);
            SwapINT(&ass.field12);
            SwapINT(&ass.field16);
            dcf_assets.push_back(ass);
        }

        {
            int size = dcf_header.soundAssets * sizeof(int);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        // new in mk9
        for (int i = 0; i < dcf_header.soundAssets; i++)
        {
            mko_sound_asset_mk9 sass;
            pFile.read((char*)&sass, sizeof(mko_sound_asset_mk9));

            SwapINT(&sass.nameOffset);
            SwapINT(&sass.archiveNameOffset);
            SwapINT(&sass.field8);
            SwapINT(&sass.field12);
            dcf_sounds.push_back(sass);
        }

        string_data = std::make_unique<char[]>(dcf_header.string_size);
        pFile.read(string_data.get(), dcf_header.string_size);


        m_pDataStartOffset = (uint32_t)(pFile.tellg());
        m_pFunctionsStartOffset = m_pDataStartOffset;

        pFile.seekg(m_pFunctionsStartOffset + dcf_header.bytecodeSize, pFile.beg);


        return true;
    }
    return false;
}

bool MKOReader::ReadMK10()
{
    pFile.open(m_szInputName, std::ifstream::binary);
    if (pFile.is_open())
    {
        int is_raw;
        unsigned int hash;
        pFile.read((char*)&is_raw, sizeof(int));
        pFile.read((char*)&hash, sizeof(int));

        if (!is_raw)
        {
            std::cout << "ERROR: BE MKX mkos are not supported!" << std::endl;
            return false;
        }


        pFile.read((char*)&mk10_header, sizeof(mko_header_mk10));

        // padding
        {
            int64 size = (mk10_header.functions) * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk10_header.functions; i++)
        {
            mko_function_header_mk10 func;
            pFile.read((char*)&func, sizeof(mko_function_header_mk10));
            mk10_funcs.push_back(func);

            for (int a = 0; a < func.local_fixup_count; a++)
            {
                mko_fixup_mk10 local_fixup;
                pFile.read((char*)&local_fixup, sizeof(mko_fixup_mk10));
            }

        }

        {
            int64 size = mk10_header.static_variables * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk10_header.static_variables; i++)
        {
            mko_variable_header_mk10 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk10));

            mk10_vars.push_back(var);
        }


        {
            int64 size = mk10_header.dynamic_variables * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk10_header.dynamic_variables; i++)
        {
            mko_variable_header_mk10 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk10));

            mk10_dyn_vars.push_back(var);
        }


        {
            int64 size = mk10_header.externs * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk10_header.externs; i++)
        {
            mko_extern_mk10 ext;
            pFile.read((char*)&ext, sizeof(mko_extern_mk10));

            mk10_externs.push_back(ext);
        }

        for (int i = 0; i < mk10_header.extern_variables; i++)
        {
            mko_extern_var_mk10 var;
            pFile.read((char*)&var, sizeof(mko_extern_var_mk10));

            mk10_extern_vars.push_back(var);
        }

        for (int i = 0; i < mk10_header.numSource; i++)
        {
            mko_source_header_mk10 src;
            pFile.read((char*)&src, sizeof(mko_source_header_mk10));

            mk10_srcs.push_back(src);
        }

        mk10_pointers = std::make_unique<char[]>(mk10_header.total_pointers * 4);
        pFile.read(mk10_pointers.get(), mk10_header.total_pointers * 4);

        {
            int64 size = mk10_header.assets * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        if (mk10_header.total_pointers & 1)
        {
            int unk;
            pFile.read((char*)&unk, sizeof(int));
        }


        for (int i = 0; i < mk10_header.assets; i++)
        {
            mko_asset_mk10 ass;
            pFile.read((char*)&ass, sizeof(mko_asset_mk10));

            mk10_assets.push_back(ass);
        }


        string_data = std::make_unique<char[]>(mk10_header.string_size);
        pFile.read(string_data.get(), mk10_header.string_size);

        tweakstring_data = std::make_unique<char[]>(mk10_header.tweakvars_stringSize);
        pFile.read(tweakstring_data.get(), mk10_header.tweakvars_stringSize);

        m_pDataStartOffset = (uintptr_t)(pFile.tellg());
        m_pFunctionsStartOffset = m_pDataStartOffset + mk10_header.stack_size;

        pFile.seekg(m_pFunctionsStartOffset + dcf_header.bytecodeSize, pFile.beg);

        for (int i = 0; i < mk10_header.fixups; i++)
        {
            mko_fixup_mk10 fix;
            pFile.read((char*)&fix, sizeof(mko_fixup_mk10));

            mk10_fixup.push_back(fix);
        }

        return true;
    }
    return false;
}

bool MKOReader::ReadDCF2()
{
    pFile.open(m_szInputName, std::ifstream::binary);
    if (pFile.is_open())
    {
        int is_raw;
        unsigned int hash;
        pFile.read((char*)&is_raw, sizeof(int));
        pFile.read((char*)&hash, sizeof(int));

        if (!is_raw)
        {
            std::cout << "ERROR: BE DCF2 mkos are not supported!" << std::endl;
            return false;
        }


        pFile.read((char*)&dcf2_header, sizeof(mko_header_dcf2));
        m_pFunctionsStartOffset = (uintptr_t)pFile.tellg();
        pFile.seekg(dcf2_header.bytecodeSize, pFile.cur);


        // padding
        {
            int64 size = (dcf2_header.functions) * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }


        for (int i = 0; i < dcf2_header.functions; i++)
        {
            mko_function_header_mk10 func;
            pFile.read((char*)&func, sizeof(mko_function_header_mk10));
            dcf2_funcs.push_back(func);

            for (int a = 0; a < func.local_fixup_count; a++)
            {
                mko_fixup_mk10 local_fixup;
                pFile.read((char*)&local_fixup, sizeof(mko_fixup_mk10));
            }

        }

        {
            int64 size = dcf2_header.static_variables * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < dcf2_header.static_variables; i++)
        {
            mko_variable_header_mk10 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk10));

            dcf2_vars.push_back(var);
        }


        {
            int64 size = dcf2_header.dynamic_variables * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < dcf2_header.dynamic_variables; i++)
        {
            mko_variable_header_mk10 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk10));

            dcf2_dyn_vars.push_back(var);
        }


        {
            int64 size = dcf2_header.externs * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < dcf2_header.externs; i++)
        {
            mko_extern_mk10 ext;
            pFile.read((char*)&ext, sizeof(mko_extern_mk10));

            dcf2_externs.push_back(ext);
        }

        for (int i = 0; i < dcf2_header.extern_variables; i++)
        {
            mko_extern_var_mk10 var;
            pFile.read((char*)&var, sizeof(mko_extern_var_mk10));

            dcf2_extern_vars.push_back(var);
        }

        for (int i = 0; i < dcf2_header.numSource; i++)
        {
            mko_source_header_mk10 src;
            pFile.read((char*)&src, sizeof(mko_source_header_mk10));

            dcf2_srcs.push_back(src);
        }

        dcf2_pointers = std::make_unique<char[]>(dcf2_header.total_pointers * 8);
        pFile.read(dcf2_pointers.get(), dcf2_header.total_pointers * 8);

        {
            int64 size = dcf2_header.assets * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < dcf2_header.assets; i++)
        {
            mko_asset_mk10 ass;
            pFile.read((char*)&ass, sizeof(mko_asset_mk10));

            dcf2_assets.push_back(ass);
        }


        string_data = std::make_unique<char[]>(dcf2_header.string_size);
        pFile.read(string_data.get(), dcf2_header.string_size);

        tweakstring_data = std::make_unique<char[]>(dcf2_header.tweakvars_stringSize);
        pFile.read(tweakstring_data.get(), dcf2_header.tweakvars_stringSize);

        m_pDataStartOffset = (uintptr_t)(pFile.tellg());

        pFile.seekg(dcf2_header.stack_size, pFile.cur);

        for (int i = 0; i < dcf2_header.fixups; i++)
        {
            mko_fixup_mk10 fix;
            pFile.read((char*)&fix, sizeof(mko_fixup_mk10));

            dcf2_fixup.push_back(fix);
        }

        for (int i = 0; i < dcf2_header.tweakvars; i++)
        {
            mko_tweakvar_dcf2 twk;
            pFile.read((char*)&twk, sizeof(mko_tweakvar_dcf2));

            dcf2_tweakvar.push_back(twk);
        }

        return true;
    }
    return false;
}

bool MKOReader::ReadMK11()
{
    pFile.open(m_szInputName, std::ifstream::binary);
    if (pFile.is_open())
    {
        int is_raw;
        unsigned int hash;
        pFile.read((char*)&is_raw, sizeof(int));
        pFile.read((char*)&hash, sizeof(int));

        if (!is_raw)
        {
            std::cout << "ERROR: BE MK11 mkos are not supported!" << std::endl;
            return false;
        }


        pFile.read((char*)&mk11_header, sizeof(mko_header_dcf2));
        m_pFunctionsStartOffset = (uintptr_t)pFile.tellg();
        pFile.seekg(mk11_header.bytecodeSize, pFile.cur);


        // padding
        {
            int64 size = (mk11_header.functions) * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }


        for (int i = 0; i < mk11_header.functions; i++)
        {
            mko_function_header_mk11 func;
            pFile.read((char*)&func, sizeof(mko_function_header_mk11));
            mk11_funcs.push_back(func);

        }

        {
            int64 size = mk11_header.static_variables * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk11_header.static_variables; i++)
        {
            mko_variable_header_mk10 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk10));

            mk11_vars.push_back(var);
        }


        {
            int64 size = mk11_header.dynamic_variables * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk11_header.dynamic_variables; i++)
        {
            mko_variable_header_mk10 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk10));

            mk11_dyn_vars.push_back(var);
        }


        {
            int64 size = mk11_header.externs * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk11_header.externs; i++)
        {
            mko_extern_mk11 ext;
            pFile.read((char*)&ext, sizeof(mko_extern_mk11));

            mk11_externs.push_back(ext);
        }

        for (int i = 0; i < mk11_header.extern_variables; i++)
        {
            mko_extern_var_mk11 var;
            pFile.read((char*)&var, sizeof(mko_extern_var_mk11));

            mk11_extern_vars.push_back(var);
        }

        for (int i = 0; i < mk11_header.numSource; i++)
        {
            mko_source_header_mk10 src;
            pFile.read((char*)&src, sizeof(mko_source_header_mk10));

            mk11_srcs.push_back(src);
        }

        mk11_pointers = std::make_unique<char[]>(mk11_header.total_pointers * 8);
        pFile.read(mk11_pointers.get(), mk11_header.total_pointers * 8);

        {
            int64 size = mk11_header.assets * sizeof(int64);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
        }

        for (int i = 0; i < mk11_header.assets; i++)
        {
            mko_asset_mk10 ass;
            pFile.read((char*)&ass, sizeof(mko_asset_mk10));

            mk11_assets.push_back(ass);
        }


        string_data = std::make_unique<char[]>(mk11_header.string_size);
        pFile.read(string_data.get(), mk11_header.string_size);

        tweakstring_data = std::make_unique<char[]>(mk11_header.tweakvars_stringSize);
        pFile.read(tweakstring_data.get(), mk11_header.tweakvars_stringSize);

        m_pDataStartOffset = (uintptr_t)(pFile.tellg());

        pFile.seekg(mk11_header.stack_size, pFile.cur);

        for (int i = 0; i < mk11_header.fixups; i++)
        {
            mko_fixup_mk10 fix;
            pFile.read((char*)&fix, sizeof(mko_fixup_mk10));

            mk11_fixup.push_back(fix);
        }

        for (int i = 0; i < mk11_header.tweakvars; i++)
        {
            mko_tweakvar_dcf2 twk;
            pFile.read((char*)&twk, sizeof(mko_tweakvar_dcf2));

            mk11_tweakvar.push_back(twk);
        }

        return true;
    }
    return false;
}

bool MKOReader::ReadMK12()
{
#ifdef _M_X64
    pFile.open(m_szInputName, std::ifstream::binary);
    
    if (!pFile.is_open())
        return false;

    bool is_raw = m_szInputName.find("raw") != std::string::npos;

    if (is_raw)
    {
        // skip header metadata
        {
            unsigned int field0 = 0;
            int field4 = 0;
            int field8 = 0;
            int fileNameLen = 0;
            int assetNameLen = 0;

            pFile.read((char*)&field0, sizeof(int));
            pFile.read((char*)&field4, sizeof(int));
            pFile.read((char*)&field8, sizeof(int));
            pFile.read((char*)&fileNameLen, sizeof(int));

            std::unique_ptr<char[]> fileName = std::make_unique<char[]>(fileNameLen);
            pFile.read(fileName.get(), fileNameLen);

            pFile.read((char*)&assetNameLen, sizeof(int));

            std::unique_ptr<char[]> assetName = std::make_unique<char[]>(assetNameLen);
            pFile.read(assetName.get(), assetNameLen);

        }
        
        mk12_header.dynamic_variables = 0;
        mk12_header.assets = 0;
        mk12_header.functions = 0;
        mk12_header.static_variables = 0;

        pFile.read((char*)&mk12_header.functions, sizeof(int));
        m_pFunctionsStartOffset = (uintptr_t)pFile.tellg();

        for (int i = 0; i < mk12_header.functions; i++)
        {
            mko_function_header_mk12 func;

            pFile.read((char*)&func.nameOffset, sizeof(int));
            pFile.read((char*)&func.functionHash, sizeof(int));
            pFile.read((char*)&func.nameOffset2, sizeof(int));
            pFile.read((char*)&func.field12, sizeof(int));
            pFile.read((char*)&func.field16, sizeof(int));
            pFile.read((char*)&func.numLinks, sizeof(int));
            
            func.linksOffset = (uintptr_t)pFile.tellg();

            mko_link_mk12 link;
            for (int a = 0; a < func.numLinks; a++)
                pFile.read((char*)&link, sizeof(mko_link_mk12));

            pFile.read((char*)&func.numArgs, sizeof(int));

            for (int a = 0; a < func.numArgs; a++)
            {
                int arg = 0;
                pFile.read((char*)&arg, sizeof(int));
                func.args.push_back(arg);
            }

            pFile.read((char*)&func.size, sizeof(int));
            func.bytecodeOffset = (uintptr_t)pFile.tellg();

            std::unique_ptr<char[]> bytecodeData = std::make_unique<char[]>(func.size);
            pFile.read(bytecodeData.get(), func.size);

            pFile.read((char*)&func.stackSize, sizeof(int));
            func.stackOffset = (uintptr_t)pFile.tellg();

            std::unique_ptr<char[]> stackData = std::make_unique<char[]>(func.stackSize);
            pFile.read(stackData.get(), func.stackSize);

            pFile.read((char*)&func.unkData[0], 24);

            mk12_funcs.push_back(func);

        }

        // unknown new sections after functions in mk12

        {
            int data1Num = 0;
            pFile.read((char*)&data1Num, sizeof(int));

            mko_function_data1_mk12 data1;
            
            for (int i = 0; i < data1Num; i++)
                pFile.read((char*)&data1, sizeof(mko_function_data1_mk12));
        }

        {
            int data2Num = 0;
            pFile.read((char*)&data2Num, sizeof(int));

            mko_function_data2_mk12 data2;

            for (int i = 0; i < data2Num; i++)
                pFile.read((char*)&data2, sizeof(mko_function_data2_mk12));
        }

        pFile.read((char*)&mk12_header.numSource, sizeof(int));

        for (int i = 0; i < mk12_header.numSource; i++)
        {
            int srcLen = 0;
            pFile.read((char*)&srcLen, sizeof(int));

            std::unique_ptr<char[]> srcName = std::make_unique<char[]>(srcLen);
            pFile.read(srcName.get(), srcLen);

            std::string src(srcName.get(), srcLen);

            mk12_srcs.push_back(src);
        }

        pFile.read((char*)&mk12_header.static_variables, sizeof(int));

        for (int i = 0; i < mk12_header.static_variables; i++)
        {
            mko_variable_header_mk12 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk12));

            mk12_vars.push_back(var);
        }

        pFile.read((char*)&mk12_header.dynamic_variables, sizeof(int));

        for (int i = 0; i < mk12_header.dynamic_variables; i++)
        {
            mko_variable_header_mk12 var;
            pFile.read((char*)&var, sizeof(mko_variable_header_mk12));

            mk12_dyn_vars.push_back(var);
        }

        pFile.read((char*)&mk12_header.assets, sizeof(int));

        for (int i = 0; i < mk12_header.assets; i++)
        {
            mko_asset_mk12 ass;
            pFile.read((char*)&ass, sizeof(mko_asset_mk12));

            mk12_assets.push_back(ass);
        }

        pFile.read((char*)&mk12_header.string_size, sizeof(int));
        string_data = std::make_unique<char[]>(mk12_header.string_size);
        pFile.read(string_data.get(), mk12_header.string_size);

        // in mk12 all variable data has been moved to ___global function stack data for whatever reason
        for (int a = 0; a < mk12_funcs.size(); a++)
        {
            unsigned int funcHash = mk12_funcs[a].functionHash;

            if (funcHash == 0x3E247564)
            {
                m_pDataStartOffset = mk12_funcs[a].stackOffset;
                break;
            }
        }

        int unk = 0;
        pFile.read((char*)&unk, sizeof(int)); // some hash
        pFile.read((char*)&unk, sizeof(int)); // always 4

        pFile.read((char*)&mk12_header.fixups, sizeof(int));


        for (int i = 0; i < mk12_header.fixups; i++)
        {
            mko_fixup_mk12 fix;
            pFile.read((char*)&fix, sizeof(mko_fixup_mk12));

            mk12_fixup.push_back(fix);
        }

        int unk1Num = 0;
        pFile.read((char*)&unk1Num, sizeof(int));

        for (int i = 0; i < unk1Num; i++)
        {
            char data[24];
            pFile.read((char*)&data, sizeof(data));
        }

        int unk2Num = 0;
        pFile.read((char*)&unk2Num, sizeof(int));

        for (int i = 0; i < unk2Num; i++)
        {
            char data[28];
            pFile.read((char*)&data, sizeof(data));
        }



        pFile.read((char*)&mk12_header.tweakvars_stringSize, sizeof(int));
        tweakstring_data = std::make_unique<char[]>(mk12_header.tweakvars_stringSize);
        pFile.read(tweakstring_data.get(), mk12_header.tweakvars_stringSize);

        pFile.clear();

        return true;
    }
    else
    {
        unsigned int rawSize;
        pFile.read((char*)&rawSize, sizeof(unsigned int));

        unsigned int size = getSizeToEnd(pFile);

        std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(rawSize);
        std::unique_ptr<char[]> compBuff = std::make_unique<char[]>(size);

        pFile.read(compBuff.get(), size);

        int lz4_result = LZ4_decompress_safe(compBuff.get(), dataBuff.get(), size, rawSize);
        if (lz4_result > 0)
        {
            std::string output = m_szInputName;
            output.insert(0, "raw_");
            std::ofstream oFile(output, std::ofstream::binary);
            oFile.write(dataBuff.get(), rawSize);

            std::cout << "INFO: Decompressed " << m_szInputName << " as " << output << "!" << std::endl;
        }


        return true;
    }
#endif
    return false;
}

std::string MKOReader::GetExtension()
{
    std::string extension = m_szInputName.substr(m_szInputName.find_last_of("."));
    std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
    return extension;
}

int MKOReader::GetAllFunctionsSize()
{
    int nSize = 0;
    for (unsigned int i = 0; i < func_sizes.size(); i++)
    {
        nSize += func_sizes[i];
    }
    nSize += sizeof(int) * funcs.size();
    return nSize;
}

int MKOReader::GetAllVariablesSize()
{
    int nSize = 0;
    for (unsigned int i = 0; i < var_sizes.size(); i++)
    {
        if (var_sizes[i] >= 0)
        nSize += var_sizes[i];
    }
    nSize += sizeof(int) * vars.size();
    return nSize;
}

int MKOReader::GetAllVariablesSizeMK8()
{
    int nSize = 0;
    for (unsigned int i = 0; i < mk8_vars.size(); i++)
    {
        nSize += mk8_vars[i].size;
    }
    return nSize;
}

int MKOReader::GetAllFunctionsSizeMK8()
{
    return 0;
}

int MKOReader::GetAllFunctionsUnkSizeMK8()
{
    int nSize = 0;
    for (unsigned int i = 0; i < mk8_funcs.size(); i++)
    {
        nSize += mk8_funcs[i].unkSize;
    }
    return nSize;
}

std::string MKOReader::GetFileName()
{
    std::string output = m_szInputName;
    output = output.substr(0, output.length() - strlen(".mko"));
    return output;
}

std::string MKOReader::GetFunctionName(int functionID)
{
    std::string func_name = (char*)(&script_names[0] + (funcs[functionID].name_offset - 1));
    return func_name;
}

std::string MKOReader::GetFunctionNameMK8(int functionID)
{
    std::string func_name = (char*)(&string_data[0] + (mk8_funcs[functionID].nameOffset - 1));
    return func_name;
}

std::string MKOReader::GetFunctionNameMK9(int functionID)
{
    std::string func_name = (char*)(&string_data[0] + (mk9_funcs[functionID].nameOffset - 1));
    return func_name;
}

std::string MKOReader::GetFunctionNameMK9Actual(int functionID)
{
    std::string func_name = (char*)(&string_data[0] + (mk9_funcs[functionID].nameOffset - 1));
    func_name += "_";

    // some have overloaded args with only difference being params hash
    char argName[128] = {};

    sprintf(argName, "%X", mk9_funcs[functionID].paramsHash);
    func_name += argName;
    return func_name;
}

std::string MKOReader::GetFunctionNameDCF(int functionID)
{
    std::string func_name = (char*)(&string_data[0] + (dcf_funcs[functionID].nameOffset - 1));
    return func_name;
}

std::string MKOReader::GetFunctionNameMK10(int functionID)
{
    std::string func_name = (char*)(&string_data[0] + (mk10_funcs[functionID].nameOffset - 1));
    return func_name;
}

int MKOReader::GetFunctionIDWithHashMK10(unsigned int hash)
{
    int id = -1;
    for (unsigned int i = 0; i < mk10_funcs.size(); i++)
    {
        if (mk10_funcs[i].functionHash == hash)
        {
            id = i;
            break;
        }
    }
    return id;
}

std::string MKOReader::GetFunctionNameDCF2(int functionID)
{
    std::string func_name = (char*)(&string_data[0] + (dcf2_funcs[functionID].nameOffset - 1));
    return func_name;
}

int MKOReader::GetFunctionIDWithHashDCF2(unsigned int hash)
{
    int id = -1;
    for (unsigned int i = 0; i < dcf2_funcs.size(); i++)
    {
        if (dcf2_funcs[i].functionHash == hash)
        {
            id = i;
            break;
        }
    }
    return id;
}

std::string MKOReader::GetFunctionNameMK11(int functionID)
{
    std::string func_name = (char*)(&string_data[0] + (mk11_funcs[functionID].nameOffset - 1));
    return func_name;
}

int MKOReader::GetFunctionIDWithHashMK11(unsigned int hash)
{
    int id = -1;
    for (unsigned int i = 0; i < mk11_funcs.size(); i++)
    {
        if (mk11_funcs[i].functionHash == hash)
        {
            id = i;
            break;
        }
    }
    return id;
}

std::string MKOReader::GetFunctionNameMK12(int functionID)
{
    std::string func_name = (char*)(&string_data[0] + (mk12_funcs[functionID].nameOffset - 1));
    return func_name;
}

int MKOReader::GetFunctionIDWithHashMK12(unsigned int hash)
{
    int id = -1;
    for (unsigned int i = 0; i < mk12_funcs.size(); i++)
    {
        if (mk12_funcs[i].functionHash == hash)
        {
            id = i;
            break;
        }
    }
    return id;
}


uint32_t MKOReader::GetFunctionOffset(int functionID)
{
    if (game == Game_DeadlyAlliance)
        return (funcs[functionID].offset * 4);

    return (funcs[functionID].offset * 4) + 4;
}

uint32_t MKOReader::GetFunctionOffsetMK8(int functionID)
{
    uint32_t offset = m_pFunctionsStartOffset;
    for (unsigned int i = 0; i < functionID; i++)
    {
        int size = mk8_funcs[i].size;
        offset += size;
    }

    return offset;
}

uint32_t MKOReader::GetFunctionOffsetMK9(int functionID)
{
    return (mk9_funcs[functionID].functionOffset * 4);
}

uint32_t MKOReader::GetFunctionForBytecodeOffsetMK9(int offset)
{
    for (unsigned int i = 0; i < mk9_funcs.size(); i++)
    {
        int start = mk9_funcs[i].functionOffset * 4;
        if (offset >= start && offset < start + mk9_funcs[i].size)
            return i;
    }
    return -1;
}

uint32_t MKOReader::GetFunctionForStackOffsetMK9(int offset)
{
    int func = -1;
    int funcOff = -1;

    for (unsigned int i = 0; i < mk9_funcs.size(); i++)
    {
        int so = mk9_funcs[i].stackOffset;
        if (so <= offset && so > funcOff && so >= (int)mk9_header.variablesSize)
        {
            func = i;
            funcOff = so;
        }
    }
    return func;
}

uintptr_t MKOReader::GetFunctionOffsetMK10(int functionID)
{
    return (mk10_funcs[functionID].functionOffset);
}

uintptr_t MKOReader::GetFunctionOffsetDCF2(int functionID)
{
    return (dcf2_funcs[functionID].functionOffset);
}

uintptr_t MKOReader::GetFunctionOffsetMK11(int functionID)
{
    return (mk11_funcs[functionID].functionOffset);
}

uintptr_t MKOReader::GetFunctionOffsetMK12(int functionID)
{
    return mk12_funcs[functionID].bytecodeOffset;
}

std::string MKOReader::GetVariableName(int variableID)
{
    std::string var_name = (char*)(&script_names[0] + (vars[variableID].name_offset - 1));
    return var_name;
}

std::string MKOReader::GetVariableNameMK8(int variableID)
{
    std::string var_name = (char*)(&string_data[0] + (mk8_vars[variableID].name_offset - 1));
    return var_name;
}

std::string MKOReader::GetVariableNameMK9(int variableID)
{
    std::string var_name = MKODict::GetHashString(mk9_vars[variableID].name_hash);
    return var_name;
}

std::string MKOReader::GetVariableNameDCF(int variableID)
{
    std::string var_name = MKODict::GetHashString(dcf_vars[variableID].name_hash);
    return var_name;
}

std::string MKOReader::GetVariableNameMK10(int variableID)
{
    std::string var_name = MKODict::GetHashString(mk10_vars[variableID].nameHash);
    return var_name;
}

std::string MKOReader::GetVariableNameDCF2(int variableID)
{
    std::string var_name = MKODict::GetHashString(dcf2_vars[variableID].nameHash);
    return var_name;
}

std::string MKOReader::GetVariableNameMK11(int variableID)
{
    std::string var_name = MKODict::GetHashString(mk11_vars[variableID].nameHash);
    return var_name;
}

std::string MKOReader::GetVariableNameMK12(int variableID)
{
    std::string var_name = (char*)(&string_data[0] + (mk12_vars[variableID].name_offset - 1));
    return var_name;
}

uint32_t MKOReader::GetVariableOffset(int variableID)
{
    return (vars[variableID].offset * 4);
}

uint32_t MKOReader::GetVariableOffsetMK8(int variableID)
{
    return (mk8_vars[variableID].offset);
}

uint32_t MKOReader::GetVariableOffsetMK9(int variableID)
{
    return (mk9_vars[variableID].offset);
}

int MKOReader::GetVariableForOffsetMK9(int offset)
{
    for (unsigned int i = 0; i < mk9_vars.size(); i++)
    {
        if (offset >= mk9_vars[i].offset && offset < mk9_vars[i].offset + mk9_vars[i].size)
            return i;
    }
    return -1;
}

uint32_t MKOReader::GetVariableOffsetDCF(int variableID)
{
    return (dcf_vars[variableID].offset);
}

uint32_t MKOReader::GetVariableOffsetMK10(int variableID)
{
    return (mk10_vars[variableID].stack_offset);
}

uint32_t MKOReader::GetVariableOffsetDCF2(int variableID)
{
    return (dcf2_vars[variableID].stack_offset);
}

uint32_t MKOReader::GetVariableOffsetMK11(int variableID)
{
    return (mk11_vars[variableID].stack_offset);
}

uint32_t MKOReader::GetVariableOffsetMK12(int variableID)
{
    return (mk12_vars[variableID].stack_offset);
}

std::string MKOReader::GetString(int stringStart)
{
    std::string str = (char*)(&string_data[0] + (stringStart - 1));
    return str;
}

void MKOReader::ExtractData()
{
    if (game < Game_MKVSDC)
        ExtractDataMKDADU();

    switch (game)
    {
    case Game_MKVSDC:
        ExtractDataMK8();
        break;
    case Game_MK9:
    case Game_MK9_Vita:
        ExtractDataMK9();
        break;
    case Game_Injustice:
        ExtractDataDCF();
        break;
    case Game_MK10:
        ExtractDataMK10();
        break;
    case Game_Injustice2:
        ExtractDataDCF2();
        break;
    case Game_MK11:
        ExtractDataMK11();
        break;
    case Game_MK12:
        ExtractDataMK12();
        break;
    default:
        break;
    }
}

void MKOReader::ExtractDataMKDADU()
{
    std::string output_folder = m_szInputName;
    output_folder = output_folder.substr(0, output_folder.length() - strlen(".mko"));

    {
        if (!std::filesystem::exists(output_folder))
            std::filesystem::create_directory(output_folder);

        std::filesystem::current_path(output_folder);
    }

    std::string output = m_szInputName;
    output = output.substr(0, output.length() - strlen(".mko"));


    std::string info = output + "_info.txt";
    DumpInfoMKDADU(info);

    std::string hdr = output + "_header.ini";
    DumpHeader(hdr);

    std::ofstream pData("string_data", std::ofstream::binary);
    pData.write(string_data.get(), header.string_data_size);
    pData.close();

    std::ofstream pNames("names_data", std::ofstream::binary);
    pNames.write(script_names.get(), header.script_string_size);
    pNames.close();

    std::ofstream pUnk("unknown_data", std::ofstream::binary);
    pUnk.write(unk_data.get(), header.unknown_data_size);
    pUnk.close();


    std::string folderName = "unpacked";


    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
          std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);
        UnpackVariables();
        std::filesystem::current_path("..");
    }

    folderName = VARIABLESFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractVariables();
        std::filesystem::current_path("..");
    }

    folderName = FUNCTIONFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractFunctions();
        std::filesystem::current_path("..");
    }

    folderName = "decompiled";
    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        DecompileAllFunctions();
        std::filesystem::current_path("..");
    }



}

void MKOReader::ExtractDataMK8()
{
    std::string output_folder = m_szInputName;
    output_folder = output_folder.substr(0, output_folder.length() - strlen(".mko"));

    {
        if (!std::filesystem::exists(output_folder))
            std::filesystem::create_directory(output_folder);

        std::filesystem::current_path(output_folder);
    }

    std::string output = m_szInputName;
    output = output.substr(0, output.length() - strlen(".mko"));

    std::string info = output + "_info.txt";
    DumpInfoMK8(info);


    std::ofstream pData("string_data", std::ofstream::binary);
    pData.write(string_data.get(), mk8_header.string_size);
    pData.close();



    std::string folderName = "unpacked";

    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);
        UnpackVariablesMK8();
        std::filesystem::current_path("..");
    }

    folderName = VARIABLESFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractVariablesMK8();
        std::filesystem::current_path("..");
    }

    folderName = FUNCTIONFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractFunctionsMK8();
        std::filesystem::current_path("..");
    }

    folderName = "decompiled";
    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        DecompileAllFunctionsMK8();
        std::filesystem::current_path("..");
    }

}

void MKOReader::ExtractDataMK9()
{
    std::string output_folder = m_szInputName;
    output_folder = output_folder.substr(0, output_folder.length() - strlen(".mko"));

    {
        if (!std::filesystem::exists(output_folder))
            std::filesystem::create_directory(output_folder);

        std::filesystem::current_path(output_folder);
    }

    std::string output = m_szInputName;
    output = output.substr(0, output.length() - strlen(".mko"));

    std::string hdr = output + "_header.ini";
    std::string globals = output + "_globals.cfg";
    std::string externs = output + "_externs.cfg";
    std::string fixups = output + "_fixups.cfg";
    std::string fixupsLocal = output + "_local_fixups.cfg";
    std::string variables = output + "_variables.cfg";
    std::string functions = output + "_functions.cfg";

    DumpVariablesMK9(variables);
    DumpFunctionsMK9(functions);
    DumpGlobalsMK9(globals);
    DumpExternsMK9(externs);
    DumpFixupsMK9(fixups);
    DumpLocalFixupsMK9(fixupsLocal);
    DumpHeaderMK9(hdr, fixups, fixupsLocal, externs, globals, variables, functions);


    std::string info = output + "_info.txt";
    DumpInfoMK9(info);


    std::ofstream pData("string_data", std::ofstream::binary);
    pData.write(string_data.get(), mk9_header.string_size);
    pData.close();


    //std::ofstream pTwk("tweakvars_data", std::ofstream::binary);
    //std::unique_ptr<char[]> tweak_data = std::make_unique<char[]>(mk9_header.tweakVarsSize);
    //pFile.seekg(m_pDataStartOffset + mk9_header.tweakVarsOffset, pFile.beg);
    //pFile.read(tweak_data.get(), mk9_header.tweakVarsSize);
    //pTwk.write(tweak_data.get(), mk9_header.tweakVarsSize);
    //pTwk.close();

    std::string folderName = "unpacked";

    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);
        UnpackVariablesMK9();
        std::filesystem::current_path("..");
    }

    folderName = VARIABLESFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractVariablesMK9();
        ExtractDynVariablesMK9();

        std::filesystem::current_path("..");
    }

    folderName = FUNCTIONFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractFunctionsMK9();
        std::filesystem::current_path("..");
    }

    folderName = "decompiled";
    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        DecompileAllFunctionsMK9();
        std::filesystem::current_path("..");
    }


    DumpRestMK9(hdr);

}

void MKOReader::ExtractDataDCF()
{

    std::string output_folder = m_szInputName;
    output_folder = output_folder.substr(0, output_folder.length() - strlen(".mko"));

    {
        if (!std::filesystem::exists(output_folder))
            std::filesystem::create_directory(output_folder);

        std::filesystem::current_path(output_folder);
    }

    std::string output = m_szInputName;
    output = output.substr(0, output.length() - strlen(".mko"));

    std::string info = output + "_info.txt";
    DumpInfoDCF(info);


    std::ofstream pData("string_data", std::ofstream::binary);
    pData.write(string_data.get(), dcf_header.string_size);
    pData.close();


    std::ofstream pTwk("tweakvars_data", std::ofstream::binary);
    std::unique_ptr<char[]> tweak_data = std::make_unique<char[]>(dcf_header.tweakVarsSize);
    pFile.seekg(m_pDataStartOffset + dcf_header.tweakVarsOffset, pFile.beg);
    pFile.read(tweak_data.get(), dcf_header.tweakVarsSize);
    pTwk.write(tweak_data.get(), dcf_header.tweakVarsSize);
    pTwk.close();

    std::string folderName = "unpacked";

    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);
        UnpackVariablesDCF();
        std::filesystem::current_path("..");
    }

    folderName = VARIABLESFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractVariablesDCF();
        std::filesystem::current_path("..");
    }

    folderName = FUNCTIONFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractFunctionsDCF();
        std::filesystem::current_path("..");
    }
}

void MKOReader::ExtractDataMK10()
{
    std::string output_folder = m_szInputName;
    output_folder = output_folder.substr(0, output_folder.length() - strlen(".mko"));

    {
        if (!std::filesystem::exists(output_folder))
            std::filesystem::create_directory(output_folder);

        std::filesystem::current_path(output_folder);
    }

    std::string output = m_szInputName;
    output = output.substr(0, output.length() - strlen(".mko"));

    std::string info = output + "_info.txt";
    DumpInfoMK10(info);


    std::ofstream pData("string_data", std::ofstream::binary);
    pData.write(string_data.get(), mk10_header.string_size);
    pData.close();


    std::ofstream pTwk("tweakvstr_data", std::ofstream::binary);
    pTwk.write(tweakstring_data.get(), mk10_header.tweakvars_stringSize);
    pTwk.close();

    std::string folderName = "unpacked";

    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);
        UnpackVariablesMK10();
        std::filesystem::current_path("..");
    }

    folderName = VARIABLESFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractVariablesMK10();
        std::filesystem::current_path("..");
    }

    folderName = FUNCTIONFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractFunctionsMK10();
        std::filesystem::current_path("..");
    }

    folderName = "decompiled";
    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        DecompileAllFunctionsMK10();
        std::filesystem::current_path("..");
    }
}

void MKOReader::ExtractDataDCF2()
{
    std::string output_folder = m_szInputName;
    output_folder = output_folder.substr(0, output_folder.length() - strlen(".mko"));

    {
        if (!std::filesystem::exists(output_folder))
            std::filesystem::create_directory(output_folder);

        std::filesystem::current_path(output_folder);
    }

    std::string output = m_szInputName;
    output = output.substr(0, output.length() - strlen(".mko"));

    std::string info = output + "_info.txt";
    DumpInfoDCF2(info);


    std::ofstream pData("string_data", std::ofstream::binary);
    pData.write(string_data.get(), dcf2_header.string_size);
    pData.close();


    std::string folderName = "unpacked";

    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);
        UnpackVariablesDCF2();
        std::filesystem::current_path("..");
    }

    folderName = VARIABLESFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractVariablesDCF2();
        std::filesystem::current_path("..");
    }

    folderName = FUNCTIONFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractFunctionsDCF2();
        std::filesystem::current_path("..");
    }

    folderName = "decompiled";
    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        DecompileAllFunctionsDCF2();
        std::filesystem::current_path("..");
    }
}

void MKOReader::ExtractDataMK11()
{
    std::string output_folder = m_szInputName;
    output_folder = output_folder.substr(0, output_folder.length() - strlen(".mko"));

    {
        if (!std::filesystem::exists(output_folder))
            std::filesystem::create_directory(output_folder);

        std::filesystem::current_path(output_folder);
    }

    std::string output = m_szInputName;
    output = output.substr(0, output.length() - strlen(".mko"));

    std::string info = output + "_info.txt";
    DumpInfoMK11(info);


    std::ofstream pData("string_data", std::ofstream::binary);
    pData.write(string_data.get(), mk11_header.string_size);
    pData.close();


    std::string folderName = "unpacked";

    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);
        UnpackVariablesMK11();
        std::filesystem::current_path("..");
    }

    folderName = VARIABLESFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractVariablesMK11();
        std::filesystem::current_path("..");
    }

    folderName = FUNCTIONFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractFunctionsMK11();
        std::filesystem::current_path("..");
    }

    folderName = "decompiled";
    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        DecompileAllFunctionsMK11();
        std::filesystem::current_path("..");
    }
}

void MKOReader::ExtractDataMK12()
{
    std::string output_folder = m_szInputName;
    output_folder = output_folder.substr(0, output_folder.length() - strlen(".mko"));

    {
        if (!std::filesystem::exists(output_folder))
            std::filesystem::create_directory(output_folder);

        std::filesystem::current_path(output_folder);
    }

    std::string output = m_szInputName;
    output = output.substr(0, output.length() - strlen(".mko"));

    std::string info = output + "_info.txt";
    DumpInfoMK12(info);


    std::ofstream pData("string_data", std::ofstream::binary);
    pData.write(string_data.get(), mk12_header.string_size);
    pData.close();

    std::ofstream pTweakData("tweakstring_data", std::ofstream::binary);
    pTweakData.write(tweakstring_data.get(), mk12_header.tweakvars_stringSize);
    pTweakData.close();

    std::string folderName = "unpacked";

    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);
       // UnpackVariablesMK11();
        std::filesystem::current_path("..");
    }

    folderName = VARIABLESFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractVariablesMK12();
        std::filesystem::current_path("..");
    }

    folderName = FUNCTIONFOLDER_NAME;
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        ExtractFunctionsMK12();
        std::filesystem::current_path("..");
    }

    folderName = "decompiled";
    if (!m_bExtractOnly)
    {
        if (!std::filesystem::exists(folderName))
            std::filesystem::create_directory(folderName);

        std::filesystem::current_path(folderName);

        DecompileAllFunctionsMK12();
        std::filesystem::current_path("..");
    }
}

void MKOReader::ExtractVariables()
{
    pFile.seekg(m_pDataStartOffset, pFile.beg);

    std::string hdr = "..\\";
    hdr += GetFileName() + "_header.ini";
    std::ofstream oInfo(hdr, std::ofstream::app);

    for (unsigned int i = 0; i < vars.size(); i++)
    {
        int size = var_sizes[i];
        if (size < 0)
            size = 0;
        if (game == Game_Armageddon)
        {
            unsigned int varOffset = m_pDataStartOffset + GetVariableOffset(i);
            varOffset -= sizeof(int);
            pFile.clear();
            pFile.seekg(varOffset, pFile.beg);
        }
          

        std::string var_name = (char*)(&script_names[0] + (vars[i].name_offset - 1));
        int varID = 0;
        pFile.read((char*)&varID, sizeof(int));
        SwapINT(&varID);


        if (oInfo) {
            oInfo << "[Variable" << std::to_string(i) << "]" << std::endl;
            oInfo << "Name = " << var_name << std::endl;
            oInfo << "NumElems = " << vars[i].numElems << std::endl;
            oInfo << "ScriptID = " << varID << std::endl;
            oInfo << "Unknown = " << vars[i].unknown << std::endl;

            if (game == Game_Armageddon)
            {
                oInfo << "NumLinks = " << mka_varLinks[i].size() << std::endl;;

                for (unsigned int a = 0; a < mka_varLinks[i].size(); a++)
                {
                    int link = mka_varLinks[i][a];

                    int linkVal = LOWORD(link);
                    int linkType = HIWORD(link);

                    if (linkType & 0x8000)
                    {
                        oInfo << "; " << GetVariableName(linkVal - 1) << std::endl;
                        oInfo << "Link" << a << " = " << linkVal << std::endl;
                        oInfo << "Link" << a << "Type = " << linkType << std::endl;
                    }
                    else
                    {
                        std::string link_name = (char*)(&string_data[0] + (linkVal));
                        oInfo << "; " << link_name << std::endl;
                        oInfo << "Link" << a << " = " << linkVal << std::endl;
                        oInfo << "Link" << a << "Type = " << linkType << std::endl;
                    }
                    
                }
            }

            oInfo << std::endl;

        }

        printf("Extracting variable %s [%d/%d]\n", var_name.c_str(), i + 1, vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);

    }
    if (oInfo)
        oInfo.close();
}

void MKOReader::ExtractVariablesMK8()
{
    for (unsigned int i = 0; i < mk8_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK8(i), pFile.beg);
        int size = mk8_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = (char*)(&string_data[0] + (mk8_vars[i].name_offset - 1));

        printf("Extracting variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, mk8_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);

    }
}

void MKOReader::ExtractVariablesMK9()
{
    // sort by offset to get the padded size (not stored)
    std::vector<int> sorted;
    for (unsigned int i = 0; i < mk9_vars.size(); i++)
        sorted.push_back(i);

    for (unsigned int i = 0; i < sorted.size(); i++)
    {
        for (unsigned int a = i + 1; a < sorted.size(); a++)
        {
            if (mk9_vars[sorted[a]].offset < mk9_vars[sorted[i]].offset)
            {
                int tmp = sorted[i];
                sorted[i] = sorted[a];
                sorted[a] = tmp;
            }
        }
    }


    std::vector<int> totalSizes;
    totalSizes.resize(mk9_vars.size());

    for (unsigned int i = 0; i < sorted.size(); i++)
    {
        int sortID = sorted[i];

        int nextOffset = mk9_header.variablesSize;
        if (i + 1 < sorted.size())
            nextOffset = mk9_vars[sorted[i + 1]].offset;

        int totalSize = nextOffset - mk9_vars[sortID].offset;
        if (totalSize < 0)
            totalSize = 0;

        totalSizes[sortID] = totalSize;
    }

    for (unsigned int i = 0; i < mk9_vars.size(); i++)
    {
        int totalSize = totalSizes[i];

        pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK9(i), pFile.beg);
        int size = mk9_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = GetVariableNameMK9(i);

        //if (oInfo) {
        //    oInfo << "[Variable" << std::to_string(i) << "]" << std::endl;
        //    oInfo << "Name = " << var_name << std::endl;
        //    oInfo << "ElemSize = " << mk9_vars[i].elemSize << std::endl;
        //    oInfo << "Size = " << size << std::endl;
        //    if (mk9_header.tweakVarsSize > 0 && mk9_vars[i].offset >= mk9_header.tweakVarsOffset)
        //        oInfo << "IsTweakVar = 1" << std::endl;
        //
        //    oInfo << std::endl;
        //}

        printf("Extracting variable %s Size: %d Total: %d [%d/%d]\n", var_name.c_str(), size, totalSize, i + 1, mk9_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(totalSize > 0 ? totalSize : 1);
        pFile.read(data.get(), totalSize);
        oFile.write(data.get(), totalSize);
    }
}
void MKOReader::ExtractDynVariablesMK9()
{
    std::string hdr = "..\\";
    hdr += GetFileName() + "_header.ini";
    std::ofstream oInfo(hdr, std::ofstream::app);
    for (unsigned int i = 0; i < mk9_extern_vars.size(); i++)
    {

        std::string var_name = (char*)(&string_data[0] + (mk9_extern_vars[i].name_hash - 1));

        if (oInfo) {
            oInfo << "[ExternVariable" << std::to_string(i) << "]" << std::endl;
            oInfo << "; " << var_name << std::endl;
            oInfo << "NameOffset = " << mk9_extern_vars[i].name_hash << std::endl;
            oInfo << "Data = " << mk9_extern_vars[i].offset<< std::endl;
            oInfo << std::endl;

        }

        printf("Extracting dyn variable %s [%d/%d]\n", var_name.c_str(), i + 1, mk9_extern_vars.size());

    }
    oInfo.close();
}

void MKOReader::ExtractVariablesDCF()
{
    for (unsigned int i = 0; i < dcf_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + GetVariableOffsetDCF(i), pFile.beg);
        int size = dcf_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = GetVariableNameDCF(i);

        printf("Extracting variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, dcf_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::ExtractVariablesMK10()
{
    for (unsigned int i = 0; i < mk10_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK10(i), pFile.beg);
        int size = mk10_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = GetVariableNameMK10(i);

        printf("Extracting variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, mk10_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
    for (unsigned int i = 0; i < mk10_dyn_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + mk10_dyn_vars[i].stack_offset, pFile.beg);
        int size = mk10_dyn_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = MKODict::GetHashString(mk10_dyn_vars[i].nameHash);

        printf("Extracting dynamic variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, mk10_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::ExtractVariablesDCF2()
{
    for (unsigned int i = 0; i < dcf2_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + GetVariableOffsetDCF2(i), pFile.beg);
        int size = dcf2_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = GetVariableNameDCF2(i);

        printf("Extracting variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, dcf2_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
    for (unsigned int i = 0; i < dcf2_dyn_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + dcf2_dyn_vars[i].stack_offset, pFile.beg);
        int size = dcf2_dyn_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = MKODict::GetHashString(dcf2_dyn_vars[i].nameHash);

        printf("Extracting dynamic variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, dcf2_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::ExtractVariablesMK11()
{
    for (unsigned int i = 0; i < mk11_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK11(i), pFile.beg);
        int size = mk11_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = GetVariableNameMK11(i);

        printf("Extracting variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, mk11_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
    for (unsigned int i = 0; i < mk11_dyn_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + mk11_dyn_vars[i].stack_offset, pFile.beg);
        int size = mk11_dyn_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = MKODict::GetHashString(mk11_dyn_vars[i].nameHash);

        printf("Extracting dynamic variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, mk11_dyn_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::ExtractVariablesMK12()
{
    for (unsigned int i = 0; i < mk12_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK12(i), pFile.beg);
        int size = mk12_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = GetVariableNameMK12(i);

        printf("Extracting variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, mk12_vars.size());

        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
    for (unsigned int i = 0; i < mk12_dyn_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + mk12_dyn_vars[i].stack_offset, pFile.beg);
        int size = mk12_dyn_vars[i].size;
        if (size < 0)
            size = 0;
   
        std::string var_name = (char*)(&string_data[0] + (mk12_dyn_vars[i].name_offset - 1));
   
        printf("Extracting dynamic variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, mk12_dyn_vars.size());
   
        std::ofstream oFile(var_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::ExtractFunctions()
{
    std::string hdr = "..\\";
    hdr += GetFileName() + "_header.ini";
    std::ofstream oInfo(hdr, std::ofstream::app);
    for (unsigned int i = 0; i < funcs.size(); i++)
    {
        int size = func_sizes[i];
        if (game == Game_Armageddon)
        {
            int mka_size = mka_funcSizes[i];

            pFile.seekg(m_pFunctionsStartOffset + (funcs[i].offset * 4), pFile.beg);
            if (mka_size >= sizeof(int))
                mka_size -= sizeof(int);
            size = mka_size;
        }



        std::string func_name = (char*)(&script_names[0] + (funcs[i].name_offset - 1));
        float funcID = 0;
        if (!(game == Game_DeadlyAlliance))
        {
            pFile.read((char*)&funcID, sizeof(float));
            SwapINT((int*)&funcID);
        }


        if (oInfo) {
            oInfo << "[Function" << std::to_string(i) << "]" << std::endl;
            oInfo << "Name = " << func_name << std::endl;
            oInfo << "Unknown = " << funcs[i].unknown << std::endl;
            oInfo << "FunctionFloat = " << funcID << std::endl;


            if (game == Game_Armageddon)
            {
                oInfo << "NumLinks = " << mka_funcLinks[i].size() << std::endl;

                for (unsigned int a = 0; a < mka_funcLinks[i].size(); a++)
                {
                    int link = mka_funcLinks[i][a];

                    int linkVal = LOWORD(link);
                    int linkType = HIWORD(link);

                    if (linkType & 0x8000)
                    {
                        oInfo << "; " << GetVariableName(linkVal - 1) << std::endl;
                        oInfo << "Link" << a << " = " << linkVal << std::endl;
                        oInfo << "Link" << a << "Type = " << linkType << std::endl;
                    }
                    else
                    {
                        std::string link_name = (char*)(&string_data[0] + (linkVal));
                        oInfo << "; " << link_name << std::endl;
                        oInfo << "Link" << a << " = " << linkVal << std::endl;
                        oInfo << "Link" << a << "Type = " << linkType << std::endl;
                    }

                }
            }

            oInfo << std::endl;
        }


        printf("Extracting function %s [%d/%d]\n", func_name.c_str(), i + 1, funcs.size());
        std::ofstream oFile(func_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }

    if (oInfo)
        oInfo.close();
}

void MKOReader::ExtractFunctionsMK8()
{
    pFile.seekg(m_pFunctionsStartOffset, pFile.beg);
    for (unsigned int i = 0; i < mk8_funcs.size(); i++)
    {
        int size = mk8_funcs[i].size;
        std::string func_name = (char*)(&string_data[0] + (mk8_funcs[i].nameOffset - 1));

        printf("Extracting function %s [%d/%d]\n", func_name.c_str(), i + 1, mk8_funcs.size());
        std::ofstream oFile(func_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
        oFile.close();

    }
}

void MKOReader::ExtractFunctionsMK9()
{
    for (unsigned int i = 0; i < mk9_funcs.size(); i++)
    {
        pFile.seekg(m_pFunctionsStartOffset + GetFunctionOffsetMK9(i), pFile.beg);
        int size = mk9_funcs[i].size;
        std::string func_name = GetFunctionNameMK9Actual(i);

        std::string func_stack_name = func_name;
        func_stack_name += "_stack";

   
        printf("Extracting function %s (stack %s) [%d/%d]\n", func_name.c_str(), func_stack_name.c_str(), i + 1, mk9_funcs.size());

        std::ofstream oFile(func_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
        oFile.close();

        int stackSize = 0;
        int stackOffset = mk9_funcs[i].stackOffset;


        // stackSize =/= stored bytes

        if (i + 1 < mk9_funcs.size() - 1)
            stackSize = mk9_funcs[i + 1].stackOffset - stackOffset;
        else if (i + 1 == mk9_funcs.size() - 1)
            stackSize = mk9_header.stack_size - stackOffset;

        if (stackSize < 0 || stackOffset + stackSize > mk9_header.stack_size)
            stackSize = 0;

        std::ofstream sFile(func_stack_name, std::ofstream::binary);
        if (stackSize > 0)
        {
            pFile.seekg(m_pDataStartOffset + stackOffset, pFile.beg);
            std::unique_ptr<char[]> stackData = std::make_unique<char[]>(stackSize);
            pFile.read(stackData.get(), stackSize);
            sFile.write(stackData.get(), stackSize);
        }




        sFile.close();
    }
}

void MKOReader::ExtractFunctionsDCF()
{
    pFile.seekg(m_pFunctionsStartOffset, pFile.beg);
    for (unsigned int i = 0; i < dcf_funcs.size(); i++)
    {
        int size = dcf_funcs[i].size;
        std::string func_name = (char*)(&string_data[0] + (dcf_funcs[i].nameOffset - 1));

        printf("Extracting function %s [%d/%d]\n", func_name.c_str(), i + 1, dcf_funcs.size());
        std::ofstream oFile(func_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::ExtractFunctionsMK10()
{
    for (unsigned int i = 0; i < mk10_funcs.size(); i++)
    {
        pFile.seekg(m_pFunctionsStartOffset + GetFunctionOffsetMK10(i), pFile.beg);
        int size = mk10_funcs[i].size;
        std::string func_name = (char*)(&string_data[0] + (mk10_funcs[i].nameOffset - 1));

        printf("Extracting function %s [%d/%d]\n", func_name.c_str(), i + 1, mk10_funcs.size());
        std::ofstream oFile(func_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::ExtractFunctionsDCF2()
{
    for (unsigned int i = 0; i < dcf2_funcs.size(); i++)
    {
        pFile.seekg(m_pFunctionsStartOffset + GetFunctionOffsetDCF2(i), pFile.beg);
        int size = dcf2_funcs[i].size;
        std::string func_name = (char*)(&string_data[0] + (dcf2_funcs[i].nameOffset - 1));

        printf("Extracting function %s [%d/%d]\n", func_name.c_str(), i + 1, dcf2_funcs.size());
        std::ofstream oFile(func_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::ExtractFunctionsMK11()
{
    for (unsigned int i = 0; i < mk11_funcs.size(); i++)
    {
        pFile.seekg(m_pFunctionsStartOffset + GetFunctionOffsetMK11(i), pFile.beg);
        int size = mk11_funcs[i].size;
        std::string func_name = (char*)(&string_data[0] + (mk11_funcs[i].nameOffset - 1));

        printf("Extracting function %s [%d/%d]\n", func_name.c_str(), i + 1, mk11_funcs.size());
        std::ofstream oFile(func_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::ExtractFunctionsMK12()
{
    for (unsigned int i = 0; i < mk12_funcs.size(); i++)
    {
        pFile.seekg(GetFunctionOffsetMK12(i), pFile.beg);
        int size = mk12_funcs[i].size;
        std::string func_name = (char*)(&string_data[0] + (mk12_funcs[i].nameOffset - 1));

        printf("Extracting function %s [%d/%d]\n", func_name.c_str(), i + 1, mk12_funcs.size());
        std::ofstream oFile(func_name, std::ofstream::binary);
        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        pFile.read(data.get(), size);
        oFile.write(data.get(), size);
    }
}

void MKOReader::DecompileFunction(int functionID)
{
    std::vector<MKOCodeEntry> codeData;
    ReadFunctionBytecode(codeData, functionID);

    if (codeData.size() > 0)
    {
        std::string output = GetFunctionName(functionID);
        output += ".c";
        std::ofstream pMKC(output);

        for (int i = 0; i < codeData.size(); i++)
        {
            MKOCodeEntry c = codeData[i];

            MKOFunctionDefinition funcDef;
            bool definitionAvailable = false;
            if (c.arguments.size() > 0)
            {
                if (c.isInternal)
                    pMKC << MKODict::GetInternalName(c.functionID) << "(";
                else
                {
                    std::string functionName = "function";

                   if (game == Game_Armageddon)
                   {
                       functionName += "_";
                       functionName += std::to_string(c.functionSet);
                       functionName += "_";
                       functionName += std::to_string(c.functionID - 1);
    
                       if (MKODict::IsDefinitionAvailable(c.functionID - 1, c.functionSet))
                       {
                           funcDef = MKODict::GetDefinition(c.functionID - 1, c.functionSet);
                           functionName = funcDef.name;
                           definitionAvailable = true;
                       }
                   }
                   else
                   {
                        functionName += "_";
                        functionName += std::to_string(c.functionID - 1);

                        if (MKODict::IsDefinitionAvailable(c.functionID - 1))
                        {
                            funcDef = MKODict::GetDefinition(c.functionID - 1);
                            functionName = funcDef.name;
                            definitionAvailable = true;
                        }
                    }



                    pMKC << functionName << "(";
                }


                for (int a = 0; a < c.arguments.size(); a++)
                {
                    if (definitionAvailable)
                    {
                        if (a < funcDef.args.size())
                        {
                            if (funcDef.args[a] == EMKOFAD_Float)
                                pMKC << c.arguments[a].floatData;
                            else if (funcDef.args[a] == EMKOFAD_Short)
                                pMKC << c.arguments[a].shortData;
                            else if (funcDef.args[a] == EMKOFAD_UInt)
                                pMKC << c.arguments[a].uintData;
                            else if (funcDef.args[a] == EMKOFAD_String)
                            {
                                if (!(game == Game_DeadlyAlliance))
                                {
                                    if (c.arguments[a].integerData > 0)
                                        pMKC << "\"" << GetString(c.arguments[a].integerData) << "\"";
                                    else
                                        pMKC << c.arguments[a].integerData;
                                }
                                else
                                    pMKC << "\"" << GetString(c.arguments[a].integerData + 1) << "\"";

                            }
                            else if (funcDef.args[a] == EMKOFAD_Hex)
                                pMKC << std::hex << "0x" << c.arguments[a].uintData << std::dec;
                            else
                                pMKC << c.arguments[a].integerData;
                        }
                        else
                            pMKC << c.arguments[a].integerData;

                        if (a < c.arguments.size() - 1)
                            pMKC << ", ";
                    }
                    else
                    {
                        pMKC << c.arguments[a].integerData;
                        if (a < c.arguments.size() - 1)
                            pMKC << ", ";
                    }

                }
                if (m_bDebugMKO)
                    pMKC << "); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << ");" << std::endl;
            }
            else
            {
                if (c.isPadding)
                    pMKC << "pad" << std::endl;
                else if (c.isInternal)
                    pMKC << MKODict::GetInternalName(c.functionID) << "();" << std::endl;
                else
                {
                    std::string functionName = "function";

                    if (game == Game_Armageddon)
                    {
                        functionName += "_";
                        functionName += std::to_string(c.functionSet);
                        functionName += "_";
                        functionName += std::to_string(c.functionID - 1);

                        if (MKODict::IsDefinitionAvailable(c.functionID - 1, c.functionSet))
                        {
                            funcDef = MKODict::GetDefinition(c.functionID - 1, c.functionSet);
                            functionName = funcDef.name;
                            definitionAvailable = true;
                        }
                    }
                    else
                    {
                        functionName += "_";
                        functionName += std::to_string(c.functionID - 1);

                        if (MKODict::IsDefinitionAvailable(c.functionID - 1))
                        {
                            funcDef = MKODict::GetDefinition(c.functionID - 1);
                            functionName = funcDef.name;
                            definitionAvailable = true;
                        }
                    }

                    if (m_bDebugMKO)
                        pMKC << functionName << "(); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                    else
                        pMKC << functionName << "();" << std::endl;

                }
            }
        }
        std::cout << "Decompiled " << output << std::endl;
        pMKC.close();
    }
}

void MKOReader::DecompileFunctionMK8(int functionID)
{
    std::vector<MKOCodeEntry_MK8> codeData;
    ReadFunctionBytecode_MK8(codeData, functionID);

    if (codeData.size() > 0)
    {
        std::string output = GetFunctionNameMK8(functionID);
        output += ".c";
        std::ofstream pMKC(output);

        for (int i = 0; i < codeData.size(); i++)
        {
            MKOCodeEntry_MK8 c = codeData[i];

            

            MKOFunctionDefinition funcDef;
            if (c.arguments.size() > 0)
            {
                {
                    std::string functionName = "function";
                    if (c.type == MK8_CT_INTERNAL)
                        functionName = "internal";
                    if (c.type == MK8_CT_STEP)
                        functionName = "native";
                    functionName += "_";
                    int id = c.functionID - 1;
                    if (c.type == MK8_CT_INTERNAL || c.type == MK8_CT_STEP)
                        id += 1;

                    functionName += std::to_string(id);



                    if (c.type == MK8_CT_EXTERN)
                    {
                        std::string ext_name = (char*)(&string_data[0] + (mk8_externs[c.functionID - 1].nameOffset - 1));
                        functionName = ext_name;
                    }
                    if (!(c.type == MK8_CT_EXTERN || c.type == MK8_CT_INTERNAL || c.type == MK8_CT_STEP))
                    {
                        functionName += "_t";
                        functionName += std::to_string(c.type);
                    }



                    pMKC << functionName << "(";
                }


                for (int a = 0; a < c.arguments.size(); a++)
                {
                    {
                        pMKC << c.arguments[a].integerData;
                        if (a < c.arguments.size() - 1)
                            pMKC << ", ";
                    }

                }
                if (m_bDebugMKO)
                    pMKC << "); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << ");" << std::endl;
            }
            else
            {
                std::string functionName = "function";
                if (c.type == MK8_CT_INTERNAL)
                    functionName = "internal";
                if (c.type == MK8_CT_STEP)
                    functionName = "native";
                functionName += "_";
                int id = c.functionID - 1;
                if (c.type == MK8_CT_INTERNAL || c.type == MK8_CT_STEP)
                    id += 1;

                functionName += std::to_string(id);



                if (c.type == MK8_CT_EXTERN)
                {
                    std::string ext_name = (char*)(&string_data[0] + (mk8_externs[c.functionID - 1].nameOffset - 1));
                    functionName = ext_name;
                }
                if (!(c.type == MK8_CT_EXTERN || c.type == MK8_CT_INTERNAL || c.type == MK8_CT_STEP))
                {
                    functionName += "_t";
                    functionName += std::to_string(c.type);
                }

                if (m_bDebugMKO)
                    pMKC << functionName << "(); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << functionName << "();" << std::endl;
            }
        }
        std::cout << "Decompiled " << output << std::endl;
        pMKC.close();
    }
}

void MKOReader::DecompileFunctionMK9(int functionID)
{
    std::vector<MKOCodeEntry_MK8> codeData;
    ReadFunctionBytecode_MK9(codeData, functionID);

    if (codeData.size() > 0)
    {
        std::string output = GetFunctionNameMK9Actual(functionID);
        output += ".c";
        std::ofstream pMKC(output);

        for (int i = 0; i < codeData.size(); i++)
        {
            MKOCodeEntry_MK8 c = codeData[i];

            MKOFunctionDefinition funcDef;
            bool definitionAvailable = false;

            if (c.pad)
            {
                if (m_bDebugMKO)
                    pMKC << "pad(); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << "pad();" << std::endl;
                continue;
            }

            bool isCall = (c.unk1 == 65);

            int defType = c.unk1;
            int defSet = isCall ? c.type : 0;
            int defID = isCall ? c.functionID : c.unk1;
            int defFlags = c.unk2;

            std::string functionName = "function";
            if (c.isInternal)
            {
                functionName = MKODict::GetInternalName(c.unk1);
                functionName += "_";
                functionName += std::to_string(c.unk2);

                MKOVariable arg0;
                arg0.integerData = (c.type << 16) | (c.functionID & 0xFFFF);
                c.arguments.insert(c.arguments.begin(), arg0);
            }
            else
            {
                if (MKODict::IsDefinitionAvailable(defID, defSet, defType))
                {
                    funcDef = MKODict::GetDefinition(defID, defSet, defType);
                    functionName = funcDef.name;
                    definitionAvailable = true;
                }
                else
                {
                    functionName = "function_";
                    functionName += std::to_string(c.unk1);
                    functionName += "_";
                    functionName += std::to_string(c.type);
                    functionName += "_";
                    functionName += std::to_string(c.functionID);
                    functionName += "_";
                    functionName += std::to_string(c.unk2);
                }
            }

            pMKC << functionName << "(";

            int base = c.offset - m_pFunctionsStartOffset;
            int firstArg = isCall ? 16 : 12;

            for (int a = 0; a < c.arguments.size(); a++)
            {
                int fx = GetBytecodeFixupMK9(base + firstArg + a * 4);

                bool notVarOrExt = definitionAvailable && a < (int)funcDef.args.size() && funcDef.args[a] != EMKOFAD_Integer;

                if (fx >= 0)
                {
                    int type = mk9_fixup[fx].type;
                    int data = mk9_fixup[fx].data;

                    if (type == MK9_Fixup_String)
                        pMKC << "str:" << data;
                    else if (type == MK9_Fixup_Asset)
                        pMKC << "asset:" << data;
                    else if (type == MK9_Fixup_Sound)
                        pMKC << "sound:" << data;
                    else
                        pMKC << "func:" << data << std::dec;
                }
                else if (notVarOrExt)
                {
                    switch (funcDef.args[a])
                    {
                    case EMKOFAD_Float:
                        pMKC << c.arguments[a].floatData;
                        break;
                    case EMKOFAD_Short:
                        pMKC << c.arguments[a].shortData;
                        break;
                    case EMKOFAD_UInt:
                        pMKC << c.arguments[a].uintData;
                        break;
                    case EMKOFAD_Hex:
                        pMKC << std::hex << "0x" << c.arguments[a].uintData << std::dec;
                        break;
                        //case EMKOFAD_String:
                        //    if (c.arguments[a].integerData > 0)
                        //        pMKC << "\"" << GetString(c.arguments[a].integerData) << "\"";
                        //    else
                        //        pMKC << c.arguments[a].integerData;
                        //    break;
                    default:
                        pMKC << c.arguments[a].integerData;
                        break;
                    }
                }
                // mk9/mkvsdc reference arguments by offset from data
                // ext is 0x8000 but it catches floats
                else if (c.arguments[a].integerData & 0x40000000)
                {
                    int var_offset = c.arguments[a].integerData & 0x3FFFFFFF;
                    int variableID = GetVariableForOffsetMK9(var_offset);

                    if (variableID >= 0)
                    {
                        pMKC << "var:" << GetVariableNameMK9(variableID);
                    }
                    else
                        pMKC << c.arguments[a].integerData;
                }
                else
                    pMKC << c.arguments[a].integerData;

                if (a < c.arguments.size() - 1)
                    pMKC << ", ";
            }

            if (m_bDebugMKO)
                pMKC << "); // Offset: " << c.offset << " Size: " << c.size << std::endl;
            else
                pMKC << ");" << std::endl;
        }
        std::cout << "Decompiled " << output << std::endl;
        pMKC.close();
    }
}

int MKOReader::GetBytecodeFixupMK9(int offset)
{
    for (unsigned int i = 0; i < mk9_fixup.size(); i++)
    {
        if (IsBytecodeFixupMK9(mk9_fixup[i].type) && mk9_fixup[i].offset == offset)
            return i;
    }
    return -1;
}

void MKOReader::DecompileFunctionMK10(int functionID)
{
    std::vector<MKOCodeEntry_MK10> codeData;
    ReadFunctionBytecode_MK10(codeData, functionID);

    if (codeData.size() > 0)
    {
        std::string output = GetFunctionNameMK10(functionID);
        output += ".c";
        std::ofstream pMKC(output);

        for (int i = 0; i < codeData.size(); i++)
        {
            MKOCodeEntry_MK10 c = codeData[i];
            bool is_my_func_call = false;
            bool is_callback = false;
            if (c.type == 12)
                is_my_func_call = true;

            if (c.arguments.size() > 0)
            {
                {
                    std::string functionName = "function";
                    if (is_my_func_call)
                    {
                        int id = GetFunctionIDWithHashMK10(c.arguments[0].integerData);

                        if (MKODict::IsHashAvailable(c.arguments[0].integerData))
                          functionName = MKODict::GetHashString(c.arguments[0].integerData);
                        else if (id >= 0)
                          functionName = GetFunctionNameMK10(id);
                        else
                        {
                            functionName = "call_";
                            functionName += std::to_string(c.arguments[0].integerData);
                        }
                         
                    }
                    else
                    {
                        functionName += "_t";
                        functionName += std::to_string(c.type);
                        functionName += "_s";
                        functionName += std::to_string(c.functionSet);
                        functionName += "_i";
                        functionName += std::to_string(c.functionID);
                    }
                    pMKC << functionName << "(";
                }


                int start_offset = 0;
                int end_offset = 0;
                
                if (is_my_func_call)
                {
                    start_offset = 1;
                    end_offset = 1;
                }

                for (int a = 0 + start_offset; a < c.arguments.size(); a++)
                {
                    {
                        pMKC << c.arguments[a].integerData;
                        if (a < c.arguments.size() - 1)
                            pMKC << ", ";
                    }

                }
                if (m_bDebugMKO)
                    pMKC << "); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << ");" << std::endl;
            }
            else
            {
                std::string functionName = "function";
                if (is_my_func_call)
                {
                    functionName = MKODict::GetHashString(c.arguments[0].integerData);
                }
                else
                {
                    functionName += "_t";
                    functionName += std::to_string(c.type);
                    functionName += "_s";
                    functionName += std::to_string(c.functionSet);
                    functionName += "_i";
                    functionName += std::to_string(c.functionID);
                }

                pMKC << functionName << "(";

                if (m_bDebugMKO)
                    pMKC << functionName << "(); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << functionName << "();" << std::endl;
            }
        }
        std::cout << "Decompiled " << output << std::endl;
        pMKC.close();
    }
}

void MKOReader::DecompileFunctionDCF2(int functionID)
{
    std::vector<MKOCodeEntry_MK10> codeData;
    ReadFunctionBytecode_DCF2(codeData, functionID);

    if (codeData.size() > 0)
    {
        std::string output = GetFunctionNameDCF2(functionID);
        output += ".c";
        std::ofstream pMKC(output);

        for (int i = 0; i < codeData.size(); i++)
        {
            MKOCodeEntry_MK10 c = codeData[i];

            if (c.arguments.size() > 0)
            {
                std::string functionName = "function";
                functionName += "_t";
                functionName += std::to_string(c.type);
                functionName += "_s";
                functionName += std::to_string(c.functionSet);
                functionName += "_i";
                functionName += std::to_string(c.functionID);
                pMKC << functionName << "(";

                for (int a = 0; a < c.arguments.size(); a++)
                {
                    {
                        pMKC << c.arguments[a].integerData;
                        if (a < c.arguments.size() - 1)
                            pMKC << ", ";
                    }

                }
                if (m_bDebugMKO)
                    pMKC << "); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << ");" << std::endl;
            }
            else
            {
                std::string functionName = "function";
                functionName += "_t";
                functionName += std::to_string(c.type);
                functionName += "_s";
                functionName += std::to_string(c.functionSet);
                functionName += "_i";
                functionName += std::to_string(c.functionID);

                pMKC << functionName << "(";

                if (m_bDebugMKO)
                    pMKC << functionName << "(); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << functionName << "();" << std::endl;
            }
        }
        std::cout << "Decompiled " << output << std::endl;
        pMKC.close();
    }
}

void MKOReader::DecompileFunctionMK11(int functionID)
{
    std::vector<MKOCodeEntry_MK10> codeData;
    ReadFunctionBytecode_MK11(codeData, functionID);

    if (codeData.size() > 0)
    {
        std::string output = GetFunctionNameMK11(functionID);
        output += ".c";
        std::ofstream pMKC(output);

        for (int i = 0; i < codeData.size(); i++)
        {
            MKOCodeEntry_MK10 c = codeData[i];
            MKOFunctionDefinition funcDef;
            bool definitionAvailable = false;

            if (c.arguments.size() > 0)
            {
                std::string functionName = "function";
                functionName += "_t";
                functionName += std::to_string(c.type);
                functionName += "_s";
                functionName += std::to_string(c.functionSet);
                functionName += "_i";
                functionName += std::to_string(c.functionID);

                if (MKODict::IsDefinitionAvailable(c.functionID, c.functionSet, c.type))
                {
                    funcDef = MKODict::GetDefinition(c.functionID, c.functionSet, c.type);
                    functionName = funcDef.name;
                    definitionAvailable = true;
                }
                pMKC << functionName << "(";

                for (int a = 0; a < c.arguments.size(); a++)
                {
                    if (definitionAvailable)
                    {
                        if (funcDef.args[a] == EMKOFAD_Float)
                            pMKC << c.arguments[a].floatData;
                        else if (funcDef.args[a] == EMKOFAD_Short)
                            pMKC << c.arguments[a].shortData;
                        else if (funcDef.args[a] == EMKOFAD_UInt)
                            pMKC << c.arguments[a].uintData;
                        else if (funcDef.args[a] == EMKOFAD_String)
                        {
                            if (c.arguments[a].integerData >= 1 && c.arguments[a].integerData < mk11_header.string_size)
                                pMKC << "\"" << GetString(c.arguments[a].integerData) << "\"";
                            else
                                pMKC << c.arguments[a].integerData;
                        }
   
                        else if (funcDef.args[a] == EMKOFAD_Hex)
                            pMKC << std::hex << "0x" << c.arguments[a].uintData << std::dec;
                        else if (funcDef.args[a] == EMKOFAD_Int64)
                            pMKC << std::hex << "0x" << c.arguments[a].qwordData << std::dec;
                        else if (funcDef.args[a] == EMKOFAD_Hash)
                            pMKC << MKODict::GetHashString(c.arguments[a].uintData);
                        else if (funcDef.args[a] == EMKOFAD_HashFunction)
                            pMKC << MKODict::GetFunctionHashString(c.arguments[a].uintData);
                        else if (funcDef.args[a] == EMKOFAD_ScriptHashFunction)
                        {
                            unsigned int fHash = c.arguments[a].uintData;
                            int id = GetFunctionIDWithHashMK11(fHash);
                            if (id >= 0)
                                pMKC << GetFunctionNameMK11(id);
                            else
                                pMKC << MKODict::GetHashString(fHash);
                        }
                        else
                            pMKC << c.arguments[a].integerData;

                        if (a < c.arguments.size() - 1)
                            pMKC << ", ";
                    }
                    else
                    {
                        pMKC << c.arguments[a].integerData;
                        if (a < c.arguments.size() - 1)
                            pMKC << ", ";
                    }

                }
                if (m_bDebugMKO)
                    pMKC << "); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << ");" << std::endl;
            }
            else
            {
                std::string functionName = "function";
                functionName += "_t";
                functionName += std::to_string(c.type);
                functionName += "_s";
                functionName += std::to_string(c.functionSet);
                functionName += "_i";
                functionName += std::to_string(c.functionID);

                pMKC << functionName << "(";

                if (m_bDebugMKO)
                    pMKC << functionName << "(); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << functionName << "();" << std::endl;
            }
        }
        std::cout << "Decompiled " << output << std::endl;
        pMKC.close();
    }
}

void MKOReader::DecompileFunctionMK12(int functionID)
{
    std::vector<MKOCodeEntry_MK10> codeData;
    ReadFunctionBytecode_MK12(codeData, functionID);

    if (codeData.size() > 0)
    {
        std::string output = GetFunctionNameMK12(functionID);
        output += ".c";
        std::ofstream pMKC(output);

        for (int i = 0; i < codeData.size(); i++)
        {
            MKOCodeEntry_MK10 c = codeData[i];
            MKOFunctionDefinition funcDef;
            bool definitionAvailable = false;

            if (c.arguments.size() > 0)
            {
                std::string functionName = "function";
                functionName += "_t";
                functionName += std::to_string(c.type);
                functionName += "_s";
                functionName += std::to_string(c.functionSet);
                functionName += "_i";
                functionName += std::to_string(c.functionID);

                if (MKODict::IsDefinitionAvailable(c.functionID, c.functionSet, c.type))
                {
                    funcDef = MKODict::GetDefinition(c.functionID, c.functionSet, c.type);
                    functionName = funcDef.name;
                    definitionAvailable = true;
                }
                pMKC << functionName << "(";

                for (int a = 0; a < c.arguments.size(); a++)
                {
                    if (definitionAvailable)
                    {
                        if (funcDef.args[a] == EMKOFAD_Float)
                            pMKC << c.arguments[a].floatData;
                        else if (funcDef.args[a] == EMKOFAD_Short)
                            pMKC << c.arguments[a].shortData;
                        else if (funcDef.args[a] == EMKOFAD_UInt)
                            pMKC << c.arguments[a].uintData;
                        else if (funcDef.args[a] == EMKOFAD_String)
                        {
                            if (c.arguments[a].integerData >= 1 && c.arguments[a].integerData < mk11_header.string_size)
                                pMKC << "\"" << GetString(c.arguments[a].integerData) << "\"";
                            else
                                pMKC << c.arguments[a].integerData;
                        }

                        else if (funcDef.args[a] == EMKOFAD_Hex)
                            pMKC << std::hex << "0x" << c.arguments[a].uintData << std::dec;
                        else if (funcDef.args[a] == EMKOFAD_Int64)
                            pMKC << std::hex << "0x" << c.arguments[a].qwordData << std::dec;
                        else if (funcDef.args[a] == EMKOFAD_Hash)
                            pMKC << MKODict::GetHashString(c.arguments[a].uintData);
                        else if (funcDef.args[a] == EMKOFAD_HashFunction)
                            pMKC << MKODict::GetFunctionHashString(c.arguments[a].uintData);
                        else if (funcDef.args[a] == EMKOFAD_ScriptHashFunction)
                        {
                            unsigned int fHash = c.arguments[a].uintData;
                            int id = GetFunctionIDWithHashMK12(fHash);
                            if (id >= 0)
                                pMKC << GetFunctionNameMK12(id);
                            else
                                pMKC << MKODict::GetHashString(fHash);
                        }
                        else
                            pMKC << c.arguments[a].integerData;

                        if (a < c.arguments.size() - 1)
                            pMKC << ", ";
                    }
                    else
                    {
                        pMKC << c.arguments[a].integerData;
                        if (a < c.arguments.size() - 1)
                            pMKC << ", ";
                    }

                }
                if (m_bDebugMKO)
                    pMKC << "); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << ");" << std::endl;
            }
            else
            {
                std::string functionName = "function";
                functionName += "_t";
                functionName += std::to_string(c.type);
                functionName += "_s";
                functionName += std::to_string(c.functionSet);
                functionName += "_i";
                functionName += std::to_string(c.functionID);

                pMKC << functionName << "(";

                if (m_bDebugMKO)
                    pMKC << functionName << "(); // Offset: " << c.offset << " Size: " << c.size << std::endl;
                else
                    pMKC << functionName << "();" << std::endl;
            }
        }
        std::cout << "Decompiled " << output << std::endl;
        pMKC.close();
    }
}

void MKOReader::UnpackVariable(int variableID)
{
    std::string varName = GetVariableName(variableID);
    if (varName.find("_movelist") != std::string::npos)
        Unpack_Movelist(variableID);
    else  if (varName.find("_styles") != std::string::npos)
        Unpack_Styles(variableID);
    else  if (varName.find("_required_art") != std::string::npos)
        Unpack_RArt(variableID);
    else  if ((varName.find("_bone_list") != std::string::npos || varName.find("_bones") != std::string::npos) && varName.find("_shadow_") == std::string::npos)
        Unpack_BList(variableID);
    else  if (varName.find("string_list") != std::string::npos)
        Unpack_SList(variableID);
    else  if (varName.find("_attributes") != std::string::npos || varName.find("_second_hit") != std::string::npos)
        Unpack_Attributes(variableID);
    else  if (varName.find("_player_data") != std::string::npos)
        Unpack_PlayerData_MKDU(variableID);
    std::cout << "Attempting to unpack " << varName << std::endl;
}

void MKOReader::UnpackVariableMK8(int variableID)
{
    std::string varName = GetVariableNameMK8(variableID);

    if (varName.find("asset_package") != std::string::npos)
        MK8_Unpack_RArt(variableID);
    else if (varName.find("movelist") != std::string::npos)
        MK8_Unpack_Movelist(variableID);
    else if (varName.find("MKCharacterBones") != std::string::npos)
        MK8_Unpack_CHRBones(variableID);
    std::cout << "Attempting to unpack " << varName << std::endl;
}

void MKOReader::UnpackVariableMK9(int variableID)
{
    std::string varName = GetVariableNameMK9(variableID);

    if (varName.find("asset_package") != std::string::npos)
        MK9_Unpack_RArt(variableID);
    else if (varName.find("movelist") != std::string::npos)
        MK9_Unpack_Movelist(variableID);

    std::cout << "Attempting to unpack " << varName << std::endl;
}

void MKOReader::UnpackVariableDCF(int variableID)
{
    std::string varName = GetVariableNameDCF(variableID);
    std::cout << "Attempting to unpack " << varName << std::endl;
}

void MKOReader::UnpackVariableMK10(int variableID)
{
    std::string varName = GetVariableNameMK10(variableID);
    std::cout << "Attempting to unpack " << varName << std::endl;
}

void MKOReader::UnpackVariableDCF2(int variableID)
{
    std::string varName = GetVariableNameDCF2(variableID);
    std::cout << "Attempting to unpack " << varName << std::endl;
}

void MKOReader::UnpackVariableMK11(int variableID)
{
    std::string varName = GetVariableNameMK11(variableID);
    std::cout << "Attempting to unpack " << varName << std::endl;
}

void MKOReader::Unpack_Movelist(int variableID)
{
    if (game == Game_Armageddon)
        return;

    std::string varName = GetVariableName(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffset(variableID), pFile.beg);

    std::string output = varName;
    output += ".txt";
    std::ofstream pMovelist(output);
    int lastStyle = EMovelist_Style_One;
    for (int i = 0; i < vars[variableID].numElems; i++)
    {
        type_movelist mv = {}; 
        pFile.read((char*)&mv, sizeof(type_movelist));

        SwapINT(&mv.category);
        SwapINT(&mv.name);
        SwapINT(&mv.tip);
        SwapINT(&mv.command);
        SwapINT(&mv.command2);
        SwapINT(&mv.unk);

        if (game == Game_Unchained)
            pFile.seekg(-4, pFile.cur);

        std::string type = std::to_string(mv.category);

        std::string str;
        std::string command;
        std::string command2;
        if (mv.name > 0)
            str = (char*)(&string_data[0] + (mv.name - 1));
        if (mv.command > 0)
            command = (char*)(&string_data[0] + (mv.command - 1));
        if (mv.command2 > 0)
            command2 = (char*)(&string_data[0] + (mv.command2 - 1));
        std::replace(str.begin(), str.end(), ' ', '_');
        std::replace(command.begin(), command.end(), ' ', '_');
        std::replace(command2.begin(), command2.end(), ' ', '_');

        if (game == Game_Unchained)
        {
            command2 = command;
            mv.unk = 0;
        }

        pMovelist << mv.category << " " << str << " " << command  << " " << command2 << " " << mv.unk << std::endl;
    }
    pMovelist.close();

}

void MKOReader::Unpack_MovelistU(int variableID)
{
}

void MKOReader::Unpack_Styles(int variableID)
{
    if (game == Game_Armageddon)
        return;
    std::string varName = GetVariableName(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffset(variableID), pFile.beg);

    std::string output = varName;
    output += ".txt";
    std::ofstream pUnpacked(output);
    int styles[3] = {};
    pFile.read((char*)&styles, sizeof(styles));

    for (int i = 0; i < 3; i++)
        SwapINT(&styles[i]);

    for (int i = 0; i < 3; i++)
    {
        std::string str;
        if (styles[i] > 0)
            str = (char*)(&string_data[0] + (styles[i] - 1));
        pUnpacked << str << std::endl;
    }
    pUnpacked.close();

}

void MKOReader::Unpack_RArt(int variableID)
{
    if (game == Game_Armageddon)
        return;
    std::string varName = GetVariableName(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffset(variableID), pFile.beg);

    std::string output = varName;
    output += ".txt";
    std::ofstream pUnpacked(output);
    int data[2] = {};
    pFile.read((char*)&data, sizeof(data));

    for (int i = 0; i < 2; i++)
        SwapINT(&data[i]);

    for (int i = 0; i < 2; i++)
    {
        std::string str;
        if (data[i] > 0)
            str = (char*)(&string_data[0] + (data[i] - 1));
        pUnpacked << str << std::endl;
    }

    pUnpacked.close();
}

void MKOReader::Unpack_BList(int variableID)
{
    std::string varName = GetVariableName(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffset(variableID), pFile.beg);

    std::string output = varName;
    output += ".txt";
    std::ofstream pUnpacked(output);
    for (int i = 0; i < vars[variableID].numElems; i++)
    {
        int boneID = 0;
        pFile.read((char*)&boneID, sizeof(int));
        SwapINT(&boneID);
        pUnpacked << boneID << std::endl;
    }
    pUnpacked.close();
}

void MKOReader::Unpack_SList(int variableID)
{
    std::string varName = GetVariableName(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffset(variableID), pFile.beg);

    std::string output = varName;
    output += ".txt";
    std::ofstream pUnpacked(output);
    std::string str;
    for (int i = 0; i < vars[variableID].numElems; i++)
    {
        int strID = 0;
        pFile.read((char*)&strID, sizeof(int));
        SwapINT(&strID);
        if (strID > 0)
            str = (char*)(&string_data[0] + (strID - 1));
        pUnpacked << str << std::endl;
    }
    pUnpacked.close();
}

void MKOReader::Unpack_Attributes(int variableID)
{
    std::string varName = GetVariableName(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffset(variableID), pFile.beg);

    std::string output = varName;
    output += ".ini";
    std::ofstream pUnpacked(output);
    type_attributes attr;
    pFile.read((char*)&attr, sizeof(type_attributes));
    SwapINT((int*)&attr.activeFrame);
    SwapINT((int*)&attr.field4);
    SwapINT((int*)&attr.field8);
    SwapINT((int*)&attr.animSpeed);
    SwapINT((int*)&attr.movementSpeed);
    SwapSHORT(&attr.field20);
    SwapSHORT(&attr.field22);
    SwapSHORT(&attr.field24);
    SwapSHORT(&attr.field26);
    SwapSHORT(&attr.field28);
    SwapSHORT(&attr.field30);
    SwapINT(&attr.field32);
    SwapINT((int*)&attr.field36);
    SwapINT((int*)&attr.field40);
    SwapINT((int*)&attr.damage);
    SwapINT(&attr.hitLevel);
    SwapSHORT(&attr.field52);
    SwapSHORT(&attr.field54);
    SwapINT((int*)&attr.field56);


    pUnpacked << "[Attributes]" << std::endl;
    pUnpacked << "activeFrame = " << attr.activeFrame << std::endl;
    pUnpacked << "animSpeed  = " << attr.animSpeed << std::endl;
    pUnpacked << "movementSpeed  = " << attr.movementSpeed << std::endl;
    pUnpacked << "damage = " << attr.damage << "    // real damage: " << (float)(attr.damage * 8 * 10) << std::endl;
    pUnpacked << "hitLevel = " << attr.hitLevel << std::endl;
    pUnpacked << "field4 = " << attr.field4 << std::endl;
    pUnpacked << "field8 = " << attr.field8 << std::endl;
    pUnpacked << "field20 = " << attr.field20 << std::endl;
    pUnpacked << "field22 = " << attr.field22 << std::endl;
    pUnpacked << "field24 = " << attr.field24 << std::endl;
    pUnpacked << "field26 = " << attr.field26 << std::endl;
    pUnpacked << "field28 = " << attr.field28 << std::endl;
    pUnpacked << "field30 = " << attr.field30 << std::endl;
    pUnpacked << "field32 = " << attr.field32 << std::endl;
    pUnpacked << "field36 = " << attr.field36 << std::endl;
    pUnpacked << "field40 = " << attr.field40 << std::endl;
    pUnpacked << "field52 = " << attr.field52 << std::endl;
    pUnpacked << "field54 = " << attr.field54 << std::endl;
    pUnpacked << "field56 = " << attr.field56 << std::endl;
    pUnpacked.close();
}

void MKOReader::Unpack_PlayerData_MKDU(int variableID)
{
    if (!(game == Game_Unchained || game == Game_Deception))
        return;

    std::string varName = GetVariableName(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffset(variableID), pFile.beg);

    std::string output = varName;
    output += ".ini";
    std::ofstream pUnpacked(output);
    type_player_data data;
    pFile.read((char*)&data, sizeof(type_player_data));

   
    {
        pUnpacked << "[PrimaryInfo]" << std::endl;

        std::string str;

        if (data.primary.archiveNameOffset > 0)
            str = (char*)(&string_data[0] + (data.primary.archiveNameOffset - 1));

        pUnpacked << "archive = " << str << std::endl;

        if (data.primary.headNameOffset > 0)
            str = (char*)(&string_data[0] + (data.primary.headNameOffset - 1));

        pUnpacked << "head = " << str << std::endl;

        if (data.primary.animsNameOffset > 0)
            str = (char*)(&string_data[0] + (data.primary.animsNameOffset- 1));

        pUnpacked << "anims = " << str << std::endl;

        pUnpacked << "field12 = " << data.primary.field12 << std::endl;
        pUnpacked << "field16 = " << data.primary.field16 << std::endl;
        pUnpacked << "field20 = " << data.primary.field20 << std::endl;
        pUnpacked << "field24 = " << data.primary.field24 << std::endl;
        pUnpacked << "startupFunctionID = " << data.primary.startupFunctionID << std::endl;
        pUnpacked << "reloadFunctionID = " << data.primary.reloadFunctionID << std::endl;
    }
    pUnpacked << std::endl;
    {
        pUnpacked << "[AlternateInfo]" << std::endl;

        std::string str;

        if (data.alt.archiveNameOffset > 0)
            str = (char*)(&string_data[0] + (data.alt.archiveNameOffset - 1));

        pUnpacked << "archive = " << str << std::endl;

        if (data.alt.headNameOffset > 0)
            str = (char*)(&string_data[0] + (data.alt.headNameOffset - 1));

        pUnpacked << "head = " << str << std::endl;

        if (data.alt.animsNameOffset > 0)
            str = (char*)(&string_data[0] + (data.alt.animsNameOffset - 1));

        pUnpacked << "anims = " << str << std::endl;

        pUnpacked << "field12 = " << data.alt.field12 << std::endl;
        pUnpacked << "field16 = " << data.alt.field16 << std::endl;
        pUnpacked << "field20 = " << data.alt.field20 << std::endl;
        pUnpacked << "field24 = " << data.alt.field24 << std::endl;
        pUnpacked << "startupFunctionID = " << data.alt.startupFunctionID << std::endl;
        pUnpacked << "reloadFunctionID = " << data.alt.reloadFunctionID << std::endl;
    }
    pUnpacked << std::endl;

    {
        pUnpacked << "[Data]" << std::endl;

        std::string str;

        if (data.apArchiveNameOffset > 0)
            str = (char*)(&string_data[0] + (data.apArchiveNameOffset- 1));

        pUnpacked << "apArchive = " << str << std::endl;

        if (data.apHeadNameOffset > 0)
            str = (char*)(&string_data[0] + (data.apHeadNameOffset - 1));

        pUnpacked << "apHead = " << str << std::endl;

        if (data.apAltArchiveNameOffset > 0)
            str = (char*)(&string_data[0] + (data.apAltArchiveNameOffset - 1));

        pUnpacked << "apAltArchive = " << str << std::endl;

        if (data.apAltHeadNameOffset > 0)
            str = (char*)(&string_data[0] + (data.apAltHeadNameOffset - 1));

        pUnpacked << "apAltHead = " << str << std::endl;

        if (data.sharedArchiveNameOffset > 0)
            str = (char*)(&string_data[0] + (data.sharedArchiveNameOffset - 1));

        pUnpacked << "sharedArchive = " << str << std::endl;

        if (data.puzzleAnimNameOffset > 0)
            str = (char*)(&string_data[0] + (data.puzzleAnimNameOffset- 1));

        pUnpacked << "puzzleAnimArchive = " << str << std::endl;

        pUnpacked << "specialsBankID = " << data.specialsSbankID<< std::endl;
        pUnpacked << "winSoundID = " << data.winVoiceID << std::endl;
        pUnpacked << "stylesVarID = " << data.stylesVariableID << std::endl;
        pUnpacked << "fatalityDataVarID = " << data.fatDataVariableID << std::endl;

        pUnpacked << "field104 = " << data.field104 << std::endl;
        pUnpacked << "field112 = " << data.field112 << std::endl;
        pUnpacked << "field116 = " << data.field116 << std::endl;
        pUnpacked << "field120 = " << data.field120 << std::endl;
        pUnpacked << "field124 = " << data.field124 << std::endl;

    }
 
    pUnpacked.close();
}

void MKOReader::MK8_Unpack_RArt(int variableID)
{
    std::string varName = GetVariableNameMK8(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK8(variableID), pFile.beg);

    std::string output = varName;
    output += ".txt";
    std::ofstream pUnpacked(output);
    int numElems = mk8_vars[variableID].size / mk8_vars[variableID].elemSize;

    for (int i = 0; i < numElems; i++)
    {
        int id;
        pFile.read((char*)&id, sizeof(id));
        SwapINT(&id);
        std::string str;
        str = (char*)(&string_data[0] + id);
        pUnpacked << str << std::endl;
    }

    pUnpacked.close();
}

void MKOReader::MK8_Unpack_Movelist(int variableID)
{
    std::string varName = GetVariableNameMK8(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK8(variableID), pFile.beg);

    std::string output = varName;
    output += ".txt";
    std::ofstream pUnpacked(output);

    int numElems = mk8_vars[variableID].size / mk8_vars[variableID].elemSize;

    for (int i = 0; i < numElems; i++)
    {
        int id[2];
        pFile.read((char*)&id, sizeof(id));
        SwapINT(&id[0]);
        SwapINT(&id[1]);
        std::string str, str2;
        str = (char*)(&string_data[0] + id[0]);
        str2 = (char*)(&string_data[0] + id[1]);
        pUnpacked << str << " " << str2 << std::endl;
    }

    pUnpacked.close();
}

void MKOReader::MK8_Unpack_CHRBones(int variableID)
{
    std::string varName = GetVariableNameMK8(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK8(variableID), pFile.beg);

    std::string output = varName;
    output += ".txt";
    std::ofstream pUnpacked(output);

    int numElems = mk8_vars[variableID].size / mk8_vars[variableID].elemSize;

    for (int i = 0; i < numElems; i++)
    {
        int id[2];
        pFile.read((char*)&id, sizeof(id));
        SwapINT(&id[0]);
        SwapINT(&id[1]);
        std::string str;
        str = (char*)(&string_data[0] + id[1]);
        pUnpacked << str << std::endl;
    }

    pUnpacked.close();
}

void MKOReader::MK9_Unpack_RArt(int variableID)
{
    std::string varName = GetVariableNameMK9(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK9(variableID), pFile.beg);
    std::string output = varName;
    output += ".txt";
    std::ofstream pUnpacked(output);
    int numElems = 0;
    if (mk9_vars[variableID].size > 0)
        numElems = mk9_vars[variableID].size / mk9_vars[variableID].elemSize;

    for (int i = 0; i < numElems; i++)
    {
        int id;
        pFile.read((char*)&id, sizeof(id));
        SwapINT(&id);
        std::string str;
        str = (char*)(&string_data[0] + id);
        pUnpacked << str << std::endl;
    }

    pUnpacked.close();
}

void MKOReader::MK9_Unpack_Movelist(int variableID)
{
    std::string varName = GetVariableNameMK9(variableID);
    pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK9(variableID), pFile.beg);

    std::string output = varName;
    output += ".txt";
    std::ofstream pUnpacked(output);

    int numElems = 0;
    if (mk9_vars[variableID].size > 0)
        numElems = mk9_vars[variableID].size / mk9_vars[variableID].elemSize;

    for (int i = 0; i < numElems; i++)
    {
        int id[4];
        pFile.read((char*)&id, sizeof(id));
        SwapINT(&id[0]);
        SwapINT(&id[1]);
        SwapINT(&id[2]);
        SwapINT(&id[3]);
        std::string str, str2;
        str = (char*)(&string_data[0] + id[0]);
        str2 = (char*)(&string_data[0] + id[1]);
        pUnpacked << str << " " << str2 << std::endl;
    }

    pUnpacked.close();
}

void MKOReader::DecompileAllFunctions()
{
    if (!IsDecompSupported())
        return;

    for (unsigned int i = 0; i < funcs.size(); i++)
        DecompileFunction(i);
}

void MKOReader::DecompileAllFunctionsMK8()
{
    if (!IsDecompSupported())
        return;

    for (unsigned int i = 0; i < mk8_funcs.size(); i++)
        DecompileFunctionMK8(i);
}

void MKOReader::DecompileAllFunctionsMK9()
{
    if (!IsDecompSupported())
        return;

    for (unsigned int i = 0; i < mk9_funcs.size(); i++)
        DecompileFunctionMK9(i);
}

void MKOReader::DecompileAllFunctionsMK10()
{
    if (!IsDecompSupported())
        return;

    for (unsigned int i = 0; i < mk10_funcs.size(); i++)
        DecompileFunctionMK10(i);
}

void MKOReader::DecompileAllFunctionsDCF2()
{
    if (!IsDecompSupported())
        return;

    for (unsigned int i = 0; i < dcf2_funcs.size(); i++)
        DecompileFunctionDCF2(i);
}

void MKOReader::DecompileAllFunctionsMK11()
{
    if (!IsDecompSupported())
        return;

    for (unsigned int i = 0; i < mk11_funcs.size(); i++)
        DecompileFunctionMK11(i);
}

void MKOReader::DecompileAllFunctionsMK12()
{
    if (!IsDecompSupported())
        return;

    for (unsigned int i = 0; i < mk12_funcs.size(); i++)
        DecompileFunctionMK12(i);
}

void MKOReader::UnpackVariables()
{
    for (unsigned int i = 0; i < vars.size(); i++)
        UnpackVariable(i);
}

void MKOReader::UnpackVariablesMK8()
{
    for (unsigned int i = 0; i < mk8_vars.size(); i++)
        UnpackVariableMK8(i);
}

void MKOReader::UnpackVariablesMK9()
{
    for (unsigned int i = 0; i < mk9_vars.size(); i++)
        UnpackVariableMK9(i);
}

void MKOReader::UnpackVariablesDCF()
{
    for (unsigned int i = 0; i < dcf_vars.size(); i++)
        UnpackVariableDCF(i);
}

void MKOReader::UnpackVariablesMK10()
{
    for (unsigned int i = 0; i < mk10_vars.size(); i++)
        UnpackVariableMK10(i);
}

void MKOReader::UnpackVariablesDCF2()
{
    for (unsigned int i = 0; i < dcf2_vars.size(); i++)
        UnpackVariableDCF2(i);
}

void MKOReader::UnpackVariablesMK11()
{
    for (unsigned int i = 0; i < mk11_vars.size(); i++)
        UnpackVariableMK11(i);
}

bool MKOReader::IsBytecodeFixupMK9(int fixup)
{
    return fixup == MK9_Fixup_String || fixup == MK9_Fixup_Asset || fixup == MK9_Fixup_Function || fixup == MK9_Fixup_Sound;
}

void MKOReader::DumpFixupsMK9(std::string name)
{
    FILE* pFixup = fopen(name.c_str(), "w");

    if (!pFixup)
        return;

    fprintf(pFixup, "; fixups dump\n");
    fprintf(pFixup, "; format: \n");
    fprintf(pFixup, "; type ref base offset data extra\n");


    for (unsigned int i = 0; i < mk9_fixup.size(); i++)
    {
        mko_fixup_mk9 fx = mk9_fixup[i];


        fprintf(pFixup, "%d\t", fx.type);

        if (IsBytecodeFixupMK9(fx.type))
        {
            int id = GetFunctionForBytecodeOffsetMK9(fx.offset);
            if (id >= 0)
            {
                fprintf(pFixup, "code\t %s\t %d\t", GetFunctionNameMK9Actual(id).c_str(), fx.offset - mk9_funcs[id].functionOffset * 4);
            }
        }
        else
        {
            int id = GetVariableForOffsetMK9(fx.offset);
            if (id >= 0)
            {
                fprintf(pFixup, "var\t %s\t %d\t", GetVariableNameMK9(id).c_str(), fx.offset - mk9_vars[id].offset);
            }
            else
            {
                int fid = GetFunctionForStackOffsetMK9(fx.offset);
                if (fid < 0)
                {
                    std::cout << "ERROR: fixup " << i << " offset " << fx.offset << " has no owner" << std::endl;
                    fclose(pFixup);
                    return;
                }
                fprintf(pFixup, "stack\t %s\t %d\t", GetFunctionNameMK9Actual(fid).c_str(), fx.offset - mk9_funcs[fid].stackOffset);
            }
        }

        fprintf(pFixup, "%d\t %d\t\n", fx.data, fx.pad);
    }
    fclose(pFixup);
}

void MKOReader::DumpLocalFixupsMK9(std::string name)
{
    FILE* pFixup = fopen(name.c_str(), "w");

    if (!pFixup)
        return;

    fprintf(pFixup, "; local fixups dump\n");
    fprintf(pFixup, "; format: \n");
    fprintf(pFixup, "; type ref base offset data extra\n");


    for (unsigned int i = 0; i < mk9_local_fixups.size(); i++)
    {
        for (unsigned int a = 0; a < mk9_local_fixups[i].size(); a++)
        {
            mko_fixup_mk9 fx = mk9_local_fixups[i][a];
            fprintf(pFixup, "%d\t local\t %s\t %d\t %d\t %d\t\n", fx.type, GetFunctionNameMK9Actual(i).c_str(), fx.offset, fx.data, fx.pad);

        }
    }

    fclose(pFixup);

}

void MKOReader::DumpGlobalsMK9(std::string name)
{
    FILE* pGlobals = fopen(name.c_str(), "w");

    if (!pGlobals)
        return;

    fprintf(pGlobals, "; globals dump\n");
    fprintf(pGlobals, "; format: \n");
    fprintf(pGlobals, "; base offset\n");

    for (unsigned int i = 0; i < mk9_globals.size(); i++)
    {
        int id = GetVariableForOffsetMK9(mk9_globals[i].offset);

        if (id < 0)
            continue;

        fprintf(pGlobals, "%s\t %d\n", GetVariableNameMK9(id).c_str(), mk9_globals[i].offset - mk9_vars[id].offset);
    }

    fclose(pGlobals);
}

void MKOReader::DumpExternsMK9(std::string name)
{
    FILE* pExterns = fopen(name.c_str(), "w");

    if (!pExterns)
        return;

    fprintf(pExterns, "; externs dump\n");
    fprintf(pExterns, "; format: \n");
    fprintf(pExterns, "; nameHash importName(offset) paramsHash externHash field16 field20 field24\n");


    for (unsigned int i = 0; i < mk9_externs.size(); i++)
    {
        mko_extern_mk9 ext = mk9_externs[i];
        std::string ext_name = (char*)(&string_data[0] + (mk9_externs[i].importName - 1));
        fprintf(pExterns, "; %s\n", ext_name.c_str());
        fprintf(pExterns, "%X\t %d\t %X\t %X\t %d\t %d\t %d\t\n",ext.nameHash, ext.importName, ext.paramsHash, ext.externHash, ext.field16, ext.field20, ext.field24);
    }

    fclose(pExterns);
}

void MKOReader::DumpVariablesMK9(std::string name)
{
    // todo: globalize

    FILE* pVariables = fopen(name.c_str(), "w");

    if (!pVariables)
        return;

    // sort by offset to get the padded size (not stored)
    std::vector<int> sorted;
    for (unsigned int i = 0; i < mk9_vars.size(); i++)
        sorted.push_back(i);

    for (unsigned int i = 0; i < sorted.size(); i++)
    {
        for (unsigned int a = i + 1; a < sorted.size(); a++)
        {
            if (mk9_vars[sorted[a]].offset < mk9_vars[sorted[i]].offset)
            {
                int tmp = sorted[i];
                sorted[i] = sorted[a];
                sorted[a] = tmp;
            }
        }
    }


    std::vector<int> totalSizes;
    totalSizes.resize(mk9_vars.size());

    for (unsigned int i = 0; i < sorted.size(); i++)
    {
        int sortID = sorted[i];

        int nextOffset = mk9_header.variablesSize;
        if (i + 1 < sorted.size())
            nextOffset = mk9_vars[sorted[i + 1]].offset;

        int totalSize = nextOffset - mk9_vars[sortID].offset;
        if (totalSize < 0)
            totalSize = 0;

        totalSizes[sortID] = totalSize;
    }

    fprintf(pVariables, "; vars dump\n");
    fprintf(pVariables, "; format: \n");
    fprintf(pVariables, "; id (reference only, unused) name elemSize size isTweakvar offset\n");

    for (unsigned int i = 0; i < mk9_vars.size(); i++)
    {
        int size = mk9_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = GetVariableNameMK9(i);
        fprintf(pVariables, "%d\t%s\t%d\t%d\t%d\t%d\n", i, var_name.c_str(), mk9_vars[i].elemSize, size, mk9_header.tweakVarsSize > 0 && mk9_vars[i].offset >= mk9_header.tweakVarsOffset, mk9_vars[i].offset);
    }

    fclose(pVariables);
}

void MKOReader::DumpFunctionsMK9(std::string name)
{
    FILE* pFunctions = fopen(name.c_str(), "w");

    if (!pFunctions)
        return;

    fprintf(pFunctions, "; functions dump\n");
    fprintf(pFunctions, "; format: \n");
    fprintf(pFunctions, "; id (reference only, unused) name rawName nameOffset argsHash stackData stackSize numArgs globalObjects field28\n");

    for (unsigned int i = 0; i < mk9_funcs.size(); i++)
    {
        std::string func_name = GetFunctionNameMK9Actual(i);

        std::string func_stack_name = func_name;
        func_stack_name += "_stack";

        fprintf(pFunctions, "%d\t%s\t%s\t%d\t%X\t%s\t%d\t%d\t%d\t%d\n",
            i,
            func_name.c_str(),
            GetFunctionNameMK9(i).c_str(),
            mk9_funcs[i].nameOffset,
            mk9_funcs[i].paramsHash,
            func_stack_name.c_str(),
            mk9_funcs[i].stackSize,
            mk9_funcs[i].numArgs,
            mk9_funcs[i].globalObjects,
            mk9_funcs[i].field28);
    }

    fclose(pFunctions);
}


void MKOReader::PrintInfo()
{
    switch (game)
    {
    case Game_Deception:
    case Game_Armageddon:
    case Game_DeadlyAlliance:
    case Game_Unchained:
        PrintInfoMKDADU();
        break;
    case Game_MKVSDC:
        PrintInfoMK8();
        break;
    case Game_MK9:
        PrintInfoMK9();
        break;
    case Game_Injustice:
        PrintInfoDCF();
        break;
    case Game_MK10:
        PrintInfoMK10();
        break;
    case Game_Injustice2:
        PrintInfoDCF2();
        break;
    case Game_MK11:
        PrintInfoMK11();
        break;
    case Game_MK12:
        PrintInfoMK12();
        break;
    default:
        break;
    }
}

void MKOReader::PrintInfoMKDADU()
{
    printf("===========\n");
    printf("Header Info\n");
    printf("===========\n");
    printf("Script strings data size: \t%d\n", header.script_string_size);
    printf("String data size        : \t%d\n", header.string_data_size);
    printf("All functions size      : \t%d\n", GetAllFunctionsSize());
    printf("All variables size      : \t%d\n", GetAllVariablesSize());
    printf("===========\n");
    printf("Function Info\n");
    printf("===========\n");
    printf("Functions               : \t%d\n", header.functions);
    for (unsigned int i = 0; i < funcs.size(); i++)
    {
        std::string func_name = (char*)(&script_names[0] + (funcs[i].name_offset - 1));
        printf("Function %04d - %s \t Size: %d Unk: %d Offset: %d", i, func_name.c_str(), func_sizes[i], funcs[i].unknown, funcs[i].offset * 4);
        if (game == Game_Armageddon)
            printf(" Links: %d\n", mka_funcLinks[i].size());
        else
            printf("\n");
    }
    printf("===========\n");
    printf("Variable Info\n");
    printf("===========\n");
    printf("Variables               : \t%d\n", header.static_variables);
    for (unsigned int i = 0; i < vars.size(); i++)
    {
        std::string var_name = (char*)(&script_names[0] + (vars[i].name_offset - 1));
        printf("Variable %04d - %s \t Size: %d Elements: %d Offset: %d", i, var_name.c_str(), var_sizes[i], vars[i].numElems, vars[i].offset * 4);
        if (game == Game_Armageddon)
            printf(" Links: %d\n", mka_varLinks[i].size());
        else
            printf("\n");
    }
}

void MKOReader::PrintInfoMK8()
{
    printf("Header Info\n");
    printf("===========\n");
    printf("String data size        : \t%d\n", mk8_header.string_size);
    printf("===========\n");
    printf("Function Info\n");
    printf("===========\n");
    printf("Functions               : \t%d\n", mk8_header.functions);
    for (unsigned int i = 0; i < mk8_funcs.size(); i++)
    {
        std::string func_name = (char*)(&string_data[0] + (mk8_funcs[i].nameOffset - 1));
        std::string arg_type = (char*)(&string_data[0] + (mk8_funcs[i].argTypeOffset - 1));
        arg_type = arg_type.substr(0, arg_type.length() - 1);
        std::replace(arg_type.begin(), arg_type.end(), '.', ',');

        printf("Function %04d - %s(%s) \t\n", i, func_name.c_str(), arg_type.c_str());

    }
    printf("===========\n");
    printf("Variable Info\n");
    printf("===========\n");
    printf("Variables               : \t%d\n", mk8_header.static_variables);
    for (unsigned int i = 0; i < mk8_vars.size(); i++)
    {
        std::string var_name = (char*)(&string_data[0] + (mk8_vars[i].name_offset - 1));
        printf("Variable %04d - %s\n", i, var_name.c_str());

    }
    printf("===========\n");
    printf("Externs               : \t%d\n", mk8_header.externs);
    for (unsigned int i = 0; i < mk8_externs.size(); i++)
    {
        std::string ext_name = (char*)(&string_data[0] + (mk8_externs[i].nameOffset - 1));
        std::string imp_name = (char*)(&string_data[0] + (mk8_externs[i].importName - 1));
        printf("Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
    }
    printf("===========\n");
    printf("Assets               : \t%d\n", mk8_header.assets);
    for (unsigned int i = 0; i < mk8_assets.size(); i++)
    {
        std::string ass_name = (char*)(&string_data[0] + (mk8_assets[i].nameOffset - 1));
        std::string arch_name = (char*)(&string_data[0] + (mk8_assets[i].archiveNameOffset - 1));
        printf("Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());

    }
    printf("===========\n");
}

void MKOReader::PrintInfoMK9()
{
    printf("Header Info\n");
    printf("===========\n");
    printf("String data size        : \t%d\n", mk9_header.string_size);
    printf("===========\n");
    printf("Function Info\n");
    printf("===========\n");
    printf("Functions               : \t%d\n", mk9_header.functions);
    for (unsigned int i = 0; i < mk9_funcs.size(); i++)
    {
        std::string func_name = (char*)(&string_data[0] + (mk9_funcs[i].nameOffset - 1));
        std::string arg_type = MKODict::GetHashString(mk9_funcs[i].paramsHash);

        if (MKODict::IsHashAvailable(mk9_funcs[i].paramsHash))
        {
            arg_type = arg_type.substr(0, arg_type.length() - 1);
            std::replace(arg_type.begin(), arg_type.end(), '.', ',');
        }

        printf("Function %04d - %s(%s)\t\n", i, func_name.c_str(), arg_type.c_str());
    }

    printf("===========\n");
    printf("Variable Info\n");
    printf("===========\n");
    printf("Variables               : \t%d\n", mk9_header.static_variables);
    for (unsigned int i = 0; i < mk9_vars.size(); i++)
    {
        std::string var_name = MKODict::GetHashString(mk9_vars[i].name_hash);
        printf("Variable %04d - %s \tSize: %d\n", i, var_name.c_str(), mk9_vars[i].size);
    }
    printf("===========\n");
    printf("Dyn.Variables               : \t%d\n", mk9_header.externVariables);
    for (unsigned int i = 0; i < mk9_extern_vars.size(); i++)
    {
        std::string var_name = (char*)(&string_data[0] + (mk9_extern_vars[i].name_hash - 1));
        printf("DVariable %04d - %s \tSize: %d\n", i, var_name.c_str(), mk9_extern_vars[i].size);
    }
    printf("===========\n");
    printf("Externs               : \t%d\n", mk9_header.externs);
    for (unsigned int i = 0; i < mk9_externs.size(); i++)
    {
        std::string ext_name = MKODict::GetHashString(mk9_externs[i].nameHash);
        std::string imp_name = (char*)(&string_data[0] + (mk9_externs[i].importName - 1));
        printf("Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
    }
    printf("===========\n");
    printf("Assets               : \t%d\n", mk9_header.assets);
    for (unsigned int i = 0; i < mk9_assets.size(); i++)
    {
        std::string ass_name = (char*)(&string_data[0] + (mk9_assets[i].nameOffset - 1));
        std::string arch_name = (char*)(&string_data[0] + (mk9_assets[i].archiveNameOffset - 1));
        printf("Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());

    }
    printf("===========\n");
    printf("Sound Assets               : \t%d\n", mk9_header.soundAssets);
    for (unsigned int i = 0; i < mk9_sounds.size(); i++)
    {
        std::string ass_name = (char*)(&string_data[0] + (mk9_sounds[i].nameOffset - 1));
        std::string arch_name = (char*)(&string_data[0] + (mk9_sounds[i].archiveNameOffset - 1));
        printf("Sound Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());

    }
    printf("===========\n");
}

void MKOReader::PrintInfoDCF()
{
    printf("Header Info\n");
    printf("===========\n");
    printf("String data size        : \t%d\n", dcf_header.string_size);
    printf("===========\n");
    printf("Function Info\n");
    printf("===========\n");
    printf("Functions               : \t%d\n", dcf_header.functions);
    for (unsigned int i = 0; i < dcf_funcs.size(); i++)
    {
        std::string func_name = (char*)(&string_data[0] + (dcf_funcs[i].nameOffset - 1));
        std::string arg_type = MKODict::GetHashString(dcf_funcs[i].paramsHash);

        if (MKODict::IsHashAvailable(dcf_funcs[i].paramsHash))
        {
            arg_type = arg_type.substr(0, arg_type.length() - 1);
            std::replace(arg_type.begin(), arg_type.end(), '.', ',');
        }

        printf("Function %04d - %s(%s)\t\n", i, func_name.c_str(), arg_type.c_str());
    }

    printf("===========\n");
    printf("Variable Info\n");
    printf("===========\n");
    printf("Variables               : \t%d\n", dcf_header.static_variables);
    for (unsigned int i = 0; i < dcf_vars.size(); i++)
    {
        std::string var_name = MKODict::GetHashString(dcf_vars[i].name_hash);
        printf("Variable %04d - %s \tSize: %d\n", i, var_name.c_str(), dcf_vars[i].size);
    }
    printf("===========\n");
    printf("Dyn.Variables               : \t%d\n", dcf_header.externVariables);
    for (unsigned int i = 0; i < dcf_dyn_vars.size(); i++)
    {
        std::string var_name = (char*)(&string_data[0] + (dcf_dyn_vars[i].name_hash - 1));
        printf("DVariable %04d - %s \tSize: %d\n", i, var_name.c_str(), dcf_dyn_vars[i].size);
    }
    printf("===========\n");
    printf("Externs               : \t%d\n", dcf_header.externs);
    for (unsigned int i = 0; i < dcf_externs.size(); i++)
    {
        std::string ext_name = MKODict::GetHashString(dcf_externs[i].nameHash);
        std::string imp_name = (char*)(&string_data[0] + (dcf_externs[i].importName - 1));
        printf("Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
    }
    printf("===========\n");
    printf("Assets               : \t%d\n", dcf_header.assets);
    for (unsigned int i = 0; i < dcf_assets.size(); i++)
    {
        std::string ass_name = (char*)(&string_data[0] + (dcf_assets[i].nameOffset - 1));
        std::string arch_name = (char*)(&string_data[0] + (dcf_assets[i].archiveNameOffset - 1));
        printf("Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());

    }
    printf("===========\n");
    printf("Sound Assets               : \t%d\n", dcf_header.soundAssets);
    for (unsigned int i = 0; i < dcf_sounds.size(); i++)
    {
        std::string ass_name = (char*)(&string_data[0] + (dcf_sounds[i].nameOffset - 1));
        std::string arch_name = (char*)(&string_data[0] + (dcf_sounds[i].archiveNameOffset - 1));
        printf("Sound Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());

    }
    printf("===========\n");
}

void MKOReader::PrintInfoMK10()
{
    printf("Header Info\n");
    printf("===========\n");
    printf("String data size        : \t%d\n", mk10_header.string_size);
    printf("Tweak string data size        : \t%d\n", mk10_header.tweakvars_stringSize);
    printf("Tweakvars data size        : \t%d\n", mk10_header.tweakvars_size);
    printf("===========\n");
    printf("Function Info\n");
    printf("===========\n");
    printf("Functions               : \t%d\n", mk10_header.functions);
    for (unsigned int i = 0; i < mk10_funcs.size(); i++)
    {
        std::string func_name = (char*)(&string_data[0] + (mk10_funcs[i].nameOffset - 1));
        std::string arg_type = MKODict::GetHashString(mk10_funcs[i].paramsHash);

        if (MKODict::IsHashAvailable(mk10_funcs[i].paramsHash))
        {
            arg_type = arg_type.substr(0, arg_type.length() - 1);
            std::replace(arg_type.begin(), arg_type.end(), '.', ',');
        }

        printf("Function %04d - %s(%s)\t\n", i, func_name.c_str(), arg_type.c_str());
    }
    printf("===========\n");
    printf("Variable Info\n");
    printf("===========\n");
    printf("Variables               : \t%d \n", mk10_header.static_variables);
    for (unsigned int i = 0; i < mk10_vars.size(); i++)
    {
        std::string name = MKODict::GetHashString(mk10_vars[i].nameHash);
        printf("Variable %04d - %s \tSize: %d\n", i, name.c_str(), mk10_vars[i].size);
    }
    printf("Dynamic Variables               : \t%d\n", mk10_header.dynamic_variables);
    for (unsigned int i = 0; i < mk10_dyn_vars.size(); i++)
    {
        std::string name = MKODict::GetHashString(mk10_dyn_vars[i].nameHash);
        printf("Dynamic Variable %04d - %s \tSize: %d\n", i, name.c_str(), mk10_vars[i].size);
    }
    printf("===========\n");
    printf("Externs               : \t%d\n", mk10_header.externs);
    for (unsigned int i = 0; i < mk10_externs.size(); i++)
    {
        std::string ext_name = MKODict::GetHashString(mk10_externs[i].nameHash);
        std::string imp_name = (char*)(&string_data[0] + (mk10_externs[i].file_name_offset - 1));
        printf("Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
    }
    printf("===========\n");
    printf("Extern Variables               : \t%d\n", mk10_header.extern_variables);
    for (unsigned int i = 0; i < mk10_extern_vars.size(); i++)
    {
        std::string  name = (char*)(&string_data[0] + (mk10_extern_vars[i].name_offset - 1));
        printf("Extern %04d - %s\n", i, name.c_str());
    }
    printf("===========\n");
    printf("Assets               : \t%d\n", mk10_header.assets);
    for (unsigned int i = 0; i < mk10_assets.size(); i++)
    {
        std::string ass_name = (char*)(&string_data[0] + (mk10_assets[i].item_name_offset - 1));
        std::string arch_name = (char*)(&string_data[0] + (mk10_assets[i].package_name_offset - 1));
        printf("Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());

    }
    printf("===========\n");
    printf("Source Files\n\n");
    for (unsigned int i = 0; i < mk10_srcs.size(); i++)
    {
        std::string  name = (char*)(&string_data[0] + (mk10_srcs[i].name_offset - 1));
        printf("%s\n", name.c_str());
    }
    printf("===========\n");
    printf("Fixup Entries               : \t%d\n", mk10_fixup.size());
    printf("===========\n");
}

void MKOReader::PrintInfoDCF2()
{
    printf("Header Info\n");
    printf("===========\n");
    printf("String data size        : \t%d\n", dcf2_header.string_size);
    printf("Tweak string data size        : \t%d\n", dcf2_header.tweakvars_stringSize);
    printf("===========\n");
    printf("Function Info\n");
    printf("===========\n");
    printf("Functions               : \t%d\n", dcf2_header.functions);
    for (unsigned int i = 0; i < dcf2_funcs.size(); i++)
    {
        std::string func_name = (char*)(&string_data[0] + (dcf2_funcs[i].nameOffset - 1));
        std::string arg_type = MKODict::GetHashString(dcf2_funcs[i].paramsHash);

        if (MKODict::IsHashAvailable(dcf2_funcs[i].paramsHash))
        {
            arg_type = arg_type.substr(0, arg_type.length() - 1);
            std::replace(arg_type.begin(), arg_type.end(), '.', ',');
        }

        printf("Function %04d - %s(%s)\t\n", i, func_name.c_str(), arg_type.c_str());
    }
    printf("===========\n");
    printf("Variable Info\n");
    printf("===========\n");
    printf("Variables               : \t%d \n", dcf2_header.static_variables);
    for (unsigned int i = 0; i < dcf2_vars.size(); i++)
    {
        std::string name = MKODict::GetHashString(dcf2_vars[i].nameHash);
        printf("Variable %04d - %s \tSize: %d\n", i, name.c_str(), dcf2_vars[i].size);
    }
    printf("Dynamic Variables               : \t%d\n", dcf2_header.dynamic_variables);
    for (unsigned int i = 0; i < dcf2_dyn_vars.size(); i++)
    {
        std::string name = MKODict::GetHashString(dcf2_dyn_vars[i].nameHash);
        printf("Dynamic Variable %04d - %s \tSize: %d\n", i, name.c_str(), dcf2_vars[i].size);
    }
    printf("===========\n");
    printf("Externs               : \t%d\n", dcf2_header.externs);
    for (unsigned int i = 0; i < dcf2_externs.size(); i++)
    {
        std::string ext_name = MKODict::GetHashString(dcf2_externs[i].nameHash);
        std::string imp_name = (char*)(&string_data[0] + (dcf2_externs[i].file_name_offset - 1));
        printf("Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
    }
    printf("===========\n");
    printf("Extern Variables               : \t%d\n", dcf2_header.extern_variables);
    for (unsigned int i = 0; i < dcf2_extern_vars.size(); i++)
    {
        std::string  name = (char*)(&string_data[0] + (dcf2_extern_vars[i].name_offset - 1));
        printf("Extern %04d - %s\n", i, name.c_str());
    }
    printf("===========\n");
    printf("Assets               : \t%d\n", dcf2_header.assets);
    for (unsigned int i = 0; i < dcf2_assets.size(); i++)
    {
        std::string ass_name = (char*)(&string_data[0] + (dcf2_assets[i].item_name_offset - 1));
        std::string arch_name = (char*)(&string_data[0] + (dcf2_assets[i].package_name_offset - 1));
        printf("Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());

    }
    printf("===========\n");
    printf("Source Files\n\n");
    for (unsigned int i = 0; i < dcf2_srcs.size(); i++)
    {
        std::string  name = (char*)(&string_data[0] + (dcf2_srcs[i].name_offset - 1));
        printf("%s\n", name.c_str());
    }
    printf("===========\n");
    printf("Fixup Entries               : \t%d\n", dcf2_fixup.size());
    printf("===========\n");
}

void MKOReader::PrintInfoMK11()
{
    printf("Header Info\n");
    printf("===========\n");
    printf("String data size        : \t%d\n", mk11_header.string_size);
    printf("Tweak string data size        : \t%d\n", mk11_header.tweakvars_stringSize);
    printf("===========\n");
    printf("Function Info\n");
    printf("===========\n");
    printf("Functions               : \t%d\n", mk11_header.functions);
    for (unsigned int i = 0; i < mk11_funcs.size(); i++)
    {
        std::string func_name = (char*)(&string_data[0] + (mk11_funcs[i].nameOffset - 1));
        std::string arg_type = MKODict::GetHashString(mk11_funcs[i].paramsHash);

        if (MKODict::IsHashAvailable(mk11_funcs[i].paramsHash))
        {
            arg_type = arg_type.substr(0, arg_type.length() - 1);
            std::replace(arg_type.begin(), arg_type.end(), '.', ',');
        }

        printf("Function %04d - %s(%s)\t\n", i, func_name.c_str(), arg_type.c_str());
    }
    printf("===========\n");
    printf("Variable Info\n");
    printf("===========\n");
    printf("Variables               : \t%d \n", mk11_header.static_variables);
    for (unsigned int i = 0; i < mk11_vars.size(); i++)
    {
        std::string name = MKODict::GetHashString(mk11_vars[i].nameHash);
        printf("Variable %04d - %s \tSize: %d\n", i, name.c_str(), mk11_vars[i].size);
    }
    printf("Dynamic Variables               : \t%d\n", mk11_header.dynamic_variables);
    for (unsigned int i = 0; i < mk11_dyn_vars.size(); i++)
    {
        std::string name = MKODict::GetHashString(mk11_dyn_vars[i].nameHash);
        printf("Dynamic Variable %04d - %s \tSize: %d\n", i, name.c_str(), mk11_vars[i].size);
    }
    printf("===========\n");
    printf("Externs               : \t%d\n", mk11_header.externs);
    for (unsigned int i = 0; i < mk11_externs.size(); i++)
    {
        std::string ext_name = MKODict::GetHashString(mk11_externs[i].nameHash);
        std::string imp_name = (char*)(&string_data[0] + (mk11_externs[i].file_name_offset - 1));
        printf("Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
    }
    printf("===========\n");
    printf("Extern Variables               : \t%d\n", mk11_header.extern_variables);
    for (unsigned int i = 0; i < mk11_extern_vars.size(); i++)
    {
        std::string  name = (char*)(&string_data[0] + (mk11_extern_vars[i].name_offset - 1));
        printf("Extern %04d - %s\n", i, name.c_str());
    }
    printf("===========\n");
    printf("Assets               : \t%d\n", mk11_header.assets);
    for (unsigned int i = 0; i < mk11_assets.size(); i++)
    {
        std::string ass_name = (char*)(&string_data[0] + (mk11_assets[i].item_name_offset - 1));
        std::string arch_name = (char*)(&string_data[0] + (mk11_assets[i].package_name_offset - 1));
        printf("Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());

    }
    printf("===========\n");
    printf("Source Files\n\n");
    for (unsigned int i = 0; i < mk11_srcs.size(); i++)
    {
        std::string  name = (char*)(&string_data[0] + (mk11_srcs[i].name_offset - 1));
        printf("%s\n", name.c_str());
    }
    printf("===========\n");
    printf("Fixup Entries               : \t%d\n", mk11_fixup.size());
    printf("===========\n");
}

void MKOReader::PrintInfoMK12()
{
    printf("Header Info\n");
    printf("===========\n");
    printf("String data size        : \t%d\n", mk12_header.string_size);
    printf("Tweak string data size        : \t%d\n", mk12_header.tweakvars_stringSize);
    printf("===========\n");
    printf("Function Info\n");
    printf("===========\n");
    printf("Functions               : \t%d\n", mk12_header.functions);
    printf("===========\n");
    printf("Variable Info\n");
    printf("===========\n");
    printf("Variables               : \t%d \n", mk12_header.static_variables);
    printf("Dynamic Variables               : \t%d\n", mk12_header.dynamic_variables);
    printf("===========\n");
    printf("Assets               : \t%d\n", mk12_header.assets);
    printf("===========\n");
    printf("Source Files\n\n");
    for (unsigned int i = 0; i < mk12_srcs.size(); i++)
    {
        printf("%s\n", mk12_srcs[i].c_str());
    }
    printf("===========\n");
    printf("Fixup Entries               : \t%d\n", mk12_fixup.size());
    printf("===========\n");
}

void MKOReader::DumpInfoMKDADU(std::string name)
{
    std::ofstream oInfo(name);
    if (oInfo) {

        static char pInfo[256] = {};
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Header Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Script strings data size: \t%d\n", header.script_string_size);
        oInfo << pInfo;
        sprintf(pInfo, "String data size        : \t%d\n", header.string_data_size);
        oInfo << pInfo;
        sprintf(pInfo, "All functions size      : \t%d\n", GetAllFunctionsSize());
        oInfo << pInfo;
        sprintf(pInfo, "All variables size      : \t%d\n", GetAllVariablesSize());
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Function Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Functions               : \t%d\n", header.functions);
        oInfo << pInfo;
        sprintf(pInfo, "Functions Start         : \t0x%X\n",m_pFunctionsStartOffset);
        oInfo << pInfo;
        for(unsigned int i = 0; i < funcs.size(); i++)
        {
            std::string func_name = (char*)(&script_names[0] + (funcs[i].name_offset - 1));
            sprintf(pInfo, "Function %04d - %s \t Size: %d Unk: %d Offset: %d", i, func_name.c_str(), func_sizes[i], funcs[i].unknown, funcs[i].offset * 4);
            oInfo << pInfo;

            if (game == Game_Armageddon)
                sprintf(pInfo, " Links: %d\n", mka_funcLinks[i].size());
            else
                sprintf(pInfo, "\n");

            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variable Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variables               : \t%d\n", header.static_variables);
        oInfo << pInfo;
        sprintf(pInfo, "Variables Start         : \t0x%X\n", m_pDataStartOffset);
        oInfo << pInfo;
        for (unsigned int i = 0; i < vars.size(); i++)
        {
            std::string var_name = (char*)(&script_names[0] + (vars[i].name_offset - 1));
            sprintf(pInfo, "Variable %04d - %s \t Size: %d Elements: %d Offset: %d", i, var_name.c_str(), var_sizes[i], vars[i].numElems, vars[i].offset * 4);
            oInfo << pInfo;

            if (game == Game_Armageddon)
                sprintf(pInfo, " Links: %d\n", mka_varLinks[i].size());
            else
                sprintf(pInfo, "\n");

            oInfo << pInfo;
        }
        oInfo.close();
    }
   
}

void MKOReader::DumpInfoMK8(std::string name)
{
    std::ofstream oInfo(name);
    if (oInfo) {

        static char pInfo[256] = {};
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Header Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "String data size        : \t%d\n", mk8_header.string_size);
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Function Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Functions               : \t%d\n", mk8_header.functions);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk8_funcs.size(); i++)
        {
            std::string func_name = (char*)(&string_data[0] + (mk8_funcs[i].nameOffset - 1));
            std::string arg_type = (char*)(&string_data[0] + (mk8_funcs[i].argTypeOffset - 1));
            arg_type = arg_type.substr(0, arg_type.length() - 1);
            std::replace(arg_type.begin(), arg_type.end(), '.', ',');

            sprintf(pInfo, "Function %04d - %s(%s) \t\n", i, func_name.c_str(), arg_type.c_str());
            oInfo << pInfo;
        }

        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variable Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variables               : \t%d\n", mk8_header.static_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk8_vars.size(); i++)
        {
            std::string var_name = (char*)(&string_data[0] + (mk8_vars[i].name_offset - 1));
            sprintf(pInfo, "Variable %04d - %s\n", i, var_name.c_str());
            oInfo << pInfo;
        }

        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Externs               : \t%d\n", mk8_header.externs);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk8_externs.size(); i++)
        {
            std::string ext_name = (char*)(&string_data[0] + (mk8_externs[i].nameOffset - 1));
            std::string imp_name = (char*)(&string_data[0] + (mk8_externs[i].importName - 1));
            sprintf(pInfo, "Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        sprintf(pInfo, "Assets               : \t%d\n", mk8_header.assets);
        for (unsigned int i = 0; i < mk8_assets.size(); i++)
        {
            std::string ass_name = (char*)(&string_data[0] + (mk8_assets[i].nameOffset - 1));
            std::string arch_name = (char*)(&string_data[0] + (mk8_assets[i].archiveNameOffset - 1));
            sprintf(pInfo, "Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
            oInfo << pInfo;
        }

        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
    }

    oInfo.close();
}

void MKOReader::DumpInfoMK9(std::string name)
{
    std::ofstream oInfo(name);
    if (oInfo) {

        static char pInfo[256] = {};
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Header Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "String data size        : \t%d\n", mk9_header.string_size);
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Function Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Functions               : \t%d\n", mk9_header.functions);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk9_funcs.size(); i++)
        {
            std::string func_name = (char*)(&string_data[0] + (mk9_funcs[i].nameOffset - 1));
            std::string arg_type = MKODict::GetHashString(mk9_funcs[i].paramsHash);

            if (MKODict::IsHashAvailable(mk9_funcs[i].paramsHash))
            {
                arg_type = arg_type.substr(0, arg_type.length() - 1);
                std::replace(arg_type.begin(), arg_type.end(), '.', ',');
            }

            sprintf(pInfo, "Function %04d - %s(%s)\t\n", i, func_name.c_str(), arg_type.c_str());
            oInfo << pInfo;
        }

        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variable Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variables               : \t%d\n", mk9_header.static_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk9_vars.size(); i++)
        {
            std::string var_name = MKODict::GetHashString(mk9_vars[i].name_hash);
            sprintf(pInfo, "Variable %04d - %s \tSize: %d\n", i, var_name.c_str(), mk9_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Dyn.Variables               : \t%d\n", mk9_header.externVariables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk9_extern_vars.size(); i++)
        {
            std::string var_name = (char*)(&string_data[0] + (mk9_extern_vars[i].name_hash - 1));
            sprintf(pInfo, "DVariable %04d - %s \tSize: %d\n", i, var_name.c_str(), mk9_extern_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Externs               : \t%d\n", mk9_header.externs);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk9_externs.size(); i++)
        {
            std::string ext_name = MKODict::GetHashString(mk9_externs[i].nameHash);
            std::string imp_name = (char*)(&string_data[0] + (mk9_externs[i].importName - 1));
            sprintf(pInfo, "Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Assets               : \t%d\n", mk9_header.assets);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk9_assets.size(); i++)
        {
            std::string ass_name = (char*)(&string_data[0] + (mk9_assets[i].nameOffset - 1));
            std::string arch_name = (char*)(&string_data[0] + (mk9_assets[i].archiveNameOffset - 1));
            sprintf(pInfo, "Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Sound Assets               : \t%d\n", mk9_header.soundAssets);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk9_sounds.size(); i++)
        {
            std::string ass_name = (char*)(&string_data[0] + (mk9_sounds[i].nameOffset - 1));
            std::string arch_name = (char*)(&string_data[0] + (mk9_sounds[i].archiveNameOffset - 1));
            sprintf(pInfo, "Sound Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
    }

    oInfo.close();

}

void MKOReader::DumpInfoDCF(std::string name)
{
    std::ofstream oInfo(name);
    if (oInfo) {

        static char pInfo[256] = {};
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Header Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "String data size        : \t%d\n", dcf_header.string_size);
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Function Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Functions               : \t%d\n", dcf_header.functions);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf_funcs.size(); i++)
        {
            std::string func_name = (char*)(&string_data[0] + (dcf_funcs[i].nameOffset - 1));
            std::string arg_type = MKODict::GetHashString(dcf_funcs[i].paramsHash);

            if (MKODict::IsHashAvailable(dcf_funcs[i].paramsHash))
            {
                arg_type = arg_type.substr(0, arg_type.length() - 1);
                std::replace(arg_type.begin(), arg_type.end(), '.', ',');
            }

            sprintf(pInfo, "Function %04d - %s(%s)\t\n", i, func_name.c_str(), arg_type.c_str());
            oInfo << pInfo;
        }

        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variable Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variables               : \t%d\n", dcf_header.static_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf_vars.size(); i++)
        {
            std::string var_name = MKODict::GetHashString(dcf_vars[i].name_hash);
            sprintf(pInfo, "Variable %04d - %s \tSize: %d\n", i, var_name.c_str(), dcf_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Dyn.Variables               : \t%d\n", dcf_header.externVariables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf_dyn_vars.size(); i++)
        {
            std::string var_name = (char*)(&string_data[0] + (dcf_dyn_vars[i].name_hash - 1));
            sprintf(pInfo, "DVariable %04d - %s \tSize: %d\n", i, var_name.c_str(), dcf_dyn_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Externs               : \t%d\n", dcf_header.externs);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf_externs.size(); i++)
        {
            std::string ext_name = MKODict::GetHashString(dcf_externs[i].nameHash);
            std::string imp_name = (char*)(&string_data[0] + (dcf_externs[i].importName - 1));
            sprintf(pInfo, "Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Assets               : \t%d\n", dcf_header.assets);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf_assets.size(); i++)
        {
            std::string ass_name = (char*)(&string_data[0] + (dcf_assets[i].nameOffset - 1));
            std::string arch_name = (char*)(&string_data[0] + (dcf_assets[i].archiveNameOffset - 1));
            sprintf(pInfo, "Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Sound Assets               : \t%d\n", dcf_header.soundAssets);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf_sounds.size(); i++)
        {
            std::string ass_name = (char*)(&string_data[0] + (dcf_sounds[i].nameOffset - 1));
            std::string arch_name = (char*)(&string_data[0] + (dcf_sounds[i].archiveNameOffset - 1));
            sprintf(pInfo, "Sound Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
    }

    oInfo.close();

}

void MKOReader::DumpInfoMK10(std::string name)
{
    std::ofstream oInfo(name);
    if (oInfo) {

        static char pInfo[1024] = {};
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Header Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "String data size        : \t%d\n", mk10_header.string_size);
        oInfo << pInfo;
        sprintf(pInfo, "Tweak string data size        : \t%d\n", mk10_header.tweakvars_stringSize);
        oInfo << pInfo;
        sprintf(pInfo, "Tweakvars data size        : \t%d\n", mk10_header.tweakvars_size);
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Function Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Functions               : \t%d\n", mk10_header.functions);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk10_funcs.size(); i++)
        {
            std::string func_name = (char*)(&string_data[0] + (mk10_funcs[i].nameOffset - 1));
            std::string arg_type = MKODict::GetHashString(mk10_funcs[i].paramsHash);

            if (MKODict::IsHashAvailable(mk10_funcs[i].paramsHash))
            {
                arg_type = arg_type.substr(0, arg_type.length() - 1);
                std::replace(arg_type.begin(), arg_type.end(), '.', ',');
            }

            sprintf(pInfo, "Function %04d - %s(%s) HASH: 0x%X\t\n", i, func_name.c_str(), arg_type.c_str(), mk10_funcs[i].functionHash);
            oInfo << pInfo;
        }

        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variable Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variables               : \t%d \n", mk10_header.static_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk10_vars.size(); i++)
        {
            std::string name = MKODict::GetHashString(mk10_vars[i].nameHash);
            sprintf(pInfo, "Variable %04d - %s \tSize: %d\n", i, name.c_str(), mk10_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo,"Dynamic Variables               : \t%d\n", mk10_header.dynamic_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk10_dyn_vars.size(); i++)
        {
            std::string name = MKODict::GetHashString(mk10_dyn_vars[i].nameHash);
            sprintf(pInfo, "Dynamic Variable %04d - %s \tSize: %d\n", i, name.c_str(), mk10_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo,"===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Externs               : \t%d\n", mk10_header.externs);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk10_externs.size(); i++)
        {
            std::string ext_name = MKODict::GetHashString(mk10_externs[i].nameHash);
            std::string imp_name = (char*)(&string_data[0] + (mk10_externs[i].file_name_offset - 1));
            sprintf(pInfo, "Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Extern Variables               : \t%d\n", mk10_header.extern_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk10_extern_vars.size(); i++)
        {
            std::string  name = (char*)(&string_data[0] + (mk10_extern_vars[i].name_offset - 1));
            sprintf(pInfo, "Extern %04d - %s\n", i, name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Assets               : \t%d\n", mk10_header.assets);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk10_assets.size(); i++)
        {
            std::string ass_name = (char*)(&string_data[0] + (mk10_assets[i].item_name_offset - 1));
            std::string arch_name = (char*)(&string_data[0] + (mk10_assets[i].package_name_offset - 1));
            sprintf(pInfo, "Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Source Files\n\n");
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk10_srcs.size(); i++)
        {
            std::string  name = (char*)(&string_data[0] + (mk10_srcs[i].name_offset - 1));
            sprintf(pInfo, "%s\n", name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Fixup Entries               : \t%d\n", mk10_fixup.size());
        oInfo << pInfo;
    }

    oInfo.close();
}

void MKOReader::DumpInfoDCF2(std::string name)
{
    std::ofstream oInfo(name);
    if (oInfo) {

        static char pInfo[1024] = {};
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Header Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "String data size        : \t%d\n", dcf2_header.string_size);
        oInfo << pInfo;
        sprintf(pInfo, "Tweak string data size        : \t%d\n", dcf2_header.tweakvars_stringSize);
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Function Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Functions               : \t%d\n", dcf2_header.functions);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf2_funcs.size(); i++)
        {
            std::string func_name = (char*)(&string_data[0] + (dcf2_funcs[i].nameOffset - 1));
            std::string arg_type = MKODict::GetHashString(dcf2_funcs[i].paramsHash);

            if (MKODict::IsHashAvailable(dcf2_funcs[i].paramsHash))
            {
                arg_type = arg_type.substr(0, arg_type.length() - 1);
                std::replace(arg_type.begin(), arg_type.end(), '.', ',');
            }

            sprintf(pInfo, "Function %04d - %s(%s) HASH: 0x%X\t\n", i, func_name.c_str(), arg_type.c_str(), dcf2_funcs[i].functionHash);
            oInfo << pInfo;
        }

        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variable Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variables               : \t%d \n", dcf2_header.static_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf2_vars.size(); i++)
        {
            std::string name = MKODict::GetHashString(dcf2_vars[i].nameHash);
            sprintf(pInfo, "Variable %04d - %s \tSize: %d\n", i, name.c_str(), dcf2_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Dynamic Variables               : \t%d\n", dcf2_header.dynamic_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf2_dyn_vars.size(); i++)
        {
            std::string name = MKODict::GetHashString(dcf2_dyn_vars[i].nameHash);
            sprintf(pInfo, "Dynamic Variable %04d - %s \tSize: %d\n", i, name.c_str(), dcf2_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Externs               : \t%d\n", dcf2_header.externs);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf2_externs.size(); i++)
        {
            std::string ext_name = MKODict::GetHashString(dcf2_externs[i].nameHash);
            std::string imp_name = (char*)(&string_data[0] + (dcf2_externs[i].file_name_offset - 1));
            sprintf(pInfo, "Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Extern Variables               : \t%d\n", dcf2_header.extern_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf2_extern_vars.size(); i++)
        {
            std::string  name = (char*)(&string_data[0] + (dcf2_extern_vars[i].name_offset - 1));
            sprintf(pInfo, "Extern %04d - %s\n", i, name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Assets               : \t%d\n", dcf2_header.assets);
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf2_assets.size(); i++)
        {
            std::string ass_name = (char*)(&string_data[0] + (dcf2_assets[i].item_name_offset - 1));
            std::string arch_name = (char*)(&string_data[0] + (dcf2_assets[i].package_name_offset - 1));
            sprintf(pInfo, "Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Source Files\n\n");
        oInfo << pInfo;
        for (unsigned int i = 0; i < dcf2_srcs.size(); i++)
        {
            std::string  name = (char*)(&string_data[0] + (dcf2_srcs[i].name_offset - 1));
            sprintf(pInfo, "%s\n", name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Fixup Entries               : \t%d\n", dcf2_fixup.size());
        oInfo << pInfo;
    }

    oInfo.close();
}

void MKOReader::DumpInfoMK11(std::string name)
{
    std::ofstream oInfo(name);
    if (oInfo) {

        static char pInfo[1024] = {};
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Header Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "String data size        : \t%d\n", mk11_header.string_size);
        oInfo << pInfo;
        sprintf(pInfo, "Tweak string data size        : \t%d\n", mk11_header.tweakvars_stringSize);
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Function Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Functions               : \t%d\n", mk11_header.functions);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk11_funcs.size(); i++)
        {
            std::string func_name = (char*)(&string_data[0] + (mk11_funcs[i].nameOffset - 1));
            std::string arg_type = MKODict::GetHashString(mk11_funcs[i].paramsHash);

            if (MKODict::IsHashAvailable(mk11_funcs[i].paramsHash))
            {
                arg_type = arg_type.substr(0, arg_type.length() - 1);
                std::replace(arg_type.begin(), arg_type.end(), '.', ',');
            }

            sprintf(pInfo, "Function %04d - %s(%s) HASH: 0x%X\t\n", i, func_name.c_str(), arg_type.c_str(), mk11_funcs[i].functionHash);
            oInfo << pInfo;
        }

        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variable Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Variables               : \t%d \n", mk11_header.static_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk11_vars.size(); i++)
        {
            std::string name = MKODict::GetHashString(mk11_vars[i].nameHash);
            sprintf(pInfo, "Variable %04d - %s \tSize: %d\n", i, name.c_str(), mk11_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Dynamic Variables               : \t%d\n", mk11_header.dynamic_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk11_dyn_vars.size(); i++)
        {
            std::string name = MKODict::GetHashString(mk11_dyn_vars[i].nameHash);
            sprintf(pInfo, "Dynamic Variable %04d - %s \tSize: %d\n", i, name.c_str(), mk11_vars[i].size);
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Externs               : \t%d\n", mk11_header.externs);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk11_externs.size(); i++)
        {
            std::string ext_name = MKODict::GetHashString(mk11_externs[i].nameHash);
            std::string imp_name = (char*)(&string_data[0] + (mk11_externs[i].file_name_offset - 1));
            sprintf(pInfo, "Extern %04d - %s\t from %s\n", i, ext_name.c_str(), imp_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Extern Variables               : \t%d\n", mk11_header.extern_variables);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk11_extern_vars.size(); i++)
        {
            std::string  name = (char*)(&string_data[0] + (mk11_extern_vars[i].name_offset - 1));
            sprintf(pInfo, "Extern %04d - %s\n", i, name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Assets               : \t%d\n", mk11_header.assets);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk11_assets.size(); i++)
        {
            std::string ass_name = (char*)(&string_data[0] + (mk11_assets[i].item_name_offset - 1));
            std::string arch_name = (char*)(&string_data[0] + (mk11_assets[i].package_name_offset - 1));
            sprintf(pInfo, "Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Source Files\n\n");
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk11_srcs.size(); i++)
        {
            std::string  name = (char*)(&string_data[0] + (mk11_srcs[i].name_offset - 1));
            sprintf(pInfo, "%s\n", name.c_str());
            oInfo << pInfo;
        }
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Fixup Entries               : \t%d\n", mk11_fixup.size());
        oInfo << pInfo;
    }

    oInfo.close();
}

void MKOReader::DumpInfoMK12(std::string name)
{
    std::ofstream oInfo(name);
    if (oInfo) {

        static char pInfo[1024] = {};
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Header Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "String data size        : \t%d\n", mk12_header.string_size);
        oInfo << pInfo;
        sprintf(pInfo, "Tweak string data size        : \t%d\n", mk12_header.tweakvars_stringSize);
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Function Info\n");
        oInfo << pInfo;
        sprintf(pInfo, "===========\n");
        oInfo << pInfo;
        sprintf(pInfo, "Functions               : \t%d\n", mk12_header.functions);
        oInfo << pInfo;


        for (unsigned int i = 0; i < mk12_funcs.size(); i++)
        {
            std::string func_name = (char*)(&string_data[0] + (mk12_funcs[i].nameOffset - 1));
            std::string arg_type = "";

            int args_size = mk12_funcs[i].args.size();

            if (args_size >= 1)
            {
                for (unsigned int a = 1; a < args_size; a++)
                {
                    std::string arg_name = (char*)(&string_data[0] + (mk12_funcs[i].args[a] - 1));
                    arg_type += arg_name;
                    if (!(a == args_size - 1))
                        arg_type += " ,";
                }
            }
           

            sprintf(pInfo, "Function %04d - %s(%s) HASH: 0x%X\t\n", i, func_name.c_str(), arg_type.c_str(), mk12_funcs[i].functionHash);
            oInfo << pInfo;
        }

       sprintf(pInfo, "===========\n");
       oInfo << pInfo;
       sprintf(pInfo, "Variable Info\n");
       oInfo << pInfo;
       sprintf(pInfo, "===========\n");
       oInfo << pInfo;
       sprintf(pInfo, "Variables               : \t%d \n", mk12_header.static_variables);
       oInfo << pInfo;
       for (unsigned int i = 0; i < mk12_vars.size(); i++)
       {
           std::string var_name = (char*)(&string_data[0] + (mk12_vars[i].name_offset - 1));
           std::string var_type = (char*)(&string_data[0] + (mk12_vars[i].type_offset - 1));

           sprintf(pInfo, "Variable %04d - %s Type: %s \tSize: %d\n", i, var_name.c_str(), var_type.c_str(), mk12_vars[i].size);
           oInfo << pInfo;
       }
       sprintf(pInfo, "===========\n");
       oInfo << pInfo;
       sprintf(pInfo, "Dynamic Variables               : \t%d\n", mk12_header.dynamic_variables);
       oInfo << pInfo;
       for (unsigned int i = 0; i < mk12_dyn_vars.size(); i++)
       {
           std::string var_name = (char*)(&string_data[0] + (mk12_dyn_vars[i].name_offset - 1));
           std::string var_type = (char*)(&string_data[0] + (mk12_dyn_vars[i].type_offset - 1));

           sprintf(pInfo, "Dynamic Variable %04d - %s Type: %s \tSize: %d\n", i, var_name.c_str(), var_type.c_str(), mk12_dyn_vars[i].size);
           oInfo << pInfo;
       }
       sprintf(pInfo, "===========\n");
       oInfo << pInfo;
       sprintf(pInfo, "Assets               : \t%d\n", mk12_header.assets);
       oInfo << pInfo;
       for (unsigned int i = 0; i < mk12_assets.size(); i++)
       {
           std::string ass_name = (char*)(&string_data[0] + (mk12_assets[i].nameOffset - 1));
           std::string ass_type = (char*)(&string_data[0] + (mk12_assets[i].pathOffset - 1));
           sprintf(pInfo, "Asset %04d - %s %s\n", i, ass_name.c_str(), ass_type.c_str());
           oInfo << pInfo;
       }
       sprintf(pInfo, "===========\n");
       oInfo << pInfo;
       sprintf(pInfo, "Source Files\n\n");
       oInfo << pInfo;
       for (unsigned int i = 0; i < mk12_srcs.size(); i++)
       {
           sprintf(pInfo, "%s\n", mk12_srcs[i].c_str());
           oInfo << pInfo;
       }
       sprintf(pInfo, "===========\n");
       oInfo << pInfo;
       sprintf(pInfo, "Fixup Entries               : \t%d\n", mk12_fixup.size());
       oInfo << pInfo;
    }

    oInfo.close();
}

void MKOReader::DumpHeader(std::string name)
{
    std::ofstream oInfo(name);
    if (oInfo) {
        oInfo << "[Header]" << std::endl;
        oInfo << "Functions = " << header.functions << std::endl;
        oInfo << "Variables = " << header.static_variables << std::endl;
        oInfo << "field20 = " << header.field20 << std::endl << std::endl;

        oInfo.close();
    }
}

void MKOReader::DumpHeaderMK9(std::string name, std::string fixups, std::string fixupsLocal, std::string externs, std::string globals, std::string variables, std::string functions)
{
    std::ofstream oInfo(name);
    if (oInfo) {
        oInfo << "[Header]" << std::endl;
        oInfo << "Functions = " << functions << std::endl;
        oInfo << "Variables = " << variables << std::endl;
        oInfo << "ExternVariables = " << mk9_header.externVariables << std::endl;
        oInfo << "Assets = " << mk9_header.assets << std::endl;
        oInfo << "SoundAssets = " << mk9_header.soundAssets << std::endl;
        oInfo << "GlobalsFile = " << globals << std::endl;
        oInfo << "ExternsFile = " << externs << std::endl;
        oInfo << "FixUpsFile = " << fixups << std::endl;
        oInfo << "LocalFixUpsFile = " << fixupsLocal << std::endl;
        oInfo << "HashID = " << headerHash << std::endl << std::endl;

        oInfo.close();
    }
}

void MKOReader::DumpRestMK9(std::string name)
{
    std::ofstream oInfo(name, std::ofstream::app);
    if (oInfo) 
    {
        oInfo << "[Assets]" << std::endl;
        for (int i = 0; i < mk9_header.assets; i++)
        {
            std::string ass_name = (char*)(&string_data[0] + (mk9_assets[i].nameOffset - 1));
            std::string arch_name = (char*)(&string_data[0] + (mk9_assets[i].archiveNameOffset - 1));
            oInfo << "; " << ass_name << " - " << arch_name << std::endl;

            oInfo << "Asset" << std::to_string(i) << "NameOffset = " << mk9_assets[i].nameOffset <<  std::endl;
            oInfo << "Asset" << std::to_string(i) << "ArchiveOffset = " << mk9_assets[i].archiveNameOffset << std::endl;
        }
        oInfo << "[Sounds]" << std::endl;
        for (int i = 0; i < mk9_header.soundAssets; i++)
        {
            std::string sass_name = (char*)(&string_data[0] + (mk9_sounds[i].nameOffset - 1));
            std::string sarch_name = (char*)(&string_data[0] + (mk9_sounds[i].archiveNameOffset - 1));
            oInfo << "; " << sass_name << " - " << sarch_name << std::endl;

            oInfo << "Sound" << std::to_string(i) << "NameOffset = " << mk9_sounds[i].nameOffset << std::endl;
            oInfo << "Sound" << std::to_string(i) << "ArchiveOffset = " << mk9_sounds[i].archiveNameOffset << std::endl;
            oInfo << "Sound" << std::to_string(i) << "Flag = " << mk9_sounds[i].field16 << std::endl;
        }
        //sprintf(pInfo, "===========\n");
        //oInfo << pInfo;
        //sprintf(pInfo, "Assets               : \t%d\n", dcf_header.assets);
        //oInfo << pInfo;
        //for (unsigned int i = 0; i < dcf_assets.size(); i++)
        //{
        //    std::string ass_name = (char*)(&string_data[0] + (dcf_assets[i].nameOffset - 1));
        //    std::string arch_name = (char*)(&string_data[0] + (dcf_assets[i].archiveNameOffset - 1));
        //    sprintf(pInfo, "Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
        //    oInfo << pInfo;
        //}
        //sprintf(pInfo, "===========\n");
        //oInfo << pInfo;
        //sprintf(pInfo, "Sound Assets               : \t%d\n", dcf_header.soundAssets);
        //oInfo << pInfo;
        //for (unsigned int i = 0; i < dcf_sounds.size(); i++)
        //{
        //    std::string ass_name = (char*)(&string_data[0] + (dcf_sounds[i].nameOffset - 1));
        //    std::string arch_name = (char*)(&string_data[0] + (dcf_sounds[i].archiveNameOffset - 1));
        //    sprintf(pInfo, "Sound Asset %04d - %s:%s\n", i, arch_name.c_str(), ass_name.c_str());
        //    oInfo << pInfo;
        //}
        //sprintf(pInfo, "===========\n");
        //oInfo << "[Header]" << std::endl;
        //oInfo << "Functions = " << mk9_header.functions << std::endl;
        //oInfo << "Variables = " << mk9_header.static_variables << std::endl;
        //oInfo << "Externs = " << mk9_header.externs << std::endl;
        //oInfo << "ExternVariables = " << mk9_header.externVariables << std::endl;
        //oInfo << "Assets = " << mk9_header.assets << std::endl;
        //oInfo << "SoundAssets = " << mk9_header.soundAssets << std::endl;
        //oInfo << "HashID = " << headerHash << std::endl << std::endl;

        oInfo.close();
    }
}

void MKOReader::ReadFunctionBytecode(std::vector<MKOCodeEntry>& data, int functionID)
{
    pFile.seekg(m_pFunctionsStartOffset + GetFunctionOffset(functionID), pFile.beg);
    nBytesRead = 0;
    int off = 0;
    if (game == Game_DeadlyAlliance)
        off = sizeof(int);

    int size = func_sizes[functionID] - off;


    while (nBytesRead < size)
    {
        MKOCodeEntry mko_entry = {};
        mko_command bc = {};
        if (mko_entry.offset == 0)
            mko_entry.offset = (int)pFile.tellg();


        ParseMKOCommand(bc);
        // mkd jade uses padding
        if (bc.is_pad)
        {
            mko_entry.isPadding = true;
            data.push_back(mko_entry);
            continue;
        }


        if (bc.functionID  - 1 < 0)
        {
            std::cout << "ERROR: Could not decompile function " << GetFunctionName(functionID) << std::endl;
            data.clear();
            break;
        }

        mko_entry.size += pFile.gcount();

        mko_entry.isInternal = bc.isInternal < 0;
        mko_entry.functionID = bc.functionID;
        if (game == Game_Armageddon)
          mko_entry.functionSet = bc.functionSet;

        for (int i = 0; i < bc.numVariables; i++)
        {
            MKOVariable var;
            pFile.read((char*)&var, sizeof(MKOVariable));
            SwapINT(&var.integerData);
            nBytesRead += pFile.gcount();
            mko_entry.size += pFile.gcount();
            mko_entry.arguments.push_back(var);
        }
       data.push_back(mko_entry);
    }
}

void MKOReader::ReadFunctionBytecode_MK8(std::vector<MKOCodeEntry_MK8>& data, int functionID)
{
    pFile.seekg(GetFunctionOffsetMK8(functionID), pFile.beg);
    nBytesRead = 0;

    int size = mk8_funcs[functionID].size;


    while (nBytesRead < size)
    {
        MKOCodeEntry_MK8 mko_entry = {};
        mko_command_mk8 bc = {};
        if (mko_entry.offset == 0)
            mko_entry.offset = (int)pFile.tellg();


        ParseMKOCommand_MK8(bc);
        if (bc.is_pad)
            continue;

        mko_entry.size += pFile.gcount();

        mko_entry.type = bc.functionType;
        mko_entry.functionID = bc.functionID;
        mko_entry.unk1 = bc.field0;
        mko_entry.unk2 = bc.field4;
        mko_entry.pad = bc.is_pad;


        if (bc.numData > 0)
        {
            for (int i = 0; i < bc.numData - 1; i++)
            {
                MKOVariable var;
                pFile.read((char*)&var, sizeof(MKOVariable));
                SwapINT(&var.integerData);
                nBytesRead += pFile.gcount();
                mko_entry.size += pFile.gcount();
                mko_entry.arguments.push_back(var);
            }
        }

        data.push_back(mko_entry);
    }
}

void MKOReader::ParseMKOCommand_MK8(mko_command_mk8& bc)
{
    int a1, a2, a3, a4;
    pFile.read((char*)&a1, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a1);

    if (a1 == 0)
    {
        bc.is_pad = true;
        return;
    }

    pFile.read((char*)&a2, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a2);




    pFile.read((char*)&a3, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a3);

    pFile.read((char*)&a4, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a4);

    bc.field0 = a1;
    bc.field4 = a2;
    bc.numData = LOWORD(a3);
    bc.functionID = LOWORD(a4);
    bc.functionType = HIWORD(a4);



}

void MKOReader::ParseMKOCommand_MK9(mko_command_mk8& bc)
{
    int a1, a2, a3, a4;
    pFile.read((char*)&a1, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a1);

    if (a1 == 0)
    {
        bc.is_pad = true;
        return;
    }

    pFile.read((char*)&a2, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a2);




    pFile.read((char*)&a3, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a3);

    pFile.read((char*)&a4, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a4);

    bc.field0 = a1;
    bc.field4 = a2;
    bc.numData = LOWORD(a3);
    bc.functionID = LOWORD(a4);
    bc.functionType = HIWORD(a4);
}

void MKOReader::ParseMKOCommand_MK9_Vita(mko_command_mk8& bc)
{
    int a1, a2, a3;
    pFile.read((char*)&a1, sizeof(int));
    nBytesRead += pFile.gcount();


    if (a1 == 0)
    {
        bc.is_pad = true;
        return;
    }

    pFile.read((char*)&a2, sizeof(int));
    nBytesRead += pFile.gcount();


    pFile.read((char*)&a3, sizeof(int));
    nBytesRead += pFile.gcount();


    bc.field0 = a1;
    bc.field4 = HIWORD(a2);
    bc.numData = LOWORD(a2);
    bc.functionID = HIWORD(a3);
    bc.functionType = LOWORD(a3);
}

void MKOReader::ReadFunctionBytecode_MK9(std::vector<MKOCodeEntry_MK8>& data, int functionID)
{
    pFile.seekg(m_pFunctionsStartOffset + GetFunctionOffsetMK9(functionID), pFile.beg);
    nBytesRead = 0;

    int size = mk9_funcs[functionID].size;


    while (nBytesRead < size)
    {
        MKOCodeEntry_MK8 mko_entry = {};
        mko_command_mk8 bc = {};
        if (mko_entry.offset == 0)
            mko_entry.offset = (int)pFile.tellg();


        if (game == Game_MK9)
            ParseMKOCommand_MK9(bc);
        else if (game == Game_MK9_Vita)
            ParseMKOCommand_MK9_Vita(bc);
        else
            ParseMKOCommand_MK8(bc);

        if (bc.is_pad)
        {
            mko_entry.pad = true;
            mko_entry.size = 4;
            data.push_back(mko_entry);
            continue;
        }

        mko_entry.size += pFile.gcount();

        mko_entry.type = bc.functionType;
        mko_entry.functionID = bc.functionID;
        mko_entry.unk1 = bc.field0;
        mko_entry.unk2 = bc.field4;
        mko_entry.pad = bc.is_pad;
        mko_entry.isInternal = (bc.field0 != 65);

        if (bc.numData > 0)
        {
            for (int i = 0; i < bc.numData - 1; i++)
            {
                MKOVariable var;
                pFile.read((char*)&var, sizeof(MKOVariable));
                SwapINT(&var.integerData);
                nBytesRead += pFile.gcount();
                mko_entry.size += pFile.gcount();
                mko_entry.arguments.push_back(var);
            }
        }

        data.push_back(mko_entry);
    }
}


void MKOReader::ReadFunctionBytecode_MK10(std::vector<MKOCodeEntry_MK10>& data, int functionID)
{
    pFile.seekg(m_pFunctionsStartOffset + GetFunctionOffsetMK10(functionID), pFile.beg);
    nBytesRead = 0;

    int size = mk10_funcs[functionID].size;


    while (nBytesRead < size)
    {
        MKOCodeEntry_MK10 mko_entry = {};
        mko_command_mk10 bc = {};
        if (mko_entry.offset == 0)
            mko_entry.offset = (int)pFile.tellg();


        ParseMKOCommand_MK10(bc);
        if (bc.is_pad)
            continue;

        mko_entry.size += pFile.gcount();

        mko_entry.type = bc.type;
        mko_entry.functionSet = bc.subType;
        mko_entry.unk1 = bc.field10;
        mko_entry.functionID = bc.field14;
        mko_entry.pad = bc.is_pad;


        if (bc.numVars > 0)
        {
            for (int i = 0; i < bc.numVars; i++)
            {
                MKOVariable var;
                pFile.read((char*)&var, sizeof(MKOVariable));
                nBytesRead += pFile.gcount();
                mko_entry.size += pFile.gcount();
                mko_entry.arguments.push_back(var);
            }
        }

        data.push_back(mko_entry);
    }
}

void MKOReader::ReadFunctionBytecode_DCF2(std::vector<MKOCodeEntry_MK10>& data, int functionID)
{
    pFile.seekg(m_pFunctionsStartOffset + GetFunctionOffsetDCF2(functionID), pFile.beg);
    nBytesRead = 0;

    int size = dcf2_funcs[functionID].size;


    while (nBytesRead < size)
    {
        MKOCodeEntry_MK10 mko_entry = {};
        mko_command_mk10 bc = {};
        if (mko_entry.offset == 0)
            mko_entry.offset = (int)pFile.tellg();


        ParseMKOCommand_MK10(bc);
        if (bc.is_pad)
            continue;

        mko_entry.size += pFile.gcount();

        mko_entry.type = bc.type;
        mko_entry.functionSet = bc.subType;
        mko_entry.unk1 = bc.field10;
        mko_entry.functionID = bc.field14;
        mko_entry.pad = bc.is_pad;


        if (bc.numVars > 0)
        {
            for (int i = 0; i < bc.numVars; i++)
            {
                MKOVariable var;
                pFile.read((char*)&var, sizeof(MKOVariable));
                nBytesRead += pFile.gcount();
                mko_entry.size += pFile.gcount();
                mko_entry.arguments.push_back(var);
            }
        }

        data.push_back(mko_entry);
    }
}

void MKOReader::ReadFunctionBytecode_MK11(std::vector<MKOCodeEntry_MK10>& data, int functionID)
{
    pFile.seekg(m_pFunctionsStartOffset + GetFunctionOffsetMK11(functionID), pFile.beg);
    nBytesRead = 0;

    int size = mk11_funcs[functionID].size;


    while (nBytesRead < size)
    {
        MKOCodeEntry_MK10 mko_entry = {};
        mko_command_mk10 bc = {};
        if (mko_entry.offset == 0)
            mko_entry.offset = (int)pFile.tellg();


        ParseMKOCommand_MK10(bc);
        if (bc.is_pad)
            continue;

        mko_entry.size += pFile.gcount();

        mko_entry.type = bc.type;
        mko_entry.functionSet = bc.subType;
        mko_entry.unk1 = bc.field10;
        mko_entry.functionID = bc.field14;
        mko_entry.pad = bc.is_pad;


        if (bc.numVars > 0)
        {
            for (int i = 0; i < bc.numVars; i++)
            {
                MKOVariable var;
                pFile.read((char*)&var, sizeof(MKOVariable));
                nBytesRead += pFile.gcount();
                mko_entry.size += pFile.gcount();
                mko_entry.arguments.push_back(var);
            }
        }

        data.push_back(mko_entry);
    }
}

void MKOReader::ReadFunctionBytecode_MK12(std::vector<MKOCodeEntry_MK10>& data, int functionID)
{
    pFile.seekg(GetFunctionOffsetMK12(functionID), pFile.beg);
    nBytesRead = 0;

    int size = mk12_funcs[functionID].size;


    while (nBytesRead < size)
    {
        MKOCodeEntry_MK10 mko_entry = {};
        mko_command_mk10 bc = {};
        if (mko_entry.offset == 0)
            mko_entry.offset = (int)pFile.tellg();


        ParseMKOCommand_MK10(bc);
        if (bc.is_pad)
            continue;

        mko_entry.size += pFile.gcount();

        mko_entry.type = bc.type;
        mko_entry.functionSet = bc.subType;
        mko_entry.unk1 = bc.field10;
        mko_entry.functionID = bc.field14;
        mko_entry.pad = bc.is_pad;


        if (bc.numVars > 0)
        {
            for (int i = 0; i < bc.numVars; i++)
            {
                MKOVariable var;
                pFile.read((char*)&var, sizeof(MKOVariable));
                nBytesRead += pFile.gcount();
                mko_entry.size += pFile.gcount();
                mko_entry.arguments.push_back(var);
            }
        }

        data.push_back(mko_entry);
    }
}

void MKOReader::ParseMKOCommand_MK10(mko_command_mk10& bc)
{
    int64 a1;
    pFile.read((char*)&a1, sizeof(int64));
    nBytesRead += pFile.gcount();

    if (a1 == 0)
    {
        bc.is_pad = true;
        return;
    }

    short a2, a3, a4, a5;

    pFile.read((char*)&a2, sizeof(short));
    nBytesRead += pFile.gcount();
    pFile.read((char*)&a3, sizeof(short));
    nBytesRead += pFile.gcount();
    pFile.read((char*)&a4, sizeof(short));
    nBytesRead += pFile.gcount();
    pFile.read((char*)&a5, sizeof(short));
    nBytesRead += pFile.gcount();


    bc.type = a1;
    bc.subType = a2;
    bc.field10 = a3;
    bc.field14 = a5;
    bc.numVars = a4;
}

bool MKOReader::ReadFixupsMK9(std::string file, std::vector<MKOFixup>& dest)
{
    FILE* pFile = fopen(file.c_str(), "rb");
    if (pFile)
    {
        char szLine[2048] = {};
        while (fgets(szLine, sizeof(szLine), pFile))
        {
            if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
                continue;

            char ref[256] = {};
            char name[256] = {};
            int type;
            int offset;
            int data;
            int extra;
            sscanf(szLine, "%d %s %s %d %d %d", &type, ref, name, &offset, &data, &extra);

            MKOFixup f;
            f.type = type;
            f.ref = ref;
            f.base = name;
            f.data = data;
            f.offset = offset;
            f.extra = extra;
            dest.push_back(f);
        }
        fclose(pFile);
        return true;
    }
    return false;
}

bool MKOReader::ReadExternsMK9(std::string file, std::vector<mko_extern_mk9>& dest)
{
    FILE* pFile = fopen(file.c_str(), "rb");
    if (pFile)
    {
        char szLine[2048] = {};
        while (fgets(szLine, sizeof(szLine), pFile))
        {
            if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
                continue;


            unsigned int nameHash;
            int nameOffset;
            unsigned int paramsHash;
            unsigned int externHash;
            int field16;
            int field20;
            int field24;

            sscanf(szLine, "%X %d %X %X %d %d %d", &nameHash, &nameOffset, &paramsHash, &externHash, &field16, &field20, &field24);

            mko_extern_mk9 e;
            e.nameHash = nameHash;
            e.importName = nameOffset;
            e.paramsHash = paramsHash;
            e.externHash = externHash;
            e.field16 = field16;
            e.field20 = field20;
            e.field24 = field24;
            dest.push_back(e);
        }
        fclose(pFile);
        return true;
    }
    return false;
}

bool MKOReader::ReadGlobalsMK9(std::string file, std::vector<MKOGlobal>& dest)
{
    FILE* pFile = fopen(file.c_str(), "rb");
    if (pFile)
    {
        char szLine[2048] = {};
        while (fgets(szLine, sizeof(szLine), pFile))
        {
            if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
                continue;


            char name[256] = {};
            int offset = 0;

            sscanf(szLine, "%s %d", &name, &offset);

            MKOGlobal g;
            g.name = name;
            g.offset = offset;
            dest.push_back(g);
        }
        fclose(pFile);
        return true;
    }
    return false;
}

bool MKOReader::ReadVariablesMK9(std::string file, std::vector<MKOVariableEntry>& dest)
{
    FILE* pFile = fopen(file.c_str(), "rb");
    if (pFile)
    {
        char szLine[2048] = {};
        while (fgets(szLine, sizeof(szLine), pFile))
        {
            if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
                continue;


            char name[256] = {};
            int id = 0;
            int elemSize = 0;
            int size = 0;
            int isTweakvar = 0;
            int offset = 0;

            sscanf(szLine, "%d %s %d %d %d %d\n", &id, name, &elemSize, &size, &isTweakvar, &offset);

            MKOVariableEntry v;
            v.name = name;
            v.isTweakvar = isTweakvar;
            v.scriptID = id;
            v.size = size;
            v.var_mk9.size = size;
            v.var_mk9.elemSize = elemSize;
            v.var_mk9.offset = offset;
            dest.push_back(v);
        }
        fclose(pFile);
        return true;
    }
    return false;
}

bool MKOReader::ReadFunctionsMK9(std::string file, std::vector<MKOFunctionEntry>& dest)
{
    FILE* pFile = fopen(file.c_str(), "rb");
    if (pFile)
    {
        char szLine[2048] = {};
        while (fgets(szLine, sizeof(szLine), pFile))
        {
            if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
                continue;


            int id = 0;
            char name[256] = {};
            char rawName[256] = {};
            int nameOffset = 0;
            unsigned int argsHash = 0;
            char stackData[256] = {};
            int stackSize = 0;
            int numArgs = 0;
            int globalObjects = 0;
            int field28 = 0;

            sscanf(szLine, "%d %s %s %d %x %s %d %d %d %d",
                &id, name, rawName, &nameOffset, &argsHash,
                stackData, &stackSize, &numArgs, &globalObjects, &field28);

            MKOFunctionEntry f;
            f.name = name;
            f.rawName = rawName;
            f.stackData = stackData;
            f.func_mk9.nameOffset = nameOffset;
            f.func_mk9.paramsHash = argsHash;
            f.func_mk9.stackSize = stackSize;
            f.func_mk9.numArgs = numArgs;
            f.func_mk9.globalObjects = globalObjects;
            f.func_mk9.field28 = field28;
            dest.push_back(f);
        }
        fclose(pFile);
        return true;
    }
    return false;
}

void MKOReader::ParseMKOCommand(mko_command& bc)
{
    switch (game)
    {
    case Game_Deception:
        ParseMKOCommand_MKDU(bc);
        break;
    case Game_Armageddon:
        ParseMKOCommand_MKA(bc);
        break;
    case Game_DeadlyAlliance:
        ParseMKOCommand_MKDA(bc);
        break;
    case Game_Unchained:
        ParseMKOCommand_MKDU(bc);
        break;
    default:
        break;
    }
}

void MKOReader::ParseMKOCommand_MKDU(mko_command& bc)
{
    int a1, a2;
    pFile.read((char*)&a1, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a1);

    if (a1 == 0)
    {
        bc.is_pad = true;
        return;
    }


    pFile.read((char*)&a2, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a2);

    bc.functionID = LOWORD(a1);
    bc.isInternal = HIWORD(a1);
    bc.numVariables = LOWORD(a2);
    bc.unk2 = HIWORD(a2);
    bc.is_pad = false;
}

void MKOReader::ParseMKOCommand_MKA(mko_command& bc)
{
    int a1, a2;
    pFile.read((char*)&a1, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a1);

    if (a1 == 0)
    {
        bc.is_pad = true;
        return;
    }


    pFile.read((char*)&a2, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a2);

    int funcSet = HIBYTE(HIWORD(a1));


    bc.functionID = LOWORD(a1);
    bc.isInternal = funcSet == 0 ? -1 : 1;
    bc.numVariables = LOWORD(a2);
    bc.unk2 = HIWORD(a2);
    bc.is_pad = false;
    bc.functionSet = funcSet;
}

void MKOReader::ParseMKOCommand_MKDA(mko_command& bc)
{
    int a1;
    pFile.read((char*)&a1, sizeof(int));
    nBytesRead += pFile.gcount();

    SwapINT(&a1);

    int functionID = LOWORD(a1);
    int otherHalf = HIWORD(a1);

    bc.functionID = functionID + 1;
    // internal functions dont exist in mkda
    bc.isInternal = 0;
    bc.numVariables = LOBYTE(otherHalf);
    bc.unk2 = HIBYTE(otherHalf);

}

bool MKOReader::Build()
{
    bool is_directory = std::filesystem::is_directory(m_szInputName);
    bool exists = std::filesystem::exists(m_szInputName);

    ZeroMemory(&header, sizeof(mko_header));


    if (!exists)
    {
        std::cout << "ERROR: " << m_szInputName << " does not exist!" << std::endl;
        return false;
    }

    if (!is_directory)
    {
        std::cout << "ERROR: " << m_szInputName << " is not a folder!" << std::endl;
        return false;
    }

    std::filesystem::current_path(m_szInputName);

    std::string ini_name = m_szInputName + "_header.ini";

    if (!std::filesystem::exists(ini_name))
    {
        std::cout << "ERROR: " << ini_name << " does not exist!" << std::endl;
        return false;
    }
    
    INIReader ini(ini_name);

    if (ini.ParseError() < 0)
    {
        std::cout << "ERROR: Failed to process header: " << ini_name << std::endl;
        return false;
    }

    std::cout << "Reading header: " << ini_name << std::endl;
 
    header.functions = ini.GetInteger("Header", "Functions", 0);
    header.static_variables = ini.GetInteger("Header", "Variables", 0);
    header.field20 = ini.GetInteger("Header", "field20", 0);

    std::vector<MKOVariableEntry> variables;
    std::vector<std::vector<MKODataLink>> variableLinks;

    for (int i = 0; i < header.static_variables; i++)
    {
        MKOVariableEntry var;
        std::string section = "Variable" + std::to_string(i);
        var.name = ini.GetString(section, "Name","");
        var.scriptID = ini.GetInteger(section, "ScriptID", -1);
        var.var.name_offset = ini.GetInteger(section, "NameOffset", 0);
        var.var.numElems = ini.GetInteger(section, "NumElems", 0);
        var.var.unknown = ini.GetInteger(section, "Unknown", 0);
        variables.push_back(var);

        if (game == Game_Armageddon)
        {
            std::vector<MKODataLink> tempLinks;

            int numLinks = ini.GetInteger(section, "NumLinks", 0);

            for (int a = 0; a < numLinks; a++)
            {
                MKODataLink link;
                char linkName[128] = {};
                snprintf(linkName, sizeof(linkName), "Link%d", a);
                link.value = ini.GetInteger(section, linkName, 0);

                snprintf(linkName, sizeof(linkName), "Link%dType", a);
                link.type = ini.GetInteger(section, linkName, 0);
                tempLinks.push_back(link);
            }
            variableLinks.push_back(tempLinks);
        }
    }

    std::cout << "Read " << variables.size() << " variable headers" << std::endl;


    std::vector<MKOFunctionEntry> functions;
    std::vector<std::vector<MKODataLink>> functionLinks;

    for (int i = 0; i < header.functions; i++)
    {
        MKOFunctionEntry func;
        std::string section = "Function" + std::to_string(i);
        func.name = ini.GetString(section, "Name", "");
        func.flt = ini.GetFloat(section, "FunctionFloat", -1);
        func.func.name_offset = ini.GetInteger(section, "NameOffset", 0);
        func.func.unknown = ini.GetInteger(section, "Unknown", 0);
        functions.push_back(func);

        if (game == Game_Armageddon)
        {
            std::vector<MKODataLink> tempLinks;

            int numLinks = ini.GetInteger(section, "NumLinks", 0);

            for (int a = 0; a < numLinks; a++)
            {
                MKODataLink link;
                char linkName[128] = {};
                snprintf(linkName, sizeof(linkName), "Link%d", a);
                link.value = ini.GetInteger(section, linkName, 0);

                snprintf(linkName, sizeof(linkName), "Link%dType", a);
                link.type = ini.GetInteger(section, linkName, 0);
                tempLinks.push_back(link);
            }
            functionLinks.push_back(tempLinks);
        }
    }

    std::cout << "Read " << functions.size() << " function headers" << std::endl;
    std::vector<int> funcSizes;
    std::vector<int> varSizes;

    // calculate var offset
    {
        std::filesystem::current_path(VARIABLESFOLDER_NAME);
        int varOffset = 0;

        for (int i = 0; i < variables.size(); i++)
        {
            if (!std::filesystem::exists(variables[i].name))
            {
                std::cout << "ERROR: Variable " << variables[i].name << " does not exist!" << std::endl;
                return false;
            }
            int size = std::filesystem::file_size(variables[i].name);

            if (!(size == 0 && game == Game_Armageddon))
               size += sizeof(int);

            varSizes.push_back(size);
        }

        for (int i = 0; i < variables.size(); i++)
        {
            varOffset += varSizes[i];
        }
        varOffset += sizeof(int);
        std::cout << "Last variable offset: " << varOffset << std::endl;
        header.last_variable_offset = varOffset;


        std::filesystem::current_path("..");
    }
    // calculate function offset
    {
        std::filesystem::current_path(FUNCTIONFOLDER_NAME);
        int funcOffset = 0;

        for (int i = 0; i < functions.size(); i++)
        {
            if (!std::filesystem::exists(functions[i].name))
            {
                std::cout << "ERROR: Function " << functions[i].name << " does not exist!" << std::endl;
                return false;
            }
            int size = std::filesystem::file_size(functions[i].name);
            if (game == Game_Armageddon)
                functions[i].functionSize= size;

            size += sizeof(int);
            funcSizes.push_back(size);
        }

        for (int i = 0; i < functions.size(); i++)
        {
            funcOffset += funcSizes[i];
        }

        std::cout << "Last function offset: " << funcOffset << std::endl;
        header.last_function_offset = funcOffset;


        std::filesystem::current_path("..");
    }

    // make new names_data
    {    
        std::ofstream oFile("names_data", std::ofstream::binary);
        int startingOffset = 1;

        for (int i = 0; i < variables.size(); i++)
        {
            std::string name = variables[i].name;
            variables[i].var.name_offset = startingOffset;
            int len = name.length() + 1;
            oFile.write(name.c_str(), len);

            int pad = makePad(len, STRING_PAD_SIZE);
            int size = pad - len;

            if (size > 0)
            {
                std::unique_ptr<char[]> dummy = std::make_unique<char[]>(size);
                oFile.write(dummy.get(), size);
            }
            startingOffset += pad;
        }
        for (int i = 0; i < functions.size(); i++)
        {
            std::string name = functions[i].name;
            functions[i].func.name_offset = startingOffset;
            int len = name.length() + 1;
            oFile.write(name.c_str(), len);

            int pad = makePad(len, STRING_PAD_SIZE);
            int size = pad - len;

            if (size > 0)
            {
                std::unique_ptr<char[]> dummy = std::make_unique<char[]>(size);
                oFile.write(dummy.get(), size);
            }
            startingOffset += pad;
        }
        oFile.close();
    }


    header.script_string_size = std::filesystem::file_size("names_data");
    header.string_data_size = std::filesystem::file_size("string_data");
    header.unknown_data_size = std::filesystem::file_size("unknown_data");

    std::filesystem::current_path("..");
    std::ofstream oFile("output.mko", std::ofstream::binary);
    std::filesystem::current_path(m_szInputName);

    int totalSize = sizeof(mko_header);
    oFile.write((char*)&header, sizeof(mko_header));

    if (game == Game_Armageddon)
    {
        int pad = 0;
        for (int i = 0; i < functions.size(); i++)
            oFile.write((char*)&pad, sizeof(int));
    }

    int offset = 0;
    for (int i = 0; i < functions.size(); i++)
    {
        mko_function_header f = functions[i].func;
        int tmpUnknown = f.unknown;
        if (game == Game_Armageddon)
        {
            f.unknown = functions[i].functionSize + sizeof(int);
        }


        f.offset = offset;
        oFile.write((char*)&f, sizeof(mko_function_header));

        offset += funcSizes[i] / 4;
        totalSize += sizeof(mko_function_header);

        if (game == Game_Armageddon)
        {
            int unk = tmpUnknown;
            oFile.write((char*)&unk, sizeof(int));

            int numLinks = functionLinks[i].size();
            oFile.write((char*)&numLinks, sizeof(int));
            for (int a = 0; a < numLinks; a++)
            {
                int linkValue = functionLinks[i][a].value;
                int linkType = functionLinks[i][a].type;

                int link = MAKELONG(linkValue, linkType);
                oFile.write((char*)&link, sizeof(int));
            }
        }
    }
    if (game == Game_Armageddon)
    {
        int pad = 0;
        for (int i = 0; i < variables.size(); i++)
            oFile.write((char*)&pad, sizeof(int));
    }

    offset = 1;
    for (int i = 0; i < variables.size(); i++)
    {
        mko_variable_header v = {};
        v.name_offset = variables[i].var.name_offset;
        v.numElems = variables[i].var.numElems;
        v.unknown = variables[i].var.unknown;
        v.offset = offset;
        oFile.write((char*)&v, sizeof(mko_variable_header));
        offset += varSizes[i] / 4;
        totalSize += sizeof(mko_variable_header);

        if (game == Game_Armageddon)
        {
            int numLinks = variableLinks[i].size();
            oFile.write((char*)&numLinks, sizeof(int));
            for (int a = 0; a < numLinks; a++)
            {
                int linkValue = variableLinks[i][a].value;
                int linkType = variableLinks[i][a].type;

                int link = MAKELONG(linkValue, linkType);
                oFile.write((char*)&link, sizeof(int));
            }
        }
    }


    std::cout << std::filesystem::current_path() << std::endl;
    std::ifstream pNames("names_data", std::ofstream::binary);
    if (!pNames)
    {
        std::cout << "ERROR: Could not open names_data!" << std::endl;
        return false;
    }
    script_names = std::make_unique<char[]>(header.script_string_size);
    pNames.read(script_names.get(), header.script_string_size);
    oFile.write(script_names.get(), header.script_string_size);
    pNames.close();

    std::ifstream pData("string_data", std::ofstream::binary);
    if (!pData)
    {
        std::cout << "ERROR: Could not open string_data!" << std::endl;
        return false;
    }
    string_data = std::make_unique<char[]>(header.string_data_size);
    pData.read(string_data.get(), header.string_data_size);
    oFile.write(string_data.get(), header.string_data_size);
    pData.close();


    std::ifstream pUnk("unknown_data", std::ofstream::binary);
    if (!pUnk)
    {
        std::cout << "ERROR: Could not open unknown_data!" << std::endl;
        return false;
    }
    unk_data = std::make_unique<char[]>(header.unknown_data_size);
    pUnk.read(unk_data.get(), header.unknown_data_size);
    oFile.write(unk_data.get(), header.unknown_data_size);
    pUnk.close();

    totalSize += header.script_string_size;
    totalSize += header.string_data_size;
    totalSize += header.unknown_data_size;


    std::cout << "MKO header built!" << std::endl;
    std::cout << "Processing variables.." << std::endl;
    {
        std::filesystem::current_path(VARIABLESFOLDER_NAME);
        for (int i = 0; i < variables.size(); i++)
        {
            int size = std::filesystem::file_size(variables[i].name);

            if (size == 0 && game == Game_Armageddon)
                continue;

            mko_variable_header v = variables[i].var;
            int varID = variables[i].scriptID;
            oFile.write((char*)&varID, sizeof(int));
            totalSize += sizeof(int);
            std::ifstream pVar(variables[i].name, std::ifstream::binary);
            if (!pVar)
            {
                std::cout << "ERROR: Variable " << variables[i].name << " could not be open!" << std::endl;
                return false;
            }

            std::unique_ptr<char[]> pVarData = std::make_unique<char[]>(size);
            pVar.read(pVarData.get(), size);
            oFile.write(pVarData.get(), size);
            totalSize += size;
            pVar.close();

            std::cout << "Processing variable " << variables[i].name << " [" << i + 1 << "/" << variables.size() << "]" << std::endl;
        }
        std::filesystem::current_path("..");
    }
    std::cout << "Processing functions.." << std::endl;
    {
        std::filesystem::current_path(FUNCTIONFOLDER_NAME);
        for (int i = 0; i < functions.size(); i++)
        {
            mko_function_header f = functions[i].func;
            float funcFloat = functions[i].flt;
            oFile.write((char*)&funcFloat, sizeof(float));
            totalSize += sizeof(float);

            std::ifstream pFunc(functions[i].name, std::ifstream::binary);
            if (!pFunc)
            {
                std::cout << "ERROR: Function " << functions[i].name << " could not be open!" << std::endl;
                return false;
            }
            int size = std::filesystem::file_size(functions[i].name);
            std::unique_ptr<char[]> pFnData = std::make_unique<char[]>(size);
            pFunc.read(pFnData.get(), size);
            oFile.write(pFnData.get(), size);
            totalSize += size;
            pFunc.close();

            std::cout << "Processing function " << functions[i].name << " [" << i + 1 << "/" << functions.size() << "]" << std::endl;
        }
        std::filesystem::current_path("..");
    }

    if (game == Game_Armageddon)
    {
        int varLinksSize = 0;
        for (unsigned int i = 0; i < variableLinks.size(); i++)
        {
            for (unsigned int a = 0; a < variableLinks[i].size(); a++)
            {
                varLinksSize += sizeof(int);
            }
        }
        varLinksSize += sizeof(int) * variableLinks.size();

        int funcLinksSize = 0;
        for (unsigned int i = 0; i < functionLinks.size(); i++)
        {
            for (unsigned int a = 0; a < functionLinks[i].size(); a++)
            {
                funcLinksSize += sizeof(int);
            }
        }
        funcLinksSize += sizeof(int) * functionLinks.size();

        totalSize += varLinksSize;
        totalSize += funcLinksSize;
        // pad areas
        totalSize += sizeof(int) * variables.size();
        totalSize += sizeof(int) * functions.size();
        // unknown funcs member
        totalSize += sizeof(int) * functions.size();
    }


    std::cout << "Output MKO size " << totalSize << std::endl;
    int pad = makePad(totalSize, 32);
    std::cout << "Padded MKO size " << pad << std::endl;

    int padAmount = pad - totalSize;
    std::unique_ptr<char[]> _pad = std::make_unique<char[]>(padAmount);
    oFile.write(_pad.get(), padAmount);

    std::cout << "Finished." << std::endl;
    return true;
}

bool MKOReader::BuildMK9()
{
    bool is_directory = std::filesystem::is_directory(m_szInputName);
    bool exists = std::filesystem::exists(m_szInputName);
    ZeroMemory(&mk9_header, sizeof(mko_header_mk9));

    if (!exists)
    {
        std::cout << "ERROR: " << m_szInputName << " does not exist!" << std::endl;
        return false;
    }

    if (!is_directory)
    {
        std::cout << "ERROR: " << m_szInputName << " is not a folder!" << std::endl;
        return false;
    }

    std::filesystem::current_path(m_szInputName);

    std::string ini_name = m_szInputName + "_header.ini";

    if (!std::filesystem::exists(ini_name))
    {
        std::cout << "ERROR: " << ini_name << " does not exist!" << std::endl;
        return false;
    }

    INIReader ini(ini_name);

    if (ini.ParseError() < 0)
    {
        std::cout << "ERROR: Failed to process header: " << ini_name << std::endl;
        return false;
    }

    std::cout << "Reading header: " << ini_name << std::endl;

    mk9_header.externVariables = ini.GetInteger("Header", "ExternVariables", 0);
    mk9_header.soundAssets = ini.GetInteger("Header", "SoundAssets", 0);
    mk9_header.assets = ini.GetInteger("Header", "Assets", 0);
    headerHash = ini.GetInteger("Header", "HashID", 0);
    std::string functionsFile = ini.GetString("Header", "Functions", "");
    std::string variablesFile = ini.GetString("Header", "Variables", "");
    std::string fixupsFile = ini.GetString("Header", "FixUpsFile", "");
    std::string localFixupsFile = ini.GetString("Header", "LocalFixUpsFile", "");
    std::string externsFile = ini.GetString("Header", "ExternsFile", "");
    std::string globalsFile = ini.GetString("Header", "GlobalsFile", "");

    std::vector<MKOFixup> globalFixups, localFixups;

    if (!ReadFixupsMK9(fixupsFile, globalFixups))
    {
        std::cout << "ERROR: Failed to open fixups: " << fixupsFile << std::endl;
        return false;
    }

    if (!ReadFixupsMK9(localFixupsFile, localFixups))
    {
        std::cout << "ERROR: Failed to open local fixups: " << localFixupsFile << std::endl;
        return false;
    }

    std::vector<mko_extern_mk9> externs;

    if (!ReadExternsMK9(externsFile, externs))
    {
        std::cout << "ERROR: Failed to open externs: " << externsFile << std::endl;
        return false;
    }

    std::vector<MKOGlobal> globals;

    if (!ReadGlobalsMK9(globalsFile, globals))
    {
        std::cout << "ERROR: Failed to open globals " << globalsFile << std::endl;
        return false;
    }

    std::cout << "Read " << globalFixups.size() << " fixups" << std::endl;
    std::cout << "Read " << externs.size() << " externs" << std::endl;
    std::cout << "Read " << globals.size() << " globals" << std::endl;

    mk9_header.fixups = globalFixups.size();
    mk9_header.externs = externs.size();
    mk9_header.globalObjects = globals.size();

    std::vector<MKOVariableEntry> variables;

    if (!ReadVariablesMK9(variablesFile, variables))
    {
        std::cout << "ERROR: Failed to open variables: " << variablesFile << std::endl;
        return false;
    }

    mk9_header.static_variables = variables.size();
    std::cout << "Read " << variables.size() << " variable headers" << std::endl;

    std::vector<MKOFunctionEntry> functions;

    if (!ReadFunctionsMK9(functionsFile, functions))
    {
        std::cout << "ERROR: Failed to open functions: " << functionsFile << std::endl;
        return false;
    }

    mk9_header.functions = functions.size();
    std::cout << "Read " << functions.size() << " function headers" << std::endl;

    std::vector<int> funcSizes;
    std::vector<int> varSizes;

    mk9_header.tweakVarsOffset = -1;

    {
        std::filesystem::current_path(VARIABLESFOLDER_NAME);

        int maxEnd = 0;
        int tweakStart = -1;
        int tweakEnd = -1;

        for (int i = 0; i < variables.size(); i++)
        {
            if (!std::filesystem::exists(variables[i].name))
            {
                std::cout << "ERROR: Variable " << variables[i].name << " does not exist!" << std::endl;
                return false;
            }

            int off = variables[i].var_mk9.offset;
            int end = off + variables[i].var_mk9.size;

            if (end > maxEnd)
                maxEnd = end;

            if (variables[i].isTweakvar)
            {
                if (tweakStart < 0 || off < tweakStart)
                    tweakStart = off;
                if (end > tweakEnd)
                    tweakEnd = end;
            }

            varSizes.push_back(variables[i].var_mk9.size);
        }

        mk9_header.variablesSize = maxEnd;

        if (tweakStart < 0)
        {
            mk9_header.tweakVarsOffset = maxEnd;
            mk9_header.tweakVarsSize = 0;
        }
        else
        {
            mk9_header.tweakVarsOffset = tweakStart;
            mk9_header.tweakVarsSize = tweakEnd - tweakStart;
        }

        std::filesystem::current_path("..");
    }

    std::cout << "Globals size: " << mk9_header.variablesSize << std::endl;
    std::cout << "Tweakvars offset: " << mk9_header.tweakVarsOffset << std::endl;
    std::cout << "Tweakvars size: " << mk9_header.tweakVarsSize << std::endl;

    // bytecode sizes
    {
        std::filesystem::current_path(FUNCTIONFOLDER_NAME);

        int funcOffset = 0;

        for (int i = 0; i < functions.size(); i++)
        {
            if (!std::filesystem::exists(functions[i].name))
            {
                std::cout << "ERROR: Function " << functions[i].name << " does not exist!" << std::endl;
                return false;
            }

            int size = (int)std::filesystem::file_size(functions[i].name);
            funcSizes.push_back(size);
            funcOffset += size;
        }

        std::cout << "Bytecode size: " << funcOffset << std::endl;
        mk9_header.bytecodeSize = funcOffset;

        std::filesystem::current_path("..");
    }

    int globalFuncID = 0;

    for (int i = 0; i < functions.size(); i++)
    {
        if (functions[i].rawName == "__global__")
        {
            globalFuncID = i;
            break;
        }
    }

    printf("Global at %d\n", globalFuncID);

    std::vector<int> funcOffsets;
    funcOffsets.resize(functions.size());

    {
        int off = funcSizes[globalFuncID];

        for (int i = 0; i < functions.size(); i++)
        {
            if (i == globalFuncID)
            {
                funcOffsets[i] = 0;
                continue;
            }

            funcOffsets[i] = off;
            off += funcSizes[i];
        }
    }

    int stackSizeMax = mk9_header.variablesSize;
    {
        std::filesystem::current_path(FUNCTIONFOLDER_NAME);

        for (int i = 0; i < functions.size(); i++)
        {
            if (i == globalFuncID)
            {
                functions[i].stackOffset = 0;
                continue;
            }

            if (!std::filesystem::exists(functions[i].stackData))
            {
                std::cout << "ERROR: Failed to open stack data " << functions[i].stackData << "!" << std::endl;
                return false;
            }

            int stackSize = (int)std::filesystem::file_size(functions[i].stackData);
            functions[i].stackOffset = stackSizeMax;
            stackSizeMax += stackSize;
        }

        std::filesystem::current_path("..");
    }

    std::cout << "Stack size: " << stackSizeMax << std::endl;
    mk9_header.stack_size = stackSizeMax;

    mk9_header.string_size = (int)std::filesystem::file_size("string_data");

    std::filesystem::current_path("..");
    std::ofstream oFile("output.mko", std::ofstream::binary);
    std::filesystem::current_path(m_szInputName);

    int totalSize = sizeof(int);
    int flag = 1;
    oFile.write((char*)&flag, sizeof(int));

    totalSize += sizeof(int);
    oFile.write((char*)&headerHash, sizeof(int));

    totalSize += sizeof(mk9_header);
    oFile.write((char*)&mk9_header, sizeof(mk9_header));

    {
        int pad = 0;
        for (int i = 0; i < functions.size(); i++)
        {
            oFile.write((char*)&pad, sizeof(int));
            totalSize += sizeof(int);
        }
    }

    for (int i = 0; i < functions.size(); i++)
    {
        mko_function_header_mk9 f = functions[i].func_mk9;

        f.functionOffset = funcOffsets[i] / 4;
        f.function_index = i + 1; 
        f.size = funcSizes[i];
        f.functionHash = _hash(functions[i].rawName.c_str());
        f.stackOffset = functions[i].stackOffset;

        std::vector<int> fixups;
        for (int a = 0; a < localFixups.size(); a++)
        {
            if (localFixups[a].base == functions[i].name)
                fixups.push_back(a);
        }

        f.local_fixup_count = fixups.size();

        oFile.write((char*)&f, sizeof(mko_function_header_mk9));
        totalSize += sizeof(mko_function_header_mk9);

        for (int a = 0; a < fixups.size(); a++)
        {
            MKOFixup src = localFixups[fixups[a]];
            mko_fixup_mk9 fixup = {};

            fixup.type = src.type;
            fixup.offset = src.offset;
            fixup.data = src.data;
            fixup.pad = src.extra;

            oFile.write((char*)&fixup, sizeof(mko_fixup_mk9));
            totalSize += sizeof(mko_fixup_mk9);
        }
    }

    {
        int pad = 0;
        for (int i = 0; i < variables.size(); i++)
        {
            oFile.write((char*)&pad, sizeof(int));
            totalSize += sizeof(int);
        }
    }

    for (int i = 0; i < variables.size(); i++)
    {
        mko_variable_header_mk9 v = variables[i].var_mk9;
        v.name_hash = HashStringAware(variables[i].name);

        oFile.write((char*)&v, sizeof(mko_variable_header_mk9));
        totalSize += sizeof(mko_variable_header_mk9);
    }

    {
        int pad = 0;
        for (int i = 0; i < externs.size(); i++)
        {
            oFile.write((char*)&pad, sizeof(int));
            totalSize += sizeof(int);
        }
    }

    for (int i = 0; i < externs.size(); i++)
    {
        mko_extern_mk9 e = externs[i];

        oFile.write((char*)&e, sizeof(mko_extern_mk9));
        totalSize += sizeof(mko_extern_mk9);
    }

    std::vector<MKOVariableEntry> extVars;

    for (int i = 0; i < mk9_header.externVariables; i++)
    {
        MKOVariableEntry var;
        std::string section = "ExternVariable" + std::to_string(i);
        var.offset = ini.GetInteger(section, "NameOffset", 0);
        var.data = ini.GetInteger(section, "Data", 0);
        extVars.push_back(var);
    }

    for (int i = 0; i < extVars.size(); i++)
    {
        mko_variable_header_mk9 e = {};
        e.name_hash = extVars[i].offset;
        e.offset = extVars[i].data;

        oFile.write((char*)&e, sizeof(mko_variable_header_mk9));
        totalSize += sizeof(mko_variable_header_mk9);
    }

    for (int i = 0; i < globals.size(); i++)
    {
        mko_global_object_mk9 g = {};

        unsigned int hash = HashStringAware(globals[i].name);

        int varIndex = -1;
        for (int a = 0; a < variables.size(); a++)
        {
            if (HashStringAware(variables[a].name) == hash)
            {
                varIndex = a;
                break;
            }
        }

        if (varIndex < 0)
        {
            std::cout << "ERROR: Global " << i << " points to missing " << globals[i].name << std::endl;
            return false;
        }

        g.offset = variables[varIndex].var_mk9.offset + globals[i].offset;
        g.data = 0;

        oFile.write((char*)&g, sizeof(mko_global_object_mk9));
        totalSize += sizeof(mko_global_object_mk9);
    }

    // asset slots
    {
        int pad = 0;
        for (int i = 0; i < mk9_header.assets; i++)
        {
            oFile.write((char*)&pad, sizeof(int));
            totalSize += sizeof(int);
        }
    }

    for (int i = 0; i < mk9_header.assets; i++)
    {
        mko_asset_mk9 var = {};
        std::string name = "Asset" + std::to_string(i) + "NameOffset";
        std::string archive = "Asset" + std::to_string(i) + "ArchiveOffset";

        var.nameOffset = ini.GetInteger("Assets", name, 0);
        var.archiveNameOffset = ini.GetInteger("Assets", archive, 0);

        oFile.write((char*)&var, sizeof(mko_asset_mk9));
        totalSize += sizeof(mko_asset_mk9);
    }

    // sound slots
    {
        int pad = 0;
        for (int i = 0; i < mk9_header.soundAssets; i++)
        {
            oFile.write((char*)&pad, sizeof(int));
            totalSize += sizeof(int);
        }
    }

    for (int i = 0; i < mk9_header.soundAssets; i++)
    {
        mko_sound_asset_mk9 var = {};
        std::string name = "Sound" + std::to_string(i) + "NameOffset";
        std::string archive = "Sound" + std::to_string(i) + "ArchiveOffset";
        std::string flag = "Sound" + std::to_string(i) + "Flag";

        var.nameOffset = ini.GetInteger("Sounds", name, 0);
        var.archiveNameOffset = ini.GetInteger("Sounds", archive, 0);
        var.field16 = ini.GetInteger("Sounds", flag, -1);

        oFile.write((char*)&var, sizeof(mko_sound_asset_mk9));
        totalSize += sizeof(mko_sound_asset_mk9);
    }

    // string table
    {
        std::ifstream pStr("string_data", std::ifstream::binary);
        if (!pStr)
        {
            std::cout << "ERROR: Could not open string_data!" << std::endl;
            return false;
        }

        std::unique_ptr<char[]> str_data = std::make_unique<char[]>(mk9_header.string_size);
        pStr.read(str_data.get(), mk9_header.string_size);
        oFile.write(str_data.get(), mk9_header.string_size);
        pStr.close();

        totalSize += mk9_header.string_size;
    }

    // stack data
    {
        int stackLen = mk9_header.stack_size;
        std::unique_ptr<char[]> stack = std::make_unique<char[]>(stackLen);
        memset(stack.get(), 0, stackLen);

        std::filesystem::current_path(VARIABLESFOLDER_NAME);
        for (int i = 0; i < variables.size(); i++)
        {
            int off = variables[i].var_mk9.offset;
            int size = (int)std::filesystem::file_size(variables[i].name);

            if (size <= 0)
                continue;

            if (off < 0 || off + size > stackLen)
            {
                std::cout << "ERROR: Variable " << variables[i].name << " does not fit the stack" << std::endl;
                return false;
            }

            std::ifstream pVar(variables[i].name, std::ifstream::binary);
            if (!pVar)
            {
                std::cout << "ERROR: Could not open variable " << variables[i].name << std::endl;
                return false;
            }

            pVar.read(stack.get() + off, size);
            pVar.close();
        }
        std::filesystem::current_path("..");

        std::filesystem::current_path(FUNCTIONFOLDER_NAME);
        for (int i = 0; i < functions.size(); i++)
        {
            if (i == globalFuncID)
                continue;

            int off = functions[i].stackOffset;
            int size = (int)std::filesystem::file_size(functions[i].stackData);

            if (size <= 0)
                continue;

            if (off < 0 || off + size > stackLen)
            {
                std::cout << "ERROR: Stack data " << functions[i].stackData << " does not fit the stack" << std::endl;
                return false;
            }

            std::ifstream pStack(functions[i].stackData, std::ifstream::binary);
            if (!pStack)
            {
                std::cout << "ERROR: Could not open stack data " << functions[i].stackData << std::endl;
                return false;
            }

            pStack.read(stack.get() + off, size);
            pStack.close();
        }
        std::filesystem::current_path("..");

        oFile.write(stack.get(), stackLen);
        totalSize += stackLen;
    }

    {
        std::filesystem::current_path(FUNCTIONFOLDER_NAME);

        std::vector<int> order;
        order.push_back(globalFuncID);

        for (int i = 0; i < functions.size(); i++)
        {
            if (i != globalFuncID)
                order.push_back(i);
        }

        for (int k = 0; k < order.size(); k++)
        {
            int i = order[k];
            int size = funcSizes[i];

            std::ifstream pFunc(functions[i].name, std::ifstream::binary);
            if (!pFunc)
            {
                std::cout << "ERROR: Could not open function " << functions[i].name << std::endl;
                return false;
            }

            std::unique_ptr<char[]> data = std::make_unique<char[]>(size > 0 ? size : 1);
            pFunc.read(data.get(), size);
            oFile.write(data.get(), size);
            pFunc.close();

            totalSize += size;
        }

        std::filesystem::current_path("..");
    }

    for (int i = 0; i < globalFixups.size(); i++)
    {
        MKOFixup src = globalFixups[i];
        mko_fixup_mk9 fx = {};

        fx.type = src.type;
        fx.data = src.data;
        fx.pad = src.extra;

        if (src.ref == "code")
        {
            int id = -1;
            for (int a = 0; a < functions.size(); a++)
            {
                if (functions[a].name == src.base)
                {
                    id = a;
                    break;
                }
            }

            if (id < 0)
            {
                std::cout << "ERROR: Fixup " << i << " points to missing function " << src.base << std::endl;
                return false;
            }

            fx.offset = funcOffsets[id] + src.offset;
        }
        else if (src.ref == "stack")
        {
            int id = -1;
            for (int a = 0; a < functions.size(); a++)
            {
                if (functions[a].name == src.base)
                {
                    id = a;
                    break;
                }
            }


            if (id < 0)
            {
                std::cout << "ERROR: Fixup " << i << " points to missing function " << src.base << std::endl;
                return false;
            }

            fx.offset = functions[id].stackOffset + src.offset;
        }
        else
        {
            unsigned int hash = HashStringAware(src.base);

            int id = -1;
            for (int a = 0; a < variables.size(); a++)
            {
                if (HashStringAware(variables[a].name) == hash)
                {
                    id = a;
                    break;
                }
            }

            if (id < 0)
            {
                std::cout << "ERROR: Fixup " << i << " points to missing variable " << src.base << std::endl;
                return false;
            }

            fx.offset = variables[id].var_mk9.offset + src.offset;
        }

        oFile.write((char*)&fx, sizeof(mko_fixup_mk9));
        totalSize += sizeof(mko_fixup_mk9);
    }

    std::cout << "Output MKO size " << totalSize << std::endl;
    int pad = makePad(totalSize, 32);
    std::cout << "Padded MKO size " << pad << std::endl;

    int padAmount = pad - totalSize;
    std::unique_ptr<char[]> _pad = std::make_unique<char[]>(padAmount);
    oFile.write(_pad.get(), padAmount);

    std::cout << "Finished." << std::endl;
    return true;
}

bool MKOReader::IsBuildingSupported(EGameMode game)
{
    switch (game)
    {
    case Game_Deception:
        return true;
        break;
    case Game_Armageddon:
        return true;
        break;
    case Game_DeadlyAlliance:
        break;
    case Game_Unchained:
        return true;
        break;
    case Game_MKVSDC:
        break;
    case Game_MK9:
        return true;
        break;
    case Game_MK9_Vita:
        break;
    case Game_Injustice:
        break;
    case Game_MK10:
        break;
    case Game_Injustice2:
        break;
    case Game_MK11:
        break;
    case Game_MK12:
        break;
    default:
        break;
    }
    return false;
}

bool MKOReader::IsDecompSupported()
{
    switch (game)
    {
    case Game_Deception:
        return true;
        break;
    case Game_Armageddon:
        return true;
        break;
    case Game_DeadlyAlliance:
        return true;
        break;
    case Game_Unchained:
        return true;
        break;
    case Game_MKVSDC:
        return true;
    case Game_MK9:
    case Game_MK9_Vita:
        return true;
    case Game_MK10:
        return true;
    case Game_Injustice2:
        return true;
    case Game_MK11:
        return true;
    case Game_MK12:
        return true;
    default:
        break;
    }
    return false;
}

bool MKOReader::Is64BitSupported()
{
#ifdef _M_X64
    return true;
#else
    return false;
#endif
}

unsigned int MKOReader::HashStringAware(std::string input)
{
    static char tmp[256];
    sprintf(tmp, input.c_str());

    if (tmp[0] == '0' && (tmp[1] == 'x' || tmp[1] == 'X'))
    {
        unsigned int hash = 0;
        sscanf(tmp, "0x%X", &hash);
        return hash;
    }
    return _hash(tmp);
}

void MKOReader::Pack(std::string name, std::string param, EGameMode game)
{
    if (name.find("_movelist") != std::string::npos)
    {
        if (game == Game_Deception)
          PackMovelistMKD(name, param);
    }

}

void MKOReader::PackMovelistMKD(std::string name, std::string param)
{
    int startingPoint = 0;

    if (param.length() > 0)
        sscanf(param.c_str(), "%d", &startingPoint);

    FILE* pFile = fopen(name.c_str(), "rb");

    struct src_movelist {
        type_movelist ent;
        std::string name;
        std::string command;
        std::string command2;
    };

    std::string movelistName = name + "_movelist";
    std::string strName = name + "_string_data";
    std::ofstream stringData(strName, std::ofstream::binary);
    std::ofstream movelistData(movelistName, std::ofstream::binary);
    std::vector<src_movelist> movelist;
    if (pFile)
    {
        char szLine[1024];

        while (fgets(szLine, sizeof(szLine), pFile))
        {
            if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
                continue;

            src_movelist mv = {};
            char name[256] = {};
            char command[64] = {};
            char command2[64] = {};
            mv.ent = {};
            sscanf(szLine, "%d %s %s %s %d", &mv.ent.category, name, command, command2, &mv.ent.unk);
            mv.name = name;
            mv.command = command;
            mv.command2 = command2;
            std::replace(mv.name.begin(), mv.name.end(), '_', ' ');
            std::replace(mv.command.begin(), mv.command.end(), '_', ' ');
            std::replace(mv.command2.begin(), mv.command2.end(), '_', ' ');
            movelist.push_back(mv);

        }
        int stringOffset = startingPoint;
        for (unsigned int i = 0; i < movelist.size(); i++)
        {
            std::string name = movelist[i].name;
            std::string cmd = movelist[i].command;
            std::string cmd2 = movelist[i].command2;

            movelist[i].ent.name = stringOffset;

            // pad name
            {
                int len = name.length() + 1;
                stringData.write(name.c_str(), len);
                int pad = makePad(len, STRING_PAD_SIZE);
                int size = pad - len;

                if (size > 0)
                {
                    std::unique_ptr<char[]> dummy = std::make_unique<char[]>(size);
                    stringData.write(dummy.get(), size);
                }
                stringOffset += pad;
            }
            movelist[i].ent.command = stringOffset;
            // pad cmd
            {
                int len = cmd.length() + 1;
                stringData.write(cmd.c_str(), len);
                int pad = makePad(len, STRING_PAD_SIZE);
                int size = pad - len;

                if (size > 0)
                {
                    std::unique_ptr<char[]> dummy = std::make_unique<char[]>(size);
                    stringData.write(dummy.get(), size);
                }
                stringOffset += pad;
            }
            movelist[i].ent.command2 = stringOffset;
            // pad cmd2
            {
                int len = cmd2.length() + 1;
                stringData.write(cmd2.c_str(), len);
                int pad = makePad(len, STRING_PAD_SIZE);
                int size = pad - len;

                if (size > 0)
                {
                    std::unique_ptr<char[]> dummy = std::make_unique<char[]>(size);
                    stringData.write(dummy.get(), size);
                }
                stringOffset += pad;
            }

            movelistData.write((char*)&movelist[i].ent, sizeof(type_movelist));
        }
    }

}

MKOReader::operator bool()
{
    return m_bIsValid;
}

void MKOReader::SwapINT(int* value)
{
    if (m_bGameCube || game == Game_MKVSDC)
        changeEndINT(value);
}

void MKOReader::SwapSHORT(short* value)
{
    if (m_bGameCube)
        changeEndSHORT(value);
}
