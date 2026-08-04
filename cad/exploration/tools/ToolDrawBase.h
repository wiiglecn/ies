#pragma once
#include "declare.h"

class cJSON;
class ToolDrawBase
{
public:
    void regen();
protected:
    ToolDrawBase(void);
    virtual ~ToolDrawBase(void);

    int actionBefore();
    int actionEnd();
    void appendHandleToResult(AcDbObjectId entityId, AiToolCommandData* data);

    AcApDocument* pDoc;
    AcDbBlockTableRecord* pModelSpace;
    AcDbDatabase* pDb;
};