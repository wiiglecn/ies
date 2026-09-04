#pragma once
#include "declare.h"

class cJSON;
class ToolDrawBase
{
public:
    void regen();

	virtual int actionBefore();
    virtual int actionEnd();
	
protected:
    ToolDrawBase(void);
    virtual ~ToolDrawBase(void);

    
    void appendHandleToResult(AcDbObjectId entityId, AiToolCommandData* data);
    AcDbObjectId GetIdFromHandle(cJSON* json, const char* key, AcDbDatabase* pDb);
    
    AcApDocument* pDoc;
    AcDbBlockTableRecord* pModelSpace;
    AcDbDatabase* pDb;
};