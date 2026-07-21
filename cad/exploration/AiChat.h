#pragma once
#include "atlstr.h"
class CDataPalette;

class AiChat
{
public:
	AiChat(void);
public:
	~AiChat(void);
	void setAcadWnd(CWnd* pWnd);
	CWnd* getAcadWnd();
	void Execute(CString& message);
private:
	CWnd* pAcadWnd;
};
