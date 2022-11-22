# mkoasm

MKO Assembler (mkoasm) is a work in progress tool designed to work with .MKO scripts from Mortal Kombat games.

Not ready for public usage yet, but feel free to make use of whats available.

Run without params to get usage.


# Support Table

| Game | View | Extract | Decompile | Platforms |
|       ---       |       ---       |       ---       |       ---       |       ---       |
| MK Deadly Alliance | ✔️ | ✔️ | ✔️| Only tested with PS2 |
| MK Deception | ✔️ | ✔️ | ✔️| PS2, GC, XBOX (not tested) |
| MK Unchained | ✔️ | ✔️ | ✔️| PSP|
| MK Armageddon | ✔️ | ✔️ | ❌|PS2, Wii (not tested), XBOX (not tested)|
| MK VS DC | ✔️ | ❌ | ❌|PS3|



Scripts from MK9 and onwards are totally unsuppported.


# Decompilation

mkoasm is able to decompile certain functions using a definition table (depending on mode), 
note the current definition table is made for MK Unchained! Functions after ID 600/700 are 
offset by 4 in MKU.


# Building

It is possible to build MKD/MKU mkos using binary data only, there's no compiler for functions
nor packer for unpacked variables yet.