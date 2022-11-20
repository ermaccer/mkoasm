#pragma once

enum EMovelistTypes {
	EMovelist_Style_One,
	EMovelist_Style_Two,
	EMovelist_Style_Three,
	EMovelist_SpecialMove

};

struct type_movelist {
	int category;
	int name;
	int unk;
	int command;
	int unk2;
};

struct type_attributes {
	float activeFrame;
	float field4;
	float field8;
	float animSpeed;
	float movementSpeed;
	short field20;
	short field22;
	short field24;
	short field26;
	short field28;
	short field30;
	int	  field32;
	float field36;
	float field40;
	float damage;
	int   hitLevel;
	short field52;
	short field54;
	float field56;
};