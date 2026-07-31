#pragma once
#include "declare.h"
class cJSON;

class ToolDraw
{
public:
	static ToolDraw& getInstance();
	ToolDraw(const ToolDraw&);            
    ToolDraw& operator=(const ToolDraw&); 
public:
	void create_line(AiToolCommandData* data);
	void create_polyline(AiToolCommandData* data);
	void create_ellipse(AiToolCommandData* data);
	void create_spline(AiToolCommandData* data);
	void create_circle(AiToolCommandData* data);
	void create_rectangle(AiToolCommandData* data);
	void create_text(AiToolCommandData* data);
	void create_layer(AiToolCommandData* data);
	void list_layers(AiToolCommandData* data);
	void query_all_layers(AiToolCommandData* data);

	void delete_entities(AiToolCommandData* data);
	void list_entities(AiToolCommandData* data);

	void query_layer(AiToolCommandData* data);
    void get_current_layer(AiToolCommandData* data);
    void set_current_layer(AiToolCommandData* data);
    void get_current_layer_color(AiToolCommandData* data);
	
	void regen();
private:
	ToolDraw(void);
    ~ToolDraw(void);

	
	int actionBefore();
	int actionEnd();
	void appendHandleToResult(AcDbObjectId entityId, AiToolCommandData* data);

	
	AcApDocument* pDoc;
	//AcDbDatabase* pDb;
	AcDbBlockTableRecord* pModelSpace;
	AcDbDatabase* pDb;
};
