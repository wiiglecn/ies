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
    DWORD dwFlags = INTERNET_FLAG_EXISTING_CONNECT;
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
    strResponse.clear();
    dwStatusCode = 0;

    if (m_pSession == NULL)
    {
        if (!Initialize())
            return FALSE;
    }

    CString strHost, strPath;
    INTERNET_PORT nPort = 0;
    DWORD dwServiceType = 0;

    if (!ParseUrl(strUrl, strHost, strPath, nPort, dwServiceType))
        return FALSE;

    BOOL bHttps = (dwServiceType == AFX_INET_SERVICE_HTTPS);
    DWORD dwFlags = INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_RELOAD;
    if (bHttps)
    {
        dwFlags |= INTERNET_FLAG_SECURE | 
                   SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                   SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                   SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    }

    CHttpConnection* pConnection = NULL;
    CHttpFile* pFile = NULL;

    try
    {
        pConnection = m_pSession->GetHttpConnection(strHost, dwFlags, nPort, NULL, NULL);
        if (pConnection == NULL)
            return FALSE;

        LPCTSTR pszVerb = (pBody != NULL) ? _T("POST") : _T("GET");
        pFile = pConnection->OpenRequest(pszVerb, strPath, NULL, 1, NULL, NULL, dwFlags);
        if (pFile == NULL)
        {
            pConnection->Close();
            delete pConnection;
            return FALSE;
        }

        // 强制忽略证书错误（适用于测试环境）
        if (bHttps)
        {
            DWORD dwSecurityFlags = 0;
            DWORD dwBuffLen = sizeof(dwSecurityFlags);
            
            // 获取现有的安全标志
            pFile->QueryOption(INTERNET_OPTION_SECURITY_FLAGS, &dwSecurityFlags, &dwBuffLen);
            
            // 追加忽略未知CA、证书日期无效、证书CN不匹配等标志
            dwSecurityFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                               SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                               SECURITY_FLAG_IGNORE_CERT_CN_INVALID ;
            
            // 设置强制覆盖的安全标志
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

        // Send request
        if (pBody != NULL)
        {
            pFile->SendRequest(NULL, 0, (LPVOID)pBody->data(), (DWORD)pBody->length());
        }
        else
        {
            pFile->SendRequest();
        }

        // Query status code
        pFile->QueryInfoStatusCode(dwStatusCode);

        // Read response
		char szBuffer[4096]={0};
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
        pConnection->Close();
        delete pFile;
        delete pConnection;

        return TRUE;
    }
    catch (CInternetException* pEx)
    {
        TCHAR szErr[256] = {0};
        pEx->GetErrorMessage(szErr, 256);
        TRACE(_T("HTTP Exception: %s\n"), szErr);
        pEx->Delete();

        if (pFile) { delete pFile; }
        if (pConnection) { delete pConnection; }
        return FALSE;
    }
    catch (...)
    {
        if (pFile) { delete pFile; }
        if (pConnection) { delete pConnection; }
        return FALSE;
    }
}

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

    std::string strResponse;
    DWORD dwStatusCode = 0;

    BOOL bSuccess = pData->pThis->DoRequest(
        pData->url,
        pData->hasBody ? &pData->body : NULL,
        pData->headers,
        strResponse,
        dwStatusCode);

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

    if (m_pSession == NULL) Initialize();

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

    if (m_pSession == NULL) Initialize();

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