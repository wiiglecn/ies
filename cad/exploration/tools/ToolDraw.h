#pragma once
#include "declare.h"
#include "ToolDrawBase.h"

class ToolDraw : public ToolDrawBase
{
public:
	static ToolDraw& getInstance();
	ToolDraw(const ToolDraw&);            
    ToolDraw& operator=(const ToolDraw&); 
public:
	void Init();
	

	void create_line(AiToolCommandData* data);
	void create_polyline(AiToolCommandData* data);
	void create_ellipse(AiToolCommandData* data);
	void create_spline(AiToolCommandData* data);
	void create_arc(AiToolCommandData* data);
	void create_hatch(AiToolCommandData* data);
	void create_mtext(AiToolCommandData* data);
	void create_circle(AiToolCommandData* data);
	void create_rectangle(AiToolCommandData* data);
	void create_text(AiToolCommandData* data);

	
	
private:
	ToolDraw(void);
    ~ToolDraw(void);
};
