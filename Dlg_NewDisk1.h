#pragma once

#include "Dlg_DiskParams.h"
#include "CDiskRepartition.h"

// CDlg_NewDisk1 dialog

class CDlg_NewDisk1 : public CDialog
{
	DECLARE_DYNAMIC(CDlg_NewDisk1)

	void AutoDetectUSBDrive();

public:
	CDlg_NewDisk1(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlg_NewDisk1();

	BYTE file_blob[5032];		///< áëîá äèñêà èç ôàéë 
	DWORD file_blob_size;

	BOOL AddDiskBlobToFile( LPCTSTR FileName, ULARGE_INTEGER DiskSizeInBytes);
	void UpdateNewDiskInfo();

	FILETIME _modification_time;

	CDiskRepartition _theDiskPartition;

	CDlg_DiskParams dlgOptions;
	DISK_BLOB_INFO pKeyInfo;

// Dialog Data
	enum { IDD = IDD_DIALOG1 };
// Dialog Data
	//{{AFX_DATA(CDlg_NewDisk1)	
	CString	_password;
	CString	_password_confirmation;
	CString	_disk_info;
	int _option_based, _option_stegano_based, _option_partition_based; 
	//int _option_partition_based; 
	//}}AFX_DATA

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
public:
	virtual BOOL OnInitDialog();
public:
	afx_msg void OnBnClickedButton1();
public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
public:
	int CreateFile_WriteBlob(void);
	int CreateBackupBlob(void);
public:
	afx_msg void OnBnClickedOption();

public:
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
public:
	afx_msg void OnStnClickedStatic6();
protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
};
