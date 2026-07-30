#include "StdAfx.h"
#include "Utils.h"
#include "arxHeaders.h"

CUtils::CUtils(void)
{
}

CUtils::~CUtils(void)
{
}
void CUtils::FocusAcadDrawing()
{
    // Try to get the active document's viewport window
    CWnd* pDwgView = acedGetAcadDwgView();
    if (pDwgView && pDwgView->GetSafeHwnd())
    {
        ::SetFocus(pDwgView->GetSafeHwnd());
    }
    else 
    {
        // Fallback: If dwg view is not available, try the main frame
        if (acedGetAcadFrame())
            ::SetFocus(acedGetAcadFrame()->m_hWnd);
    }
}
bool CUtils::IsCircleExisting(const AcGePoint3d& center)
{
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    if (!pDb) return false;

    AcDbBlockTable* pBlockTable = NULL;
    AcDbBlockTableRecord* pModelSpace = NULL;

    // Open Model Space for read
    if (pDb->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
        return false;
    
    if (pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForRead) != Acad::eOk)
    {
        pBlockTable->close();
        return false;
    }
    pBlockTable->close();

    bool found = false;
    AcDbBlockTableRecordIterator* pIter = NULL;
    if (pModelSpace->newIterator(pIter) == Acad::eOk)
    {
        for (pIter->start(); !pIter->done(); pIter->step())
        {
            AcDbEntity* pEnt = NULL;
            if (pIter->getEntity(pEnt, AcDb::kForRead) == Acad::eOk)
            {
                // Check if it's a circle
                AcDbCircle* pCircle = AcDbCircle::cast(pEnt);
                if (pCircle)
                {
                    // Check radius and center
                    // Using a small tolerance for floating point comparison
                    const double tol = 1e-4; 
                    if (pCircle->center().distanceTo(center) < tol)
                    {
                        found = true;
                        pCircle->close();
                        break; // Found one, no need to check further
                    }
                }
                pEnt->close();
            }
        }
        delete pIter;
    }
    pModelSpace->close();

    return found;
}
CString GetArxPath()
{
    TCHAR szPath[MAX_PATH] = {0};

    // 注意这里填写你的ARX文件名
    HMODULE hModule = GetModuleHandle(_T("KsExploration.arx"));

    if (hModule)
    {
        GetModuleFileName(hModule, szPath, MAX_PATH);
        return CString(szPath);
    }

    return _T("");
}
CString CUtils::GetArxFolder()
{
    CString path = GetArxPath();

    int pos = path.ReverseFind('\\');
    if (pos != -1)
        path = path.Left(pos + 1);

    return path;
}
// Helper function to convert std::string to std::wstring
std::wstring CUtils::StringToWString(const std::string& str)
{
    if (str.empty()) return std::wstring();
    
    // Get required size
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
bool CUtils::utf8ToTChar(const char* utf8, TCHAR* outBuf, int bufLen)
{
    if (!utf8 || !outBuf || bufLen <= 0)
        return false;
#ifdef _UNICODE
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (needed <= 0 || needed > bufLen)
        return false;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, outBuf, bufLen);
    return true;
#else
    return strcpy_s(outBuf, bufLen, utf8) == 0;
#endif
}
std::string CUtils::CStringToUTF8(const CString& str)
{
    if (str.IsEmpty())
        return std::string();

#ifdef _UNICODE
    int len = str.GetLength();
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, str.GetString(), len, NULL, 0, NULL, NULL);
    if (utf8Len == 0)
        return std::string();
    std::string result(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.GetString(), len, &result[0], utf8Len, NULL, NULL);
    return result;
#else
    int len = str.GetLength();
    int wLen = MultiByteToWideChar(CP_ACP, 0, str.GetString(), len, NULL, 0);
    if (wLen == 0)
        return std::string();
    std::wstring wstr(wLen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.GetString(), len, &wstr[0], wLen);
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wLen, NULL, 0, NULL, NULL);
    if (utf8Len == 0)
        return std::string();
    std::string result(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wLen, &result[0], utf8Len, NULL, NULL);
    return result;
#endif
}
void CUtils::TCharToUtf8(const TCHAR* tStr, char* utf8Str, int utf8Size)
{
    if (!tStr || !utf8Str || utf8Size <= 0) {
        if (utf8Str && utf8Size > 0) utf8Str[0] = '\0';
        return;
    }
#ifdef _UNICODE
    WideCharToMultiByte(CP_UTF8, 0, tStr, -1, utf8Str, utf8Size, NULL, NULL);
#else
    strcpy_s(utf8Str, utf8Size, tStr);
#endif
}