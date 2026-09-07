#pragma once
#include <vector>   
#include <string>
#include "dbents.h"
#include "gedll.h"
#include "gepnt3d.h"
#include "declare.h"


#ifdef _CAD2005
    typedef std::string KString;

#define CADYEAR _T("2005")

#else
    typedef std::wstring KString;

#define CADYEAR _T("2007")

#endif

// 单行文本水平对齐方式
enum DbTextHAlign
{
    DBTA_LEFT   = 0,   // 左对齐（默认）：pt = 文本基线左端
    DBTA_CENTER = 1,   // 居中：pt = 文本水平中心（基线高度不变）
    DBTA_RIGHT  = 2    // 右对齐：pt = 文本基线右端
};

class CUtils
{
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
                              DbTextHAlign hAlign = DBTA_LEFT);   // 默认值只能写在头文件
	static double getTextWidth(AcDbText* pText);
	static double CStringToDouble(const CString& str);
	// 创建多行文本：dWidth <= 0 表示不自动折行；attach 为九宫格附着点（默认左上）
	static AcDbObjectId AddDbMText(AcDbBlockTableRecord* container, const ACHAR* pszText, const AcGePoint3d& pt,
                               double dHeight, double dWidth,const ACHAR* pszStyleName = NULL,
                               AcDbMText::AttachmentPoint attach = AcDbMText::kTopLeft);
#ifdef _CAD2005
	static void safeVsnprintfVC7(char* szBuffer, size_t bufferSize, const char* pszFmt, va_list args);
#endif

};
