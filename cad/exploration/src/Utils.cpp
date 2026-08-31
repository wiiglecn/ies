#include "StdAfx.h"
#include "Utils.h"
#include "arxHeaders.h"
#include <wincrypt.h>
#include <string>
#include <fstream>
#include <vector>

#ifdef _CAD2005
	// ---- VC7 (VS2002) 的旧 SDK 头文件缺少以下常量，手动补上（与新版 wincrypt.h 完全一致）----
	#ifndef PROV_RSA_AES
	#define PROV_RSA_AES 24                                       // Microsoft Enhanced RSA and AES Cryptographic Provider
	#endif
	#ifndef CALG_SHA_256
	#define CALG_SHA_256  (ALG_CLASS_HASH | ALG_TYPE_ANY | 4)     // 4 = ALG_SID_SHA_256
	#endif
	#ifndef CALG_AES_256
	#define CALG_AES_256  (ALG_CLASS_DATA_ENCRYPT | ALG_TYPE_BLOCK | 16)  // 16 = ALG_SID_AES_256
	#endif
	#ifndef PLAINTEXTKEYBLOB
	#define PLAINTEXTKEYBLOB 0x8
	#endif
#endif

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
    // 1. 参数校验
    if (!utf8 || !outBuf || bufLen <= 0)
    {
        if (outBuf && bufLen > 0)
            outBuf = _T('\0');
        return false;
    }

    // 初始化输出缓冲区为空
    outBuf = _T('\0');

#ifdef _UNICODE
    // ---------------------------------------------------------
    // 情况 A: UNICODE 模式 (TCHAR == wchar_t)
    // 路径: UTF-8 -> UTF-16 (wchar_t)
    // ---------------------------------------------------------
    
    // 第一步：计算转换所需的 wchar_t 数量
    // CP_UTF8 表示源字符串是 UTF-8
    int wLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    
    if (wLen == 0)
        return false;

    // 检查缓冲区是否足够 (wLen 包含结尾的 \0)
    if (wLen > bufLen)
        return false;

    // 第二步：执行转换
    int result = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, outBuf, bufLen);
    
    return (result > 0);

#else
    // ---------------------------------------------------------
    // 情况 B: ANSI/MBCS 模式 (TCHAR == char)
    // 路径: UTF-8 -> UTF-16 (中间缓冲) -> ANSI (CP_ACP)
    // 注意：不能直接从 UTF-8 转到 ANSI，必须经过 Unicode 中转，
    // 否则非 ASCII 字符会乱码或丢失。
    // ---------------------------------------------------------

    // 第一步：UTF-8 -> wchar_t (临时缓冲)
    int wLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wLen == 0)
        return false;

    // 分配临时宽字符缓冲区
    wchar_t* pWBuf = new wchar_t[wLen];
    if (!pWBuf)
        return false;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, pWBuf, wLen) == 0)
    {
        delete[] pWBuf;
        return false;
    }

    // 第二步：wchar_t -> ANSI (TCHAR)
    // CP_ACP 表示使用系统当前的 ANSI 代码页
    int aLen = WideCharToMultiByte(CP_ACP, 0, pWBuf, -1, NULL, 0, NULL, NULL);
    
    if (aLen == 0)
    {
        delete[] pWBuf;
        return false;
    }

    // 检查缓冲区是否足够
    if (aLen > bufLen)
    {
        delete[] pWBuf;
        return false;
    }

    // 执行转换
    int result = WideCharToMultiByte(CP_ACP, 0, pWBuf, -1, outBuf, bufLen, NULL, NULL);
    
    // 清理临时内存
    delete[] pWBuf;

    return (result > 0);
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
    // 1. 参数校验
    if (!tStr || !utf8Str || utf8Size <= 0)
    {
        if (utf8Str && utf8Size > 0)
            utf8Str = '\0';
        return;
    }

    // 初始化输出缓冲区
    utf8Str = '\0';

#ifdef _UNICODE
    // ---------------------------------------------------------
    // 情况 A: UNICODE 模式 (TCHAR == wchar_t)
    // 路径: UTF-16 (wchar_t) -> UTF-8
    // ---------------------------------------------------------
    
    // 第一步：计算转换所需的 UTF-8 字节数
    // CP_UTF8 表示目标编码是 UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, tStr, -1, NULL, 0, NULL, NULL);
    
    if (utf8Len == 0)
        return;

    // 检查缓冲区是否足够 (utf8Len 包含结尾的 \0)
    if (utf8Len > utf8Size)
        return;

    // 第二步：执行转换
    WideCharToMultiByte(CP_UTF8, 0, tStr, -1, utf8Str, utf8Size, NULL, NULL);

#else
    // ---------------------------------------------------------
    // 情况 B: ANSI/MBCS 模式 (TCHAR == char)
    // 路径: ANSI (CP_ACP) -> UTF-16 (中间缓冲) -> UTF-8
    // 注意：不能直接从 ANSI 转到 UTF-8，必须经过 Unicode 中转
    // ---------------------------------------------------------

    // 第一步：ANSI -> UTF-16 (wchar_t)
    // 计算所需的宽字符数量
    int wLen = MultiByteToWideChar(CP_ACP, 0, tStr, -1, NULL, 0);
    if (wLen == 0)
        return;

    // 分配临时宽字符缓冲区
    wchar_t* pWBuf = new wchar_t[wLen];
    if (!pWBuf)
        return;

    // 执行 ANSI 到 UTF-16 的转换
    if (MultiByteToWideChar(CP_ACP, 0, tStr, -1, pWBuf, wLen) == 0)
    {
        delete[] pWBuf;
        return;
    }

    // 第二步：UTF-16 -> UTF-8
    // 计算所需的 UTF-8 字节数
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pWBuf, -1, NULL, 0, NULL, NULL);
    
    if (utf8Len == 0)
    {
        delete[] pWBuf;
        return;
    }

    // 检查缓冲区是否足够
    if (utf8Len > utf8Size)
    {
        delete[] pWBuf;
        return;
    }

    // 执行 UTF-16 到 UTF-8 的转换
    WideCharToMultiByte(CP_UTF8, 0, pWBuf, -1, utf8Str, utf8Size, NULL, NULL);

    // 清理临时内存
    delete[] pWBuf;
#endif
}

// 使用密码解密文件，返回解密后的字符串内容
std::string CUtils::DecryptFileWithPassword(const KString& filePath, const std::string& password)
{
    // 1. 读取加密文件内容
    std::ifstream file(filePath.c_str(), std::ios::binary);
    if (!file.is_open())
    {
        return "";
    }

    std::istreambuf_iterator<char> dataBegin(file);
    std::istreambuf_iterator<char> dataEnd;
    std::vector<BYTE> encryptedData(dataBegin, dataEnd);
    // std::vector<BYTE> encryptedData((std::istreambuf_iterator<char>(file)),
    //                                 std::istreambuf_iterator<char>());

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
Acad::ErrorStatus CUtils::sendStringToExecute(AcApDocument* pAcTargetDocument,
                                       const ACHAR * pszExecute,
                                       bool bActivate,
                                       bool bWrapUpInactiveDoc,
                                       bool bEchoString)
{
	return acDocManager->sendStringToExecute(pAcTargetDocument, pszExecute, bActivate, bWrapUpInactiveDoc, bEchoString);
}
int CUtils::acutPrintf (const ACHAR *format, ...)
{
	va_list args;
    va_start(args, format);

    ACHAR buffer[4096];
#ifdef _CAD2005
	 _vstprintf(buffer, format, args);
#else
    _vstprintf_s(buffer, _countof(buffer), format, args);
#endif

    va_end(args);

	return ::acutPrintf(buffer);
    
}
#ifdef _CAD2005
void CUtils::safeVsnprintfVC7(char* szBuffer, size_t bufferSize, const char* pszFmt, va_list args)
{
    if (!szBuffer || !pszFmt || bufferSize == 0) {
        return;
    }

    // 预留一个字节给 '\0'
    int result = _vsnprintf(szBuffer, bufferSize - 1, pszFmt, args);

    // _vsnprintf 在 VC7 中：
    // 1. 如果 result >= 0 且 result < bufferSize-1: 正常写入，已含 '\0'
    // 2. 如果 result == -1: 表示输出被截断，缓冲区已满，但'\0'可能未正确放置或取决于具体实现
    //    为了绝对安全，强制在末尾添加 '\0'
    
    szBuffer[bufferSize - 1] = '\0'; 
}
#endif