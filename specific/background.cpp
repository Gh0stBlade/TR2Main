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

#include "global/precompiled.h"
#include "specific/background.h"
#include "specific/hwr.h"
#include "specific/init.h"
#include "specific/texture.h"
#include "specific/utils.h"
#include "global/vars.h"
#include <math.h>

void __cdecl BGND_Make640x480(BYTE *bitmap, RGB *palette) {
	// NOTE: 8 bit bitmap may be converted to 16 bit right in the tmpBuffer
	// so we need to allocate memory for 16 bit bitmap anyway
	DWORD tmpBufSize = 256*256*2;
	BYTE *tmpBuffer = (BYTE *)game_malloc(tmpBufSize, 0); // Temp Alloc

	BGND_PaletteIndex = (TextureFormat.bpp < 16) ? CreateTexturePalette(palette) : -1;

	UT_MemBlt(tmpBuffer, 0, 0, 256, 256, 256, bitmap, 0, 0, 640);
	BGND_AddTexture(0, tmpBuffer, BGND_PaletteIndex, palette);

	UT_MemBlt(tmpBuffer, 0, 0, 256, 256, 256, bitmap, 256, 0, 640);
	BGND_AddTexture(1, tmpBuffer, BGND_PaletteIndex, palette);

	UT_MemBlt(tmpBuffer, 0,   0, 128, 256, 256, bitmap, 512, 0,   640);
	UT_MemBlt(tmpBuffer, 128, 0, 128, 224, 256, bitmap, 512, 256, 640);
	BGND_AddTexture(2, tmpBuffer, BGND_PaletteIndex, palette);

	UT_MemBlt(tmpBuffer, 0, 0, 256, 224, 256, bitmap, 0, 256, 640);
	BGND_AddTexture(3, tmpBuffer, BGND_PaletteIndex, palette);

	UT_MemBlt(tmpBuffer, 0, 0, 256, 224, 256, bitmap, 256, 256, 640);
	BGND_AddTexture(4, tmpBuffer, BGND_PaletteIndex, palette);

	game_free(tmpBufSize);
	BGND_GetPageHandles();
	BGND_PictureIsReady = true;
}

int __cdecl BGND_AddTexture(int tileIndex, BYTE *bitmap, int palIndex, RGB *bmpPal) {
	int pageIndex;
	BYTE *bmpSrc;
	UINT16 *bmpDst;

	// If destination palette is absent we convert 8 bit to 16 bit right in the buffer
	if( palIndex < 0 ) {
		bmpDst = (UINT16 *)&bitmap[256*256*2]; // (16 bit bitmap end)
		bmpSrc = (BYTE *)&bitmap[256*256*1]; // (8 bit bitmap end / 16 bit bitmap center)

		// Translating bitmap data from 8 bit bitmap end to 16 bit bitmap end
		for( int i=256*256; i>0; --i ) {
			RGB *color = &bmpPal[*(--bmpSrc)]; // get RGB color from palette
			*(--bmpDst) = (1 << 15) // convert RGB to 16 bit
						| (((UINT16)color->red   >> 3) << 10)
						| (((UINT16)color->green >> 3) << 5)
						| (((UINT16)color->blue  >> 3));
		}

		// Now both bmpSrc and bmpDst pointers = bitmap pointer
		pageIndex = AddTexturePage16(256, 256, bitmap);
	} else {
		pageIndex = AddTexturePage8(256, 256, bitmap, palIndex);
	}

	BGND_TexturePageIndexes[tileIndex] = ( pageIndex >= 0 ) ? pageIndex : -1;
	return pageIndex;
}

void __cdecl BGND_GetPageHandles() {
	for( DWORD i=0; i<5; ++i ) {
		if( BGND_TexturePageIndexes[i] < 0 )
			BGND_PageHandles[i] = 0;
		else
			BGND_PageHandles[i] = GetTexturePageHandle(BGND_TexturePageIndexes[i]);
	}
}

void __cdecl BGND_DrawInGameBlack() {
	HWR_EnableZBuffer(false, false);
	DrawQuad((float)PhdWinMinX, (float)PhdWinMinY, (float)PhdWinWidth, (float)PhdWinHeight, 0); // black colored screen sized quad
	HWR_EnableZBuffer(true, true);
}

void __cdecl DrawQuad(float sx, float sy, float width, float height, D3DCOLOR color) {
	D3DTLVERTEX vertex[4];

	vertex[0].sx = sx;
	vertex[0].sy = sy;

	vertex[1].sx = sx + width;
	vertex[1].sy = sy;

	vertex[2].sx = sx;
	vertex[2].sy = sy + height;

	vertex[3].sx = sx + width;
	vertex[3].sy = sy + height;

	for( int i=0; i<4; ++i ) {
		vertex[i].sz = 0;
		vertex[i].rhw = FltRhwONearZ;
		vertex[i].color = color | 0xFF000000;
		vertex[i].specular = 0;
	}

	HWR_TexSource(0);
	HWR_EnableColorKey(false);
	_Direct3DDevice2->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DVT_TLVERTEX, &vertex, 4, D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTCLIP);
}

void __cdecl BGND_DrawInGameBackground() {
	__int16 *meshPtr;
	int numVertices, numNormals, numQuads;
	DWORD textureIndex;
	PHD_TEXTURE *textureInfo; // eax@5
	int x0, y0, x1, y1;
	int x_current, y_current, x_next, y_next;
	int y0_pos, y1_pos;
	int tu, tv, twidth, theight;
	D3DTEXTUREHANDLE texSource;
	D3DCOLOR color[4];

	if( (Objects[ID_INV_BACKGROUND].flags & 1) == 0 ) {
BLACK : // NOTE: some additional checks are absent in the original code, so I've added few
		BGND_DrawInGameBlack();
		return;
	}

	meshPtr = MeshPtr[Objects[ID_INV_BACKGROUND].meshIndex];
	meshPtr += 3+2; // skip mesh coords (3*INT16) and radius (1*INT32)

	numVertices = *(meshPtr++);
	meshPtr += numVertices*3; // skip vertices (each one is 3xINT16)

	numNormals = *(meshPtr++);
	if( numNormals >= 0 ) // negative num means lights instead of normals
		meshPtr += numNormals*3; // skip normals (each is 3xINT16)
	else // NOTE: additional check. Absent in the original code
		meshPtr -= numNormals; // skip lights (each one is INT16)

	numQuads = *(meshPtr++);
	if( numQuads < 1 ) goto BLACK; // NOTE: additional check. Absent in the original code
	meshPtr += 4; // skip 4 vertice indices of 1st textured quad (each one is INT16)
	textureIndex = *(meshPtr++); // get texture index of 1st textured quad.

	texSource = HWR_PageHandles[PhdTextureInfo[textureIndex].tpage];
	textureInfo = &PhdTextureInfo[textureIndex];

	tu = textureInfo->uv[0].u / PHD_HALF;
	tv = textureInfo->uv[0].v / PHD_HALF;
	twidth  = textureInfo->uv[2].u / PHD_HALF - tu + 1;
	theight = textureInfo->uv[2].v / PHD_HALF - tv + 1;

	HWR_EnableZBuffer(false, false);
	y_current = 0;
	for( int i=0; i<6; ++i ) {
		y_next = y_current + PhdWinHeight;

		y0_pos = y_current / 6;
		y1_pos = y_next / 6;

		x_current = 0;
		for( int j=0; j<8; ++j ) {
			x_next = x_current + PhdWinWidth;

			x0 = PhdWinMinX + x_current / 8;
			y0 = y0_pos + PhdWinMinY;
			x1 = PhdWinMinX + x_next / 8;
			y1 = y1_pos + PhdWinMinY;

			color[0] = BGND_CenterLighting(x0, y0, PhdWinWidth, PhdWinHeight);
			color[1] = BGND_CenterLighting(x1, y0, PhdWinWidth, PhdWinHeight);
			color[2] = BGND_CenterLighting(x0, y1, PhdWinWidth, PhdWinHeight);
			color[3] = BGND_CenterLighting(x1, y1, PhdWinWidth, PhdWinHeight);

			DrawTextureTile(x0, y0, x1-x0, y1-y0, texSource, tu, tv, twidth, theight,
							color[0], color[1], color[2], color[3]);

			x_current = x_next;
		}
		y_current = y_next;
	}
	HWR_EnableZBuffer(true, true);
}

void __cdecl DrawTextureTile(int sx, int sy, int width, int height, D3DTEXTUREHANDLE texSource,
							 int tu, int tv, int t_width, int t_height,
							 D3DCOLOR color0, D3DCOLOR color1, D3DCOLOR color2, D3DCOLOR color3)
{
	float sx0, sy0, sx1, sy1;
	float tu0, tv0, tu1, tv1;
	float uvAdjust;
	D3DTLVERTEX vertex[4];

	sx0 = (double)sx;
	sy0 = (double)sy;
	sx1 = (double)(sx + width);
	sy1 = (double)(sy + height);

	uvAdjust = (double)UvAdd / (double)(PHD_ONE);
	tu0 = (double)tu / 256.0 + uvAdjust;
	tv0 = (double)tv / 256.0 + uvAdjust;
	tu1 = (double)(tu + t_width)  / 256.0 - uvAdjust;
	tv1 = (double)(tv + t_height) / 256.0 - uvAdjust;

	vertex[0].sx = sx0;
	vertex[0].sy = sy0;
	vertex[0].color = color0;
	vertex[0].tu = tu0;
	vertex[0].tv = tv0;

	vertex[1].sx = sx1;
	vertex[1].sy = sy0;
	vertex[1].color = color1;
	vertex[1].tu = tu1;
	vertex[1].tv = tv0;

	vertex[2].sx = sx0;
	vertex[2].sy = sy1;
	vertex[2].color = color2;
	vertex[2].tu = tu0;
	vertex[2].tv = tv1;

	vertex[3].sx = sx1;
	vertex[3].sy = sy1;
	vertex[3].color = color3;
	vertex[3].tu = tu1;
	vertex[3].tv = tv1;

	for( int i=0; i<4; ++i ) {
		vertex[i].sz = 0.995;
		vertex[i].rhw = RhwFactor / FltFarZ;
		vertex[i].specular = 0;
	}

	HWR_TexSource(texSource);
	HWR_EnableColorKey(false);
	_Direct3DDevice2->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DVT_TLVERTEX, &vertex, 4, D3DDP_DONOTUPDATEEXTENTS|D3DDP_DONOTCLIP);
}

D3DCOLOR __cdecl BGND_CenterLighting(int x, int y, int width, int height) {
	double xDist, yDist;
	int light;

	xDist = (double)(x - (width/2)) / (double)width; // xDist range will be: -0.5..0.5
	yDist = (double)(y - (height/2)) / (double)height; // yDist range will be: -0.5..0.5
	light = 256 - (sqrt(xDist * xDist + yDist * yDist) * 300.0); // light range will be: 44..256

	// Do light range checks just in case
	CLAMP(light, 0, 255);
	// combine gray color using light value
	return RGBA_MAKE(light, light, light, 0xFFu);
}

void __cdecl BGND_Free() {
	for( int i=0; i<5 ; ++i ) {
		if( BGND_TexturePageIndexes[i] >= 0 ) {
			SafeFreeTexturePage(BGND_TexturePageIndexes[i]);
			BGND_TexturePageIndexes[i] = -1;
		}
		BGND_PageHandles[i] = 0;
	}

	if( BGND_PaletteIndex >= 0 ) {
		SafeFreePalette(BGND_PaletteIndex);
		BGND_PaletteIndex = -1;
	}
}

bool __cdecl BGND_Init() {
	BGND_PictureIsReady = false;
	BGND_PaletteIndex = -1;

	for(int i=0; i<5; ++i)
		BGND_TexturePageIndexes[i] = -1;

	return true;
}

/*
 * Inject function
 */
void Inject_Background() {
	INJECT(0x00443A40, BGND_Make640x480);
	INJECT(0x00443C00, BGND_AddTexture);
	INJECT(0x00443CC0, BGND_GetPageHandles);
	INJECT(0x00443D00, BGND_DrawInGameBlack);
	INJECT(0x00443D60, DrawQuad);
	INJECT(0x00443E40, BGND_DrawInGameBackground);
	INJECT(0x00444060, DrawTextureTile);
	INJECT(0x004442C0, BGND_CenterLighting);
	INJECT(0x00444570, BGND_Free);
	INJECT(0x004445C0, BGND_Init);
}
