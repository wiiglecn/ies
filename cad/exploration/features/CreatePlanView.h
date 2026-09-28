#pragma once
#include "arxHeaders.h"
#include "./CreateBaseView.h"

class CCreatePlanView : public CCreateBaseView
{
public:
	CCreatePlanView(void);

	bool Execute(const TCHAR* requestPath = NULL);
public:
	//~CCreatePlanView(void);

protected:
	cJSON* s_blockConfigData;
	TCHAR* pszStyleName;


	virtual void ClearExtDbCache();
	void ChangeText(AcDbObjectId blkDefId,AcDbObjectId styleId, ACHAR* holeName,ACHAR* elevation,ACHAR* depth);
	//创建图例
	//创建图例（样例孔的孔号/标高/孔深取第一个孔的值）
    void CreateLegend(std::vector<CString>& arrUsedHoles,
					const CString& fistCadBlockUrl,
                    const CString& blockStyleName);
};
