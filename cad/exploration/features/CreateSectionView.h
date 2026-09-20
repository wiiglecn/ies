#pragma once
#include "arxHeaders.h"
#include "./CreateBaseView.h"

class CCreateSectionView  : public CCreateBaseView
{
public:
	CCreateSectionView(void);
public:
	//~CCreateSectionView(void);


	void Execute();
	virtual void ClearExtDbCache();

	void showAllLayer();
protected:
	cJSON* s_ConfigData;


	void drawTable();
};
