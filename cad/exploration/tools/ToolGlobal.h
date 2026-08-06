#pragma once
#include "declare.h"
#include "ToolDrawBase.h"
class cJSON;

class ToolGlobal: public ToolDrawBase
{
public:
	static ToolGlobal& getInstance();
    void dispatch_command(AiToolCommandData* data);
    static void CMD();

private:
	ToolGlobal(void);
    ~ToolGlobal(void);
};