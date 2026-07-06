#include "StdAfx.h"
#include "PropertyTablePalette.h"

IMPLEMENT_DYNCREATE(CPropertyTablePalette, CAdUiPalette)

BEGIN_MESSAGE_MAP(CPropertyTablePalette, CAdUiPalette)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_EN_CHANGE(ID_PROP_EDIT_1, &CPropertyTablePalette::OnEnChangeEdit1)
    ON_EN_CHANGE(ID_PROP_EDIT_2, &CPropertyTablePalette::OnEnChangeEdit2)
END_MESSAGE_MAP()

CPropertyTablePalette::CPropertyTablePalette()
{
    m_strProp1 = _T("");
    m_strProp2 = _T("");
}

CPropertyTablePalette::~CPropertyTablePalette()
{
}

int CPropertyTablePalette::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CAdUiPalette::OnCreate(lpCreateStruct) == -1)
        return -1;

    // 布局参数
    int nLeftMargin = 10;      // 左边距
    int nLabelWidth = 80;      // 标签宽度
    int nEditWidth = 150;      // 输入框宽度
    int nRowHeight = 25;       // 行高
    int nStartY = 10;          // 起始Y坐标

    // --- 第一行属性 ---
    // 创建标签 "Property 1:"
    m_Label1.Create(_T("属性 1:"), SS_LEFT | WS_VISIBLE | WS_CHILD, 
                    CRect(nLeftMargin, nStartY, nLeftMargin + nLabelWidth, nStartY + nRowHeight), 
                    this, ID_PROP_LABEL_1);
    
    // 创建输入框，位于标签右侧
    m_Edit1.Create(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | WS_VISIBLE | WS_CHILD, 
                   CRect(nLeftMargin + nLabelWidth + 5, nStartY, nLeftMargin + nLabelWidth + 5 + nEditWidth, nStartY + nRowHeight), 
                   this, ID_PROP_EDIT_1);

    // --- 第二行属性 ---
    int nRow2Y = nStartY + nRowHeight + 5; // 下一行 Y 坐标
    
    m_Label2.Create(_T("属性 2:"), SS_LEFT | WS_VISIBLE | WS_CHILD, 
                    CRect(nLeftMargin, nRow2Y, nLeftMargin + nLabelWidth, nRow2Y + nRowHeight), 
                    this, ID_PROP_LABEL_2);
                    
    m_Edit2.Create(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | WS_VISIBLE | WS_CHILD, 
                   CRect(nLeftMargin + nLabelWidth + 5, nRow2Y, nLeftMargin + nLabelWidth + 5 + nEditWidth, nRow2Y + nRowHeight), 
                   this, ID_PROP_EDIT_2);

    // 设置面板标题
    SetName(_T("属性表"));

    // 初始化界面显示
    UpdateControlsFromData();

    return 0;
}

void CPropertyTablePalette::OnDestroy()
{
    // 销毁前保存数据
    UpdateDataFromControls();
    // TODO: 在此处调用类似 SavePalette() 的逻辑保存 XML
    CAdUiPalette::OnDestroy();
}

// 将界面上的文字读取到成员变量
void CPropertyTablePalette::UpdateDataFromControls()
{
    m_Edit1.GetWindowText(m_strProp1);
    m_Edit2.GetWindowText(m_strProp2);
}

// 将成员变量的文字显示到界面上
void CPropertyTablePalette::UpdateControlsFromData()
{
    m_Edit1.SetWindowText(m_strProp1);
    m_Edit2.SetWindowText(m_strProp2);
}

void CPropertyTablePalette::OnEnChangeEdit1()
{
    UpdateDataFromControls();
    // TODO: 如果需要实时响应，可以在此触发其他逻辑
}

void CPropertyTablePalette::OnEnChangeEdit2()
{
    UpdateDataFromControls();
}