#pragma once
#include <vector>
#include "code/MKScript.h"
#include "code/MKScript_MK8.h"
#include "code/MKScript_MK9.h"
#include <fstream>
#include "code/MKScriptTypes.h"
#include "code/MKODict.h"

#include "enums.h"

#define VARIABLESFOLDER_NAME "vars"
#define FUNCTIONFOLDER_NAME "funcs"

#define STRING_PAD_SIZE 4


union MKOVariable {
    int integerData;
    float floatData;
    unsigned short shortData;
    unsigned int uintData;
};


struct MKOCodeEntry {
    int functionID;
    int functionSet;
    bool isInternal;
    std::vector<MKOVariable> arguments;

    // debug/helper
    int offset;
    int size;
    int localOffset;

};

struct MKOCodeEntry_MK8 {
    int functionID;
    int type;
    int unk1;
    int unk2;
    int pad;
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

    EGameMode game = Game_Unchained;
    std::string m_szInputName;

    uint32_t m_pDataStartOffset = 0;
    uint32_t m_pFunctionsStartOffset = 0;

    std::vector<mko_function_header> funcs;
    std::vector<mko_variable_header> vars;
    std::vector<int> var_sizes;
    std::vector<int> func_sizes;
    mko_header header;


    std::vector<mko_function_header_mka> mka_funcs;

    // mk8 (Mortal Kombat vs DC Universe)
    mko_header_mk8 mk8_header;
    std::vector<mko_function_header_mk8> mk8_funcs;
    std::vector<mko_variable_header_mk8> mk8_vars;
    std::vector<mko_extern_mk8> mk8_externs;
    std::vector<mko_asset_mk8> mk8_assets;

    // mk9
    mko_header_mk9 mk9_header;
    std::vector<mko_function_header_mk9> mk9_funcs;
    std::vector<mko_variable_header_mk9> mk9_vars;
    std::vector<mko_variable_header_mk9> mk9_dyn_vars;
    std::vector<mko_extern_mk9> mk9_externs;
    std::vector<mko_asset_mk9> mk9_assets;
    std::vector<mko_sound_asset_mk9> mk9_sounds;
    std::vector<mko_unknown_mk9> mk9_unknowns;

    std::unique_ptr<char[]> script_names;
    std::unique_ptr<char[]> string_data;
    std::unique_ptr<char[]> unk_data;



    int nBytesRead = 0;

    std::ifstream pFile;

    MKOReader(const char* file, bool isGameCube = false, EGameMode game = Game_Deception);

    bool Read(const char* file);
    bool ReadMK8();
    bool ReadMK9();

    std::string GetExtension();

    int  GetAllFunctionsSize();
    int  GetAllVariablesSize();
    int  GetAllVariablesSizeMK8();
    int  GetAllFunctionsSizeMK8();
    int  GetAllFunctionsUnkSizeMK8();
    std::string GetFileName();

    std::string GetFunctionName(int functionID);
    std::string GetFunctionNameMK8(int functionID);
    std::string GetFunctionNameMK9(int functionID);

    uint32_t GetFunctionOffset(int functionID);
    uint32_t GetFunctionOffsetMK8(int functionID);

    std::string GetVariableName(int variableID);
    std::string GetVariableNameMK8(int variableID);
    std::string GetVariableNameMK9(int variableID);

    uint32_t GetVariableOffset(int variableID);
    uint32_t GetVariableOffsetMK8(int variableID);
    uint32_t GetVariableOffsetMK9(int variableID);

    std::string GetString(int stringStart);

    void ExtractData();
    void ExtractDataMKDADU();
    void ExtractDataMK8();
    void ExtractDataMK9();

    void ExtractVariables();
    void ExtractVariablesMK8();
    void ExtractVariablesMK9();

    void ExtractFunctions();
    void ExtractFunctionsMK8();
    void ExtractFunctionsMK9();
    void DecompileFunction(int functionID);
    void DecompileFunctionMK8(int functionID);
    void UnpackVariable(int variableID);

    void UnpackVariableMK8(int variableID);
    void UnpackVariableMK9(int variableID);
    // unpackers
    void Unpack_Movelist(int variableID);
    void Unpack_MovelistU(int variableID);
    void Unpack_Styles(int variableID);
    void Unpack_RArt(int variableID);
    void Unpack_BList(int variableID);
    void Unpack_SList(int variableID);
    void Unpack_Attributes(int variableID);
    void Unpack_PlayerData_MKDU(int variableID);

    // mk8 unpackers
    void MK8_Unpack_RArt(int variableID);
    void MK8_Unpack_Movelist(int variableID);
    void MK8_Unpack_CHRBones(int variableID);

    // mk9 unpackers
    void MK9_Unpack_RArt(int variableID);
    void MK9_Unpack_Movelist(int variableID);


    void DecompileAllFunctions();
    void DecompileAllFunctionsMK8();
    void UnpackVariables();
    void UnpackVariablesMK8();
    void UnpackVariablesMK9();

    void PrintInfo();

    void PrintInfoMKDADU();
    void PrintInfoMK8();
    void PrintInfoMK9();

    void DumpInfoMKDADU(std::string name);
    void DumpInfoMK8(std::string name);
    void DumpInfoMK9(std::string name);

    void DumpHeader(std::string header);

    void ReadFunctionBytecode(std::vector<MKOCodeEntry>& data, int functionID);
    void ParseMKOCommand(mko_command& bc);
    void ParseMKOCommand_MKDU(mko_command& bc);
    void ParseMKOCommand_MKA(mko_command& bc);
    void ParseMKOCommand_MKDA(mko_command& bc);

    void ReadFunctionBytecode_MK8(std::vector<MKOCodeEntry_MK8>& data, int functionID);
    void ParseMKOCommand_MK8(mko_command_mk8& bc);
    // building

    bool Build();

    bool IsDecompSupported();
    bool Is64BitSupported();


    // packing

    static void Pack(std::string name, std::string param,  EGameMode game);

    static void PackMovelistMKD(std::string name, std::string param);

    operator bool();

    void SwapINT(int* value);
    void SwapSHORT(short* value);
};