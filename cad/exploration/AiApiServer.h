// coding: gb18030
#pragma once
#include <map>
#include <string>
#include "mongoose/mongoose.h"

// API 回调函数类型定义
typedef void (*ApiHandlerFunc)(struct mg_connection* nc, 
							   struct mg_http_message* hm,
                                const std::map<std::string, std::string>& params,
                                const std::string& body,
                                void* user_data);



class AiApiServer
{
public:
    AiApiServer(void);
    ~AiApiServer(void);

    // 启动 HTTP 服务
    bool StartHttpServer(const std::string& port = "7100");
    
    // 启动 HTTPS 服务
    bool StartHttpsServer(const std::string& port = "7443",
                          const std::string& sslCert = "server.pem",
                          const std::string& sslKey = "server.key");
    
    // 停止服务
    void Stop();

    
    
    // 注册 API 路由
    void RegisterRoute(const std::string& method, 
                       const std::string& uri, 
                       ApiHandlerFunc handler,
                        void* user_data = NULL);
	// 发送 JSON 响应
    static void SendJsonResponse(struct mg_connection* nc, 
                                  int statusCode, 
                                  const std::string& jsonStr);
    
private:
    // Mongoose 事件回调
    static void EventHandler(struct mg_connection* nc, int ev, void *ev_data);
    // Add this to start the polling loop
    void StartPolling(); 
    // Thread handling
    static DWORD WINAPI PollThreadFunc(LPVOID lpParam);
    HANDLE m_hThread;
    
    // 处理 HTTP 请求
    void HandleHttpRequest(struct mg_connection* nc, struct mg_http_message* hm);
    
    

    struct mg_mgr m_mgr;
    struct mg_connection* m_nc;
    struct mg_connection* m_sslNc;
    bool m_running;
    
    // 路由表
    struct RouteKey
	{
		std::string method;
		std::string uri;
        void* user_data; // Store the context here
        
		bool operator<(const RouteKey& o) const
		{
			return std::make_pair(method, uri) <
				   std::make_pair(o.method, o.uri);
		}
	};
    std::map<RouteKey, ApiHandlerFunc> m_routes;
};