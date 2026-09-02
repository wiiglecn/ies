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
	AcDbDatabase* pExtDb;

	//-------------------------------------------------------------------------
    // pszDwgPath  : DWG 文件完整路径
    // ptInsert    : 插入点（当前图形 WCS 坐标）
    // dScale      : 统一缩放比例（1.0 = 不缩放）
    // dRotation   : 旋转角（弧度，0 = 不旋转）
    // pszLayer    : 块参照放置的图层（NULL 或空串 = 当前图层）
    // pBlkRefId   : [输出] 成功时返回块参照 ObjectId，可为 NULL
    // 返回        : true 成功 / false 失败（失败原因打印到命令行）
    //-------------------------------------------------------------------------
    bool ImportDwgAsBlock(const ACHAR* pszDwgPath,
                                 const AcGePoint3d& ptInsert,
								 CMapStringToString& mapVals,
								 const char* arrayName,
                                 double dScale = 1.0,
                                 double dRotation = 0.0,
                                 const ACHAR* pszLayer = NULL,
                                 AcDbObjectId* pBlkRefId = NULL);
	// 从文件路径提取合法块名（去目录、去扩展名、替换非法字符）
    void MakeBlockName(const ACHAR* pszDwgPath, ACHAR* pszName, int nNameCount);

    // 若块名已存在则追加 _1、_2 ... 生成不冲突块名（原地修改 pszName）
    bool GetUniqueBlockName(AcDbDatabase* pDb, ACHAR* pszName, int nNameCount);
    // 递归遍历块定义，打印所有文本
    void CollectTexts(AcDbObjectId blkDefId, CMapStringToString& mapVals, int depth = 0);
    // MText 内容去掉格式码（\P、\f..; 等），转纯文本
    CString MTextToPlain(const ACHAR* s);
};
