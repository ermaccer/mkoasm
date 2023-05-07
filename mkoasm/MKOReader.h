#pragma once
#include <vector>
#include "code/MKScript.h"
#include "code/MKScript_MK8.h"
#include "code/MKScript_MK9.h"
#include "code/MKScript_DCF.h"
#include "code/MKScript_MK10.h"
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
#ifdef _M_X64
    int64 qwordData;
#endif // _X_M64

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

struct MKOCodeEntry_MK9 {
    int type;
    int subType;
    int unk1;
    int unk2;
    int pad;
    std::vector<MKOVariable> arguments;

    // debug/helper
    int offset;
    int size;
    int localOffset;
};




struct MKOCodeEntry_MK10 {
    int type;
    int subType;
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

    uintptr_t m_pDataStartOffset = 0;
    uintptr_t m_pFunctionsStartOffset = 0;

    std::vector<mko_function_header> funcs;
    std::vector<mko_variable_header> vars;
    std::vector<int> var_sizes;
    std::vector<int> func_sizes;
    mko_header header;


    std::vector<std::vector<int>> mka_funcLinks;
    std::vector<std::vector<int>> mka_varLinks;

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
    std::vector<mko_variable_header_mk9> mk9_extern_vars;
    std::vector<mko_extern_mk9> mk9_externs;
    std::vector<mko_asset_mk9> mk9_assets;
    std::vector<mko_sound_asset_mk9> mk9_sounds;
    std::vector<mko_fixup_mk9> mk9_fixup;

    // dcf1 (injustice)
    mko_header_dcf dcf_header;
    std::vector<mko_function_header_mk9> dcf_funcs;
    std::vector<mko_variable_header_mk9> dcf_vars;
    std::vector<mko_variable_header_mk9> dcf_dyn_vars;
    std::vector<mko_extern_mk9> dcf_externs;
    std::vector<mko_asset_dcf> dcf_assets;
    std::vector<mko_sound_asset_mk9> dcf_sounds;

    // mk10
    mko_header_mk10 mk10_header;
    std::vector<mko_function_header_mk10> mk10_funcs;
    std::vector<mko_variable_header_mk10> mk10_vars;
    std::vector<mko_variable_header_mk10> mk10_dyn_vars;
    std::vector<mko_extern_mk10> mk10_externs;
    std::vector<mko_extern_var_mk10> mk10_extern_vars;
    std::vector<mko_source_header_mk10> mk10_srcs;
    std::vector<mko_asset_mk10> mk10_assets;
    std::vector<mko_fixup_mk10> mk10_fixup;
    std::unique_ptr<char[]> mk10_pointers;
    std::unique_ptr<char[]> mk10_tweakstring_data;

    std::unique_ptr<char[]> script_names;
    std::unique_ptr<char[]> string_data;
    std::unique_ptr<char[]> unk_data;



    int nBytesRead = 0;

    std::ifstream pFile;

    MKOReader(const char* file, bool isGameCube = false, EGameMode game = Game_Deception);

    bool Read(const char* file);
    bool ReadMK8();
    bool ReadMK9();
    bool ReadDCF();
    bool ReadMK10();


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
    std::string GetFunctionNameDCF(int functionID);
    std::string GetFunctionNameMK10(int functionID);


    uint32_t GetFunctionOffset(int functionID);
    uint32_t GetFunctionOffsetMK8(int functionID);
    uint32_t GetFunctionOffsetMK9(int functionID);
    uintptr_t GetFunctionOffsetMK10(int functionID);

    std::string GetVariableName(int variableID);
    std::string GetVariableNameMK8(int variableID);
    std::string GetVariableNameMK9(int variableID);
    std::string GetVariableNameDCF(int variableID);
    std::string GetVariableNameMK10(int variableID);


    uint32_t GetVariableOffset(int variableID);
    uint32_t GetVariableOffsetMK8(int variableID);
    uint32_t GetVariableOffsetMK9(int variableID);
    uint32_t GetVariableOffsetDCF(int variableID);
    uint32_t GetVariableOffsetMK10(int variableID);

    std::string GetString(int stringStart);

    void ExtractData();
    void ExtractDataMKDADU();
    void ExtractDataMK8();
    void ExtractDataMK9();
    void ExtractDataDCF();
    void ExtractDataMK10();

    void ExtractVariables();
    void ExtractVariablesMK8();
    void ExtractVariablesMK9();
    void ExtractVariablesDCF();
    void ExtractVariablesMK10();


    void ExtractFunctions();
    void ExtractFunctionsMK8();
    void ExtractFunctionsMK9();
    void ExtractFunctionsDCF();
    void ExtractFunctionsMK10();

    void DecompileFunction(int functionID);
    void DecompileFunctionMK8(int functionID);
    void DecompileFunctionMK9(int functionID);
    void DecompileFunctionMK10(int functionID);


    void UnpackVariable(int variableID);

    void UnpackVariableMK8(int variableID);
    void UnpackVariableMK9(int variableID);
    void UnpackVariableDCF(int variableID);
    void UnpackVariableMK10(int variableID);

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
    void DecompileAllFunctionsMK9();
    void DecompileAllFunctionsMK10();
    void UnpackVariables();
    void UnpackVariablesMK8();
    void UnpackVariablesMK9();
    void UnpackVariablesDCF();
    void UnpackVariablesMK10();

    void PrintInfo();

    void PrintInfoMKDADU();
    void PrintInfoMK8();
    void PrintInfoMK9();
    void PrintInfoDCF();
    void PrintInfoMK10();

    void DumpInfoMKDADU(std::string name);
    void DumpInfoMK8(std::string name);
    void DumpInfoMK9(std::string name);
    void DumpInfoDCF(std::string name);
    void DumpInfoMK10(std::string name);

    void DumpHeader(std::string header);

    void ReadFunctionBytecode(std::vector<MKOCodeEntry>& data, int functionID);
    void ParseMKOCommand(mko_command& bc);
    void ParseMKOCommand_MKDU(mko_command& bc);
    void ParseMKOCommand_MKA(mko_command& bc);
    void ParseMKOCommand_MKDA(mko_command& bc);

    void ReadFunctionBytecode_MK8(std::vector<MKOCodeEntry_MK8>& data, int functionID);
    void ParseMKOCommand_MK8(mko_command_mk8& bc);
    void ParseMKOCommand_MK9_Vita(mko_command_mk8& bc);

    void ReadFunctionBytecode_MK9(std::vector<MKOCodeEntry_MK8>& data, int functionID);

    void ReadFunctionBytecode_MK10(std::vector<MKOCodeEntry_MK10>& data, int functionID);
    void ParseMKOCommand_MK10(mko_command_mk10& bc);
    // building

    bool Build();

    bool IsDecompSupported();
    static bool Is64BitSupported();


    // packing

    static void Pack(std::string name, std::string param,  EGameMode game);

    static void PackMovelistMKD(std::string name, std::string param);

    operator bool();

    void SwapINT(int* value);
    void SwapSHORT(short* value);
};