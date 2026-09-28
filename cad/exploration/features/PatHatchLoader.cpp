// ============================================================================
//  PatHatchLoader.cpp
// ============================================================================
// 本工程启用预编译头时保留下面这行且必须位于最前; 独立编译时删除即可
#include "StdAfx.h"
#include "PatHatchLoader.h"
#include <tchar.h>
#include <stdio.h>

#define PAT_DEG2RAD 0.017453292519943295769236907684886

// ---------------------------------------------------------------- 字符串小工具
static CString PatTrim(const CString& s)
{
    CString r(s);
    r.TrimRight(_T(" \t\r\n"));
    r.TrimLeft(_T(" \t\r\n"));
    return r;
}

static CString PatToLower(const CString& s)
{
    CString r(s);
    r.MakeLower();
    return r;
}

static bool PatContainsNoCase(const CString& s, const CString& sub)
{
    if (sub.IsEmpty()) return true;
    return PatToLower(s).Find(PatToLower(sub)) >= 0;
}

static void PatSplitLines(const CString& text, std::vector<CString>& out)
{
    int pos = 0;
    for (;;)
    {
        int nl = text.Find(_T('\n'), pos);
        if (nl < 0) { out.push_back(text.Mid(pos)); break; }
        out.push_back(text.Mid(pos, nl - pos));
        pos = nl + 1;
    }
}

static void PatSplitComma(const CString& line, std::vector<CString>& out)
{
    int pos = 0;
    for (;;)
    {
        int comma = line.Find(_T(','), pos);
        if (comma < 0) { out.push_back(PatTrim(line.Mid(pos))); break; }
        out.push_back(PatTrim(line.Mid(pos, comma - pos)));
        pos = comma + 1;
    }
}

static bool PatToDouble(const CString& tok, double& val)
{
    CString t = PatTrim(tok);
    if (t.IsEmpty()) return false;
    TCHAR* pEnd = NULL;
    val = _tcstod((LPCTSTR)t, &pEnd);              // 自带处理前导空格/正负号
    return (pEnd != NULL && *pEnd == _T('\0'));
}

// wchar_t 串 -> CString: Unicode 下直通, 多字节下转 ANSI
static bool PatW2T(const wchar_t* lpsw, CString& out)
{
    if (lpsw == NULL || lpsw[0] == L'\0') return false;
#ifdef _UNICODE
    out = lpsw;                                    // TCHAR 即 wchar_t
    return true;
#else
    int n = WideCharToMultiByte(CP_ACP, 0, lpsw, -1, NULL, 0, NULL, NULL) - 1;
    if (n <= 0) return false;                      // n 为不含 '\0' 的字符数
    LPTSTR pBuf = out.GetBuffer(n + 1);
    WideCharToMultiByte(CP_ACP, 0, lpsw, -1, pBuf, n + 1, NULL, NULL);
    out.ReleaseBuffer();
    return true;
#endif
}

// 读文件文本: 带 BOM 的 UTF-8 按 UTF-8, 其余按 ANSI
static bool PatReadFileText(const TCHAR* lpszPath, CString& text)
{
    FILE* fp = _tfopen(lpszPath, _T("rb"));        // TCHAR 路径, VC7/VC8 通用
    if (fp == NULL) return false;

    long nBytes = 0;
    if (fseek(fp, 0, SEEK_END) == 0)
        nBytes = ftell(fp);
    rewind(fp);
    if (nBytes <= 0) { fclose(fp); return false; }

    char* pData = new char[nBytes];
    size_t nRead = fread(pData, 1, (size_t)nBytes, fp);
    fclose(fp);
    if (nRead == 0) { delete[] pData; return false; }

    UINT cp = CP_ACP;
    size_t off = 0;
    if (nRead >= 3 &&
        (unsigned char)pData[0] == 0xEF &&
        (unsigned char)pData[1] == 0xBB &&
        (unsigned char)pData[2] == 0xBF)
    {
        cp = CP_UTF8;
        off = 3;                                   // 跳过 BOM
    }

    // 字节 -> 宽字符, 再转 TCHAR/CString
    bool bOk = false;
    int nWide = MultiByteToWideChar(cp, 0, pData + off,
                                    (int)(nRead - off), NULL, 0);
    if (nWide > 0)
    {
        wchar_t* pWide = new wchar_t[nWide + 1];
        MultiByteToWideChar(cp, 0, pData + off,
                            (int)(nRead - off), pWide, nWide);
        pWide[nWide] = L'\0';
        bOk = PatW2T(pWide, text);
        delete[] pWide;
    }
    delete[] pData;
    return bOk;
}

// ---------------------------------------------------------------- 1. 解析 pat
bool PatHatchLoader::ParsePatFile(const TCHAR* lpszPatPath,
                                  std::vector<PAT_PATTERN>& patterns)
{
    patterns.clear();
    if (lpszPatPath == NULL || lpszPatPath[0] == _T('\0'))
        return false;

    CString text;
    if (!PatReadFileText(lpszPatPath, text))
        return false;                              // 文件不存在/读失败

    std::vector<CString> lines;
    PatSplitLines(text, lines);

    for (size_t i = 0; i < lines.size(); ++i)
    {
        CString line = PatTrim(lines[i]);
        if (line.IsEmpty() || line.GetAt(0) == _T(';'))   // 空行 / 注释(;;)
            continue;

        if (line.GetAt(0) == _T('*'))              // 图案头: *NAME,描述
        {
            PAT_PATTERN pat;
            CString rest = line.Mid(1);
            int comma = rest.Find(_T(','));
            if (comma >= 0)
            {
                pat.name        = PatTrim(rest.Left(comma));
                pat.description = PatTrim(rest.Mid(comma + 1));
            }
            else
            {
                pat.name = PatTrim(rest);
            }
            if (!pat.name.IsEmpty())
                patterns.push_back(pat);
            continue;
        }

        // 定义行: angle, baseX, baseY, deltaX, deltaY [, dash ...]
        if (patterns.empty())
            continue;                              // 出现在任何 *NAME 之前: 忽略

        std::vector<CString> tok;
        PatSplitComma(line, tok);
        if (tok.size() < 5)
            continue;

        PAT_DASHLINE dl;
        if (!PatToDouble(tok[0], dl.angle)   ||
            !PatToDouble(tok[1], dl.baseX)   ||
            !PatToDouble(tok[2], dl.baseY)   ||
            !PatToDouble(tok[3], dl.offsetX) ||
            !PatToDouble(tok[4], dl.offsetY))
            continue;                              // 非法定义行: 跳过

        for (size_t k = 5; k < tok.size(); ++k)
        {
            double d = 0.0;
            if (!PatToDouble(tok[k], d))
                break;                             // 笔划段非法: 丢弃剩余笔划
            dl.dashes.push_back(d);
        }
        patterns.back().lines.push_back(dl);
    }

    return !patterns.empty();
}

// ---------------------------------------------------------------- 2. 支持路径
bool PatHatchLoader::AddSupportPath(const TCHAR* lpszFolder)
{
    if (lpszFolder == NULL || lpszFolder[0] == _T('\0'))
        return false;

    CString folder = PatTrim(lpszFolder);
    while (folder.GetLength() > 1)                 // 去掉末尾 '\', 便于去重比较
    {
        TCHAR ch = folder.GetAt(folder.GetLength() - 1);
        if (ch != _T('\\') && ch != _T('/'))
            break;
        folder = folder.Left(folder.GetLength() - 1);
    }

    // 当前的支持路径保存在 "ACAD" 环境里(相当于 LISP 的 (getenv "ACAD"))
    TCHAR szCur[4096] = {0};
    if (acedGetEnv(_T("ACAD"), szCur) != RTNORM)
        return false;

    if (PatContainsNoCase(szCur, folder))
        return true;                               // 已存在, 幂等返回

    CString all(szCur);
    if (!all.IsEmpty())
        all += _T(";");
    all += folder;
    if (all.GetLength() >= 4095)
        return false;                              // 超长保护

    return (acedSetEnv(_T("ACAD"), (LPCTSTR)all) == RTNORM);
}

// ---------------------------------------------------------------- 内部辅助
static const PAT_PATTERN* PatSelect(const std::vector<PAT_PATTERN>& patterns,
                                    const TCHAR* lpszName)
{
    if (lpszName != NULL && lpszName[0] != _T('\0'))
    {
        CString want(lpszName);
        for (size_t i = 0; i < patterns.size(); ++i)
            if (patterns[i].name.CompareNoCase(want) == 0)
                return patterns[i].lines.empty() ? NULL : &patterns[i];
    }
    for (size_t i = 0; i < patterns.size(); ++i)
        if (!patterns[i].lines.empty())
            return &patterns[i];
    return NULL;
}
// ------------------------------------------------ 3a. 注册 pat 所在目录
bool PatHatchLoader::RegisterPatFile(const TCHAR* lpszPatPath)
{
    if (lpszPatPath == NULL || lpszPatPath[0] == _T('\0'))
        return false;

    TCHAR szDir[MAX_PATH] = {0};
    _tcsncpy(szDir, lpszPatPath, MAX_PATH - 1);

    //TCHAR* pSlash = _tcsrchr(szDir, _T('\\'));
    //if (pSlash == NULL) pSlash = _tcsrchr(szDir, _T('/'));
    //if (pSlash == NULL)
    //    return false;                              // 路径中没有目录部分
    //*pSlash = _T('\0');

    return AddSupportPath(szDir);                  // 幂等, 内部已去重
}
// ------------------------------------------------ 3b. 按名称创建填充
Acad::ErrorStatus PatHatchLoader::CreateHatchByName(const TCHAR* lpszPatternName,
                                                    const AcGePoint2dArray& vertices,
                                                    const AcGeDoubleArray& bulges,
                                                    double dScale,
                                                    double dAngleDeg,
                                                    const TCHAR* lpszLayer,
                                                    AcDbObjectId& hatchId)
{
    hatchId = AcDbObjectId::kNull;

    if (lpszPatternName == NULL || lpszPatternName[0] == _T('\0')
        || vertices.length() < 3)
        return Acad::eInvalidInput;

    AcDbBlockTableRecord* pMS = pModelSpace;       // 需先 actionBefore()

    AcDbHatch* pHatch = new AcDbHatch();
    pHatch->setNormal(AcGeVector3d::kZAxis);
    pHatch->setElevation(0.0);

    Acad::ErrorStatus es = pHatch->setPattern(AcDbHatch::kCustomDefined, lpszPatternName);
    if (es == Acad::eOk)
        es = pHatch->setPatternScale(dScale > 0.0 ? dScale : 1.0);
    if (es == Acad::eOk)
        es = pHatch->setPatternAngle(dAngleDeg * PAT_DEG2RAD);
    if (es == Acad::eOk)
        es = pHatch->appendLoop(AcDbHatch::kExternal, vertices, bulges);
    if (es == Acad::eOk)
        es = pHatch->evaluateHatch();              // AutoCAD 此时经支持路径解析定义
    if (es == Acad::eOk && lpszLayer != NULL && lpszLayer[0] != _T('\0'))
        es = pHatch->setLayer(lpszLayer);

    if (es == Acad::eOk)
        es = pMS->appendAcDbEntity(hatchId, pHatch);

    if (es == Acad::eOk) pHatch->close();
    else                 delete pHatch;
    return es;
}
Acad::ErrorStatus PatHatchLoader::CreateHatchFromPat(const TCHAR* lpszPatPath,
                                                     const TCHAR* lpszPatternName,
                                                     const AcGePoint2dArray& vertices,
                                                     const AcGeDoubleArray& bulges,
                                                     double dScale, double dAngleDeg,
                                                     const TCHAR* lpszLayer,
                                                     AcDbObjectId& hatchId)
{
    hatchId = AcDbObjectId::kNull;
    if (lpszPatPath == NULL || lpszPatPath[0] == _T('\0'))
        return Acad::eInvalidInput;

    // 1) 解析 pat, 解析出实际图案名(NULL -> 第一个有效图案)
    std::vector<PAT_PATTERN> patterns;
    if (!ParsePatFile(lpszPatPath, patterns))
        return Acad::eInvalidInput;
    const PAT_PATTERN* pPat = PatSelect(patterns, lpszPatternName);
    if (pPat == NULL)
        return Acad::eInvalidInput;

    // 2) 注册支持路径 + 3) 按名创建
    RegisterPatFile(lpszPatPath);                  // 失败交给 setPattern 报错
    return CreateHatchByName((LPCTSTR)pPat->name, vertices, bulges,
                             dScale, dAngleDeg, lpszLayer, hatchId);
}
// ---------------------------------------------------------------- 便捷接口
Acad::ErrorStatus PatHatchLoader::HatchClosedPolyline(const TCHAR* lpszPatPath,
                                                      const AcDbObjectId& plineId,
                                                      double dScale,
                                                      double dAngleDeg,
                                                      AcDbObjectId& hatchId)
{
    hatchId = AcDbObjectId::kNull;

    AcDbEntity* pEnt = NULL;
    Acad::ErrorStatus es = acdbOpenAcDbEntity(pEnt, plineId, AcDb::kForRead);
    if (es != Acad::eOk) return es;

    AcDbPolyline* pPline = AcDbPolyline::cast(pEnt);
    if (pPline == NULL)
    {
        pEnt->close();
        return Acad::eNotThatKindOfClass;          // 仅支持 LWPOLYLINE
    }

    AcGePoint2dArray vertices;
    AcGeDoubleArray bulges;
    int n = pPline->numVerts();
    for (int i = 0; i < n; ++i)
    {
        AcGePoint2d pt;
        double bulge = 0.0;
        if (pPline->getPointAt(i, pt) != Acad::eOk ||
            pPline->getBulgeAt(i, bulge) != Acad::eOk)
        {
            pEnt->close();
            return Acad::eInvalidInput;
        }
        vertices.append(pt);
        bulges.append(bulge);
    }
    pEnt->close();

    return CreateHatchFromPat((LPCTSTR)lpszPatPath, NULL, vertices, bulges,
                              dScale, dAngleDeg, NULL, hatchId);
}

// ---------------------------------------------------------------- 演示
static bool PatPickFile(CString& path)
{
    resbuf* rb = NULL;
    if (acedGetFileNavDialog(_T("选择填充图案文件(*.pat)"), NULL,
                             _T("pat"), _T("选择 PAT"), 0, &rb) != RTNORM)
        return false;
    if (rb == NULL || rb->restype != RTSTR || rb->resval.rstring == NULL)
    {
        if (rb) acutRelRb(rb);
        return false;
    }
    path = rb->resval.rstring;                     // ACHAR* 与 TCHAR 一致
    acutRelRb(rb);
    return true;
}

void PatHatchLoader_Demo(const TCHAR* lpszPatPath, double dScale)
{
    CString patPath;
    if (lpszPatPath != NULL && lpszPatPath[0] != _T('\0'))
        patPath = lpszPatPath;
    else if (!PatPickFile(patPath))
        return;

    AcGePoint2dArray rect;                         // 100 x 60 测试矩形
    rect.append(AcGePoint2d(0.0,   0.0));
    rect.append(AcGePoint2d(0.0,  60.0));
    rect.append(AcGePoint2d(100.0, 60.0));
    rect.append(AcGePoint2d(100.0,  0.0));
	rect.append(AcGePoint2d(0.0,   0.0));

	AcGeDoubleArray bulges;
    int i;
    for (i = 0; i < rect.length(); ++i)
    {
        bulges.append(0.0);
    }


    AcDbObjectId hatchId = AcDbObjectId::kNull;
	PatHatchLoader loader;
	if (loader.actionBefore()!=0){
		acutPrintf(_T("空间打开失败！\n"));
		return;
	}

    Acad::ErrorStatus es = loader.CreateHatchFromPat(
        (LPCTSTR)patPath, NULL, rect, bulges, dScale, 0.0, NULL, hatchId);

	loader.actionEnd();
    acutPrintf(es == Acad::eOk ? _T("\n图案填充创建成功。")
                               : _T("\n图案填充创建失败, ErrorStatus=%d。"), (int)es);
}