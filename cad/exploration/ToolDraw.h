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
private:
	ToolDraw(void);
    ~ToolDraw(void);

	int actionBefore();
	int actionEnd();
	AcApDocument* pDoc;
	//AcDbDatabase* pDb;
	AcDbBlockTableRecord* pModelSpace;
};
