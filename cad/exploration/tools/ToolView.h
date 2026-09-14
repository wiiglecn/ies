#pragma once
#include "declare.h"
#include "ToolDrawBase.h"


class ToolView : public ToolDrawBase
{
public:
	static ToolView& getInstance();
	ToolView(const ToolView&);            
    ToolView& operator=(const ToolView&); 
public:
	void zoom_extents(AiToolCommandData* data);
	void zoom_window(AiToolCommandData* data);
	void get_screenshot(AiToolCommandData* data);
private:
	ToolView(void);
    ~ToolView(void);
};
