#pragma once
#include "arxHeaders.h"
#include "./CreateBaseView.h"

// V1.6 P0-4 底图配置（详设 §2.4）
// 必须在头文件里完整定义：CCreatePlanView 有值类型成员 m_baseCfg
struct BaseMapCfg {
    CString     dwg;                              // 底图 DWG 路径（相对 ARX_DIR）
    double      insertX;                           // 插入点 X 坐标
    double      insertY;                           // 插入点 Y 坐标
    double      scale;                            // 缩放比例（未配置时默认1）
    double      rotation;                         // 旋转角度（弧度）
    CString     layer;                            // 目标图层（空=用默认"底图-现状"）
    bool        dim;                              // 是否置灰
    double      plotScale;                        // 出图比例分母（<=0=未配置）
    AcGePoint3d sourceBasePoint;                  // BASEPOINT
    bool        hasBasePoint;                     // 是否成功读到 BASEPOINT
    BaseMapCfg() : insertX(0.0), insertY(0.0), scale(1.0), rotation(0.0),
                   dim(false), plotScale(0.0),
                   sourceBasePoint(AcGePoint3d::kOrigin), hasBasePoint(false) {}
};

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
	//����ͼ��
	//����ͼ���������׵Ŀ׺�/���/����ȡ��һ���׵�ֵ��
    void CreateLegend(std::vector<CString>& arrUsedHoles,
					const CString& fistCadBlockUrl,
                    const CString& blockStyleName);

    // ---- ��ͼ���� + ��ͼ������V1.6 P0-4��----
    // ����ڣ�����һ/�����ã�������һ�޵�ͼʱ�ڲ���ӡ�澯������������ false
    bool DrawBaseMap();
    // ����ί�з���ͼ DWG����������ߡ�BASEPOINT У�����̶���������ûң�
    bool ImportBaseMap(const cJSON* cfg);
    // ��ͼ�������������ͼͳһ���ţ�D6 �׷��������� BASEPOINT ����
    bool ApplyPlotScale(double plotScaleDenom);
    // ��ɵ�ͼ-��״ͼ�� + �ɵ�ͼ�鶨�壨�ݵ��ؽ���
    void EraseBaseMapLayers();
    // ��ͼ������ûң��ڲ�������dim=true ʱ���ã�
    void DimBaseMapReference();
    // V1.6 BASEPOINT У�����󣨾�̬��������D6 �׷������� ApplyPlotScale �����ã�
    //   ����Ϊ BASEPOINT У������Ŀɸ�����ѧ���ߣ���У���� ImportBaseMap ��"�����
    //   ǰ����"��ɡ����ؾ��� M = T(insert)��R(��)��S��T(B)��T(-B)����Դ BASEPOINT=B
    //   �ĵ� P ʩ�� M��P �ɵ� "�ۻ������������תƽ��" ��Ŀ��λ�ã�
    static AcGeMatrix3d BuildImportMatrix(
        const AcGePoint3d& ptInsert,
        double dScale,
        double dRotation,
        const AcGePoint3d& sourceBasePoint);
    // ��ȫ�� JSON �����ֶΣ�ȱ�� / �� number �� ����Ĭ��ֵ���޸澯
    static double GetJsonNumber(const cJSON* obj, const TCHAR* key, double def);

    // ---- ������׼��V1.6 P0-6 ���黯��----
    // ���� base_map.registration �ڵ㣬У�� [x,y] �����ʽ��
    // 1~2 �ԣ���һ����ʧ�� �� ���� false�����澯���ɵ��÷�ͳһ WARN��
    static bool ParseRegistration(const cJSON* bm,
                                  std::vector<AcGePoint2d>& source,
                                  std::vector<AcGePoint2d>& target);
    // �Ĳ������Ʊ任��С���ˣ����� mode��s, ��, T�������������
    // 1 �Ե� mode��s, �� ȡ cfg ��ʽֵ������ T��
    // ����� <1e-6 / ���������ֵ / s��0 �� ���� false
    static bool ComputeRegistration(double sAlign,
                                    double rotAlign,
                                    const std::vector<AcGePoint2d>& source,
                                    const std::vector<AcGePoint2d>& target,
                                    double& sOut,
                                    double& rotOut,
                                    double& insertXOut,
                                    double& insertYOut);

    // ---- ��Ա��V1.6 P0-4��class ������ͼ ��2.4 ������¶�ھ���----
    // ��ͼ���ã�ImportBaseMap д�롢ApplyPlotScale ��ȡ BASEPOINT/insert
    BaseMapCfg m_baseCfg;
    // ��ͼ������ĸ��DrawBaseMap д�루��0 = δ���ã������ţ���Execute ĩβ���� ApplyPlotScale
    double     m_plotScale;
    // ��ͼ����� ObjectId�����̶�·����Ч������һΪ null��
    AcDbObjectId m_pBaseMapBlkRefId;
    // �Ƿ�����������׼��m_regSourcePoints/m_regTargetPoints ���� true ʱ�����壩
    bool       m_regMode;
    // ��׼Դ�㣨DWG �ڲ����꣩��1~2 ����
    std::vector<AcGePoint2d> m_regSourcePoints;
    // ��׼Ŀ��㣨�������ϵ���꣩���� m_regSourcePoints ͬ����
    std::vector<AcGePoint2d> m_regTargetPoints;
};