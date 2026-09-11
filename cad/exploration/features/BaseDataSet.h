#pragma once
#include "../tools/ToolDrawBase.h"

class CBaseDataSet : public ToolDrawBase
{
public:
	//CBaseDataSet(void);

	void Execute(const TCHAR* cmd);
//public:
	//~CBaseDataSet(void);
private:
	void ExecuteHoleDiameter();
	void ExecuteSaveBlock(const TCHAR* blockname);
	void ExecuteDrafting();
	void ExecuteProofread();
};
