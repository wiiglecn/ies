#include "stdafx.h"
#include "ToolView.h"
#include "cJSON.h"
#include "arxHeaders.h"
#include "utils.h"

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* bitmapToBase64(const BITMAP& bm, const BYTE* bits, int* outLen) {
    int rowSize = ((bm.bmWidth * 24 + 31) / 32) * 4;
    int dataSize = rowSize * bm.bmHeight;
    int encLen = 4 * ((dataSize + 2) / 3);
    char* encoded = (char*)malloc(encLen + 1);
    if (!encoded) return NULL;

    int i = 0, j = 0;
    while (i < dataSize) {
        DWORD a = (i < dataSize) ? bits[i++] : 0;
        DWORD b = (i < dataSize) ? bits[i++] : 0;
        DWORD c = (i < dataSize) ? bits[i++] : 0;
        DWORD triple = (a << 16) | (b << 8) | c;
        encoded[j++] = base64_table[(triple >> 18) & 0x3F];
        encoded[j++] = base64_table[(triple >> 12) & 0x3F];
        encoded[j++] = base64_table[(triple >> 6) & 0x3F];
        encoded[j++] = base64_table[triple & 0x3F];
    }
    // Padding
    int mod = dataSize % 3;
    if (mod == 1) { encoded[j - 1] = '='; encoded[j - 2] = '='; }
    else if (mod == 2) { encoded[j - 1] = '='; }
    encoded[j] = '\0';
    if (outLen) *outLen = j;
    return encoded;

}

ToolView::ToolView(void):ToolDrawBase()
{
}

ToolView::~ToolView(void)
{
}
ToolView& ToolView::getInstance()
{
    static ToolView instance;
    return instance;
}

void ToolView::zoom_extents(AiToolCommandData* data) {
    if (actionBefore() != 0) return;
    acDocManager->sendStringToExecute(acDocManager->curDocument(), _T("_.ZOOM _E "));
    data->ret = 0;
    actionEnd();
}

void ToolView::zoom_window(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    double x1 = cJSON_GetObjectItem(data->jsonRoot, "x1")->valuedouble;
    double y1 = cJSON_GetObjectItem(data->jsonRoot, "y1")->valuedouble;
    double x2 = cJSON_GetObjectItem(data->jsonRoot, "x2")->valuedouble;
    double y2 = cJSON_GetObjectItem(data->jsonRoot, "y2")->valuedouble;

    CString cmd;
    cmd.Format(_T("_.ZOOM _W %f,%f %f,%f "), x1, y1, x2, y2);
    acDocManager->sendStringToExecute(acDocManager->curDocument(), cmd);

    data->ret = 0;
    actionEnd();
}

void ToolView::get_screenshot(AiToolCommandData* data) {
    if (actionBefore() != 0) return;

    CWnd* pWnd = acedGetAcadDwgView();
    if (!pWnd) { data->ret = 1; actionEnd(); return; }

    CRect rect;
    pWnd->GetClientRect(&rect);
    int width = rect.Width();
    int height = rect.Height();

    if (width <= 0 || height <= 0) { data->ret = 1; actionEnd(); return; }

    HDC hScreenDC = GetDC(pWnd->m_hWnd);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBitmap);

    BitBlt(hMemDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    BITMAP bm;
    GetObject(hBitmap, sizeof(BITMAP), &bm);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = -bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = (BYTE*)malloc(bm.bmWidth * ((bm.bmWidth * 24 + 31) / 32) * 4 * bm.bmHeight);
    GetDIBits(hMemDC, hBitmap, 0, bm.bmHeight, bits, &bmi, DIB_RGB_COLORS);

    int encLen = 0;
    char* encoded = bitmapToBase64(bm, bits, &encLen);

    SelectObject(hMemDC, hOldBmp);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(pWnd->m_hWnd, hScreenDC);
    free(bits);

    if (encoded) {
        cJSON* pResult = cJSON_CreateObject();
        cJSON_AddNumberToObject(pResult, "width", width);
        cJSON_AddNumberToObject(pResult, "height", height);
        cJSON_AddStringToObject(pResult, "format", "bmp");
        cJSON_AddStringToObject(pResult, "encoding", "base64");
        cJSON_AddStringToObject(pResult, "data", encoded);
        data->resultJson = pResult;
        free(encoded);
        data->ret = 0;
    } 

    actionEnd();
}