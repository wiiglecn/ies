#pragma once
#include <vector>   
#include <string>
#include "dbents.h"
#include "gedll.h"
#include "gepnt3d.h"
#include "declare.h"

// 单行文本水平对齐方式
enum DbTextHAlign
{
    DBTA_LEFT   = 0,   // 左对齐（默认）：pt = 文本基线左端
    DBTA_CENTER = 1,   // 居中：pt = 文本水平中心（基线高度不变）
    DBTA_RIGHT  = 2    // 右对齐：pt = 文本基线右端
};

#ifdef _CAD2005
    typedef std::string KString;

	#define CADYEAR _T("2005")

#else

	typedef std::wstring KString;
	#define CADYEAR _T("2007")

	#define DBTA_CENTER DbTextHAlign::DBTA_CENTER
    #define DBTA_LEFT   DbTextHAlign::DBTA_LEFT
    #define DBTA_RIGHT  DbTextHAlign::DBTA_RIGHT
#endif


class CUtils
{
protected:
	static cJSON* s_pJsonConfig;  // 静态成员变量，用于存储 JSON 配置
public:
	CUtils(void);
public:
	~CUtils(void);
	static bool checkEmpty(const TCHAR* str);
	static bool IsCircleExisting(const AcGePoint3d& center);
	static void FocusAcadDrawing();
	static CString GetArxFolder();
	static std::wstring StringToWString(const std::string& str);
	static bool utf8ToTChar(const char* utf8, TCHAR* outBuf, int bufLen);
	static std::string CStringToUTF8(const CString& str);
	static void TCharToUtf8(const TCHAR* tStr, char* utf8Str, int utf8Size);
	static std::string DecryptFileWithPassword(const KString& filePath, const std::string& password);
	static Acad::ErrorStatus sendStringToExecute(AcApDocument* pAcTargetDocument,
                                       const ACHAR * pszExecute,
                                       bool bActivate = true,
                                       bool bWrapUpInactiveDoc = false,
                                       bool bEchoString = true);  
	static int  acutPrintf (const ACHAR *format, ...);
	static bool IsIllegalBlockNameChar(ACHAR ch);
	static void SafeCopy(ACHAR* pszDst, int nDstCount, const ACHAR* pszSrc, int nCount);
	static CString NormalizeDwgPath(const ACHAR* pszDwgPath);
	static void IntToAChar(int nValue, ACHAR* pszBuf, int nBufCount);
	// 按 sep 分隔 UTF-8 字符串，每段转成 TCHAR 后存入 outArr，返回段数
	static int SplitUtf8(const char* utf8Str, char sep, std::vector<CString>& outArr);
	static AcDbObjectId FindTextStyle(const ACHAR* pszStyleName);
	static AcDbObjectId AddDbText(AcDbBlockTableRecord* container,const ACHAR* pszText, const AcGePoint3d& pt,
						double dHeight, const ACHAR* pszStyleName,
                              int hAlign,double dWidthFactor = 1.0);   // 默认值只能写在头文件
	static AcDbObjectId AddDbText(AcDbBlockTableRecord* container,const ACHAR* pszText, const AcGePoint3d& pt,
		AcDbObjectId styleId,int hAlign);   // 默认值只能写在头文件
	    // 获取实体几何包围盒的宽和高（世界坐标系）；失败返回 false，宽高置 0
    static bool GetEntExtent(AcDbEntity* pEnt, double& dWidth, double& dHeight);
	static bool GetEntExtentById(const AcDbObjectId& id, double& dWidth, double& dHeight);
	static double CStringToDouble(const CString& str);
	// 创建多行文本：dWidth <= 0 表示不自动折行；attach 为九宫格附着点（默认左上）
	static AcDbObjectId AddDbMText(AcDbBlockTableRecord* container, const ACHAR* pszText, const AcGePoint3d& pt,
                               double dHeight, double dWidth,const ACHAR* pszStyleName = NULL,
                               AcDbMText::AttachmentPoint attach = AcDbMText::kTopLeft);
	// 读取整个文件并解析为 JSON；失败返回 NULL（bSilent=true 时不打印错误，由调用方自行提示）
    static cJSON* LoadJsonFile(const TCHAR* pszFilePath, bool bSilent = true);
	static bool FindPatNameBySoilName(cJSON* pPatConfig, const TCHAR* pszSoilName,
                                  TCHAR* pszPatName, int nBufCount);
	static bool FindBlockNameByHoleName(cJSON* pPatConfig, const TCHAR* pszHoleName,
                                  TCHAR* pszBlockName, int nBufCount);

	static double JsonValToDouble(cJSON* node);
	    // 按 id 打开实体并返回指针；失败返回 NULL（id 为空 / 已删除 / 类型不是实体）
    // ★ 所有权归 CAD：调用方用完必须 pEnt->close()，不能 delete
    static AcDbEntity* OpenEntityById(const AcDbObjectId& id,
                                      AcDb::OpenMode openMode = AcDb::kForRead);

									  // 确保文字样式存在：不存在则创建（pszFontName 以 .shx 结尾按形字体处理，否则按 Windows 字体）
    // dHeight <= 0 表示不固定字高（推荐，字高由实体决定）；返回样式 ID，失败返回 kNull
    static AcDbObjectId EnsureTextStyle(const ACHAR* pszStyleName,
                                        const ACHAR* pszFontName,
                                        double dHeight = 0.0,
                                        double dWidthFactor = 1.0);
	// 确保图层存在：不存在则创建；bSetCurrent=true 时同时切换为当前层（相当于设置 CLAYER）
    // 返回图层 ID，失败返回 kNull
    static AcDbObjectId EnsureLayer(const ACHAR* pszLayerName, bool bSetCurrent = true);
	    // 清除指定图层上的实体（模型空间 + 所有布局，不动块定义内部；pszLayerName 为 NULL/空 = 当前层）
    // 返回删除的实体数量，失败返回 -1
        // 在调用方已打开的容器（如 pModelSpace）里删除指定图层实体
    // ★ 容器的开/关归调用方（含事务打开的情况），本函数不 close 它；pszLayerName 为 NULL/空 = 当前层
    // 返回删除数量，失败返回 -1
    static int EraseLayerEntities(AcDbBlockTableRecord* pOpenedBtr, const ACHAR* pszLayerName = NULL);
	// 在调用方已打开的容器（如 pModelSpace）里删除指向块名的所有块参照，并删除该块定义
    // ★ 容器开/关归调用方（事务打开也兼容）；嵌套在其他块里的参照不在处理范围
    // 块本就不存在也算成功；返回 false = 找到了但删失败
    static bool EraseBlock(AcDbBlockTableRecord* pOpenedBtr, const ACHAR* pszBlockName);
	// 取消所有选中的元素（含夹点）；返回 true 表示成功
	static bool ClearSelection();
	//获得空间大小
	static bool GetModelSpaceBounds(AcDbBlockTableRecord* pBTR, AcDbExtents& extOut);
	    // 创建轻量多段线：arrPts 至少 2 个点；dWidth = 全局线宽（0 = 细线）；bClosed = 是否闭合
    // 顶点取三维点的 x/y，Z 忽略（多段线是 OCS 平面实体）；返回实体 ID，失败 kNull
    static AcDbObjectId AddDbPolyline(AcDbBlockTableRecord* container,
                                   const std::vector<AcGePoint3d>& arrPts,
                                   double dWidth = 0.0,
                                   bool bClosed = false);
	    // 以 ptCenter 为中心画矩形（高 dHeight × 宽 dWidth），dLineWidth = 全局线宽；返回实体 ID，失败 kNull
    static AcDbObjectId AddDbRect(AcDbBlockTableRecord* container,
                                  const AcGePoint3d& ptCenter,
								  double dWidth,
                                  double dHeight,
                                  double dLineWidth = 0.0);
	//画直线
	static AcDbObjectId AddLine(AcDbBlockTableRecord* container,AcGePoint3d& ptStart,AcGePoint3d& ptEnd);
	
	// 把已入库实体改到指定层（"0" 层始终存在，无需 EnsureLayer）
	static bool SetEntLayer(const AcDbObjectId& id, const ACHAR* pszLayer);

	static cJSON* GetJsonObjectItemUtf8(cJSON* root, const TCHAR* key);
	static cJSON* GetJsonConfigDirect();
	static cJSON* GetJsonConfig();
	static void FreeJsonConfig();
#ifdef _CAD2005
	static void safeVsnprintfVC7(char* szBuffer, size_t bufferSize, const char* pszFmt, va_list args);
#endif

};
