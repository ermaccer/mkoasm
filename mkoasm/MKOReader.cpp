#include "MKOReader.h"
#include <iostream>
#include <filesystem>
#include <memory>
#include <string>
#include "code/FileFunctions.h"
#include "IniReader.h"
#include <Windows.h>

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

    bool build = std::filesystem::is_directory(m_szInputName);
    m_bBuildMode = build;
    m_bGameCube = isGameCube;
    game = _game;
    if (!build)
        m_bIsValid = Read(file);
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
            int newSize = next_size - cur_size - 4;

            func_sizes.push_back(newSize);
        }

        script_names = std::make_unique<char[]>(header.script_string_size);
        pFile.read(script_names.get(), header.script_string_size);
        string_data = std::make_unique<char[]>(header.string_data_size);
        pFile.read(string_data.get(), header.string_data_size);

        unk_data = std::make_unique<char[]>(header.unknown_data_size);
        pFile.read(unk_data.get(), header.unknown_data_size);

        m_pDataStartOffset = (uint32_t)(pFile.tellg());
        m_pFunctionsStartOffset = m_pDataStartOffset + GetAllVariablesSize();
        return true;
	}
	return false;
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

uint32_t MKOReader::GetFunctionOffset(int functionID)
{
    return (funcs[functionID].offset * 4) + 4;
}

std::string MKOReader::GetVariableName(int variableID)
{
    std::string var_name = (char*)(&script_names[0] + (vars[variableID].name_offset - 1));
    return var_name;
}

uint32_t MKOReader::GetVariableOffset(int variableID)
{
    return (vars[variableID].offset * 4);
}

std::string MKOReader::GetString(int stringStart)
{
    std::string str = (char*)(&string_data[0] + (stringStart - 1));
    return str;
}

void MKOReader::ExtractData()
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
    DumpInfo(info);

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

    folderName = "funcs";
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

void MKOReader::ExtractFunctions()
{
    pFile.seekg(m_pFunctionsStartOffset, pFile.beg);
    std::string hdr = "..\\";
    hdr += GetFileName() + "_header.ini";
    std::ofstream oInfo(hdr, std::ofstream::app);
    for (unsigned int i = 0; i < funcs.size(); i++)
    {
        int size = func_sizes[i];
        std::string func_name = (char*)(&script_names[0] + (funcs[i].name_offset - 1));
        float funcID = 0;
        pFile.read((char*)&funcID, sizeof(float));
        SwapINT((int*)&funcID);

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
                            if (c.arguments[a].integerData > 0)
                                pMKC << "\"" << GetString(c.arguments[a].integerData) << "\"";
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
    else  if (varName.find("_attributes") != std::string::npos)
        Unpack_Attributes(variableID);
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

void MKOReader::DecompileAllFunctions()
{
    for (unsigned int i = 0; i < funcs.size(); i++)
        DecompileFunction(i);
}

void MKOReader::UnpackVariables()
{
    for (unsigned int i = 0; i < vars.size(); i++)
        UnpackVariable(i);
}


void MKOReader::PrintInfo()
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

void MKOReader::DumpInfo(std::string name)
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

    int nBytesRead = 0;

    while (nBytesRead < func_sizes[functionID])
    {
        MKOCodeEntry mko_entry = {};
        mko_command bc = {};
        if (mko_entry.offset == 0)
            mko_entry.offset = (int)pFile.tellg();

        int a1, a2;
        pFile.read((char*)&a1, sizeof(int));
        nBytesRead += pFile.gcount();
        pFile.read((char*)&a2, sizeof(int));
        nBytesRead += pFile.gcount();
        SwapINT(&a1);
        SwapINT(&a2);

        bc.functionID = LOWORD(a1);
        bc.isInternal = HIWORD(a1);
        bc.numVariables = LOWORD(a2);
        bc.unk2 = HIWORD(a2);

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

MKOReader::operator bool()
{
    return m_bIsValid;
}

void MKOReader::SwapINT(int* value)
{
    if (m_bGameCube)
        changeEndINT(value);
}

void MKOReader::SwapSHORT(short* value)
{
    if (m_bGameCube)
        changeEndSHORT(value);
}
