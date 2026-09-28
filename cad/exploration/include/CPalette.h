// coding: gb18030
//
//
// (C) Copyright 2000-2006 by Autodesk, Inc. 
//
// Permission to use, copy, modify, and distribute this software in
// object code form for any purpose and without fee is hereby granted, 
// provided that the above copyright notice appears in all copies and 
// that both that copyright notice and the limited warranty and
// restricted rights notice below appear in all supporting 
// documentation.
//
// AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS. 
// AUTODESK SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTY OF
// MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE.  AUTODESK, INC. 
// DOES NOT WARRANT THAT THE OPERATION OF THE PROGRAM WILL BE
// UNINTERRUPTED OR ERROR FREE.
//
// Use, duplication, or disclosure by the U.S. Government is subject to 
// restrictions set forth in FAR 52.227-19 (Commercial Computer
// Software - Restricted Rights) and DFAR 252.227-7013(c)(1)(ii)
// (Rights in Technical Data and Computer Software), as applicable.
//
//


#pragma once

//-----------------------------------------------------------------------------
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "afxctl.h"
#include "aduiPalette.h"
#include "resource.h"
#include "HtmlPaletteView.h"
#include "BaseMarker.h"
/////////////////////////////////////////////////////////////////////////////
// Custom Palette class

class AiApiRequest;
class AiChat;
class CDataPalette : public CAdUiPalette 
{ 
	DECLARE_DYNCREATE(CDataPalette)
public:
	CDataPalette();
	virtual ~CDataPalette();

	CHtmlPaletteView* m_pHtmlView; // 改为新类型

	
	public:
    // Load the data from xml.
    virtual BOOL Load(IUnknown* pUnk);
    // Save the data to xml.
    virtual BOOL Save(IUnknown* pUnk);
    // Called by the palette set when the palette is made active
    virtual void OnSetActive();
	//Called by AutoCAD to steal focus from the palette
	virtual bool CanFrameworkTakeFocus();

protected:
	DECLARE_MESSAGE_MAP()
	//afx_msg LRESULT HandleJsCommandInternal(WPARAM wParam, LPARAM lParam);
public:
	afx_msg int OnCreate (LPCREATESTRUCT lpCreateStruct);
	//afx_msg void OnDestroy(); 
	afx_msg void OnSize(UINT nType, int cx, int cy);
	// 新增：处理来自 HTML 视图的命令
    LRESULT HandleJsCommand(LPCTSTR szCommand, LPCTSTR szParam);
	
	// 新增：供视图调用，更新 HTML 显示
    void UpdateHtmlDisplay();
private:
	// Load and save palette information
	void LoadPalette();
	void SavePalette();

	void onPageReady();
	void SetDiameter(CBaseMarker& marker);

	void clean();

	afx_msg LRESULT HandleApiCommand(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT HandleChatReply(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT HandleChatError(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT onAcadKeepFocus(WPARAM wParam, LPARAM lParam);
private:
	AiApiRequest* hServer;
	AiChat* hChat;

};

