#pragma once
#include "arxHeaders.h"
#include "./CreateBaseView.h"

class CCreatePlanView : public CCreateBaseView
{
public:
	//CCreatePlanView(void);

	void Execute();
public:
	//~CCreatePlanView(void);

protected:
	cJSON* s_blockConfigData;


	virtual void ClearExtDbCache();
	void ChangeText(AcDbObjectId blkDefId,AcDbObjectId styleId, ACHAR* holeName,ACHAR* elevation,ACHAR* depth);
};
