#pragma once

#include <afxinet.h> // MFC Internet classes (WinINet wrapper)
#include <string>
#include <map>


class CHttpClient
{
public:
    // Async response callback (called from worker thread)
    typedef void (*ResponseCallback)(BOOL bSuccess, const std::string& strResponse,
        DWORD dwStatusCode, LPVOID pUserData);

    CHttpClient();
    virtual ~CHttpClient();

    // Initialize session
    BOOL Initialize(LPCTSTR pszUserAgent = _T("MFC-HttpClient/1.0"));

    // ---- Header management ----
    void SetHeader(LPCTSTR pszName, LPCTSTR pszValue);
    void ClearHeaders();

    // ---- Timeout (ms) ----
    void SetTimeout(DWORD dwConnect, DWORD dwSend, DWORD dwReceive);

    // ---- Synchronous requests ----
    BOOL Get(LPCTSTR pszUrl, std::string& strResponse, DWORD& dwStatusCode);
    BOOL Post(LPCTSTR pszUrl, const std::string& strBody,
              std::string& strResponse, DWORD& dwStatusCode);

    // ---- Asynchronous requests (return immediately, callback in worker thread) ----
    BOOL GetAsync(LPCTSTR pszUrl, ResponseCallback pCallback, LPVOID pUserData = NULL);
    BOOL PostAsync(LPCTSTR pszUrl, const std::string& strBody,
                   ResponseCallback pCallback, LPVOID pUserData = NULL);

    void Cleanup();
protected:
    BOOL DoRequestWithSession(CInternetSession* pSession,
                              const CString& strUrl,
                              const std::string* pBody,
                              const std::map<CString, CString>& headers,
                              std::string& strResponse,
                              DWORD& dwStatusCode);
private:
    CInternetSession* m_pSession;
    std::map<CString, CString> m_mapHeaders;

    DWORD m_dwConnectTimeout;
    DWORD m_dwSendTimeout;
    DWORD m_dwReceiveTimeout;

    // Internal: perform the actual HTTP request
    BOOL DoRequest(const CString& strUrl,
                   const std::string* pBody,
                   const std::map<CString, CString>& headers,
                   std::string& strResponse,
                   DWORD& dwStatusCode);

    // Parse URL
    BOOL ParseUrl(const CString& strUrl,
                  CString& strHost,
                  CString& strPath,
                  INTERNET_PORT& nPort,
                  DWORD& dwServiceType);

    // ---- Async worker ----
    struct AsyncData
    {
        CHttpClient* pThis;
        CString url;
        std::string body;
        bool hasBody;
        std::map<CString, CString> headers;
        ResponseCallback callback;
        LPVOID userData;
    };

    static UINT __cdecl AsyncThreadProc(LPVOID pParam);
};