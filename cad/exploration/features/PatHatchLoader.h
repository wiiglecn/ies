// ============================================================================
//  PatHatchLoader.h
//  功能: 读取外部 .pat 填充图案文件, 在 AutoCAD 中创建使用该图案的填充实体
//  平台: AutoCAD 2007 / ObjectARX 2007 (VS2005, Unicode)
//  说明: 独立模块, 不依赖工程内其他文件
// ============================================================================
#pragma once
#include "arxHeaders.h"
#include "../tools/ToolDrawBase.h"

// .pat 中的一条图案定义行:  angle, baseX, baseY, dX, dY [, dash...]
struct PAT_DASHLINE
{
    double angle;                 // 线角度(度)
    double baseX, baseY;          // 该线族基准原点
    double offsetX, offsetY;      // 族内相邻线增量(delta)
    std::vector<double> dashes;   // 笔划序列; 空=实线, 正=落笔, 负=抬笔, 0=点
    PAT_DASHLINE() : angle(0), baseX(0), baseY(0), offsetX(0), offsetY(0) {}
};

// .pat 中的一个完整图案
struct PAT_PATTERN
{
    CString name;            // 图案名( * 后面、逗号前面 )
    CString description;     // 描述(逗号后)
    std::vector<PAT_DASHLINE> lines;
};

class PatHatchLoader : public ToolDrawBase
{
public:
    // ---- 1. 解析 ----
    // 解析 .pat 文件(可含多个 *NAME 图案), 返回全部图案定义
    bool ParsePatFile(const TCHAR* lpszPatPath,
                             std::vector<PAT_PATTERN>& patterns);

    // ---- 2. 注册 ----
    // 把目录加入 AutoCAD "支持文件搜索路径"(ACAD 环境变量), 幂等。
    // 成功后 pat 中的图案即可被 HATCH 命令和 kCustomDefined 填充使用
    bool AddSupportPath(const TCHAR* lpszFolder);

        // ---- 3a. 注册 ----
    // 提取 lpszPatPath 所在目录并加入 AutoCAD 支持文件搜索路径(幂等)。
    // 成功后 pat 中的图案即可被 HATCH 命令和 kCustomDefined 填充按名称使用
    bool RegisterPatFile(const TCHAR* lpszPatPath);

    // ---- 3b. 创建填充 ----
    // 直接以图案名创建非关联填充(前提: 该名称已能被 AutoCAD 通过支持路径解析,
    // 即已调用过 RegisterPatFile / AddSupportPath), 并加入模型空间
    Acad::ErrorStatus CreateHatchByName(const TCHAR* lpszPatternName,
                                        const AcGePoint2dArray& vertices, // 闭合边界(多段线环, OCS)
                                        const AcGeDoubleArray& bulges,    // 与顶点数一致, 可传空
                                        double dScale,                    // 图案比例
                                        double dAngleDeg,                 // 图案角度(度)
                                        const TCHAR* lpszLayer,           // NULL=当前图层
                                        AcDbObjectId& hatchId);           // [out] 新实体 id

    Acad::ErrorStatus CreateHatchFromPat(const TCHAR* lpszPatPath,
                                                     const TCHAR* lpszPatternName,
                                                     const AcGePoint2dArray& vertices,
                                                     const AcGeDoubleArray& bulges,
                                                     double dScale, double dAngleDeg,
                                                     const TCHAR* lpszLayer,
                                                     AcDbObjectId& hatchId);
                                                                                      
    // 便捷: 对一个已存在的闭合多段线(LWPOLYLINE)做填充
    Acad::ErrorStatus HatchClosedPolyline(const TCHAR* lpszPatPath,
                                                 const AcDbObjectId& plineId,
                                                 double dScale,
                                                 double dAngleDeg,
                                                 AcDbObjectId& hatchId);
};

// 演示/自测: lpszPatPath 为空时弹文件对话框选择 pat, 在模型空间画 100x60 矩形并填充
void PatHatchLoader_Demo(const TCHAR* lpszPatPath = NULL, double dScale = 0.5);
