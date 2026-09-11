#pragma once
#include "arxHeaders.h"
#include "../tools/ToolDrawBase.h"

// class cJSON;
class CCreateColumnView : public ToolDrawBase
{
public:
	CCreateColumnView(void);
public:
	~CCreateColumnView(void);

	void Execute();
	virtual int actionEnd();
private:
	// 按归一化路径缓存 side database（不同 DWG 路径 → 不同库）
    std::map<CString, AcDbDatabase*> m_extDbCache;
    // 取（无则读入）路径对应的外部库；失败返回 NULL
    AcDbDatabase* GetExtDb(const ACHAR* pszDwgPath);
    // 释放全部缓存库
    void ClearExtDbCache();
    
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
	//-------------------------------------------------------------------------
    // 确保 blockName 块定义存在：已存在直接返回；不存在则从 pszDwgPath 导入并
    // 命名为 blockName（不插入块参照）。返回块定义 ObjectId，调用方可用
    // setBlockTableRecord 在任意位置创建块参照。失败返回 kNull。
    //-------------------------------------------------------------------------
	AcDbObjectId ImportDwgAsBlock(const ACHAR* pszDwgPath,const ACHAR* blockName);
	// 从文件路径提取合法块名（去目录、去扩展名、替换非法字符）
    void MakeBlockName(const ACHAR* pszDwgPath, ACHAR* pszName, int nNameCount);

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
