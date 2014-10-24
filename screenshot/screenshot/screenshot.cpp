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
	BITMAPINFOHEADER bmiHeader;
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
	printf("lpBitmap = %p\n", lpBitmap);
	if (!lpBitmap) { ret = -1; goto done; }

	nRet = GetDIBits(hdcMemDC, hbmScreen, 0, (UINT) bmpScreen.bmHeight, lpBitmap, (BITMAPINFO *) &bmiHeader, DIB_RGB_COLORS);
	printf("GetDIBits returns %d\n", nRet);
	if (!nRet) { ret = -1; goto done; }

	dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmfHeader.bfType = 0x4D42;
	bmfHeader.bfSize = dwSizeofDIB;
	bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);
	
	*dest = (char *) realloc(*dest, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize);
	printf("dest = %p\n", *dest);
	if (!*dest) { ret = -1; goto done; }

	memcpy(*dest, &bmfHeader, sizeof(BITMAPFILEHEADER));
	ret = sizeof(BITMAPFILEHEADER);
	memcpy(*dest + ret, &bmiHeader, sizeof(BITMAPINFOHEADER));
	ret += sizeof(BITMAPINFOHEADER);
	memcpy(*dest + ret, lpBitmap, dwBmpSize);
	ret += dwBmpSize;

done:
	if (lpBitmap) free(lpBitmap);
	if (hbmScreen) DeleteObject(hbmScreen);
	if (hdcMemDC) DeleteObject(hdcMemDC);
	if (hdcScreen) ReleaseDC(NULL, hdcScreen);

	return ret;
}

int main()
{
	char *ptr = NULL;
	int ret;
	int cnt;

	ret = screenshot(&ptr);
	printf("ret = %d\n", ret);
	if (ret < 0) {
		printf("screenshot failed!\n");
		goto done;
	}

	FILE *f = fopen("screenshot.bmp", "wb");
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