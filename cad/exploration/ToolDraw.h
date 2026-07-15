#pragma once

class cJSON;

class ToolDraw
{
public:
	static ToolDraw& getInstance();
	ToolDraw(const ToolDraw&);            
    ToolDraw& operator=(const ToolDraw&); 
public:
	void create_line(cJSON* params);
	void create_circle(cJSON* params);
	void create_rectangle(cJSON* params);
	void create_text(cJSON* params);
	void create_layer(cJSON* params);
	void list_layers(cJSON* params);
private:
	ToolDraw(void);
    ~ToolDraw(void);

	int actionBefore();
	int actionEnd();
	AcApDocument* pDoc;
	//AcDbDatabase* pDb;
	AcDbBlockTableRecord* pModelSpace;
};
