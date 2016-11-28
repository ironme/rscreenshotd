#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int screenshot(char **dest)
{
	HDC hdcScreen;
	HDC hdcMemDC = NULL;
	HBITMAP hbmScreen = NULL;
	BITMAP bmpScreen;
	DWORD dwBmpSize;
	DWORD dwSizeofDIB;
	BITMAPFILEHEADER bmfHeader;
	BITMAPINFO *lpbmiInfo = NULL;
	DWORD dwSizeofInfo;
	WORD biBitCount = 24;
	void *lpBitmap = NULL;
	int width, height;
	BOOL bRet;
	int nRet;
	HBITMAP hbmRet;
	int ret;

	hdcScreen = GetDC(NULL);
	hdcMemDC = CreateCompatibleDC(NULL);
	printf("hdcScreen = %p\nhdcMemDC = %p\n", hdcScreen, hdcMemDC);
	if (!hdcScreen || !hdcMemDC) { ret = -1; goto done; }

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);
	printf("width = %d\nheight = %d\n", width, height);
	if (!width || !height) { ret = -1; goto done; }

	hbmScreen = CreateCompatibleBitmap(hdcScreen, width, height);
	printf("hbmScreen = %p\n", hbmScreen);
	if (!hbmScreen) { ret = -1; goto done; }

	hbmRet = (HBITMAP) SelectObject(hdcMemDC, hbmScreen);
	printf("SelectObject returns %p\n", hbmRet);
	if (!hbmRet) { ret = -1; goto done; }

	bRet = BitBlt(hdcMemDC, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
	printf("BitBlt returns %d\n", (int) bRet);
	if (!bRet) { ret = -1; goto done; }

	nRet = GetObject(hbmScreen, sizeof(BITMAP), &bmpScreen);
	printf("GetObject returns %d\n", nRet);
	if (!nRet) { ret = -1; goto done; }

	dwSizeofInfo = sizeof(BITMAPINFOHEADER);
	switch (biBitCount) {
		case 1: dwSizeofInfo += 2 * sizeof(RGBQUAD); break;
		case 4: dwSizeofInfo += 16 * sizeof(RGBQUAD); break;
		case 8: dwSizeofInfo += 256 * sizeof(RGBQUAD); break;
		case 16: case 24: case 32: break;
		default: ret = -1; goto done;
	};
	lpbmiInfo = (BITMAPINFO *) malloc(dwSizeofInfo);
	printf("lpbmiInfo = %p\n", lpbmiInfo);
	if (!lpbmiInfo) { ret = -1; goto done; }
	memset(lpbmiInfo, 0, dwSizeofInfo);
	lpbmiInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);	
	lpbmiInfo->bmiHeader.biWidth = bmpScreen.bmWidth;
	lpbmiInfo->bmiHeader.biHeight = bmpScreen.bmHeight;
	lpbmiInfo->bmiHeader.biPlanes = 1;
	lpbmiInfo->bmiHeader.biBitCount = biBitCount;
	lpbmiInfo->bmiHeader.biCompression = BI_RGB;
	dwBmpSize = (((bmpScreen.bmWidth * lpbmiInfo->bmiHeader.biBitCount + 7) / 8) + 3) / 4 * 4 * bmpScreen.bmHeight;

	lpBitmap = malloc(dwBmpSize);
	printf("lpBitmap = %p\n", lpBitmap);
	if (!lpBitmap) { ret = -1; goto done; }
	memset(lpBitmap, 0, sizeof(dwBmpSize));

	nRet = GetDIBits(hdcMemDC, hbmScreen, 0, (UINT) bmpScreen.bmHeight, lpBitmap, lpbmiInfo, DIB_RGB_COLORS);
	printf("GetDIBits returns %d\n", nRet);
	if (!nRet) { ret = -1; goto done; }

	dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + dwSizeofInfo;
	memset(&bmfHeader, 0, sizeof(bmfHeader));
	bmfHeader.bfType = 0x4D42;
	bmfHeader.bfSize = dwSizeofDIB;
	bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + dwSizeofInfo;
	
	*dest = (char *) realloc(*dest, sizeof(BITMAPFILEHEADER) + dwSizeofInfo + dwBmpSize);
	printf("dest = %p\n", *dest);
	if (!*dest) { ret = -1; goto done; }

	memcpy(*dest, &bmfHeader, sizeof(BITMAPFILEHEADER));
	ret = sizeof(BITMAPFILEHEADER);
	memcpy(*dest + ret, lpbmiInfo, dwSizeofInfo);
	ret += dwSizeofInfo;
	memcpy(*dest + ret, lpBitmap, dwBmpSize);
	ret += dwBmpSize;

done:
	if (lpbmiInfo) free(lpbmiInfo);
	if (lpBitmap) free(lpBitmap);
	if (hbmScreen) DeleteObject(hbmScreen);
	if (hdcMemDC) DeleteObject(hdcMemDC);
	if (hdcScreen) ReleaseDC(NULL, hdcScreen);

	return ret;
}

int main()
{
	char *ptr = NULL;
	FILE *f = NULL;
	int ret;
	int cnt;

	ret = screenshot(&ptr);
	printf("ret = %d\n", ret);
	if (ret < 0) {
		printf("screenshot failed!\n");
		goto done;
	}

	f = fopen("screenshot.bmp", "wb");
	if (!f) {
		printf("can't open file!\n");
		goto done;
	}

	cnt = fwrite(ptr, 1, ret, f);
	printf("written %d bytes.\n", cnt);
	if (cnt != ret)
		printf("fwrite failed!\n");

done:
	if (f) fclose(f);
	if (ptr) free(ptr);

	system("pause");
	return 0;
}