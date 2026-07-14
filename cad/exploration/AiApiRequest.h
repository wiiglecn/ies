#pragma once

class AiApiServer;
class AiApiRequest
{
public:
	AiApiRequest(void);
	void Start();
	void Stop();
	void setAcadWnd(CWnd* pWnd);
public:
	~AiApiRequest(void);

private:
	AiApiServer* m_Server;
	CWnd* pAcadWnd;
};
