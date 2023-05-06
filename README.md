# mkoasm

MKO Assembler (mkoasm) is a work in progress tool designed to work with .MKO scripts from Mortal Kombat games.

Not ready for public usage yet, but feel free to make use of whats available.

Run without params to get usage.


# Support Table

**Note: There's no way to detect the format of the MKO script, use -m param to change
game mode**


| Game | View | Extract | Decompile | Platforms |
|       ---       |       ---       |       ---       |       ---       |       ---       |
| MK Deadly Alliance | ✔️ | ✔️ | ✔️| Only tested with PS2 |
| MK Deception | ✔️ | ✔️ | ✔️| PS2, GC, XBOX (not tested) |
| MK Unchained | ✔️ | ✔️ | ✔️| PSP|
| MK Armageddon | ✔️ | ✔️ | ✔️|PS2, Wii (not tested), XBOX (not tested)|
| MK VS DC | ✔️ | ✔️ | ✔️|PS3|
| MK9 | ✔️ | ⚠ | ❌| PC, functions aren't extracted properly |


Scripts from Injustice and onwards are totally unsuppported.


# Decompilation

MKOASM is able to decompile functions for supported games using definition tables, there's a table
with plenty of functions for every supported game (except MKvsDC) which should produce human readable
output.


# Building

MKOASM can only build new MKOS for MK Deception/MK Unchained as of now.


# MKO Overview

## MK Deadly Alliance
 - First version of MKO (cmo) script format
 - Only functions can be used
 - No variables
 - Heavyily dependant on executable functions for almost everything

## MK Deception
 - Added static variables
 - Added local variables
 - Added operations on local and static variables
 - Script functions can be called from other script functions
 - Script function calls can have parameters (underused in MKD)
 - Function calls can have variable/dynamic variables
 - Limited procedure creation from scripts
 - Most executable objects exported into MKO (fightstyle, character, stage data and more)
 - FX fully moved to MKO

## MK Armageddon
 - Added variable links
 - Added multiple function sets (eg. FX, game, core)
 - Added ID based string variables instead of data offsets
 - Functions and variables no longer have access to every variable, they need to be linked first
 
## MK VS DC
 - Added imports from other MKOs
 - Added assets section, which replaces previously hardcoded asset tables

## MK 9
 - Added seperate sounds asset section which includes character specific sounds
 - Added variable name hashing (FNV1)
 - Added tweakvars section which allows quick access to important variables