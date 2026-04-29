#pragma once
#include "resource.h"

// CLauncherDlg dialog

class CLauncherDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CLauncherDlg)

public:
	CLauncherDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CLauncherDlg();

	// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_LAUNCHER_DIALOG };
#endif

    // Selected output
    int m_nType;        // 1 = Document, 2 = Grid
    int m_nGridSubType; // 0 = Tabulator, 1 = AG Grid

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedRadioType();
    afx_msg void OnBnClickedOk();

	DECLARE_MESSAGE_MAP()
private:
    CButton m_radioDoc;
    CButton m_radioGrid;
    CComboBox m_comboGridType;
};
