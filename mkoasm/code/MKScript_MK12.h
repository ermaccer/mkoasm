#pragma once
#include "MKScriptTypes.h"
#include <vector>

// wrapper only, in mk12 data is after each size
struct mko_header_mk12 {
    int functions;
    int static_variables;
    int dynamic_variables;
    int assets;
    int numSource;
    int string_size;
    int tweakvars_stringSize;
    int fixups;
};

// returns in mk12, similar to mka
struct mko_link_mk12 {
    int type;
    int field4;
    int data;
};


#pragma pack(push, 1)
struct mko_function_header_mk12 {
    int nameOffset;
    unsigned int functionHash;
    int nameOffset2;
    int field12;
    int field16;
    int numLinks;
    int linksOffset;
    //mko_link_mk12 links[numLinks];
    int numArgs;
    std::vector<int> args;
    int size;
    int bytecodeOffset;
  //  char bytecode[bytecodeSize];
    int stackSize;
    int stackOffset;
  //  char stackData[stackSize];
    char unkData[24];
};
#pragma pack(pop)

struct mko_variable_header_mk12 {
    int stack_offset;
    int hash;
    int name_offset;
    int type_offset;
    int size;
    int field20;
};


struct mko_fixup_mk12 {
    int field0;
    int field4;
    int field8;
    int field12;
    int field16;
};


struct mko_asset_mk12 {
    int pathOffset;
    int nameOffset;
};

// new unknown sections

struct mko_function_data1_mk12 {
    int field0;
    int field4;
    int field8;
    int field12;
    int feild16;
    int field20;
};

struct mko_function_data2_mk12 {
    int field0;
    int field4;
    int field8;
    int field12;
    int feild16;
    int field20;
    int field24;
};