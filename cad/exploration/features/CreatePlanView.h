#pragma once
#include "arxHeaders.h"
#include "./CreateBaseView.h"

// 前向声明 BaseMapCfg（实际定义见 .cpp 匿名 namespace）
// 该结构体仅在 CCreatePlanView 内部使用，不污染头文件
struct BaseMapCfg;

class CCreatePlanView : public CCreateBaseView
{
public:
	CCreatePlanView(void);

	void Execute();
public:
	//~CCreatePlanView(void);

protected:
	cJSON* s_blockConfigData;
	TCHAR* pszStyleName;


	virtual void ClearExtDbCache();
	void ChangeText(AcDbObjectId blkDefId,AcDbObjectId styleId, ACHAR* holeName,ACHAR* elevation,ACHAR* depth);
	//创建图例
	//创建图例（样例孔的孔号/标高/孔深取第一个孔的值）
    void CreateLegend(std::vector<CString>& arrUsedHoles,
					const CString& fistCadBlockUrl,
                    const CString& blockStyleName);

    // ---- 底图导入 + 出图比例（V1.6 P0-4）----
    // 主入口（流程一/二共用）：流程一无底图时内部打印告警并跳过，返回 false
    bool DrawBaseMap();
    // 导入委托方底图 DWG（含对齐决策、BASEPOINT 校正、固定名块参照置灰）
    bool ImportBaseMap(const cJSON* cfg);
    // 出图比例：对整体成图统一缩放；D6 甲方案——无 BASEPOINT 特判
    bool ApplyPlotScale(double plotScaleDenom);
    // 清旧底图-现状图层 + 旧底图块定义（幂等重建）
    void EraseBaseMapLayers();
    // 底图块参照置灰（内部辅助，dim=true 时调用）
    void DimBaseMapReference();
    // V1.6 BASEPOINT 校正矩阵（静态纯函数；D6 甲方案中在 ApplyPlotScale 不调用，
    //   仅作为 BASEPOINT 校正语义的可复用数学工具，主校正由 ImportBaseMap 内"插入点
    //   前修正"完成。返回矩阵 M = T(insert)·R(θ)·S·T(B)·T(-B)，对源 BASEPOINT=B
    //   的点 P 施加 M·P 可得 "扣基点后再缩放旋转平移" 的目标位置）
    static AcGeMatrix3d BuildImportMatrix(
        const AcGePoint3d& ptInsert,
        double dScale,
        double dRotation,
        const AcGePoint3d& sourceBasePoint);
    // 安全读 JSON 数字字段：缺键 / 非 number → 返回默认值；无告警
    static double GetJsonNumber(const cJSON* obj, const TCHAR* key, double def);

    // ---- 两点配准（V1.6 P0-6 数组化）----
    // 解析 base_map.registration 节点，校验 [x,y] 数组格式；
    // 1~2 对；任一解析失败 → 返回 false（不告警，由调用方统一 WARN）
    static bool ParseRegistration(const cJSON* bm,
                                  std::vector<AcGePoint2d>& source,
                                  std::vector<AcGePoint2d>& target);
    // 四参数相似变换最小二乘（两点 mode：s, θ, T；三参数回填）；
    // 1 对点 mode：s, θ 取 cfg 显式值，仅算 T；
    // 两点距 <1e-6 / 结果非有限值 / s≤0 → 返回 false
    static bool ComputeRegistration(double sAlign,
                                    double rotAlign,
                                    const std::vector<AcGePoint2d>& source,
                                    const std::vector<AcGePoint2d>& target,
                                    double& sOut,
                                    double& rotOut,
                                    double& insertXOut,
                                    double& insertYOut);

    // ---- 成员（V1.6 P0-4；class 级，蓝图 §2.4 冻结外露口径）----
    // 底图配置；ImportBaseMap 写入、ApplyPlotScale 读取 BASEPOINT/insert
    BaseMapCfg m_baseCfg;
    // 出图比例分母；DrawBaseMap 写入（≤0 = 未配置，不缩放）、Execute 末尾传给 ApplyPlotScale
    double     m_plotScale       = 0.0;
    // 底图块参照 ObjectId（流程二路径有效，流程一为 null）
    AcDbObjectId m_pBaseMapBlkRefId;
    // 是否启用两点配准（m_regSourcePoints/m_regTargetPoints 仅在 true 时有意义）
    bool       m_regMode        = false;
    // 配准源点（DWG 内部坐标），1~2 个点
    std::vector<AcGePoint2d> m_regSourcePoints;
    // 配准目标点（钻孔坐标系坐标），与 m_regSourcePoints 同点数
    std::vector<AcGePoint2d> m_regTargetPoints;
};