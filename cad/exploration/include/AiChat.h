#pragma once
#include "atlstr.h"
class CDataPalette;
struct cJSON;

class AiChat
{
public:
	AiChat(void);
public:
	~AiChat(void);
	void setAcadWnd(CWnd* pWnd);
	CWnd* getAcadWnd();
	void Execute(CString& message);
	void SendChatRequest(const std::string& jsonBody);
	void ProcessToolResponse(const std::string& strResponse);
	void ContinueChat();
	void SendError();
private:
	CWnd* pAcadWnd;
	cJSON* m_root;
    cJSON* m_messages;
};
