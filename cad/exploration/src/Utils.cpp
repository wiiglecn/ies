#include "StdAfx.h"
#include "Utils.h"
#include "arxHeaders.h"
#include <wincrypt.h>
#include <string>
#include <fstream>
#include <vector>

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


// 使用密码解密文件，返回解密后的字符串内容
std::string CUtils::DecryptFileWithPassword(const std::wstring& filePath, const std::string& password)
{
    // 1. 读取加密文件内容
    std::ifstream file(filePath.c_str(), std::ios::binary);
    if (!file.is_open())
    {
        return "";
    }

    std::vector<BYTE> encryptedData((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());

    // Python 加密格式: 前 16 字节是 IV，后续是真正的密文
    if (encryptedData.size() <= 16)
    {
        return "";
    }

    // 2. 提取前 16 字节作为 IV
    BYTE iv[16] = {0};
    memcpy(iv, &encryptedData[0], 16);

    // 3. 获取真正的密文部分
    std::vector<BYTE> cipherText(encryptedData.begin() + 16, encryptedData.end());
    if (cipherText.empty())
    {
        return "";
    }

    HCRYPTPROV hProv = NULL;
    HCRYPTHASH hHash = NULL;
    HCRYPTKEY hKey = NULL;
    std::string decryptedContent = "";

    // 4. 获取 CSP 句柄
    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
    {
        return "";
    }

    // 5. 对密码进行 SHA-256 哈希计算，以匹配 Python 的 hashlib.sha256
    if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
    {
        if (CryptHashData(hHash, reinterpret_cast<const BYTE*>(password.c_str()), static_cast<DWORD>(password.length()), 0))
        {
            DWORD hashLen = 32;
            BYTE keyBytes[32] = {0};
            
            // 获取哈希结果作为 32 字节的密钥
            if (CryptGetHashParam(hHash, HP_HASHVAL, keyBytes, &hashLen, 0))
            {
                // 6. 构造 PLAINTEXTKEYBLOB，与 Python 的 _import_aes_key 对应
                struct {
                    BLOBHEADER header;
                    DWORD dwKeySize;
                } keyBlobHeader;

                keyBlobHeader.header.bType = PLAINTEXTKEYBLOB;
                keyBlobHeader.header.bVersion = CUR_BLOB_VERSION;
                keyBlobHeader.header.reserved = 0;
                keyBlobHeader.header.aiKeyAlg = CALG_AES_256;
                keyBlobHeader.dwKeySize = 32;

                std::vector<BYTE> blob(sizeof(keyBlobHeader) + 32);
                memcpy(&blob[0], &keyBlobHeader, sizeof(keyBlobHeader));
                memcpy(&blob[sizeof(keyBlobHeader)], keyBytes, 32);

                // 7. 使用 CryptImportKey 导入密钥
                if (CryptImportKey(hProv, &blob[0], static_cast<DWORD>(blob.size()), 0, 0, &hKey))
                {
                    // 设置为 CBC 模式
                    DWORD dwMode = CRYPT_MODE_CBC;
                    CryptSetKeyParam(hKey, KP_MODE, reinterpret_cast<BYTE*>(&dwMode), 0);
                    
                    // 设置从文件头部提取的 IV
                    CryptSetKeyParam(hKey, KP_IV, iv, 0);

                    // 8. 准备解密缓冲区
                    DWORD dwBufferLen = static_cast<DWORD>(cipherText.size());
                    std::vector<BYTE> buffer(dwBufferLen);
                    memcpy(&buffer[0], &cipherText[0], dwBufferLen);

                    // 9. 执行解密
                    if (CryptDecrypt(hKey, 0, TRUE, 0, &buffer[0], &dwBufferLen))
                    {
                        buffer.resize(dwBufferLen);
                        decryptedContent.assign(reinterpret_cast<char*>(&buffer[0]), buffer.size());
                    }

                    CryptDestroyKey(hKey);
                }
            }
        }
        CryptDestroyHash(hHash);
    }

    // 10. 释放资源
    CryptReleaseContext(hProv, 0);

    return decryptedContent;
}