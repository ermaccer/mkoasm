#pragma once
#include <vector>
#include "code/MKScript.h"
#include "code/MKScript_MK8.h"
#include <fstream>
#include "code/MKScriptTypes.h"
#include "code/MKODict.h"

#include "enums.h"

#define VARIABLESFOLDER_NAME "vars"
#define FUNCTIONFOLDER_NAME "funcs"


union MKOVariable {
    int integerData;
    float floatData;
    unsigned short shortData;
    unsigned int uintData;
};


struct MKOCodeEntry {
    int functionID;
    bool isInternal;
    std::vector<MKOVariable> arguments;

    // debug/helper
    int offset;
    int size;
    int localOffset;

};

struct MKOFunctionEntry {
    std::string name;
    mko_function_header func = {};
    float flt;
};

struct MKOVariableEntry {
    std::string name;
    mko_variable_header var = {};
    int scriptID;
};



class MKOReader {
public:
    bool m_bIsValid = false;
    bool m_bDebugMKO = false;
    bool m_bExtractOnly = false;
    bool m_bGameCube = false;
    bool m_bBuildMode = false;

    EGameMode game = Game_Deception;
    std::string m_szInputName;

    uint32_t m_pDataStartOffset = 0;
    uint32_t m_pFunctionsStartOffset = 0;

    std::vector<mko_function_header> funcs;
    std::vector<mko_variable_header> vars;
    std::vector<int> var_sizes;
    std::vector<int> func_sizes;


    std::unique_ptr<char[]> script_names;
    std::unique_ptr<char[]> string_data;
    std::unique_ptr<char[]> unk_data;

    mko_header header;

    int nBytesRead = 0;

    std::ifstream pFile;

    MKOReader(const char* file, bool isGameCube = false, EGameMode game = Game_Deception);

    bool Read(const char* file);
    bool ReadMK8();
    int  GetAllFunctionsSize();
    int  GetAllVariablesSize();

    std::string GetFileName();

    std::string GetFunctionName(int functionID);
    uint32_t GetFunctionOffset(int functionID);

    std::string GetVariableName(int variableID);
    uint32_t GetVariableOffset(int functionID);

    std::string GetString(int stringStart);

    void ExtractData();

    void ExtractVariables();
    void ExtractFunctions();
    void DecompileFunction(int functionID);
    void UnpackVariable(int variableID);

    // unpackers
    void Unpack_Movelist(int variableID);
    void Unpack_Styles(int variableID);
    void Unpack_RArt(int variableID);
    void Unpack_BList(int variableID);
    void Unpack_SList(int variableID);
    void Unpack_Attributes(int variableID);

    void DecompileAllFunctions();
    void UnpackVariables();

    void PrintInfo();
    void DumpInfo(std::string name);
    void DumpHeader(std::string header);

    void ReadFunctionBytecode(std::vector<MKOCodeEntry>& data, int functionID);
    void ParseMKOCommand(mko_command& bc);
    void ParseMKOCommand_MKDU(mko_command& bc);
    void ParseMKOCommand_MKDA(mko_command& bc);
    // building

    bool Build();

    bool IsDecompSupported();

    operator bool();

    void SwapINT(int* value);
    void SwapSHORT(short* value);
};