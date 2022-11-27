#include "MKODict.h"
#include <memory>

const char* szInternalNames[] = {
    "copy_register_to_instruction",
    "copy_constant_to_register",
    "copy_register_to_register",
    "copy_variable_to_register",
    "copy_register_to_variable",
    "copy_constant_to_variable",
    "copy_column_to_register",
    "copy_column_address_to_register",
    "copy_register_to_address",
    "combine_int_int",
    "combine_uint_uint",
    "combine_float_float",
    "compare_int_int",
    "compare_uint_uint",
    "compare_float_float",
    "conditional_branch",
    "unconditional_branch",
    "load_table_address",
    "call_script_function",
    "copy_stream_to_address",
    "get_bit_field",
    "set_bit_field",
};

std::vector<MKOFunctionDefinition> MKODict::ms_vFunctions;
std::vector<MidwayHashEntry> MKODict::ms_vHashes;

void MKODict::InitDict(EGameMode game)
{
    const char* file = nullptr;
    switch (game)
    {
    case Game_Deception:
        file = "mkd_def.txt";
        break;
    case Game_Armageddon:
        file = "mka_def.txt";
        break;
    case Game_DeadlyAlliance:
        file = "mkda_def.txt";
        break;
    case Game_Unchained:
        file = "mku_def.txt";
        break;
    default:
        break;
    }

    if (file == nullptr)
        return;

    FILE* pFile = fopen(file, "rb");
    if (pFile)
    {
        char szLine[2048];
        char* tempLine;
        int  errorCheck = 0;
        while (fgets(szLine, sizeof(szLine), pFile))
        {
            if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
                continue;
            tempLine = strtok(szLine, " ");
            char name[256] = {};
            if (sscanf(szLine, "%s", &name) == 1)
            {
                std::unique_ptr<int[]> argTypes;
                int funcID = -1;
                int numArgs = 0;
                tempLine = strtok(NULL, " ");
                sscanf(tempLine, "%d", &funcID);
                tempLine = strtok(NULL, " ");
                sscanf(tempLine, "%d", &numArgs);

                argTypes = std::make_unique<int[]>(numArgs);

                for (int i = 0; i < numArgs; i++)
                {
                    tempLine = strtok(NULL, " ");

                    if (!tempLine)
                    {
                        printf("error at definition: %s\n", name);
                        break;
                    }
                    errorCheck++;
                    sscanf(tempLine, "%d", &argTypes[i]);
                }

                MKOFunctionDefinition def;
                sprintf(def.name, name);
                def.num_arguments = numArgs;
                def.functionID = funcID;

                for (int i = 0; i < numArgs; i++)
                {
                    def.args.push_back((EMKOFunctionArgumentDefinition_Type)argTypes[i]);
                }

                ms_vFunctions.push_back(def);
            }
        }
        fclose(pFile);
    }
}

void MKODict::InitHashTable()
{
    FILE* pFile = fopen("hashtable.txt", "rb");
    if (pFile)
    {
        char szLine[2048] = {};
        char* tempLine;
        int  errorCheck = 0;
        while (fgets(szLine, sizeof(szLine), pFile))
        {
            if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
                continue;

            char name[256] = {};
            unsigned int hash = 0;
            sscanf(szLine, "0x%X %s", &hash, &name);

            MidwayHashEntry h;
            h.hash = hash;
            sprintf(h.name, name);
            ms_vHashes.push_back(h);
        }
        fclose(pFile);
    }
}


const char* MKODict::GetInternalName(int functionID)
{
    return szInternalNames[functionID - 1];
}

bool MKODict::IsDefinitionAvailable(int functionID)
{
    for (unsigned int i = 0; i < ms_vFunctions.size(); i++)
    {
        if (ms_vFunctions[i].functionID == functionID)
            return true;
    }
    return false;
}

MKOFunctionDefinition MKODict::GetDefinition(int functionID)
{
    MKOFunctionDefinition def = {};

    for (unsigned int i = 0; i < ms_vFunctions.size(); i++)
    {
        if (ms_vFunctions[i].functionID == functionID)
        {
            def = ms_vFunctions[i];
            break;
        }
    }

    return def;
}

std::string MKODict::GetHashString(unsigned int hash)
{
    static char tmp[256] = {};
    sprintf(tmp, "0x%X", hash);
    for (unsigned int i = 0; i < ms_vHashes.size(); i++)
    {
        if (ms_vHashes[i].hash == hash)
        {
            sprintf(tmp, ms_vHashes[i].name);
            break;
        }
    }
    return tmp;
}

bool MKODict::IsHashAvailable(unsigned int hash)
{
    for (unsigned int i = 0; i < ms_vHashes.size(); i++)
    {
        if (ms_vHashes[i].hash == hash)
        {
            return true;
        }
    }
    return false;
}
