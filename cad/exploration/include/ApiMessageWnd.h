#pragma once

class CApiMessageWnd : public CWnd
{
/*public:
	CApiMessageWnd(void);
public:
	~CApiMessageWnd(void);*/


public:
    BOOL Create();

protected:
    afx_msg LRESULT HandleApiCommand(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()
};
