#include <windows.h>
#include <string.h>
#include <stdlib.h>

size_t screenshot(void **bmpfile)
{
	HDC hdcScreen;
	HDC hdcMemDC = NULL;
	HBITMAP hbmScreen = NULL;
	BITMAP bmpScreen;
	DWORD dwBmpSize;
	DWORD dwSizeofDIB;
	BITMAPFILEHEADER bmfHeader;
	BITMAPINFOHEADER bmiHeader;
	void *lpBitmap = NULL;
	int width, height;
	BOOL bRet;
	int nRet;
	HBITMAP hbmRet;
	size_t ret = 0;

	hdcScreen = GetDC(NULL);
	hdcMemDC = CreateCompatibleDC(NULL);
	if (!hdcScreen || !hdcMemDC) goto done;

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);
	if (!width || !height) goto done;

	hbmScreen = CreateCompatibleBitmap(hdcScreen, width, height);
	if (!hbmScreen) goto done;

	hbmRet = (HBITMAP) SelectObject(hdcMemDC, hbmScreen);
	if (!hbmRet) goto done;

	bRet = BitBlt(hdcMemDC, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
	if (!bRet) goto done;

	nRet = GetObject(hbmScreen, sizeof(BITMAP), &bmpScreen);
	if (!nRet) goto done;

	memset(&bmiHeader, 0, sizeof(bmiHeader));
	memset(&bmfHeader, 0, sizeof(bmfHeader));
	bmiHeader.biSize = sizeof(BITMAPINFOHEADER);	
	bmiHeader.biWidth = bmpScreen.bmWidth;
	bmiHeader.biHeight = bmpScreen.bmHeight;
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 32;
	bmiHeader.biCompression = BI_RGB;
	dwBmpSize = ((bmpScreen.bmWidth * bmiHeader.biBitCount + 31) / 32) * 4 * bmpScreen.bmHeight;

	lpBitmap = malloc(dwBmpSize);
	if (!lpBitmap) goto done;

	nRet = GetDIBits(hdcMemDC, hbmScreen, 0, (UINT) bmpScreen.bmHeight, lpBitmap, (BITMAPINFO *) &bmiHeader, DIB_RGB_COLORS);
	if (!nRet) goto done;

	dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmfHeader.bfType = 0x4D42;
	bmfHeader.bfSize = dwSizeofDIB;
	bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);
	
	*bmpfile = (char *) realloc(*bmpfile, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize);
	if (!*bmpfile) goto done;

	memcpy(*bmpfile, &bmfHeader, sizeof(BITMAPFILEHEADER));
	ret += sizeof(BITMAPFILEHEADER);
	memcpy(*(char **)bmpfile + ret, &bmiHeader, sizeof(BITMAPINFOHEADER));
	ret += sizeof(BITMAPINFOHEADER);
	memcpy(*(char **)bmpfile + ret, lpBitmap, dwBmpSize);
	ret += dwBmpSize;

done:
	if (lpBitmap) free(lpBitmap);
	if (hbmScreen) DeleteObject(hbmScreen);
	if (hdcMemDC) DeleteObject(hdcMemDC);
	if (hdcScreen) ReleaseDC(NULL, hdcScreen);

	return ret;
}