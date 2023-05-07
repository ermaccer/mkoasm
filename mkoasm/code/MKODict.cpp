#include "MKODict.h"
#include <memory>
#include <algorithm>
#include <fstream>
#include <iostream>

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
    // new in MKA
    "load_string_variable",
};

std::vector<MKOFunctionDefinition> MKODict::ms_vFunctions;
std::vector<HashEntry> MKODict::ms_vHashes;

EGameMode MKODict::ms_gameMode;

#ifdef SORT_FUNCTIONS
bool sort_functions(MKOFunctionDefinition& a, MKOFunctionDefinition& b)
{
    return b.functionID > a.functionID;
}
#endif

void MKODict::InitDict(EGameMode game)
{
    const char* file = nullptr;

    switch (game)
    {
    case Game_Deception:
        file = "data\\mkd_def.txt";
        break;
    case Game_Armageddon:
        file = "data\\mka_def.txt";
        break;
    case Game_DeadlyAlliance:
        file = "data\\mkda_def.txt";
        break;
    case Game_Unchained:
        file = "data\\mku_def.txt";
        break;
    default:
        break;
    }

    if (file == nullptr)
        return;

    ms_gameMode = game;

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
                int funcSet = 0;

                if (game == Game_Armageddon)
                {
                    tempLine = strtok(NULL, " ");
                    sscanf(tempLine, "%d", &funcSet);
                }

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
                def.functionSet = funcSet;

                for (int i = 0; i < numArgs; i++)
                {
                    def.args.push_back((EMKOFunctionArgumentDefinition_Type)argTypes[i]);
                }

                ms_vFunctions.push_back(def);
            }
        }
        fclose(pFile);
    }
#ifdef SORT_FUNCTIONS
    std::sort(ms_vFunctions.begin(), ms_vFunctions.end(), sort_functions);

    for (unsigned int i = 0; i < ms_vFunctions.size(); i++)
    {
        MKOFunctionDefinition def = {};
        def = ms_vFunctions[i];
        printf("%s %d %d ", def.name, def.functionID, def.num_arguments);
        for (int a = 0; a < def.num_arguments; a++)
        {
            printf("%d ", def.args[a]);
        }
        printf("\n");
    }
#endif
}

void MKODict::InitHashTable()
{
    std::ifstream pFile("data\\hashdb.bin", std::ifstream::binary);

    if (pFile)
    {
        int hashNum = 0;

        pFile.read((char*)&hashNum, sizeof(int));

        for (int i = 0; i < hashNum; i++)
        {
            HashEntry h;
            pFile.read((char*)&h, sizeof(HashEntry));
            ms_vHashes.push_back(h);
        }

    }
    else
    {
        std::cout << "INFO: Could not open hashdb.bin!" << std::endl;
    }
    std::cout << "INFO: Read " << ms_vHashes.size() << " hash entries!" << std::endl;
}

void MKODict::hash2txt()
{

}

void MKODict::txt2hash()
{
    FILE* pFile = fopen("data\\hashlist.txt", "rb");
    if (pFile)
    {
        char szLine[2048] = {};
        while (fgets(szLine, sizeof(szLine), pFile))
        {
            if (szLine[0] == ';' || szLine[0] == '#' || szLine[0] == '\n')
                continue;

            char name[256] = {};

            sscanf(szLine, "%s", &name);

            HashEntry h;
            h.hash = _hash(name);
            sprintf(h.name, name);
            ms_vHashes.push_back(h);
        }
        fclose(pFile);
    }
    std::ofstream oFile("data\\hashdb.bin", std::ofstream::binary);

    int hashNum = ms_vHashes.size();
    oFile.write((char*)&hashNum, sizeof(int));

    for (unsigned int i = 0; i < ms_vHashes.size(); i++)
    {
        oFile.write((char*)&ms_vHashes[i], sizeof(HashEntry));
    }

    std::cout << "INFO: Built " << ms_vHashes.size() << " hash entries!" << std::endl;
}


const char* MKODict::GetInternalName(int functionID)
{
    static int internalSize = sizeof(szInternalNames) / sizeof(szInternalNames[0]);

    if (functionID > internalSize)
    {
        static char buff[128] = {};
        sprintf(buff, "internal_%d", functionID);
        return buff;
    }
    else
     return szInternalNames[functionID - 1];
}

int MKODict::GetInternalID(const char* name)
{
    static int internalSize = sizeof(szInternalNames) / sizeof(szInternalNames[0]);

    for (int i = 0; i < internalSize; i++)
    {
        if (strcmp(name, szInternalNames[i]) == 0)
            return i;
    }
    return 0;
}

bool MKODict::IsFunctionInternal(const char* name)
{
    static int internalSize = sizeof(szInternalNames) / sizeof(szInternalNames[0]);

    for (int i = 0; i < internalSize; i++)
    {
        if (strcmp(name, szInternalNames[i]) == 0)
            return true;
    }
    return false;
}

bool MKODict::IsDefinitionAvailable(int functionID, int functionSet)
{
    for (unsigned int i = 0; i < ms_vFunctions.size(); i++)
    {
        if (ms_gameMode == Game_Armageddon)
        {
            if (ms_vFunctions[i].functionID == functionID && ms_vFunctions[i].functionSet == functionSet)
                return true;
        }
        else
        {
            if (ms_vFunctions[i].functionID == functionID)
                return true;
        }
    }

    return false;
}

bool MKODict::IsDefinitionAvailable(const char* name)
{
    for (unsigned int i = 0; i < ms_vFunctions.size(); i++)
    {
        if (strcmp(ms_vFunctions[i].name, name) == 0)
            return true;
    }
    return false;
}

MKOFunctionDefinition MKODict::GetDefinition(int functionID, int functionSet)
{
    MKOFunctionDefinition def = {};

    for (unsigned int i = 0; i < ms_vFunctions.size(); i++)
    {
        if (ms_gameMode == Game_Armageddon)
        {
            if (ms_vFunctions[i].functionID == functionID && ms_vFunctions[i].functionSet == functionSet)
            {
                def = ms_vFunctions[i];
                break;
            }
        }
        else
        {
            if (ms_vFunctions[i].functionID == functionID)
            {
                def = ms_vFunctions[i];
                break;
            }
        }

    }

    return def;
}

MKOFunctionDefinition MKODict::GetDefinition(const char* name)
{
    MKOFunctionDefinition def = {};

    for (unsigned int i = 0; i < ms_vFunctions.size(); i++)
    {
        if (strcmp(ms_vFunctions[i].name, name) == 0)
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

unsigned int _hash(const char* input)
{
    unsigned int result;
    int stringLength;
    int character;

    if (!input)
        return 0;
    stringLength = -1;

    do
        ++stringLength;
    while (input[stringLength]);

    for (result = 0x811C9DC5; stringLength; --stringLength)
    {
        character = *(unsigned char*)input++;
        result = character ^ (unsigned int)(0x1000193 * result);
    }
    return result;
}
