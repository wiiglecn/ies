#pragma once
#include "arxHeaders.h"
#include "../tools/ToolDrawBase.h"

class CCreateBaseView : public ToolDrawBase
{
public:
	CCreateBaseView(void);
public:
	~CCreateBaseView(void);
protected:
	cJSON* s_pJsonData;
	cJSON* s_patConfigData;
	CString strFilePath;


	//-------------------------------------------------------------------------
    // pszDwgPath  : DWG 文件完整路径
    // ptInsert    : 插入点（当前图形 WCS 坐标）
    // dScale      : 统一缩放比例（1.0 = 不缩放）
    // dRotation   : 旋转角（弧度，0 = 不旋转）
    // pszLayer    : 块参照放置的图层（NULL 或空串 = 当前图层）
    // pBlkRefId   : [输出] 成功时返回块参照 ObjectId，可为 NULL
    // 返回        : true 成功 / false 失败（失败原因打印到命令行）
    //-------------------------------------------------------------------------
    AcDbObjectId ImportDwgAsBlock(const ACHAR* pszDwgPath,
                                 const AcGePoint3d& ptInsert,
								 const ACHAR* arrayName,
                                 double dScale = 1.0,
                                 double dRotation = 0.0,
                                 const ACHAR* pszLayer = NULL);
	AcDbObjectId ImportDwgAsBlock(const ACHAR* pszDwgPath,const ACHAR* blockName);
	// 从文件路径提取合法块名（去目录、去扩展名、替换非法字符）
    void MakeBlockName(const ACHAR* pszDwgPath, ACHAR* pszName, int nNameCount);
	// 取（无则读入）路径对应的外部库；失败返回 NULL
    AcDbDatabase* GetExtDb(const ACHAR* pszDwgPath);

	bool prepare();
	virtual void ClearExtDbCache();
private:
	// 按归一化路径缓存 side database（不同 DWG 路径 → 不同库）
    std::map<CString, AcDbDatabase*> m_extDbCache;
};
