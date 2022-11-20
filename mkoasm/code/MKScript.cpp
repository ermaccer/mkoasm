#include "MKScript.h"
#include <iostream>

const char* GetVariableTypeName(int id)
{
	char tmp[256] = {};
	//if (id == VAR_STRING_TABLE)
	//	return "String Table";
	sprintf(tmp, "%04d", id);
	return tmp;
}
