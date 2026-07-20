// coding: gb18030
//#include "StdAfx.h"
#include "AiApiServer.h"
#include <sstream>
#include "tchar.h"
#include "acutads.h"
#include <process.h> // For _beginthreadex if preferred, or use CreateThread


static std::wstring StringToWString(const std::string& str)
{
    if (str.empty()) return std::wstring();
    
    // Get required size
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

AiApiServer::AiApiServer(void) 
    : m_nc(NULL), m_sslNc(NULL), m_running(false),m_hThread(NULL)
{
    // 新版 mg_mgr_init 不需要额外参数
    mg_mgr_init(&m_mgr);
}

AiApiServer::~AiApiServer(void)
{
    Stop();
    mg_mgr_free(&m_mgr);
}

bool AiApiServer::StartHttpServer(const std::string& port)
{
    std::string bindAddr = "http://0.0.0.0:" + port;
    std::wstring wBindAddr = StringToWString(bindAddr);

    acutPrintf(_T("\n[CAD DEBUG] StartHttpServer: %s"), wBindAddr.c_str());
    
    m_nc = mg_http_listen(&m_mgr, bindAddr.c_str(), EventHandler, this);
    if (m_nc == NULL) {
        acutPrintf(_T("\n[CAD ERROR] Failed to listen on port %s"), wBindAddr.c_str());
        return false;
    }
    
    acutPrintf(_T("\n[CAD DEBUG] Listening started. Starting polling thread..."));
    
    // Start the background polling loop
    StartPolling();
    
    return true;
}

bool AiApiServer::StartHttpsServer(const std::string& port,
                                    const std::string& sslCert,
                                    const std::string& sslKey)
{
    std::string bindAddr = "https://0.0.0.0:" + port;
    
    
    m_sslNc = mg_http_listen(&m_mgr, bindAddr.c_str(), EventHandler, this);
    if (m_sslNc == NULL) {
        return false;
    }

    // 配置 TLS/SSL (新版 API)
    struct mg_tls_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.cert = mg_str(sslCert.c_str());
    opts.key = mg_str(sslKey.c_str());
    
    // 为该监听连接设置 TLS，后续衍生出来的连接也会自动继承
    mg_tls_init(m_sslNc, &opts);
    
    m_running = true;
    return true;
}

void AiApiServer::Stop()
{
    m_running = false;
    // Wait for the thread to finish
    if (m_hThread != NULL) {
        WaitForSingleObject(m_hThread, 1000);

        // 只关闭一次，关闭后立即置空，防止重复 CloseHandle
        try {
            CloseHandle(m_hThread);
        }
        catch (...) {
            //acutPrintf(_T("\n[CAD ERROR] Exception caught while calling CloseHandle."));
        }
        m_hThread = NULL;
        // acutPrintf(_T("\n[CAD DEBUG] Polling thread stopped."));
    }
    
    // Clean up mongoose resources
    mg_mgr_free(&m_mgr);
    m_nc = NULL;
    m_sslNc = NULL;
}



void AiApiServer::EventHandler(struct mg_connection* nc, int ev, void *ev_data)
{
    // Retrieve the server instance from nc->user_data
    // This was set when mg_http_listen was called with 'this' as the last arg
    AiApiServer* server = (AiApiServer*)nc->fn_data;
    
    if (server == NULL) return;

    switch (ev) {
        case MG_EV_HTTP_MSG: {
            // For MG_EV_HTTP_MSG, ev_data is indeed the http message
            struct mg_http_message* hm = (struct mg_http_message*)ev_data;
            server->HandleHttpRequest(nc, hm);
            break;
        }
        case MG_EV_ACCEPT: {
            // Optional: Ensure user_data is propagated if necessary
            // Usually mg_http_listen handles this, but sometimes manual setting is needed
            // nc->user_data = server; 
            break;
        }
        default:
            break;
    }
}

void AiApiServer::HandleHttpRequest(struct mg_connection* nc, struct mg_http_message* hm)
{
    std::string method(hm->method.buf, hm->method.len);
    std::string uri(hm->uri.buf, hm->uri.len);
    std::string body(hm->body.buf, hm->body.len);

    // Convert to wide strings
    std::wstring wMethod = StringToWString(method);
    std::wstring wUri = StringToWString(uri);

    // Debug: Print incoming request details
    acutPrintf(_T("\n[CAD DEBUG] Incoming Request: %s %s"), 
               wMethod.c_str(), 
               wUri.c_str());
    
    // Routing logic
    RouteKey key = { method, uri, NULL };
    std::map<RouteKey, ApiHandlerFunc>::iterator it = m_routes.find(key);
    
    if (it != m_routes.end()) {
        std::map<std::string, std::string> params;
        
        char query[1024];
        // Note: Ensure hm->query is valid for your version of mongoose
        int len = mg_http_get_var(&hm->query, "query", query, sizeof(query));
        if (len > 0) {
            params["query"] = std::string(query, len);
            std::wstring wQuery = StringToWString(std::string(query, len));
            acutPrintf(_T("\n[CAD DEBUG] Parsed Param 'query': %s"), wQuery.c_str());
        }
        
        acutPrintf(_T("\n[CAD DEBUG] Executing handler for: %s"), wUri.c_str());

		
		it->second(nc, hm, params, body, it->first.user_data);
        
        // Debug: Handler finished
        acutPrintf(_T("\n[CAD DEBUG] Handler completed."));

    } else {
        // Debug: 404 Not Found
        acutPrintf(_T("\n[CAD DEBUG] No route found for: %s %s"), 
                   wMethod.c_str(), 
                   wUri.c_str());

        SendJsonResponse(nc, 404, "{\"error\":\"Not Found\"}");
    }
}

void AiApiServer::RegisterRoute(const std::string& method, 
                                 const std::string& uri, 
                                 ApiHandlerFunc handler,
                                void* user_data)
{
    RouteKey key = { method, uri, user_data };
    m_routes[key] = handler;
}

void AiApiServer::SendJsonResponse(struct mg_connection* nc, 
                                    int statusCode,
                                    const std::string& jsonStr)
{
    // 使用 mg_http_reply 简化响应发送，它会自动帮我们加上 Content-Length 等头部
    mg_http_reply(nc, statusCode, 
                  "Content-Type: application/json\r\n"
                  "Access-Control-Allow-Origin: *\r\n", 
                  "%s", jsonStr.c_str());
}
// Static thread function
DWORD WINAPI AiApiServer::PollThreadFunc(LPVOID lpParam)
{
    AiApiServer* server = (AiApiServer*)lpParam;
    acutPrintf(_T("\n[CAD DEBUG] Polling thread started."));
    
    // The polling loop
    while (server->m_running) {
        // Poll with a 1000ms timeout. 
        // This allows the thread to wake up quickly when m_running becomes false.
        mg_mgr_poll(&server->m_mgr, 1000);
    }
    
    acutPrintf(_T("\n[CAD DEBUG] Polling thread exited."));
    return 0;
}

void AiApiServer::StartPolling()
{
    if (m_hThread != NULL) return; // Already running

    m_running = true;
    // Create a suspended thread so we can set priority if needed, then resume
    m_hThread = CreateThread(NULL, 0, PollThreadFunc, this, 0, NULL);
    
    if (m_hThread == NULL) {
        acutPrintf(_T("\n[CAD ERROR] Failed to create polling thread."));
        m_running = false;
    } else {
        acutPrintf(_T("\n[CAD DEBUG] Polling thread created successfully."));
    }
}