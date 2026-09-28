#pragma once

class AiApiServer;
class AiApiRequest
{
public:
	AiApiRequest(void);
	void Start();
	void Stop();
	void setAcadWnd(CWnd* pWnd);
	CWnd* getAcadWnd();
public:
	~AiApiRequest(void);

private:
	AiApiServer* m_Server;
	CWnd* pAcadWnd;
};
