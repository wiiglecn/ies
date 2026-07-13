#pragma once

class AiApiServer;
class AiApiRequest
{
public:
	AiApiRequest(void);
	void Start();
	void Stop();
public:
	~AiApiRequest(void);

private:
	AiApiServer* m_Server;
};
