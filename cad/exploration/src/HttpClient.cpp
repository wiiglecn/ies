#include "StdAfx.h"
#include "HttpClient.h"

CHttpClient::CHttpClient()
    : m_pSession(NULL)
    , m_dwConnectTimeout(60000)
    , m_dwSendTimeout(30000)
    , m_dwReceiveTimeout(30000)
{
}

CHttpClient::~CHttpClient()
{
    Cleanup();
}

BOOL CHttpClient::Initialize(LPCTSTR pszUserAgent)
{
    if (m_pSession != NULL)
        return TRUE;

    m_pSession = new CInternetSession(
        pszUserAgent ? pszUserAgent : _T("MFC-HttpClient/1.0"),
        1,
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL, NULL, 0);

    if (m_pSession == NULL)
        return FALSE;

    m_pSession->SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, m_dwConnectTimeout);
    m_pSession->SetOption(INTERNET_OPTION_SEND_TIMEOUT, m_dwSendTimeout);
    m_pSession->SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, m_dwReceiveTimeout);

    return TRUE;
}

void CHttpClient::SetHeader(LPCTSTR pszName, LPCTSTR pszValue)
{
    if (pszName == NULL || pszValue == NULL)
        return;
    m_mapHeaders[pszName] = pszValue;
}

void CHttpClient::ClearHeaders()
{
    m_mapHeaders.clear();
}

void CHttpClient::SetTimeout(DWORD dwConnect, DWORD dwSend, DWORD dwReceive)
{
    m_dwConnectTimeout = dwConnect;
    m_dwSendTimeout    = dwSend;
    m_dwReceiveTimeout = dwReceive;

    if (m_pSession != NULL)
    {
        m_pSession->SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, m_dwConnectTimeout);
        m_pSession->SetOption(INTERNET_OPTION_SEND_TIMEOUT, m_dwSendTimeout);
        m_pSession->SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, m_dwReceiveTimeout);
    }
}

BOOL CHttpClient::ParseUrl(const CString& strUrl,
                          CString& strHost,
                          CString& strPath,
                          INTERNET_PORT& nPort,
                          DWORD& dwServiceType)
{
    DWORD dwFlags = 0;
    // Declare temporary strings for username and password
    CString strUserName;
    CString strPassword;

    // Pass the temporary strings instead of NULL
    return AfxParseURLEx(strUrl, dwServiceType, strHost, strPath, nPort, 
                         strUserName, strPassword, dwFlags) != FALSE;
}


BOOL CHttpClient::DoRequest(const CString& strUrl,
                            const std::string* pBody,
                            const std::map<CString, CString>& headers,
                            std::string& strResponse,
                            DWORD& dwStatusCode)
{
    if (m_pSession == NULL)
    {
        if (!Initialize())
            return FALSE;
    }

    return DoRequestWithSession(m_pSession, strUrl, pBody, headers, strResponse, dwStatusCode);
}

BOOL CHttpClient::DoRequestWithSession(CInternetSession* pSession,
                                       const CString& strUrl,
                                       const std::string* pBody,
                                       const std::map<CString, CString>& headers,
                                       std::string& strResponse,
                                       DWORD& dwStatusCode)
{
    strResponse.clear();
    dwStatusCode = 0;

    if (pSession == NULL)
        return FALSE;

    CString strHost, strPath;
    INTERNET_PORT nPort = 0;
    DWORD dwServiceType = 0;

    if (!ParseUrl(strUrl, strHost, strPath, nPort, dwServiceType))
        return FALSE;

    BOOL bHttps = (dwServiceType == AFX_INET_SERVICE_HTTPS);
    // 连接级标志
    DWORD dwConnFlags = 0;
    // 使用 INTERNET_FLAG_KEEP_CONNECTION 来控制连接复用
    // 如果设置此标志，MFC 会尝试保持连接；如果不设置，每次请求后关闭
    // 为了避免 Session 耗尽，我们显式地不使用连接池
    dwConnFlags = 0;

    // 请求级标志
    DWORD dwReqFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (bHttps)
    {
        dwReqFlags |= INTERNET_FLAG_SECURE |
                      SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                      SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                      SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    }

    CHttpConnection* pConnection = NULL;
    CHttpFile* pFile = NULL;

    try
    {
        // 不使用连接池，避免 Session 耗尽问题
        // 每次请求都创建新的 Connection，用完立即删除
        pConnection = pSession->GetHttpConnection(strHost, dwConnFlags, nPort, NULL, NULL);
        if (pConnection == NULL)
            return FALSE;

        LPCTSTR pszVerb = (pBody != NULL) ? _T("POST") : _T("GET");
        
        // 添加调试输出以便追踪问题
        ::OutputDebugString(_T("Opening request: "));  
        ::OutputDebugString(strPath.GetBuffer());
        ::OutputDebugString(_T("\n"));
        
        pFile = pConnection->OpenRequest(pszVerb, strPath, NULL, 1, NULL, NULL, dwReqFlags);
        if (pFile == NULL)
        {
            ::OutputDebugString(_T("OpenRequest failed!\n"));
            // 关键修复：先 Close 再 Delete，且不要重复调用 Close
            if (pConnection)
            {
                delete pConnection;
                pConnection = NULL;
            }
            return FALSE;
        }

        // 强制忽略证书错误（适用于测试环境）
        if (bHttps)
        {
            DWORD dwSecurityFlags = 0;
            DWORD dwBuffLen = sizeof(dwSecurityFlags);
            
            pFile->QueryOption(INTERNET_OPTION_SECURITY_FLAGS, &dwSecurityFlags, &dwBuffLen);
            
            dwSecurityFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                               SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                               SECURITY_FLAG_IGNORE_CERT_CN_INVALID ;
            
            pFile->SetOption(INTERNET_OPTION_SECURITY_FLAGS, &dwSecurityFlags, sizeof(dwSecurityFlags));
        }

        // Add custom headers
        std::map<CString, CString>::const_iterator it;
        for (it = headers.begin(); it != headers.end(); ++it)
        {
            CString strHeader;
            strHeader.Format(_T("%s: %s\r\n"), it->first, it->second);
            pFile->AddRequestHeaders(strHeader);
        }

        // 关键修复：POST 请求时设置正确的 Content-Length
        if (pBody != NULL)
        {
            CString strLength;
            strLength.Format(_T("Content-Length: %d\r\n"), pBody->length());
            pFile->AddRequestHeaders(strLength);
        }

        // Send request - 直接传递数据并等待完成
        BOOL bSendSuccess = FALSE;
        
        if (pBody != NULL)
        {
            // POST: 立即发送 body 数据
            bSendSuccess = pFile->SendRequest(NULL, 0, (LPVOID)pBody->data(), (DWORD)pBody->length());
            if (!bSendSuccess)
            {
                ::OutputDebugString(_T("SendRequest (POST) failed immediately\n"));
                throw new CInternetException(ERROR_CAN_NOT_COMPLETE);
            }
        }
        else
        {
            // GET: 先发送请求头，然后等待响应
            bSendSuccess = pFile->SendRequest();
            if (!bSendSuccess)
            {
                ::OutputDebugString(_T("SendRequest (GET) failed immediately\n"));
                throw new CInternetException(ERROR_FILE_INVALID);
            }
        }

        // 等待服务器响应 - QueryInfoStatusCode 会阻塞直到收到响应
        bSendSuccess = pFile->QueryInfoStatusCode(dwStatusCode);
        if (!bSendSuccess)
        {
            ::OutputDebugString(_T("QueryInfoStatusCode failed\n"));
            throw new CInternetException(ERROR_INTERNET_TIMEOUT);
        }

        // Read response
        char szBuffer[4096] = {0};
        UINT nRead = 0;
        do
        {
            nRead = pFile->Read(szBuffer, 4096);
            if (nRead > 0)
            {
                strResponse.append(szBuffer, nRead);
            }
        } while (nRead > 0);

        pFile->Close();
        delete pFile;
        pFile = NULL;
        
        // 关键：立即删除 Connection，不重用连接
        pConnection->Close();
        delete pConnection;
        pConnection = NULL;

        return TRUE;
    }
    catch (CInternetException* pEx)
    {
        TCHAR szErr[256] = {0};
        pEx->GetErrorMessage(szErr, 256);
        TRACE(_T("HTTP Exception: %s\n"), szErr);
        ::OutputDebugString(szErr);
        ::OutputDebugString(_T("\n"));
        pEx->Delete();

        // 确保所有资源都被清理 - VC8 MFC 没有 TryClose，使用普通 Close+try-catch
        if (pFile) 
        {
            try { pFile->Close(); } catch (...) {}
            delete pFile;
            pFile = NULL;
        }
        if (pConnection) 
        {
            try { pConnection->Close(); } catch (...) {}
            delete pConnection;
            pConnection = NULL;
        }
        return FALSE;
    }
    catch (...)
    {
        ::OutputDebugString(_T("Unknown exception\n"));
        // 确保所有资源都被清理 - VC8 MFC 没有 TryClose，使用普通 Close+try-catch
        if (pFile) 
        {
            try { pFile->Close(); } catch (...) {}
            delete pFile;
            pFile = NULL;
        }
        if (pConnection) 
        {
            try { pConnection->Close(); } catch (...) {}
            delete pConnection;
            pConnection = NULL;
        }
        return FALSE;
    }
}

// ...existing code...

BOOL CHttpClient::Get(LPCTSTR pszUrl, std::string& strResponse, DWORD& dwStatusCode)
{
    if (pszUrl == NULL) return FALSE;
    return DoRequest(pszUrl, NULL, m_mapHeaders, strResponse, dwStatusCode);
}

BOOL CHttpClient::Post(LPCTSTR pszUrl, const std::string& strBody,
                       std::string& strResponse, DWORD& dwStatusCode)
{
    if (pszUrl == NULL) return FALSE;
    return DoRequest(pszUrl, &strBody, m_mapHeaders, strResponse, dwStatusCode);
}

UINT CHttpClient::AsyncThreadProc(LPVOID pParam)
{
    AsyncData* pData = (AsyncData*)pParam;
    if (pData == NULL || pData->pThis == NULL) return 1;

    // 为每个异步请求创建独立的 Session
    CInternetSession localSession(_T("MFC-HttpClient/1.0"), 1, INTERNET_OPEN_TYPE_PRECONFIG);
    localSession.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, pData->pThis->m_dwConnectTimeout);
    localSession.SetOption(INTERNET_OPTION_SEND_TIMEOUT, pData->pThis->m_dwSendTimeout);
    localSession.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, pData->pThis->m_dwReceiveTimeout);

    std::string strResponse;
    DWORD dwStatusCode = 0;

    // 使用 localSession 进行请求
    BOOL bSuccess = pData->pThis->DoRequestWithSession(
        &localSession,
        pData->url,
        pData->hasBody ? &pData->body : NULL,
        pData->headers,
        strResponse,
        dwStatusCode);

    // 确保资源完全释放
    localSession.Close();

    if (pData->callback)
    {
        pData->callback(bSuccess, strResponse, dwStatusCode, pData->userData);
    }

    delete pData;
    return 0;
}

BOOL CHttpClient::GetAsync(LPCTSTR pszUrl, ResponseCallback pCallback, LPVOID pUserData)
{
    if (pszUrl == NULL || pCallback == NULL) return FALSE;

    AsyncData* pData = new AsyncData;
    pData->pThis     = this;
    pData->url       = pszUrl;
    pData->hasBody   = false;
    pData->headers   = m_mapHeaders;
    pData->callback  = pCallback;
    pData->userData  = pUserData;

    // 不再依赖 m_pSession，每个异步请求都有自己独立的 session
    CWinThread* pThread = AfxBeginThread(AsyncThreadProc, pData,
        THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);

    if (pThread == NULL)
    {
        delete pData;
        return FALSE;
    }
    pThread->m_bAutoDelete = TRUE;
    pThread->ResumeThread();

    return TRUE;
}

BOOL CHttpClient::PostAsync(LPCTSTR pszUrl, const std::string& strBody,
                            ResponseCallback pCallback, LPVOID pUserData)
{
    if (pszUrl == NULL || pCallback == NULL) return FALSE;

    AsyncData* pData = new AsyncData;
    pData->pThis     = this;
    pData->url       = pszUrl;
    pData->body      = strBody;
    pData->hasBody   = true;
    pData->headers   = m_mapHeaders;
    pData->callback  = pCallback;
    pData->userData  = pUserData;

    // 不再依赖 m_pSession，每个异步请求都有自己独立的 session
    CWinThread* pThread = AfxBeginThread(AsyncThreadProc, pData,
        THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);

    if (pThread == NULL)
    {
        delete pData;
        return FALSE;
    }
    pThread->m_bAutoDelete = TRUE;
    pThread->ResumeThread();

    return TRUE;
}

void CHttpClient::Cleanup()
{
    if (m_pSession != NULL)
    {
        m_pSession->Close();
        delete m_pSession;
        m_pSession = NULL;
    }
    m_mapHeaders.clear();
}