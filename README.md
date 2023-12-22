# mkoasm

MKO Assembler (mkoasm) is a work in progress tool designed to work with .MKO scripts from Mortal Kombat games.

Not ready for public usage yet, but feel free to make use of whats available.

Run without params to get usage.


# Support Table

**Note: There's no way to detect the format of the MKO script, use -m param to change
game mode**

| Icon | Description |
|       ---       |       ---       | 
| ✔️ | Full support |
| ⚠ | Partial support, data might not be extracted correctly |
| ❌ | Not supported |


| Game | View | Extract | Decompile |  Compile | Platforms |
|       ---       |       ---       |       ---       |       ---       |      ---       |      ---       |
| MK Deadly Alliance | ✔️ | ✔️ | ✔️| ❌ | PS2 |
| MK Deception | ✔️ | ✔️ | ✔️|  ✔️ |PS2, GC, XBOX  |
| MK Unchained | ✔️ | ✔️ | ✔️|  ✔️ |PSP|
| MK Armageddon | ✔️ | ✔️ | ✔️| ❌ |PS2, Wii, XBOX|
| MK VS DC | ✔️ | ✔️ | ✔️| ❌ |PS3|
| MK9 | ✔️ | ✔️ | ✔️|  ❌ |PC, Vita |
| Injustice | ✔️ | ⚠ | ❌| ❌ | PC |
| Mortal Kombat X | ✔️ | ✔️ | ✔️| ❌ | PC|
| Injustice 2 | ✔️ | ✔️ | ✔️| ❌ | PC|
| Mortal Kombat 11 | ✔️ | ✔️ | ✔️| ❌ | PC|
| Mortal Kombat 12 (1) | ❌ | ❌ | ❌| ❌ | PC (decompression only)|

**Mortal Kombat X and up requires x64 version of mkoasm!**



# Decompilation

MKOASM is able to decompile functions for supported games using definition tables, there's a table
with plenty of functions for every supported game (except MKvsDC+) which should produce human readable
output.


# Building

MKOASM can only build new MKOS for MK Deception/MK Unchained as of now.


# MKO Overview

## MK Deadly Alliance
 - First version of MKO (cmo) script format
 - Only functions can be used
 - No variables
 - Heavily dependant on executable functions for almost everything

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
 - Changed bytecode format

## MK 9
 - Added seperate sounds asset section which includes character specific sounds
 - Added variable name hashing (FNV1)
 - Added tweakvars section which allows quick access to important variables
 - Added "fixup" section for quick file patches/edits

## Injustice
 - Basically identical to MK9 structure wise

## Mortal Kombat X
 - Changed bytecode format
 - Tweakvars exported to a seperate file
 - Merged sounds section with assets like in MKvsDC

## Injustice 2
 - New tweakvar info section
 - Moved bytecode section right after header
 - Otherwise identical to MKX

## Mortal Kombat 11
 - Changed function entry struct
 - Changed extern/extern vars entry struct
 - Otherwise identical to Injustice 2

## Mortal Kombat 12 (1)
 - Removed variable and function name hashing
 - Functions now require multiple callback types (Brutality, Victory etc.)
