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
#include "specific/screenshot.h"
#include "specific/winvid.h"
#include "global/vars.h"

// NOTE: the original game overwrites screenshots made before game launch
// That is fixed now with FEATURE_SCREENSHOT_FIX define
static int screenShotPcxNumber = 0;
static int screenShotTgaNumber = 0;

static TGA_HEADER ScreenShotTgaHeader = {
	0, 0,
	.dataTypeCode = 2, // Uncompressed, RGB images
	0, 0, 0, 0, 0,
	.width = 320,
	.height = 256,
	.bpp = 16,
	.imageDescriptor = 0,
};


// NOTE: This function is not presented in the original code
// but the code is taken away form ScreenShot() and extended
// to be compatible with 24/32 bit
static void __cdecl ScreenShotTGA(LPDIRECTDRAWSURFACE3 screen, BYTE tgaBpp) {
	DWORD i, j;
	BYTE *src, *dst;
	DDSURFACEDESC desc;
	BYTE *tgaPic = NULL;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD bytesWritten;
	char fileName[128];

	if( tgaBpp != 16 && tgaBpp != 24 )
		return;

	memset(&desc, 0, sizeof(DDSURFACEDESC));
	desc.dwSize = sizeof(DDSURFACEDESC);
	if FAILED(WinVidBufferLock(screen, &desc, DDLOCK_WRITEONLY|DDLOCK_WAIT))
		return;

#if defined(FEATURE_SCREENSHOT_FIX)
	do { // search first free screenshot slot
		if( ++screenShotTgaNumber > 9999 ) goto CLEANUP;
		wsprintf(fileName, "screenshots\\tomb%04d.tga", screenShotTgaNumber);
	} while( INVALID_FILE_ATTRIBUTES != GetFileAttributes(fileName) );
	CreateDirectory("screenshots", NULL); // just in case if it is not created yet
#else // !FEATURE_SCREENSHOT_FIX
	wsprintf(fileName, "tomb%04d.tga", screenShotTgaNumber++);
#endif // FEATURE_SCREENSHOT_FIX

	hFile = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE )
		goto CLEANUP;

	ScreenShotTgaHeader.width = desc.dwWidth;
	ScreenShotTgaHeader.height = desc.dwHeight;
	ScreenShotTgaHeader.bpp = tgaBpp;
	WriteFile(hFile, &ScreenShotTgaHeader, sizeof(TGA_HEADER), &bytesWritten, NULL);

#if defined(FEATURE_SCREENSHOT_FIX)
	// NOTE: There was unsafe memory usage in the original code. The game just used GameAllocMemPointer buffer!
	// No new memory allocations. On higher resolutions there was critical data overwriting and game crashed
	tgaPic = (BYTE *)GlobalAlloc(GMEM_FIXED, desc.dwWidth*desc.dwHeight*(tgaBpp/8));
#else // !FEATURE_SCREENSHOT_FIX
	tgaPic = (BYTE *)GameAllocMemPointer;
#endif // FEATURE_SCREENSHOT_FIX

	if( tgaPic == NULL )
		goto CLEANUP;

	// We need to load bitmap lines to TGA starting from the bottom line
#if defined(FEATURE_SCREENSHOT_FIX)
	// NOTE: There was bug in the original formula: src = lpSurface + lPitch * dwHeight
	// Height must be subtracted by 1 in this formula
	src = (BYTE *)desc.lpSurface + desc.lPitch*(desc.dwHeight - 1);
#else // !FEATURE_SCREENSHOT_FIX
	src = (BYTE *)desc.lpSurface + desc.lPitch*desc.dwHeight;
#endif // FEATURE_SCREENSHOT_FIX

	dst = tgaPic;
	if( tgaBpp == 16 ) {
		for( i=0; i < desc.dwHeight; ++i ) {
			// R5G6B5 - not TGA compatible
			if( desc.ddpfPixelFormat.dwRBitMask == 0xF800 ) {
				// right shift highest 10 bits (R+G) over lowest G bit
				for( j=0; j < desc.dwWidth; ++j ) {
					UINT16 sample = ((UINT16 *)src)[j];
					((UINT16 *)dst)[j] = ((sample & 0xFFC0) >> 1) | (sample & 0x001F);
				}
			} else {
				// X1R5G5B5 - already TGA compatible
				memcpy(dst, src, sizeof(UINT16)*desc.dwWidth);
			}
			src -= desc.lPitch;
			dst += sizeof(UINT16)*desc.dwWidth;
		}
	} else {
		for( i=0; i < desc.dwHeight; ++i ) {
			if( desc.ddpfPixelFormat.dwRGBBitCount == 24 ) {
				memcpy(dst, src, sizeof(RGB)*desc.dwWidth);
			} else {
				for( j=0; j < desc.dwWidth; ++j ) {
					((RGB *)dst)[j] = *(RGB*)(src + j * (desc.ddpfPixelFormat.dwRGBBitCount / 8));
				}
			}
			src -= desc.lPitch;
			dst += sizeof(RGB)*desc.dwWidth;
		}
	}
	WriteFile(hFile, tgaPic, desc.dwWidth*desc.dwHeight*(tgaBpp/8), &bytesWritten, NULL);

CLEANUP :
#if defined(FEATURE_SCREENSHOT_FIX)
	if( tgaPic != NULL )
		GlobalFree((HGLOBAL)tgaPic);
#endif // FEATURE_SCREENSHOT_FIX

	if( hFile != INVALID_HANDLE_VALUE )
		CloseHandle(hFile);

	WinVidBufferUnlock(screen, &desc);
}


void __cdecl ScreenShotPCX() {
	HRESULT rc;
	LPDIRECTDRAWSURFACE3 screen;
	DDSURFACEDESC desc;
	BYTE *pcxData = NULL;
	DWORD pcxSize;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD bytesWritten;
	char fileName[128];

	screen = ( SavedAppSettings.RenderMode != RM_Software ) ? RenderBufferSurface : PrimaryBufferSurface;
	desc.dwSize = sizeof(DDSURFACEDESC);

	do {
		rc = screen->Lock(NULL, &desc, DDLOCK_SURFACEMEMORYPTR, NULL);
	} while( rc == DDERR_WASSTILLDRAWING );

	if( rc == DDERR_SURFACELOST )
		rc = screen->Restore();
	if FAILED(rc)
		return;

	pcxSize = CompPCX((BYTE *)desc.lpSurface, desc.dwWidth, desc.dwHeight, GamePalette8, &pcxData);
	screen->Unlock(&desc);

	if( pcxSize == 0 || pcxData == NULL )
		return;

#if defined(FEATURE_SCREENSHOT_FIX)
	do { // search first free screenshot slot
		if( ++screenShotPcxNumber > 9999 ) {
			GlobalFree(pcxData);
			return;
		}
		wsprintf(fileName, "screenshots\\tomb%04d.pcx", screenShotPcxNumber);
	} while( INVALID_FILE_ATTRIBUTES != GetFileAttributes(fileName) );
	CreateDirectory("screenshots", NULL); // just in case if it is not created yet
#else // !FEATURE_SCREENSHOT_FIX
	if( ++screenShotPcxNumber > 9999 ) screenShotPcxNumber = 1;
	wsprintf(fileName, "tomb%04d.pcx", screenShotPcxNumber);
#endif // FEATURE_SCREENSHOT_FIX

	hFile = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile != INVALID_HANDLE_VALUE ) {
		WriteFile(hFile, pcxData, pcxSize, &bytesWritten, NULL);
		CloseHandle(hFile);
	}
	GlobalFree(pcxData);
}


DWORD __cdecl CompPCX(BYTE *bitmap, DWORD width, DWORD height, RGB *palette, BYTE **pcxData) {
	DWORD i;
	PCX_HEADER *pcxHeader;
	BYTE *picData;

	*pcxData = (BYTE *)GlobalAlloc(GMEM_FIXED, width*height*2 + sizeof(PCX_HEADER) + sizeof(RGB)*256);
	if( *pcxData == NULL )
		return 0;

	pcxHeader = *(PCX_HEADER **)pcxData;

	pcxHeader->manufacturer = 10;
	pcxHeader->version = 5;
	pcxHeader->rle = 1;
	pcxHeader->bpp = 8;
	pcxHeader->planes = 1;

	pcxHeader->xMin = 0;
	pcxHeader->yMin = 0;
	pcxHeader->xMax = width - 1;
	pcxHeader->yMax = height - 1;
	pcxHeader->h_dpi = width;
	pcxHeader->v_dpi = height;
	pcxHeader->bytesPerLine = width;

	picData = *pcxData + sizeof(PCX_HEADER);
	for( i=0; i<height; ++i ) {
		picData += EncodeLinePCX(bitmap, width, picData);
		bitmap += width;
	}

	*(picData++) = 0x0C;
	memcpy(picData, palette, sizeof(RGB)*256);

	return (DWORD)(picData - *pcxData + sizeof(RGB)*256); // pcx data size
}


DWORD __cdecl EncodeLinePCX(BYTE *src, DWORD width, BYTE *dst) {
	BYTE current, add;
	DWORD total = 0;
	BYTE runCount = 1;
	BYTE last = *src;

	for( DWORD i=1; i<width; ++i ) {
		current = *(++src);
		if( current == last ) {
			++runCount;
			if( runCount == 63 ) {
				add = EncodePutPCX(last, runCount, dst);
				if( add == 0 ) {
					return 0;
				}
				total += add;
				dst += add;
				runCount = 0;
			}
		} else {
			if( runCount != 0 ) {
				add = EncodePutPCX(last, runCount, dst);
				if( add == 0 ) {
					return 0;
				}
				total += add;
				dst += add;
			}
			last = current;
			runCount = 1;
		}
	}

	if( runCount ) {
		add = EncodePutPCX(last, runCount, dst);
		if( add == 0) {
			return 0;
		}
		total += add;
		dst += add;
	}
	return total;
}


DWORD __cdecl EncodePutPCX(BYTE value, BYTE num, BYTE *buffer) {
	if( num == 0 || num > 63 ) {
		return 0;
	}

	if( num == 1 && (value & 0xC0) != 0xC0 ) {
		buffer[0] = value;
		return 1;
	}

	buffer[0] = num | 0xC0;
	buffer[1] = value;
	return 2;
}


void __cdecl ScreenShot(LPDIRECTDRAWSURFACE3 screen) {
	DDSURFACEDESC desc;

	memset(&desc, 0, sizeof(DDSURFACEDESC));
	desc.dwSize = sizeof(DDSURFACEDESC);

	if SUCCEEDED( screen->GetSurfaceDesc(&desc)) {
		switch( desc.ddpfPixelFormat.dwRGBBitCount ) {
			case 8 :
				ScreenShotPCX();
				break;

			case 16 :
				ScreenShotTGA(screen, 16);
				break;

#if defined(FEATURE_SCREENSHOT_FIX)
			case 24 :
			case 32 :
				// NOTE: the original game cannot make 24/32 bit screenshots
				ScreenShotTGA(screen, 24);
				break;
#endif // FEATURE_SCREENSHOT_FIX

			default :
				break;
		}
	}
}


/*
 * Inject function
 */
void Inject_Screenshot() {
	INJECT(0x0044E9A0, ScreenShotPCX);
	INJECT(0x0044EAB0, CompPCX);
	INJECT(0x0044EB60, EncodeLinePCX);
	INJECT(0x0044EC40, EncodePutPCX);
	INJECT(0x0044EC80, ScreenShot);
}
