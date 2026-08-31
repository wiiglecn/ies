#pragma once

// =====================================================================
// HttpClientCurl.h
// 基于 libcurl 的 HTTP/HTTPS 客户端 (兼容 VC8 / VS2005)
//
// 特性:
//   - HTTP / HTTPS 的 GET / POST 请求
//   - 同步请求与异步请求(工作线程 + 回调)
//   - 自定义请求头 / 超时控制 / SSL 证书校验开关
//
// 依赖:
//   - libcurl 静态库(需使用 VC8 编译, 并开启 SSL 支持, 如 OpenSSL)
//     Release: libcurl.lib   Debug: libcurld.lib
//   - curl 头文件放在 include\curl\ 目录(即本工程 include 目录下)
//   - 库文件放在 ..\lib 或 ..\lib\2005 / ..\lib\2007 目录
//   - 如需在工程设置中手动链接, 可定义 HTTPCLIENTCURL_NO_AUTOLINK
//
// 使用示例:
//   CHttpClientCurl client;
//   client.Initialize();
//   client.SetHeader(_T("Content-Type"), _T("application/json"));
//
//   // 同步
//   std::string strResp; DWORD dwCode = 0;
//   client.Post(_T("https://host/api"), "{\"a\":1}", strResp, dwCode);
//
//   // 异步(回调在工作线程中执行, 回调里不要直接操作 UI)
//   client.PostAsync(_T("https://host/api"), "{\"a\":1}", OnResp, pCtx);
//
// 线程说明:
//   - 异步请求会拷贝全部请求参数, 工作线程不访问对象成员
//   - 回调在工作线程中调用, 如需操作 UI 请自行投递到主线程
// =====================================================================

#include <string>
#include <map>

class CHttpClientCurl
{
public:
    // 异步响应回调(在工作线程中调用!)
    typedef void (*ResponseCallback)(BOOL bSuccess, const std::string& strResponse,
                                     DWORD dwStatusCode, LPVOID pUserData);

    CHttpClientCurl();
    virtual ~CHttpClientCurl();

    // 全局初始化(带引用计数, 幂等; 请在主线程调用一次)
    BOOL Initialize(LPCTSTR pszUserAgent = _T("SL CAD Agent/1.0"));

    // 释放全局资源(与 Initialize 配对, 幂等)
    void Cleanup();

    // ---- 请求头管理 ----
    void SetHeader(LPCTSTR pszName, LPCTSTR pszValue);
    void ClearHeaders();

    // ---- 超时(毫秒) ----
    // dwConnect: 连接超时; dwSend/dwReceive: 收发停滞超时(近似实现)
    void SetTimeout(DWORD dwConnect, DWORD dwSend, DWORD dwReceive);
    // 整个请求的总超时, 0 = 不限制
    void SetTotalTimeout(DWORD dwTotalMs);

    // ---- SSL 选项(HTTPS) ----
    void SetVerifyPeer(BOOL bVerify);   // 是否校验服务器证书链
    void SetVerifyHost(BOOL bVerify);   // 是否校验证书主机名

    // ---- 同步请求 ----
    BOOL Get(LPCTSTR pszUrl, std::string& strResponse, DWORD& dwStatusCode);
    BOOL Post(LPCTSTR pszUrl, const std::string& strBody,
              std::string& strResponse, DWORD& dwStatusCode);

    // ---- 异步请求(立即返回, 回调在工作线程执行) ----
    BOOL GetAsync(LPCTSTR pszUrl, ResponseCallback pCallback, LPVOID pUserData = NULL);
    BOOL PostAsync(LPCTSTR pszUrl, const std::string& strBody,
                   ResponseCallback pCallback, LPVOID pUserData = NULL);

    // ---- 最近一次错误说明 ----
    LPCTSTR GetLastError() const;

private:
    // 一次请求的全部参数(同步/异步共用, 异步时整体拷贝到工作线程)
    struct RequestData
    {
        CString strUrl;
        std::string strBody;
        BOOL bHasBody;
        std::map<CString, CString> mapHeaders;
        CString strUserAgent;
        DWORD dwConnectTimeout;
        DWORD dwSendTimeout;
        DWORD dwReceiveTimeout;
        DWORD dwTotalTimeout;
        BOOL bVerifyPeer;
        BOOL bVerifyHost;

        RequestData()
            : bHasBody(FALSE)
            , dwConnectTimeout(60000)
            , dwSendTimeout(60000)
            , dwReceiveTimeout(60000)
            , dwTotalTimeout(0)
            , bVerifyPeer(FALSE)
            , bVerifyHost(FALSE)
        {
        }
    };

    // 核心: 用一个独立 easy handle 执行请求(可在线程中安全调用)
    static BOOL PerformRequest(const RequestData& req,
                               std::string& strResponse,
                               DWORD& dwStatusCode,
                               CString& strError);

    // 用当前成员设置填充请求参数(同步/异步共用)
    void SnapshotSettings(RequestData& req) const;

    // 组装 Async 请求数据并启动工作线程
    BOOL StartAsync(LPCTSTR pszUrl,
                    const std::string* pBody,
                    ResponseCallback pCallback,
                    LPVOID pUserData);

    // ---- 异步工作线程 ----
    struct AsyncData
    {
        RequestData req;
        ResponseCallback pCallback;
        LPVOID pUserData;
    };

    static UINT __cdecl AsyncThreadProc(LPVOID pParam);

    CString m_strUserAgent;
    CString m_strLastError;
    std::map<CString, CString> m_mapHeaders;

    DWORD m_dwConnectTimeout;
    DWORD m_dwSendTimeout;
    DWORD m_dwReceiveTimeout;
    DWORD m_dwTotalTimeout;

    BOOL m_bVerifyPeer;
    BOOL m_bVerifyHost;

    static LONG s_nGlobalInitRef;    // curl_global_init 引用计数
};
