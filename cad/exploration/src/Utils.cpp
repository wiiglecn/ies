#include "StdAfx.h"
#include "Utils.h"
#include "arxHeaders.h"
#include <wincrypt.h>
#include <string>
#include <fstream>
#include <vector>
#include "cJSON.h"

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

// Initialize static member
cJSON* CUtils::s_pJsonConfig = NULL;

CUtils::CUtils(void)
{
}

CUtils::~CUtils(void)
{
}
bool CUtils::checkEmpty(const TCHAR* str)
{
	return str == NULL || str[0] == _T('\0');
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
        /*if (acedGetAcadFrame())
            ::SetFocus(acedGetAcadFrame()->m_hWnd);*/
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
            outBuf[0] = _T('\0');
        return false;
    }

    // 初始化输出缓冲区为空
    outBuf[0] = _T('\0');

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
            utf8Str[0] = '\0';
        return;
    }

    // 初始化输出缓冲区
    utf8Str[0] = '\0';

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

//-----------------------------------------------------------------------------
// 块名中的非法字符（AutoCAD 符号表命名限制）
bool CUtils::IsIllegalBlockNameChar(ACHAR ch)
{
    //  > / \ " : ; ? * | = ' 以及控制字符
    if (ch < 0x20) return true;
    const ACHAR* pIllegal = _T(">/\\\":;?*|='");
    while (*pIllegal)
    {
        if (ch == *pIllegal) return true;
        ++pIllegal;
    }
    return false;
}

//-----------------------------------------------------------------------------
// 简单字符串拷贝（避免 VC8 安全 CRT 警告）
void CUtils::SafeCopy(ACHAR* pszDst, int nDstCount, const ACHAR* pszSrc, int nCount)
{
    int i = 0;
    for (; i <  nCount && i < nDstCount - 1; ++i)
        pszDst[i] = pszSrc[i];
    pszDst[i] = _T('\0');
}
// 路径归一化：统一大小写和分隔符，否则 "a/dwg" 和 "A\dwg" 会被当成两个键
CString CUtils::NormalizeDwgPath(const ACHAR* pszDwgPath)
{
    CString s(pszDwgPath);
    s.MakeLower();                 // Windows 路径不区分大小写（中文不受影响）
    s.Replace(_T('/'), _T('\\'));  // Execute() 里用 '/'，readDwgFile 里可能是 '\'
    return s;
}

void CUtils::IntToAChar(int nValue, ACHAR* pszBuf, int nBufCount)
{
    if (pszBuf == NULL || nBufCount <= 0)
        return;

#ifdef _CAD2005
    _stprintf(pszBuf, _T("%d"), nValue);
#else
    _stprintf_s(pszBuf, nBufCount, _T("%d"), nValue);
#endif
}
int CUtils::SplitUtf8(const char* utf8Str, char sep, std::vector<CString>& outArr)
{
    outArr.clear();
    if (utf8Str == NULL)
        return 0;

    const char* p = utf8Str;
    for (;;)
    {
        const char* pSep = strchr(p, sep);
        size_t nLen = (pSep != NULL) ? (size_t)(pSep - p) : strlen(p);

        // 拷到局部缓冲并保证 NUL 结尾（utf8ToTChar 的要求）
        char szSeg[256];
        if (nLen >= sizeof(szSeg))
            nLen = sizeof(szSeg) - 1;
        memcpy(szSeg, p, nLen);
        szSeg[nLen] = '\0';

        TCHAR buf[512];
        CUtils::utf8ToTChar(szSeg, buf, ARRAYSIZE(buf));
        outArr.push_back(CString(buf));

        if (pSep == NULL)
            break;
        p = pSep + 1;
    }
    return (int)outArr.size();
}
// 按名称查文字样式 ID（找不到返回 kNull）
AcDbObjectId CUtils::FindTextStyle(const ACHAR* pszStyleName)
{
    AcDbObjectId styleId = AcDbObjectId::kNull;
    AcDbTextStyleTable* pTsTbl = NULL;
    if (acdbHostApplicationServices()->workingDatabase()
            ->getTextStyleTable(pTsTbl, AcDb::kForRead) == Acad::eOk)
    {
        pTsTbl->getAt(pszStyleName, styleId);
        pTsTbl->close();
    }
    return styleId;
}

// 在指定位置创建单行文本并设置样式
AcDbObjectId CUtils::AddDbText(AcDbBlockTableRecord* container, const ACHAR* pszText, const AcGePoint3d& pt,
                               double dHeight, const ACHAR* pszStyleName, int hAlign,double dWidthFactor)
{
    AcDbText* pText = new AcDbText;
    pText->setPosition(pt);
    pText->setTextString(pszText);
    pText->setRotation(0.0);
    

    if (pszStyleName != NULL)
    {
        AcDbObjectId styleId = FindTextStyle(pszStyleName);
        if (!styleId.isNull())
            pText->setTextStyle(styleId);
	}

	pText->setHeight(dHeight);
	pText->setWidthFactor(dWidthFactor);

    // ★ 对齐：先设模式，再设对齐点（顺序不能反）
    switch (hAlign)
    {
    case DBTA_CENTER:
        pText->setHorizontalMode(AcDb::kTextMid);
        break;
    case DBTA_RIGHT:
        pText->setHorizontalMode(AcDb::kTextRight);
        break;
    case DBTA_LEFT:
    default:
        pText->setHorizontalMode(AcDb::kTextLeft);
        break;
    }
    pText->setAlignmentPoint(pt);   // 左对齐时被忽略（用 position），其他模式接管定位

    AcDbObjectId textId = AcDbObjectId::kNull;
    Acad::ErrorStatus es = container->appendAcDbEntity(textId, pText);
    if (es == Acad::eOk)
        pText->close();
    else
        delete pText;
    return textId;
}
AcDbObjectId CUtils::AddDbText(AcDbBlockTableRecord* container,const ACHAR* pszText, const AcGePoint3d& pt,
		AcDbObjectId styleId,int hAlign)
{
	AcDbText* pText = new AcDbText;
    pText->setPosition(pt);
    pText->setTextString(pszText);
    pText->setRotation(0.0);
	pText->setTextStyle(styleId);

    // ★ 对齐：先设模式，再设对齐点（顺序不能反）
    switch (hAlign)
    {
    case DBTA_CENTER:
        pText->setHorizontalMode(AcDb::kTextMid);
        break;
    case DBTA_RIGHT:
        pText->setHorizontalMode(AcDb::kTextRight);
        break;
    case DBTA_LEFT:
    default:
        pText->setHorizontalMode(AcDb::kTextLeft);
        break;
    }
    pText->setAlignmentPoint(pt);   // 左对齐时被忽略（用 position），其他模式接管定位

    AcDbObjectId textId = AcDbObjectId::kNull;
    Acad::ErrorStatus es = container->appendAcDbEntity(textId, pText);
    if (es == Acad::eOk)
        pText->close();
    else
        delete pText;
    return textId;
}
// 获取实体几何包围盒的宽和高（世界坐标系下的包围盒尺寸）
bool CUtils::GetEntExtent(AcDbEntity* pEnt, double& dWidth, double& dHeight)
{
    dWidth  = 0.0;
    dHeight = 0.0;
    if (pEnt == NULL)
        return false;

    AcDbExtents extents;
    Acad::ErrorStatus es = pEnt->getGeomExtents(extents);
    if (es != Acad::eOk)
        return false;

    AcGePoint3d minPt = extents.minPoint();
    AcGePoint3d maxPt = extents.maxPoint();
    dWidth  = maxPt.x - minPt.x;
    dHeight = maxPt.y - minPt.y;
    return true;
}
// 按实体 ID 取几何包围盒宽高（世界坐标系）；失败返回 false
bool CUtils::GetEntExtentById(const AcDbObjectId& id, double& dWidth, double& dHeight)
{
    dWidth = 0.0;
    dHeight = 0.0;
    AcDbEntity* pEnt = OpenEntityById(id, AcDb::kForRead);
    if (pEnt == NULL)
        return false;
    bool bOk = GetEntExtent(pEnt, dWidth, dHeight);
    pEnt->close();
    return bOk;
}
double CUtils::CStringToDouble(const CString& str)
{
    if (str.IsEmpty())
        return 0.0;

    // _tstof 会自动根据 Unicode/ANSI 设置选择正确的函数
    // 在 ANSI 模式下等价于 atof(str)
    // 在 Unicode 模式下等价于 _wtof(str)
    return _tstof(str);
}
AcDbObjectId CUtils::AddDbMText(AcDbBlockTableRecord* container, const ACHAR* pszText, const AcGePoint3d& pt,
                                double dHeight, double dWidth,const ACHAR* pszStyleName,
                                AcDbMText::AttachmentPoint attach)
{
    if (container == NULL)
        return AcDbObjectId::kNull;

    AcDbMText* pMText = new AcDbMText;
    pMText->setLocation(pt);

    // 样式：先设样式再设高度（样式里的固定字高会覆盖）
    if (pszStyleName != NULL)
    {
        AcDbObjectId styleId = FindTextStyle(pszStyleName);
        if (!styleId.isNull())
            pMText->setTextStyle(styleId);
    }
    pMText->setTextHeight(dHeight);

    // 内容：MText 把 \n 当普通字符，换行必须是 \P —— 先替换，JSON 里的真实换行才能生效
    CString sContents(pszText != NULL ? pszText : _T(""));
    sContents.Replace(_T("\r\n"), _T("\\P"));
    sContents.Replace(_T("\n"), _T("\\P"));
    sContents.Replace(_T("\r"), _T("\\P"));
    pMText->setContents(sContents.GetString());

    // ★ 宽度必须在 setContents 之后设：设置宽度会触发重新排版折行
    if (dWidth > 0.0)
        pMText->setWidth(dWidth);

    pMText->setAttachment(attach);     // location 指文本框的哪个角/边
    pMText->setRotation(0.0);

    AcDbObjectId textId = AcDbObjectId::kNull;
    Acad::ErrorStatus es = container->appendAcDbEntity(textId, pMText);
    if (es == Acad::eOk)
        pMText->close();               // 项目约定：入库成功 close，失败 delete
    else
        delete pMText;
    return textId;
}
// 读取整个文件内容并以 cJSON 解析；任何一步失败返回 NULL
cJSON* CUtils::LoadJsonFile(const TCHAR* pszFilePath, bool bSilent)
{
    if (pszFilePath == NULL || pszFilePath[0] == _T('\0'))
        return NULL;

    // Read file content
    FILE* fp = _tfopen(pszFilePath, _T("rb"));   // 二进制模式，避免文本模式改动内容
    if (!fp) {
        if (!bSilent)
            CUtils::acutPrintf(_T("[CAD ERROR] 无法打开文件: %s\n"), pszFilePath);
        return NULL;
    }

    // Seek to end to get file size
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize <= 0) {
        fclose(fp);
        return NULL;
    }

    // Allocate buffer and read file
    char* fileContent = (char*)malloc(fileSize + 1);
    if (!fileContent) {
        fclose(fp);
        return NULL;
    }

    size_t readSize = fread(fileContent, 1, fileSize, fp);
    fileContent[readSize] = '\0';
    fclose(fp);

    // Parse JSON
    cJSON* pJson = cJSON_Parse(fileContent);
    free(fileContent);

    if (pJson == NULL && !bSilent) {
        CUtils::acutPrintf(_T("[CAD ERROR] JSON 解析失败: %s\n"), pszFilePath);
        const char* errPtr = cJSON_GetErrorPtr();
        if (errPtr != NULL)
            CUtils::acutPrintf(_T("[CAD ERROR] JSON 出错位置附近: %hs\n"), errPtr);
    }
    return pJson;
}
bool CUtils::FindPatNameBySoilName(cJSON* pPatConfig, const TCHAR* pszSoilName,
                                  TCHAR* pszPatName, int nBufCount)
{
    if (pPatConfig == NULL || pszSoilName == NULL || pszPatName == NULL || nBufCount <= 0)
        return false;
    pszPatName[0] = _T('\0');

    // 查找键（岩性名）转 UTF-8，用于与 JSON 值做字节比较
    char utf8Name[256] = { 0 };
    TCharToUtf8(pszSoilName, utf8Name, sizeof(utf8Name));

    // 两个中文键名也转成 UTF-8
    char utf8KeyName[64], utf8KeyPat[64];
    TCharToUtf8(_T("岩性名称"), utf8KeyName, sizeof(utf8KeyName));
    TCharToUtf8(_T("图例名称"), utf8KeyPat, sizeof(utf8KeyPat));

    cJSON* patRow = NULL;
    cJSON_ArrayForEach(patRow, pPatConfig)
    {
        cJSON* jName = cJSON_GetObjectItem(patRow, utf8KeyName);
        if (jName == NULL || !cJSON_IsString(jName))
            continue;
        if (strcmp(jName->valuestring, utf8Name) != 0)      // UTF-8 字节精确比较
            continue;

        cJSON* jPat = cJSON_GetObjectItem(patRow, utf8KeyPat);
        if (jPat != NULL && cJSON_IsString(jPat))
            utf8ToTChar(jPat->valuestring, pszPatName, nBufCount);  // "gt": UTF-8 → TCHAR
        return (pszPatName[0] != _T('\0'));
    }
    return false;
}
bool CUtils::FindBlockNameByHoleName(cJSON* pPatConfig, const TCHAR* pszHoleName,
                                  TCHAR* pszBlockName, int nBufCount)
{
	if (pPatConfig == NULL || pszHoleName == NULL || pszBlockName == NULL || nBufCount <= 0)
        return false;
    pszBlockName[0] = _T('\0');

    // 查找键（岩性名）转 UTF-8，用于与 JSON 值做字节比较
    char utf8Name[256] = { 0 };
    TCharToUtf8(pszHoleName, utf8Name, sizeof(utf8Name));

    // 两个中文键名也转成 UTF-8
    char utf8KeyName[64], utf8KeyPat[64];
    TCharToUtf8(_T("符号名称"), utf8KeyName, sizeof(utf8KeyName));
    TCharToUtf8(_T("图块名称"), utf8KeyPat, sizeof(utf8KeyPat));

    cJSON* patRow = NULL;
    cJSON_ArrayForEach(patRow, pPatConfig)
    {
        cJSON* jName = cJSON_GetObjectItem(patRow, utf8KeyName);
        if (jName == NULL || !cJSON_IsString(jName))
            continue;
        if (strcmp(jName->valuestring, utf8Name) != 0)      // UTF-8 字节精确比较
            continue;

        cJSON* jPat = cJSON_GetObjectItem(patRow, utf8KeyPat);
        if (jPat != NULL && cJSON_IsString(jPat))
            utf8ToTChar(jPat->valuestring, pszBlockName, nBufCount);  // "gt": UTF-8 → TCHAR
        return (pszBlockName[0] != _T('\0'));
    }
    return false;
}
// JSON 数值兼容读取：number 直接取；字符串转 double 后取（标贯各列可能是数字或字符串）
double CUtils::JsonValToDouble(cJSON* node)
{
    if (node == NULL)
        return 0.0;
    if (cJSON_IsNumber(node))
        return node->valuedouble;
    if (cJSON_IsString(node) && node->valuestring != NULL)
    {
        TCHAR buf[128] = { 0 };
        utf8ToTChar(node->valuestring, buf, ARRAYSIZE(buf));
        return CStringToDouble(buf);
    }
    return 0.0;
}
// 按 id 打开实体；acdbOpenAcDbEntity 自带类型校验（非 AcDbEntity 会失败）
AcDbEntity* CUtils::OpenEntityById(const AcDbObjectId& id, AcDb::OpenMode openMode)
{
    if (id.isNull())
        return NULL;

    AcDbEntity* pEnt = NULL;
    Acad::ErrorStatus es = acdbOpenAcDbEntity(pEnt, id, openMode);
    if (es != Acad::eOk || pEnt == NULL)
        return NULL;
    return pEnt;
}

#pragma message("========== GetJsonConfig compiling ==========")

// Static accessor implementation
cJSON* CUtils::GetJsonConfig()
{
    // 1. Initialize if NULL
    if (!s_pJsonConfig) {
        s_pJsonConfig = cJSON_CreateObject();
    }else{
        return s_pJsonConfig;
    }

    // 2. Check if 'profile' object exists, if not, create it and set defaults
    cJSON* profile = cJSON_GetObjectItem(s_pJsonConfig, "profile");
    
    if (!profile || !cJSON_IsObject(profile)) {
        // Create the profile object
        profile = cJSON_AddObjectToObject(s_pJsonConfig, "profile");
        

        // Add default values
        // Note: cJSON_AddStringToObject creates a copy of the string
        cJSON_AddStringToObject(profile, "hole_diameter", "2");
        cJSON_AddStringToObject(profile, HOLE_DIAMETER_ZK, "130");
    } else {
        // 3. Optional: Check if specific keys inside 'profile' are missing
        // If you want to ensure hole_diameter exists even if 'profile' exists but is empty
        cJSON* holeDiameter = cJSON_GetObjectItem(profile, "hole_diameter");
        if (!holeDiameter || !cJSON_IsString(holeDiameter)) {
            cJSON_AddStringToObject(profile, "hole_diameter", "2");
        }
        cJSON* holeDiameterZK = cJSON_GetObjectItem(profile, HOLE_DIAMETER_ZK);
        if (!holeDiameterZK || !cJSON_IsString(holeDiameterZK)) {
            cJSON_AddStringToObject(profile, HOLE_DIAMETER_ZK, "130");
        }
    }
//#error TEST_GET_JSON_CONFIG
    return s_pJsonConfig;
}
void CUtils::FreeJsonConfig()
{
	if (s_pJsonConfig) {
		cJSON_Delete(s_pJsonConfig);
		s_pJsonConfig = NULL;
	}
}
cJSON* CUtils::GetJsonConfigDirect()
{
    return s_pJsonConfig;
}
cJSON* CUtils::GetJsonObjectItemUtf8(cJSON* root, const TCHAR* key)
{
	if (root == NULL || key == NULL)
        return NULL;

    char utf8Key[256] = {0};
    TCharToUtf8(key, utf8Key, sizeof(utf8Key));

    return cJSON_GetObjectItemCaseSensitive(root, utf8Key);
}