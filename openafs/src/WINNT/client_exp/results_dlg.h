/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

class CResultsDlg : public CDialog
{
	CStringArray m_Files;
	CStringArray m_Results;
	CString m_strDlgTitle;
	CString m_strResultsTitle;
	DWORD m_nHelpID;

// Construction
public:
	CResultsDlg(DWORD nHelpID, CWnd* pParent = NULL);   // standard constructor

	void SetContents(const CString& strDlgTitle, const CString& strResultsTitle, const CStringArray& files, const CStringArray& results);

// Dialog Data
	//{{AFX_DATA(CResultsDlg)
	enum { IDD = IDD_RESULTS };
	CStatic	m_ResultsLabel;
	CListBox	m_List;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CResultsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CResultsDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
