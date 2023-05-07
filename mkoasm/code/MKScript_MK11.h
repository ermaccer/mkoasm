#pragma once
#include "MKScriptTypes.h"



#pragma pack(push, 1)
struct mko_function_header_mk11 {
    int64 nameOffset;
    unsigned int functionHash;
    int field12;
    int function_index;
    unsigned int paramsHash;
    int64 field24;
    int64 functionOffset;
    int64 field40;
    int64 field48;
    int64 field56;
    int   size;
    int   field68;
    int   field72;
    int   field76;
    int   field80;
    unsigned int hash;
    unsigned int hash2;
    int   field92;
    int   field96;
    int   field100;
    int   field104;
    int   field112;
};
#pragma pack(pop)

struct mko_extern_mk11 {
    int file_name_offset;
    int file_name;
    unsigned int nameHash;
    int field12;
    int field16;
    int field20;
    int field24;
    int field28;
    int name_offset;
    int name;
};

struct mko_extern_var_mk11 {
    int name_offset;
    int field4;
    int64 addr;
    int field16;
    int field20;
};
