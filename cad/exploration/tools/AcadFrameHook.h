#pragma once
class CAcadFrameHook 
{
public:
    CAcadFrameHook();
    virtual ~CAcadFrameHook();
private:
    // Call this to attach the hook to the AutoCAD main frame
    void StartHook();
	void StopHook();

protected:
    // 1. 将旧的窗口过程保存为静态成员变量
    static WNDPROC m_pfnOldAcadWndProc; 

    // 2. 将消息处理函数定义为静态成员函数
    static LRESULT CALLBACK AcadInternalCmdHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};
