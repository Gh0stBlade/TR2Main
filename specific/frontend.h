/*
 * Copyright (c) 2017 Michael Chaban. All rights reserved.
 * Original game is written by Core Design Ltd. in 1997.
 * Lara Croft and Tomb Raider are trademarks of Square Enix Ltd.
 *
 * This file is part of TR2Main.
 *
 * TR2Main is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * TR2Main is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with TR2Main.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FRONTEND_H_INCLUDED
#define FRONTEND_H_INCLUDED

#include "global/types.h"

/*
 * Function list
 */
UINT16 __cdecl S_COLOUR(int red, int green, int blue); // 0x0044C2A0
void __cdecl S_DrawScreenLine(int x, int y, int z, int xLen, int yLen, BYTE colorIdx); // 0x0044C2D0
void __cdecl S_DrawScreenBox(int sx, int sy, int z, int width, int height, BYTE colorIdx, UINT16 *grdPtr, UINT16 flags); // 0x0044C310
void __cdecl S_DrawScreenFBox(int sx, int sy, int z, int width, int height); // 0x0044C430
void __cdecl S_FinishInventory(); // 0x0044C460
void __cdecl S_FadeToBlack(); // 0x0044C470
void __cdecl S_Wait(int timeout, BOOL inputCheck); // 0x0044C4C0
bool __cdecl S_PlayFMV(LPCTSTR fileName); // 0x0044C520
bool __cdecl S_IntroFMV(LPCTSTR fileName1, LPCTSTR fileName2); // 0x0044C530
__int16 __cdecl StartGame(int levelID, int levelType); // 0x0044C550
int __cdecl GameLoop(BOOL demoMode); // 0x0044C6A0
int __cdecl LevelCompleteSequence(); // 0x0044C740
int __cdecl LevelStats(int levelID); // 0x0044C750
int __cdecl GameStats(); // 0x0044C920
int __cdecl GetRandomControl(); // 0x0044CA40
void __cdecl SeedRandomControl(int seed); // 0x0044CA60
int __cdecl GetRandomDraw(); // 0x0044CA70
void __cdecl SeedRandomDraw(int seed); // 0x0044CA90
void __cdecl GetValidLevelsList(REQUEST_INFO *req); // 0x0044CAA0
void __cdecl GetSavedGamesList(REQUEST_INFO *req); // 0x0044CAF0
void __cdecl DisplayCredits(); // 0x0044CB40
BOOL __cdecl S_FrontEndCheck(); // 0x0044CD80
BOOL __cdecl S_SaveGame(LPCVOID saveData, DWORD saveSize, int slotNumber); // 0x0044CEF0
BOOL __cdecl S_LoadGame(LPVOID saveData, DWORD saveSize, int saveNumber); // 0x0044D010

#endif // FRONTEND_H_INCLUDED
