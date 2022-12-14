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

    if (build && !(game == Game_Deception || game == Game_Unchained))
    {
        build = false;
        std::cout << "ERROR: Building is only supported for MKD or MKU." << std::endl;
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
            m_bIsValid = ReadMK9();
            break;
        case Game_Injustice:
            break;
        case Game_MK10:
            break;
        case Game_Injustice2:
            break;
        case Game_MK11:
            break;
        default:
            break;
        }
    }
    else
        m_bIsValid = Build();

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
                int field12, field16;
                pFile.read((char*)&field12, sizeof(int));
                pFile.read((char*)&field16, sizeof(int));

                SwapINT(&field12);
                SwapINT(&field16);
                if (field16 > 0)
                {
                    int size = field16 * sizeof(int);
                    std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
                    pFile.read(unkData.get(),size);
                }
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
                int field16;
                pFile.read((char*)&field16, sizeof(int));

                SwapINT(&field16);
                if (field16 > 0)
                {
                    int size = field16 * sizeof(int);
                    std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
                    pFile.read(unkData.get(), size);
                }
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

        pFile.read((char*)&mk9_header, sizeof(mko_header_mk9));

        SwapINT(&mk9_header.functions);
        SwapINT(&mk9_header.static_variables);
        SwapINT(&mk9_header.externs);
        SwapINT(&mk9_header.externVariables);
        SwapINT(&mk9_header.assets);
        SwapINT(&mk9_header.soundAssets);
        SwapINT(&mk9_header.field24);
        SwapINT(&mk9_header.field28);
        SwapINT(&mk9_header.bytecodeSize);
        SwapINT(&mk9_header.string_size);
        SwapINT(&mk9_header.functionsOffset);
        SwapINT(&mk9_header.unknowns);
        SwapINT(&mk9_header.tweakVarsOffset);
        SwapINT(&mk9_header.tweakVarsSize);


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
            SwapINT(&func.field8);
            SwapINT(&func.size);
            SwapINT(&func.field16);
            SwapINT(&func.field20);
            SwapINT(&func.field24);
            SwapINT(&func.field28);
            SwapINT(&func.id);
            SwapINT(&func.numUnk);
            SwapINT(&func.field40);
            SwapINT((int*)&func.paramsHash);

            {
                if (func.numUnk > 0)
                {
                    for (int a = 0; a < func.numUnk; a++)
                    {
                        int count = 0;
                        pFile.read((char*)&count, sizeof(int));
                        int size = count* sizeof(int);
                        std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
                        pFile.read(unkData.get(), size);
                    }

                }
            }


            mk9_funcs.push_back(func);
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
            SwapINT((int*)&ext.field8);
            SwapINT(&ext.field12);
            SwapINT(&ext.field16);
            SwapINT(&ext.field20);
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

            mk9_dyn_vars.push_back(var);
        }


        {
            int size = mk9_header.field28 * (sizeof(int) * 2);
            std::unique_ptr<char[]> unkData = std::make_unique<char[]>(size);
            pFile.read(unkData.get(), size);
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
            int size = mk9_header.soundAssets* sizeof(int);
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
            mk9_sounds.push_back(sass);
        }

        string_data = std::make_unique<char[]>(mk9_header.string_size);
        pFile.read(string_data.get(), mk9_header.string_size);


        m_pDataStartOffset = (uint32_t)(pFile.tellg());
        m_pFunctionsStartOffset = m_pDataStartOffset + mk9_header.functionsOffset;

        pFile.seekg(m_pFunctionsStartOffset + mk9_header.bytecodeSize, pFile.beg);

        // new in mk9, probably fixup?
        for (int i = 0; i < mk9_header.unknowns; i++)
        {
            mko_unknown_mk9 unk;
            pFile.read((char*)&unk, sizeof(mko_unknown_mk9));

            SwapINT(&unk.field0);
            SwapINT(&unk.name_offset);
            SwapINT(&unk.offset);
            SwapINT(&unk.field12);
            mk9_unknowns.push_back(unk);
        }

        return true;
    }
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

uint32_t MKOReader::GetFunctionOffset(int functionID)
{
    if (game == Game_DeadlyAlliance)
        return (funcs[functionID].offset * 4);

    return (funcs[functionID].offset * 4) + 4;
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

std::string MKOReader::GetString(int stringStart)
{
    std::string str = (char*)(&string_data[0] + (stringStart - 1));
    return str;
}

void MKOReader::ExtractData()
{
    if (game < Game_MKVSDC)
        ExtractDataMKDADU();

    if (game == Game_MKVSDC)
        ExtractDataMK8();
    if (game == Game_MK9)
        ExtractDataMK9();
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

    std::string info = output + "_info.txt";
    DumpInfoMK9(info);


    std::ofstream pData("string_data", std::ofstream::binary);
    pData.write(string_data.get(), mk9_header.string_size);
    pData.close();


    std::ofstream pTwk("tweakvars_data", std::ofstream::binary);
    std::unique_ptr<char[]> tweak_data = std::make_unique<char[]>(mk9_header.tweakVarsSize);
    pFile.seekg(m_pDataStartOffset + mk9_header.tweakVarsOffset, pFile.beg);
    pFile.read(tweak_data.get(), mk9_header.tweakVarsSize);
    pTwk.write(tweak_data.get(), mk9_header.tweakVarsSize);
    pTwk.close();

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

        std::string var_name = (char*)(&script_names[0] + (vars[i].name_offset - 1));
        int varID = 0;
        pFile.read((char*)&varID, sizeof(int));
        SwapINT(&varID);


        if (oInfo) {
            oInfo << "[Variable" << std::to_string(i) << "]" << std::endl;
            oInfo << "Name = " << var_name << std::endl;
            oInfo << "NameOffset = " << vars[i].name_offset << std::endl;
            oInfo << "NumElems = " << vars[i].numElems << std::endl;
            oInfo << "ScriptID = " << varID << std::endl;
            oInfo << "Unknown = " << vars[i].unknown << std::endl << std::endl;

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
    for (unsigned int i = 0; i < mk9_vars.size(); i++)
    {
        pFile.seekg(m_pDataStartOffset + GetVariableOffsetMK9(i), pFile.beg);
        int size = mk9_vars[i].size;
        if (size < 0)
            size = 0;

        std::string var_name = GetVariableNameMK9(i);

        printf("Extracting variable %s Size: %d [%d/%d]\n", var_name.c_str(), size, i + 1, mk9_vars.size());

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
            pFile.seekg(m_pFunctionsStartOffset + (funcs[i].offset * 4), pFile.beg);
            size = funcs[i].unknown;
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
            oInfo << "NameOffset = " << funcs[i].name_offset << std::endl;
            oInfo << "Unknown = " << funcs[i].unknown << std::endl;
            oInfo << "FunctionFloat = " << funcID << std::endl << std::endl;
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
    }
}

void MKOReader::ExtractFunctionsMK9()
{
    pFile.seekg(m_pFunctionsStartOffset, pFile.beg);
    for (unsigned int i = 0; i < mk9_funcs.size(); i++)
    {
        int size = mk9_funcs[i].size;
        std::string func_name = (char*)(&string_data[0] + (mk9_funcs[i].nameOffset - 1));

        printf("Extracting function %s [%d/%d]\n", func_name.c_str(), i + 1, mk9_funcs.size());
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
                    functionName += "_";
                    functionName += std::to_string(c.functionID - 1);

                    if (MKODict::IsDefinitionAvailable(c.functionID - 1))
                    {
                        funcDef = MKODict::GetDefinition(c.functionID - 1);
                        functionName = funcDef.name;
                        definitionAvailable = true;
                    }


                    pMKC << functionName << "(";
                }


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
                if (c.isInternal)
                    pMKC << MKODict::GetInternalName(c.functionID) << "();" << std::endl;
                else
                {
                    std::string functionName = "function";
                    functionName += "_";
                    functionName += std::to_string(c.functionID - 1);

                    if (MKODict::IsDefinitionAvailable(c.functionID - 1))
                    {
                        funcDef = MKODict::GetDefinition(c.functionID - 1);
                        functionName = funcDef.name;
                        definitionAvailable = true;
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
        SwapINT(&mv.unk);
        SwapINT(&mv.command);
        SwapINT(&mv.unk2);

        std::string type = "Style One";
        if (mv.category == EMovelist_Style_One)
            type = "Style One   \t";
        if (mv.category == EMovelist_Style_Two)
            type = "Style Two   \t";
        if (mv.category == EMovelist_Style_Three)
            type = "Style Three \t";
        if (mv.category == EMovelist_SpecialMove)
            type = "Special Move\t";

        std::string str;
        std::string command;
        if (mv.name > 0)
            str = (char*)(&string_data[0] + (mv.name - 1));
        if (mv.command > 0)
            command = (char*)(&string_data[0] + (mv.command - 1));

        pMovelist << type << " " << str << " " << command << std::endl;
    }
    pMovelist.close();

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
        break;
    case Game_MK10:
        break;
    case Game_Injustice2:
        break;
    case Game_MK11:
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
        printf("Function %04d - %s \t Size: %d Unk: %d Offset: %d\n", i, func_name.c_str(), func_sizes[i], funcs[i].unknown, funcs[i].offset * 4);
    }
    printf("===========\n");
    printf("Variable Info\n");
    printf("===========\n");
    printf("Variables               : \t%d\n", header.static_variables);
    for (unsigned int i = 0; i < vars.size(); i++)
    {
        std::string var_name = (char*)(&script_names[0] + (vars[i].name_offset - 1));
        printf("Variable %04d - %s \t Size: %d Elements: %d Offset: %d\n", i, var_name.c_str(), var_sizes[i], vars[i].numElems, vars[i].offset * 4);
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

        printf("Function %04d - %s(%s)\t\n", i, func_name.c_str(), arg_type.c_str(), mk9_funcs[i].nameOffset);
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
    for (unsigned int i = 0; i < mk9_dyn_vars.size(); i++)
    {
        std::string var_name = (char*)(&string_data[0] + (mk9_dyn_vars[i].name_hash - 1));
        printf("DVariable %04d - %s \tSize: %d\n", i, var_name.c_str(), mk9_dyn_vars[i].size);
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
    printf("Unknown               : \t%d\n", mk9_header.unknowns);
    for (unsigned int i = 0; i < mk9_unknowns.size(); i++)
    {
        if (mk9_unknowns[i].field0 == 0)
        {
            std::string name = (char*)(&string_data[0] + (mk9_unknowns[i].name_offset));
            printf("Unknown %04d - %s %d\n", i, name.c_str(), mk9_unknowns[i].offset);
        }
        else
        {
            printf("Unknown %04d - Type: %d Data: %d %d\n", i, mk9_unknowns[i].field0, mk9_unknowns[i].name_offset, mk9_unknowns[i].offset);
        }

    }
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
            sprintf(pInfo, "Function %04d - %s \t Size: %d Unk: %d Offset: %d\n", i, func_name.c_str(), func_sizes[i], funcs[i].unknown, funcs[i].offset * 4);
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
            sprintf(pInfo, "Variable %04d - %s \t Size: %d Elements: %d Offset: %d\n", i, var_name.c_str(), var_sizes[i], vars[i].numElems, vars[i].offset * 4);
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

            sprintf(pInfo, "Function %04d - %s(%s)\t\n", i, func_name.c_str(), arg_type.c_str(), mk9_funcs[i].nameOffset);
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
        for (unsigned int i = 0; i < mk9_dyn_vars.size(); i++)
        {
            std::string var_name = (char*)(&string_data[0] + (mk9_dyn_vars[i].name_hash - 1));
            sprintf(pInfo, "DVariable %04d - %s \tSize: %d\n", i, var_name.c_str(), mk9_dyn_vars[i].size);
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
        sprintf(pInfo, "Unknown               : \t%d\n", mk9_header.unknowns);
        oInfo << pInfo;
        for (unsigned int i = 0; i < mk9_unknowns.size(); i++)
        {
            if (mk9_unknowns[i].field0 == 0)
            {
                std::string name = (char*)(&string_data[0] + (mk9_unknowns[i].name_offset));
                sprintf(pInfo, "Unknown %04d - %s %d\n", i, name.c_str(), mk9_unknowns[i].offset);
                oInfo << pInfo;
            }
            else
            {
                sprintf(pInfo, "Unknown %04d - Type: %d Data: %d %d\n", i, mk9_unknowns[i].field0, mk9_unknowns[i].name_offset, mk9_unknowns[i].offset);
                oInfo << pInfo;
            }

        }
        sprintf(pInfo, "===========\n");
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
        if (bc.is_pad)
            continue;

        if (bc.functionID  - 1 < 0)
        {
            std::cout << "ERROR: Could not decompile function " << GetFunctionName(functionID) << std::endl;
            data.clear();
            break;
        }

        mko_entry.size += pFile.gcount();

        mko_entry.isInternal = bc.isInternal < 0;
        mko_entry.functionID = bc.functionID;

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

    bc.functionID = LOWORD(a1);
    bc.isInternal = HIWORD(a1) == 0 ? -1 : HIWORD(a1);
    bc.numVariables = LOWORD(a2);
    bc.unk2 = HIWORD(a2);
    bc.is_pad = false;
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
    }

    std::cout << "Read " << variables.size() << " variable headers" << std::endl;


    std::vector<MKOFunctionEntry> functions;

    for (int i = 0; i < header.functions; i++)
    {
        MKOFunctionEntry func;
        std::string section = "Function" + std::to_string(i);
        func.name = ini.GetString(section, "Name", "");
        func.flt = ini.GetFloat(section, "FunctionFloat", -1);
        func.func.name_offset = ini.GetInteger(section, "NameOffset", 0);
        func.func.unknown = ini.GetInteger(section, "Unknown", 0);
        functions.push_back(func);
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

    header.script_string_size = std::filesystem::file_size("names_data");
    header.string_data_size = std::filesystem::file_size("string_data");
    header.unknown_data_size = std::filesystem::file_size("unknown_data");

    std::filesystem::current_path("..");
    std::ofstream oFile("output.mko", std::ofstream::binary);
    std::filesystem::current_path(m_szInputName);

    int totalSize = sizeof(mko_header);
    oFile.write((char*)&header, sizeof(mko_header));

    int offset = 0;
    for (int i = 0; i < functions.size(); i++)
    {
        mko_function_header f = functions[i].func;
        f.offset = offset;
        oFile.write((char*)&f, sizeof(mko_function_header));
        offset += funcSizes[i] / 4;
        totalSize += sizeof(mko_function_header);
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
            int size = std::filesystem::file_size(variables[i].name);
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

    std::cout << "Output MKO size " << totalSize << std::endl;
    int pad = makePad(totalSize, 32);
    std::cout << "Padded MKO size " << pad << std::endl;

    int padAmount = pad - totalSize;
    std::unique_ptr<char[]> _pad = std::make_unique<char[]>(padAmount);
    oFile.write(_pad.get(), padAmount);

    std::cout << "Finished." << std::endl;
    return true;
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
