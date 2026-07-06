#pragma once
#include "StdAfx.h"
#include "CPalette.h"

// 定义控件 ID，避免冲突
#define ID_PROP_LABEL_1     1001
#define ID_PROP_EDIT_1      1002
#define ID_PROP_LABEL_2     1003
#define ID_PROP_EDIT_2      1004

class CPropertyTablePalette : public CAdUiPalette
{
    DECLARE_DYNCREATE(CPropertyTablePalette)

public:
    CPropertyTablePalette();
    virtual ~CPropertyTablePalette();

    // 控件成员
    CStatic m_Label1;
    CEdit   m_Edit1;
    CStatic m_Label2;
    CEdit   m_Edit2;

    // 数据成员 (存储实际值)
    CString m_strProp1;
    CString m_strProp2;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnEnChangeEdit1();
    afx_msg void OnEnChangeEdit2();

    DECLARE_MESSAGE_MAP()

private:
    void UpdateDataFromControls(); // 从控件读取数据到变量
    void UpdateControlsFromData(); // 从变量更新控件显示
};