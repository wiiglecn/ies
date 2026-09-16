#pragma once
#include "declare.h"
#include "ToolDrawBase.h"


class ToolGlobal: public ToolDrawBase
{
public:
	static ToolGlobal& getInstance();
    
    void dispatch_command(AiToolCommandData* data);
    void get_drawing_units(AiToolCommandData* data);


    static void CMD();

	static void CreateColumnView();
private:
	ToolGlobal(void);
    ~ToolGlobal(void);
};