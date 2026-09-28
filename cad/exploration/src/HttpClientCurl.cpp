#include "StdAfx.h"
#include "HttpClientCurl.h"

#include <vector>
#include <cstring>
#include <curl/curl.h>

// 自动链接 libcurl(如由工程设置链接, 可定义 HTTPCLIENTCURL_NO_AUTOLINK;
// 库名可通过 HTTPCLIENTCURL_LIB_RELEASE / HTTPCLIENTCURL_LIB_DEBUG 覆盖,
// 例如 Makefile.vc 静态库为 libcurl_a.lib)
#if !defined(HTTPCLIENTCURL_NO_AUTOLINK)
#ifndef HTTPCLIENTCURL_LIB_RELEASE
#define HTTPCLIENTCURL_LIB_RELEASE "libcurl.lib"
#endif
#ifndef HTTPCLIENTCURL_LIB_DEBUG
#define HTTPCLIENTCURL_LIB_DEBUG "libcurld.lib"
#endif
#ifdef _DEBUG
#pragma comment(lib, HTTPCLIENTCURL_LIB_DEBUG)
#else
#pragma comment(lib, HTTPCLIENTCURL_LIB_RELEASE)
#endif
#endif

LONG CHttpClientCurl::s_nGlobalInitRef = 0;

// ---------------------------------------------------------------------
// 辅助函数
// ---------------------------------------------------------------------

// TCHAR 串 -> UTF-8 (URL / 头 / body 均以 UTF-8 传给 curl)
static std::string CurlT2Utf8(LPCTSTR pszText)
{
    std::string strResult;
    if (pszText == NULL || *pszText == 0)
        return strResult;

#ifdef _UNICODE
    int cb = ::WideCharToMultiByte(CP_UTF8, 0, pszText, -1, NULL, 0, NULL, NULL);
    if (cb <= 1)
        return strResult;

    std::vector<char> buf(cb);
    ::WideCharToMultiByte(CP_UTF8, 0, pszText, -1, &buf[0], cb, NULL, NULL);
    strResult.assign(&buf[0], (size_t)(cb - 1));
#else
    // ANSI 工程: 直接按本地编码传递(ASCII URL 不受影响)
    strResult.assign(pszText);
#endif
    return strResult;
}

// curl 错误串(ASCII) -> CString
static CString CurlErr2T(const char* pszErr)
{
    CString strResult;
    if (pszErr == NULL)
        return strResult;

#ifdef _UNICODE
    int n = (int)::strlen(pszErr);
    int cw = ::MultiByteToWideChar(CP_UTF8, 0, pszErr, n, NULL, 0);
    if (cw > 0)
    {
        LPWSTR pwsz = strResult.GetBuffer(cw);
        ::MultiByteToWideChar(CP_UTF8, 0, pszErr, n, pwsz, cw);
        strResult.ReleaseBuffer(cw);
    }
#else
    strResult = pszErr;
#endif
    return strResult;
}

// curl 写数据回调: 把响应体追加到 std::string
static size_t CurlWriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::string* pStr = (std::string*)userdata;
    size_t nTotal = size * nmemb;
    if (pStr != NULL && nTotal > 0)
        pStr->append((const char*)ptr, nTotal);
    return nTotal;
}

// 把头映射组装为 curl_slist(返回 NULL 表示无自定义头)
static struct curl_slist* CurlBuildHeaderList(const std::map<CString, CString>& mapHeaders)
{
    struct curl_slist* pList = NULL;

    std::map<CString, CString>::const_iterator it = mapHeaders.begin();
    for (; it != mapHeaders.end(); ++it)
    {
        std::string strLine = CurlT2Utf8(it->first);
        strLine += ": ";
        strLine += CurlT2Utf8(it->second);

        struct curl_slist* pNew = curl_slist_append(pList, strLine.c_str());
        if (pNew == NULL)
        {
            // 追加失败(内存不足), 释放已建立的列表
            if (pList != NULL)
                curl_slist_free_all(pList);
            return NULL;
        }
        pList = pNew;
    }
    return pList;
}

// ---------------------------------------------------------------------
// 构造 / 析构 / 全局初始化
// ---------------------------------------------------------------------

CHttpClientCurl::CHttpClientCurl()
    : m_dwConnectTimeout(60000)
    , m_dwSendTimeout(60000)
    , m_dwReceiveTimeout(60000)
    , m_dwTotalTimeout(0)
    , m_bVerifyPeer(FALSE)
    , m_bVerifyHost(FALSE)
{
}

CHttpClientCurl::~CHttpClientCurl()
{
    // 异步工作线程只使用自己的参数拷贝, 这里无需等待
}

BOOL CHttpClientCurl::Initialize(LPCTSTR pszUserAgent)
{
    if (pszUserAgent != NULL && *pszUserAgent != 0)
        m_strUserAgent = pszUserAgent;
    else
        m_strUserAgent = _T("Curl-HttpClient/1.0");

    // curl_global_init 非线程安全, 必须在创建任何线程前(主线程)调用
    if (::InterlockedIncrement(&s_nGlobalInitRef) == 1)
    {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        {
            ::InterlockedDecrement(&s_nGlobalInitRef);
            m_strLastError = _T("curl_global_init failed");
            return FALSE;
        }
    }
    return TRUE;
}

void CHttpClientCurl::Cleanup()
{
    if (s_nGlobalInitRef > 0)
    {
        if (::InterlockedDecrement(&s_nGlobalInitRef) == 0)
            curl_global_cleanup();
    }
    m_mapHeaders.clear();
    m_strLastError.Empty();
}

// ---------------------------------------------------------------------
// 请求头 / 超时 / SSL 选项
// ---------------------------------------------------------------------

void CHttpClientCurl::SetHeader(LPCTSTR pszName, LPCTSTR pszValue)
{
    if (pszName == NULL || pszValue == NULL)
        return;
    m_mapHeaders[pszName] = pszValue;
}

void CHttpClientCurl::ClearHeaders()
{
    m_mapHeaders.clear();
}

void CHttpClientCurl::SetTimeout(DWORD dwConnect, DWORD dwSend, DWORD dwReceive)
{
    m_dwConnectTimeout = dwConnect;
    m_dwSendTimeout    = dwSend;
    m_dwReceiveTimeout = dwReceive;
}

void CHttpClientCurl::SetTotalTimeout(DWORD dwTotalMs)
{
    m_dwTotalTimeout = dwTotalMs;
}

void CHttpClientCurl::SetVerifyPeer(BOOL bVerify)
{
    m_bVerifyPeer = bVerify;
}

void CHttpClientCurl::SetVerifyHost(BOOL bVerify)
{
    m_bVerifyHost = bVerify;
}

LPCTSTR CHttpClientCurl::GetLastError() const
{
    return (LPCTSTR)m_strLastError;
}

// 用当前成员设置填充请求参数
void CHttpClientCurl::SnapshotSettings(RequestData& req) const
{
    req.mapHeaders       = m_mapHeaders;
    req.strUserAgent     = m_strUserAgent.IsEmpty() ? CString(_T("Curl-HttpClient/1.0"))
                                                    : m_strUserAgent;
    req.dwConnectTimeout = m_dwConnectTimeout;
    req.dwSendTimeout    = m_dwSendTimeout;
    req.dwReceiveTimeout = m_dwReceiveTimeout;
    req.dwTotalTimeout   = m_dwTotalTimeout;
    req.bVerifyPeer      = m_bVerifyPeer;
    req.bVerifyHost      = m_bVerifyHost;
}

// ---------------------------------------------------------------------
// 核心: 执行一次请求
// ---------------------------------------------------------------------

BOOL CHttpClientCurl::PerformRequest(const RequestData& req,
                                     std::string& strResponse,
                                     DWORD& dwStatusCode,
                                     CString& strError)
{
    strResponse.clear();
    dwStatusCode = 0;
    strError.Empty();

    // URL 与 UA 需要 UTF-8, 且在 perform 期间保持有效
    std::string strUrlUtf8 = CurlT2Utf8(req.strUrl);
    std::string strAgentUtf8 = CurlT2Utf8(req.strUserAgent);

    if (strUrlUtf8.empty())
    {
        strError = _T("URL is empty");
        return FALSE;
    }

    // 每个请求使用独立 easy handle(线程安全, 用完即释放)
    CURL* pCurl = curl_easy_init();
    if (pCurl == NULL)
    {
        strError = _T("curl_easy_init failed");
        return FALSE;
    }

    struct curl_slist* pHeaderList = CurlBuildHeaderList(req.mapHeaders);

    // ---- 基本选项 ----
    curl_easy_setopt(pCurl, CURLOPT_URL, strUrlUtf8.c_str());
    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &strResponse);
    curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1L);        // 多线程环境必设
    curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);  // 跟随 3xx 跳转
    curl_easy_setopt(pCurl, CURLOPT_MAXREDIRS, 5L);

    if (!strAgentUtf8.empty())
        curl_easy_setopt(pCurl, CURLOPT_USERAGENT, strAgentUtf8.c_str());

    // ---- 超时 ----
    curl_easy_setopt(pCurl, CURLOPT_CONNECTTIMEOUT_MS, (long)req.dwConnectTimeout);

    if (req.dwTotalTimeout > 0)
        curl_easy_setopt(pCurl, CURLOPT_TIMEOUT_MS, (long)req.dwTotalTimeout);

    // curl 没有独立的收/发超时, 用低速中止近似:
    // 速度低于 1 字节/秒并持续 n 秒即中止
    DWORD dwStall = (req.dwSendTimeout > req.dwReceiveTimeout) ? req.dwSendTimeout
                                                               : req.dwReceiveTimeout;
    if (dwStall > 0)
    {
        long lStallSec = (long)(dwStall / 1000);
        if (lStallSec > 0)
        {
            curl_easy_setopt(pCurl, CURLOPT_LOW_SPEED_LIMIT, 1L);
            curl_easy_setopt(pCurl, CURLOPT_LOW_SPEED_TIME, lStallSec);
        }
    }

    // ---- SSL (HTTPS) ----
    curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYPEER, req.bVerifyPeer ? 1L : 0L);
    curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYHOST, req.bVerifyHost ? 2L : 0L);

    // ---- POST ----
    if (req.bHasBody)
    {
        curl_easy_setopt(pCurl, CURLOPT_POST, 1L);
        // CURLOPT_POSTFIELDS 不复制数据, req.strBody 在 perform 期间始终有效
        curl_easy_setopt(pCurl, CURLOPT_POSTFIELDS, req.strBody.c_str());
        curl_easy_setopt(pCurl, CURLOPT_POSTFIELDSIZE, (long)req.strBody.length());
    }

    // ---- 自定义请求头 ----
    if (pHeaderList != NULL)
        curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, pHeaderList);

    // ---- 执行 ----
    CURLcode res = curl_easy_perform(pCurl);

    BOOL bSuccess = FALSE;
    if (res == CURLE_OK)
    {
        long lCode = 0;
        curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &lCode);
        dwStatusCode = (DWORD)lCode;
        bSuccess = TRUE;
    }
    else
    {
        CString strTmp;
        strTmp.Format(_T("curl error %d: "), (int)res);
        strError = strTmp + CurlErr2T(curl_easy_strerror(res));
    }

    // ---- 清理 ----
    if (pHeaderList != NULL)
        curl_slist_free_all(pHeaderList);
    curl_easy_cleanup(pCurl);

    return bSuccess;
}

// ---------------------------------------------------------------------
// 同步请求
// ---------------------------------------------------------------------

BOOL CHttpClientCurl::Get(LPCTSTR pszUrl, std::string& strResponse, DWORD& dwStatusCode)
{
    if (pszUrl == NULL)
        return FALSE;

    // 确保全局初始化(幂等)
    if (!Initialize())
        return FALSE;

    RequestData req;
    req.strUrl          = pszUrl;
    req.bHasBody        = FALSE;
    SnapshotSettings(req);

    m_strLastError.Empty();
    BOOL bOK = PerformRequest(req, strResponse, dwStatusCode, m_strLastError);
    return bOK;
}

BOOL CHttpClientCurl::Post(LPCTSTR pszUrl, const std::string& strBody,
                           std::string& strResponse, DWORD& dwStatusCode)
{
    if (pszUrl == NULL)
        return FALSE;

    // 确保全局初始化(幂等)
    if (!Initialize())
        return FALSE;

    RequestData req;
    req.strUrl          = pszUrl;
    req.bHasBody        = TRUE;
    req.strBody         = strBody;
    SnapshotSettings(req);

    m_strLastError.Empty();
    BOOL bOK = PerformRequest(req, strResponse, dwStatusCode, m_strLastError);
    return bOK;
}

// ---------------------------------------------------------------------
// 异步请求
// ---------------------------------------------------------------------

UINT CHttpClientCurl::AsyncThreadProc(LPVOID pParam)
{
    AsyncData* pData = (AsyncData*)pParam;
    if (pData == NULL)
        return 1;

    // 工作线程只使用参数拷贝, 不访问任何对象成员(线程安全)
    std::string strResponse;
    DWORD dwStatusCode = 0;
    CString strError;

    BOOL bSuccess = PerformRequest(pData->req, strResponse, dwStatusCode, strError);

    if (pData->pCallback != NULL)
    {
        // 注意: 此回调运行在工作线程中, 不要直接操作 UI
        pData->pCallback(bSuccess, strResponse, dwStatusCode, pData->pUserData);
    }

    delete pData;
    return 0;
}

BOOL CHttpClientCurl::StartAsync(LPCTSTR pszUrl,
                                 const std::string* pBody,
                                 ResponseCallback pCallback,
                                 LPVOID pUserData)
{
    if (pszUrl == NULL || pCallback == NULL)
        return FALSE;

    // 确保全局初始化在调用线程中完成(幂等)
    if (!Initialize())
        return FALSE;

    AsyncData* pData = new AsyncData;
    pData->req.strUrl     = pszUrl;
    pData->req.bHasBody   = (pBody != NULL);
    if (pBody != NULL)
        pData->req.strBody = *pBody;
    SnapshotSettings(pData->req);

    pData->pCallback  = pCallback;
    pData->pUserData  = pUserData;

    // 挂起方式创建, 失败时可安全回收数据
    CWinThread* pThread = AfxBeginThread(AsyncThreadProc, pData,
                                         THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    if (pThread == NULL)
    {
        delete pData;
        m_strLastError = _T("AfxBeginThread failed");
        return FALSE;
    }
    pThread->m_bAutoDelete = TRUE;
    pThread->ResumeThread();

    return TRUE;
}

BOOL CHttpClientCurl::GetAsync(LPCTSTR pszUrl, ResponseCallback pCallback, LPVOID pUserData)
{
    return StartAsync(pszUrl, NULL, pCallback, pUserData);
}

BOOL CHttpClientCurl::PostAsync(LPCTSTR pszUrl, const std::string& strBody,
                                ResponseCallback pCallback, LPVOID pUserData)
{
    return StartAsync(pszUrl, &strBody, pCallback, pUserData);
}
