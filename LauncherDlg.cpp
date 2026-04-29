#include "pch.h"
#include "WebView2App.h"
#include "LauncherDlg.h"

// CLauncherDlg dialog

IMPLEMENT_DYNAMIC(CLauncherDlg, CDialogEx)

CLauncherDlg::CLauncherDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_LAUNCHER_DIALOG, pParent)
    , m_nType(1)
    , m_nGridSubType(0)
{
}

CLauncherDlg::~CLauncherDlg()
{
}

void CLauncherDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_RADIO_DOC, m_radioDoc);
    DDX_Control(pDX, IDC_RADIO_GRID, m_radioGrid);
    DDX_Control(pDX, IDC_COMBO_GRIDTYPE, m_comboGridType);
}


BEGIN_MESSAGE_MAP(CLauncherDlg, CDialogEx)
    ON_BN_CLICKED(IDC_RADIO_DOC, &CLauncherDlg::OnBnClickedRadioType)
    ON_BN_CLICKED(IDC_RADIO_GRID, &CLauncherDlg::OnBnClickedRadioType)
    ON_BN_CLICKED(IDOK, &CLauncherDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CLauncherDlg message handlers

BOOL CLauncherDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Init combo
    m_comboGridType.AddString(_T("Tabulator"));
    m_comboGridType.AddString(_T("AG Grid"));
    m_comboGridType.SetCurSel(0);

    // Default to Document
    m_radioDoc.SetCheck(BST_CHECKED);
    m_comboGridType.EnableWindow(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
}

void CLauncherDlg::OnBnClickedRadioType()
{
    if (m_radioGrid.GetCheck() == BST_CHECKED) {
        m_comboGridType.EnableWindow(TRUE);
    } else {
        m_comboGridType.EnableWindow(FALSE);
    }
}

void CLauncherDlg::OnBnClickedOk()
{
    if (m_radioGrid.GetCheck() == BST_CHECKED) {
        m_nType = 2; // Grid
        m_nGridSubType = m_comboGridType.GetCurSel();
    } else {
        m_nType = 1; // Doc
    }

    CDialogEx::OnOK();
}

