#pragma once
#include "declare.h"
#include "dbtrans.h"
class ToolDrawBase
{
public:
    void regen();

	virtual int actionBefore();
    virtual int actionEnd();

	
	void setDocument(AcApDocument* pDocSrc);
    void setModelSpace(AcDbBlockTableRecord* pModelSpaceSrc);
    void setDatabase(AcDbDatabase* pDbSrc);
	void setTrans(AcTransaction* pTransSrc);


	void aboutTrans();
	bool startTrans();
	bool commitTrans();
protected:
    ToolDrawBase(void);
    virtual ~ToolDrawBase(void);

    
    void appendHandleToResult(AcDbObjectId entityId, AiToolCommandData* data);
    AcDbObjectId GetIdFromHandle(cJSON* json, const char* key, AcDbDatabase* pDb);
    
    AcApDocument* pDoc;
    AcDbBlockTableRecord* pModelSpace;
    AcDbDatabase* pDb;
	AcTransaction* pTrans;
};