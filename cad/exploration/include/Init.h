#pragma once

class AiApiRequest;
class CApiMessageWnd;
//class AiChat;
class CInit
{
public:
	//CInit(void);
	static void Prepare();
	static void Unload();
	static void CmdUnloadSLTools();

public:
	//~CInit(void);

private:
	static void unloadMenu();
	static void loadMenu();

	
	static AiApiRequest* hServer;
	static CApiMessageWnd* pApiWnd;
	//static AiChat* hChat;
};
