#include "AiApiRequest.h"
#include "AiApiServer.h"

AiApiRequest::AiApiRequest(void):m_Server(NULL)
{
}

AiApiRequest::~AiApiRequest(void)
{
}

// AI 聊天接口
void HandleAiChat(struct mg_connection* nc, 
                  const std::map<std::string, std::string>& params,
                  const std::string& body)
{
    // 这里可以调用 OpenAI/百度文心/讯飞等 AI API
    // body 包含客户端发来的 JSON 请求数据
    
    std::string response = "{\"status\":\"success\",\"message\":\"AI reply placeholder\"}";
    AiApiServer::SendJsonResponse(nc, 200, response);
}

// AI 文本生成接口
void HandleAiGenerate(struct mg_connection* nc,
                      const std::map<std::string, std::string>& params,
                      const std::string& body)
{
    // 调用 AI 接口生成文本
    std::string response = "{\"text\":\"Generated content...\"}";
    AiApiServer::SendJsonResponse(nc, 200, response);
}

void AiApiRequest::Start()
{
	m_Server=new AiApiServer();
	// 注册路由
    m_Server->RegisterRoute("POST", "/api/ai/chat", HandleAiChat);
    m_Server->RegisterRoute("POST", "/api/ai/generate", HandleAiGenerate);

	m_Server->StartHttpServer();
}
void AiApiRequest::Stop()
{
	if (m_Server)
	{
		m_Server->Stop();
	}
}