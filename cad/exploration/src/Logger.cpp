// coding: gb18030
#include "stdafx.h"
#include "Logger.h"
#include <ctime>
#include <cstdarg>
#include <sys/stat.h>
#include "Utils.h"

// 日志文件最大保留天数（0表示不清理）
#define MAX_LOG_RETAIN_DAYS 0

CLogger& CLogger::GetInstance()
{
    static CLogger instance;
    return instance;
}

CLogger::CLogger(void)
    : m_nLevel(LOG_LEVEL_INFO)
{
}

CLogger::~CLogger(void)
{
    if (m_ofs.is_open())
    {
        m_ofs.close();
    }
}

void CLogger::Init(const CString& strPrefix, const CString& strDir /*= _T("")*/)
{
    m_strPrefix = strPrefix;

    if (strDir.IsEmpty())
    {
        // 默认使用程序目录下的 logs 文件夹
		CString strModulePath = CUtils::GetArxFolder();
        int nPos = strModulePath.ReverseFind(_T('\\'));
        if (nPos >= 0)
        {
            m_strDir = strModulePath.Left(nPos + 1) + _T("logs\\");
        }
        else
        {
            m_strDir = _T("logs\\");
        }
    }
    else
    {
        m_strDir = strDir;
        // 确保以 \\ 结尾
        if (m_strDir.Right(1) != _T("\\"))
        {
            m_strDir += _T("\\");
        }
    }

    EnsureDirectory(m_strDir);

    // 首次打开文件
    CheckAndRotateFile();
}

void CLogger::SetLevel(LogLevel nLevel)
{
    m_nLevel = nLevel;
}



void CLogger::CheckAndRotateFile()
{
    CString strToday = GetCurrentDateString();

    if (m_strCurrentDate != strToday || !m_ofs.is_open())
    {
        // 日期变化或文件未打开，需要创建新文件
        if (m_ofs.is_open())
        {
            m_ofs.close();
        }

        m_strCurrentDate = strToday;

        // 文件名格式: 前缀_YYYY-MM-DD.log
        m_strFilePath = m_strDir + m_strPrefix + _T("_") + strToday + _T(".log");

        // 以追加模式打开
        CStringA strFilePathA(m_strFilePath);
        m_ofs.open(strFilePathA.GetString(), std::ios::app | std::ios::out);

        if (m_ofs.is_open())
        {
            // 写入分隔线
            CString strHeader;
            strHeader.Format(_T("======== Log Start: %s ========\n"), strToday);
            CStringA strHeaderA(strHeader);
            m_ofs << strHeaderA.GetString();
            m_ofs.flush();
        }
    }
}

CString CLogger::GetCurrentDateString() const
{
    time_t tNow = time(NULL);
    struct tm tmNow;
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    localtime_s(&tmNow, &tNow);
#else
    tmNow = *localtime(&tNow);
#endif

    CString strDate;
    strDate.Format(_T("%04d-%02d-%02d"),
                   tmNow.tm_year + 1900,
                   tmNow.tm_mon + 1,
                   tmNow.tm_mday);

    return strDate;
}

CString CLogger::GetCurrentTimeString() const
{
    time_t tNow = time(NULL);
    struct tm tmNow;
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    localtime_s(&tmNow, &tNow);
#else
    tmNow = *localtime(&tNow);
#endif

    CString strTime;
    strTime.Format(_T("%04d-%02d-%02d %02d:%02d:%02d"),
                   tmNow.tm_year + 1900,
                   tmNow.tm_mon + 1,
                   tmNow.tm_mday,
                   tmNow.tm_hour,
                   tmNow.tm_min,
                   tmNow.tm_sec);

    return strTime;
}

LPCTSTR CLogger::LevelToString(LogLevel nLevel) const
{
    switch (nLevel)
    {
    case LOG_LEVEL_DEBUG:   return _T("DEBUG");
    case LOG_LEVEL_INFO:    return _T("INFO");
    case LOG_LEVEL_WARNING: return _T("WARN");
    case LOG_LEVEL_ERROR:   return _T("ERROR");
    default:                return _T("UNKNOWN");
    }
}

void CLogger::EnsureDirectory(const CString& strDir)
{
    // 逐级创建目录
    CString strPath = strDir;
    strPath.TrimRight(_T('\\'));

    int nPos = 0;
    CString strPart;

    while ((nPos = strPath.Find(_T('\\'))) >= 0)
    {
        strPart += strPath.Left(nPos + 1);
        strPath = strPath.Mid(nPos + 1);

        // 去掉末尾的 '\'
        CString strDirToCreate = strPart;
        strDirToCreate.TrimRight(_T('\\'));

        if (!strDirToCreate.IsEmpty())
        {
            ::CreateDirectory(strDirToCreate, NULL);
        }
    }

    // 创建最后一部分
    if (!strPath.IsEmpty())
    {
        strPart += strPath;
        ::CreateDirectory(strPart, NULL);
    }
}
void CLogger::LogUtf8(LogLevel nLevel, const char* szBuffer)
{
    if (nLevel < m_nLevel)
    {
        return;
    }

    CSingleLock lock(&m_cs, TRUE);
    CheckAndRotateFile();

    if (!m_ofs.is_open())
    {
        return;
    }

    // 时间戳和级别直接格式化为 CStringA (ASCII兼容，无需转码)
    CStringA strTimeA = CT2A(GetCurrentTimeString());
    CStringA strLevelA = CT2A(LevelToString(nLevel));

    CStringA strLine;
    strLine.Format("[%s] [%s] %s\n", strTimeA, strLevelA, szBuffer);

    // 直接写入文件，无需二次转换
    m_ofs << strLine.GetString();
    m_ofs.flush();

    // 调试窗口输出（需要将 UTF-8 转回系统需要的格式）
#ifdef _UNICODE
    int nLen = MultiByteToWideChar(CP_UTF8, 0, strLine, -1, NULL, 0);
    if (nLen > 0)
    {
        wchar_t* pWBuf = new wchar_t[nLen];
        MultiByteToWideChar(CP_UTF8, 0, strLine, -1, pWBuf, nLen);
        OutputDebugStringW(pWBuf);
        delete[] pWBuf;
    }
#else
    // 非 Unicode 模式下转为 ANSI 以便在调试器正确显示
    int nLenW = MultiByteToWideChar(CP_UTF8, 0, strLine, -1, NULL, 0);
    if (nLenW > 0)
    {
        wchar_t* pWBuf = new wchar_t[nLenW];
        MultiByteToWideChar(CP_UTF8, 0, strLine, -1, pWBuf, nLenW);
        
        int nLenA = WideCharToMultiByte(CP_ACP, 0, pWBuf, -1, NULL, 0, NULL, NULL);
        if (nLenA > 0)
        {
            char* pABuf = new char[nLenA];
            WideCharToMultiByte(CP_ACP, 0, pWBuf, -1, pABuf, nLenA, NULL, NULL);
            OutputDebugStringA(pABuf);
            delete[] pABuf;
        }
        delete[] pWBuf;
    }
#endif
}

void CLogger::Log(LogLevel nLevel, const char* pszFmt, ...)
{
    if (nLevel < m_nLevel)
        return;

    char szBuffer[4096] = { 0 };
    va_list args;
    va_start(args, pszFmt);
#ifdef _CAD2005
	CUtils::safeVsnprintfVC7(szBuffer, sizeof(szBuffer), pszFmt, args);
#else
    vsnprintf_s(szBuffer, 4096, _TRUNCATE, pszFmt, args);
#endif
    va_end(args);

    LogUtf8(nLevel, szBuffer);
}

void CLogger::Debug(const char* pszFmt, ...)
{
    if (LOG_LEVEL_DEBUG < m_nLevel)
        return;

    char szBuffer[4096] = { 0 };
    va_list args;
    va_start(args, pszFmt);
#ifdef _CAD2005
	CUtils::safeVsnprintfVC7(szBuffer, sizeof(szBuffer), pszFmt, args);
#else
    vsnprintf_s(szBuffer, 4096, _TRUNCATE, pszFmt, args);
#endif
    va_end(args);

    LogUtf8(LOG_LEVEL_DEBUG, szBuffer);
}
void CLogger::Debug(const std::string& s)
{
    if (LOG_LEVEL_DEBUG < m_nLevel)
        return;

    LogUtf8(LOG_LEVEL_DEBUG, s.c_str());
}
void CLogger::Info(const char* pszFmt, ...)
{
    if (LOG_LEVEL_INFO <  m_nLevel)
        return;

    char szBuffer[4096] = { 0 };
    va_list args;
    va_start(args, pszFmt);
#ifdef _CAD2005
	CUtils::safeVsnprintfVC7(szBuffer, sizeof(szBuffer), pszFmt, args);
#else
    vsnprintf_s(szBuffer, 4096, _TRUNCATE, pszFmt, args);
#endif
    va_end(args);

    LogUtf8(LOG_LEVEL_INFO, szBuffer);
}

void CLogger::Warning(const char* pszFmt, ...)
{
    if (LOG_LEVEL_WARNING < m_nLevel)
        return;

    char szBuffer[4096] = { 0 };
    va_list args;
    va_start(args, pszFmt);
#ifdef _CAD2005
	CUtils::safeVsnprintfVC7(szBuffer, sizeof(szBuffer), pszFmt, args);
#else
    vsnprintf_s(szBuffer, 4096, _TRUNCATE, pszFmt, args);
#endif
    va_end(args);

    LogUtf8(LOG_LEVEL_WARNING, szBuffer);
}

void CLogger::Error(const char* pszFmt, ...)
{
    if (LOG_LEVEL_ERROR < m_nLevel)
        return;

    char szBuffer[4096] = { 0 };
    va_list args;
    va_start(args, pszFmt);
#ifdef _CAD2005
	CUtils::safeVsnprintfVC7(szBuffer, sizeof(szBuffer), pszFmt, args);
#else
    vsnprintf_s(szBuffer, 4096, _TRUNCATE, pszFmt, args);
#endif
    va_end(args);

    LogUtf8(LOG_LEVEL_ERROR, szBuffer);
}

#if defined(_UNICODE) || defined(UNICODE)

void CLogger::Log(LogLevel nLevel, const TCHAR* pszFmt, ...)
{ 
    if (nLevel < m_nLevel)
    {
        return;
    }

    CSingleLock lock(&m_cs, TRUE);

    // 检查是否需要切换日期文件
    CheckAndRotateFile();

    if (!m_ofs.is_open())
    {
        return;
    }

    // 格式化用户消息
    wchar_t szBuffer[4096] = {0};
    va_list args;
    va_start(args, pszFmt);
	_vstprintf_s(szBuffer, 4096, pszFmt, args);

    va_end(args);

    // 写入时间戳和级别
    CString strTime = GetCurrentTimeString();
    CString strLine;
    strLine.Format(_T("[%s] [%s] %s\n"),
                   strTime,
                   LevelToString(nLevel),
                   szBuffer);

    // 写入文件（UTF-8 格式）
	m_ofs << CUtils::CStringToUTF8(strLine);
    m_ofs.flush();

    // 同时输出到调试窗口
    OutputDebugString(strLine);

    // 控制台输出也可以选做
    // _tprintf(strLine);
}

void CLogger::Debug(const TCHAR* pszFmt, ...)
{
    if (LOG_LEVEL_DEBUG < m_nLevel)
        return;

    va_list args;
    va_start(args, pszFmt);

    TCHAR szBuffer[4096] = {0};
#if defined(_UNICODE) || defined(UNICODE)
    _vstprintf_s(szBuffer, 4096, pszFmt, args);
#else
    _vstprintf(szBuffer, pszFmt, args);
#endif
    va_end(args);

    Log(LOG_LEVEL_DEBUG, _T("%s"), szBuffer);
}

void CLogger::Info(const TCHAR* pszFmt, ...)
{
    if (LOG_LEVEL_INFO < m_nLevel)
        return;

    va_list args;
    va_start(args, pszFmt);

    TCHAR szBuffer[4096] = {0};
#if defined(_UNICODE) || defined(UNICODE)
    _vstprintf_s(szBuffer, 4096, pszFmt, args);
#else
    _vstprintf(szBuffer, pszFmt, args);
#endif
    va_end(args);

    Log(LOG_LEVEL_INFO, _T("%s"), szBuffer);
}

void CLogger::Warning(const TCHAR* pszFmt, ...)
{
    if (LOG_LEVEL_WARNING < m_nLevel)
        return;

    va_list args;
    va_start(args, pszFmt);

    TCHAR szBuffer[4096] = {0};
#if defined(_UNICODE) || defined(UNICODE)
    _vstprintf_s(szBuffer, 4096, pszFmt, args);
#else
    _vstprintf(szBuffer, pszFmt, args);
#endif
    va_end(args);

    Log(LOG_LEVEL_WARNING, _T("%s"), szBuffer);
}

void CLogger::Error(const TCHAR* pszFmt, ...)
{
    if (LOG_LEVEL_ERROR < m_nLevel)
        return;

    va_list args;
    va_start(args, pszFmt);

    TCHAR szBuffer[4096] = {0};
#if defined(_UNICODE) || defined(UNICODE)
    _vstprintf_s(szBuffer, 4096, pszFmt, args);
#else
    _vstprintf(szBuffer, pszFmt, args);
#endif
    va_end(args);

    Log(LOG_LEVEL_ERROR, _T("%s"), szBuffer);
}


#endif