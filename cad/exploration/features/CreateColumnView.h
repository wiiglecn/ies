#pragma once
#include "arxHeaders.h"
#include "../tools/ToolDrawBase.h"
#include "./CreateBaseView.h"

// class cJSON;
class CCreateColumnView : public CCreateBaseView
{
public:
	//CCreateColumnView(void);
public:
	//~CCreateColumnView(void);

	void Execute();
	virtual int actionEnd();
private:
	
	

    // 若块名已存在则追加 _1、_2 ... 生成不冲突块名（原地修改 pszName）
    bool GetUniqueBlockName(AcDbDatabase* pDb, ACHAR* pszName, int nNameCount);
    // 递归遍历块定义，打印所有文本
    void CollectTexts(AcDbObjectId blkDefId, CMapStringToString& mapVals, int depth = 0);
	//制图人和校对人
	void CollectMapping(AcDbBlockTableRecord* pBTR,AcGePoint3d& pt,TCHAR* blockname);

    // MText 内容去掉格式码（\P、\f..; 等），转纯文本
    CString MTextToPlain(const ACHAR* s);

	void CreateHeader(const AcGePoint3d& ptInsert);
	// 改变序号
    void ChangeNo(AcDbObjectId blkDefId,ACHAR* no);


};
