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
	    // 获取实体几何包围盒的宽和高（世界坐标系）；失败返回 false，宽高置 0
    static bool GetEntExtent(AcDbEntity* pEnt, double& dWidth, double& dHeight);
	static double CStringToDouble(const CString& str);
	// 创建多行文本：dWidth <= 0 表示不自动折行；attach 为九宫格附着点（默认左上）
	static AcDbObjectId AddDbMText(AcDbBlockTableRecord* container, const ACHAR* pszText, const AcGePoint3d& pt,
                               double dHeight, double dWidth,const ACHAR* pszStyleName = NULL,
                               AcDbMText::AttachmentPoint attach = AcDbMText::kTopLeft);
	// 读取整个文件并解析为 JSON；失败返回 NULL（bSilent=true 时不打印错误，由调用方自行提示）
    static cJSON* LoadJsonFile(const TCHAR* pszFilePath, bool bSilent = true);
	static bool FindPatNameBySoilName(cJSON* pPatConfig, const TCHAR* pszSoilName,
                                  TCHAR* pszPatName, int nBufCount);
	static double JsonValToDouble(cJSON* node);
	    // 按 id 打开实体并返回指针；失败返回 NULL（id 为空 / 已删除 / 类型不是实体）
    // ★ 所有权归 CAD：调用方用完必须 pEnt->close()，不能 delete
    static AcDbEntity* OpenEntityById(const AcDbObjectId& id,
                                      AcDb::OpenMode openMode = AcDb::kForRead);

	static cJSON* GetJsonConfigDirect();
	static cJSON* GetJsonConfig();

	static void FreeJsonConfig();
#ifdef _CAD2005
	static void safeVsnprintfVC7(char* szBuffer, size_t bufferSize, const char* pszFmt, va_list args);
#endif

};
