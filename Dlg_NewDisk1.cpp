/**********************************************************************
** Copyright (C) 2005-2015 Tesline-Service S.R.L. All rights reserved.
**
** Rohos Disk Browser, Rohos Mini Drive Portable.
** 
**
** This file may be distributed and/or modified under the terms of the
** GNU Affero General Public license version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file. If not, see <http://www.gnu.org/licenses/>
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.rohos.com/ for GPL licensing information.
**
** Contact info@rohos.com http://rohos.com
**/


#include "stdafx.h"
#include "DiskBrowserApp.h"
#include "Dlg_NewDisk1.h"
#include "WinIoctl.H"
#include "Wincrypt.h"
#include <dbt.h>

// CDlg_NewDisk1 dialog

extern char Key[32];
extern char IV[64];
extern int encryption_mode ;

// disk data offset in disk image (in MB-ytes)
#define DEFAULT_DISK_DATA_GAP	1 
#define RH_BLOB_VERSION 0x20000


bool FormatImage(HANDLE hImageFile, LARGE_INTEGER disk_size, HWND hWnd, LPCTSTR fs, CDiskRepartition *partitionClass, int diskOffset);
bool FormatRealDrive(HWND hWnd, LPCTSTR fs, LPCTSTR drive);

IMPLEMENT_DYNAMIC(CDlg_NewDisk1, CDialog)

CDlg_NewDisk1::CDlg_NewDisk1(CWnd* pParent /*=NULL*/)
	: CDialog(CDlg_NewDisk1::IDD, pParent)
{
	_option_based = 0; 
	_option_partition_based = _option_stegano_based = 0;
	
}

CDlg_NewDisk1::~CDlg_NewDisk1()
{
}

void CDlg_NewDisk1::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlg_NewDisk1)		
	DDX_Text(pDX, IDC_EDIT1, _password);
	DDX_Text(pDX, IDC_EDIT2, _password_confirmation);
	DDX_Text(pDX, IDC_STATIC1, _disk_info);
	DDX_Radio(pDX, IDC_RADIO1, _option_based);
	//DDX_Radio(pDX, IDC_RADIO2, _option_partition_based);
	//DDX_Radio(pDX, IDC_RADIO3, _option_stegano_based);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlg_NewDisk1, CDialog)
	ON_BN_CLICKED(IDOK, &CDlg_NewDisk1::OnBnClickedOk)
	ON_BN_CLICKED(IDC_BUTTON1, &CDlg_NewDisk1::OnBnClickedButton1)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_RADIO1, &CDlg_NewDisk1::OnBnClickedOption)
	ON_BN_CLICKED(IDC_RADIO2, &CDlg_NewDisk1::OnBnClickedOption)
	ON_BN_CLICKED(IDC_RADIO3, &CDlg_NewDisk1::OnBnClickedOption)
	ON_WM_CTLCOLOR()
	ON_STN_CLICKED(IDC_STATIC6, &CDlg_NewDisk1::OnStnClickedStatic6)
END_MESSAGE_MAP()


// CDlg_NewDisk1 message handlers

extern CFsys _FSys;// Для работы с файловой системой

// create disk-image file, BLOG, append BLOB, backup BLOB, format NTFS.
void CDlg_NewDisk1::OnBnClickedOk()
{
	UpdateData();

	if ( _password != _password_confirmation )
	{
		AfxMessageBox(IDS_PASW_NOT_MATCH);
		return;
	}

	if (dlgOptions._partition_based )
	{
		
		CString s1; 

		s1 = LS(IDS_AREYOU_SURE);
		s1 += "\n\n";
		s1 += LS(IDS_USB_DRIVE_REPARTITIONED);
		
		if ( AfxMessageBox(s1, MB_YESNO) == IDNO) // YES - allow prev USB key to login here.
				return;

	}

	_FSys.CloseVolume(); // close current volume

	AfxGetApp()->BeginWaitCursor();


	//dlgOptions._DiskSizeInBytes.u.LowPart = 8000;
	//dlgOptions._DiskSizeInBytes.QuadPart *= 1048576;/* bytes in 1 mb*/;		
	//dlgOptions._path = "\\\\.\\F:"; 

	if ( CreateFile_WriteBlob()  )	
	{	
		HANDLE hFile=INVALID_HANDLE_VALUE;

		LARGE_INTEGER disk_size;
		disk_size.QuadPart = dlgOptions._DiskSizeInBytes.QuadPart;		

		DWORD data_offset_mb  = DEFAULT_DISK_DATA_GAP;
		
		if (dlgOptions._partition_based )
		{
			// offset to the disk data; Format() need to start from this offset
			_theDiskPartition.partOffset2.QuadPart = MAKELONG(pKeyInfo.Reserved1[5], pKeyInfo.Reserved1[6] )/*in MB*/ * 1024 * 1024; 

		}
		else
		{
			// open disk image file for Virtual Format

			if ( IsFileUseSteganos(dlgOptions._path) == true)
			{
				DWORD last_mb;
				if ( !GetSteganosOffset(dlgOptions._path, &data_offset_mb, &last_mb) ) {
					CString s1;
					s1.Format(LS(IDS_WRONG_FOR_STEGANOS), dlgOptions._path);
					AfxMessageBox(s1, MB_ICONINFORMATION);
					return ;
				}	
			}

			hFile = CreateFile( dlgOptions._path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL,OPEN_EXISTING , FILE_ATTRIBUTE_NORMAL, NULL );
			if (hFile==INVALID_HANDLE_VALUE)
				hFile = CreateFile( dlgOptions._path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

			if (hFile==INVALID_HANDLE_VALUE)
			{
				WriteLog("Error opening file");
			}
		}
		
		// format new partition 		

		if (!FormatImage(hFile, disk_size, m_hWnd, "NTFS", &_theDiskPartition, data_offset_mb))
		{
			AfxGetApp()->EndWaitCursor();

			CString s1 = LS(IDS_ERR_FORMAT);
			int c = s1.Find(".");
			if (c > 0)
				s1 = s1.Mid(0, c);
			AfxMessageBox( s1, MB_ICONWARNING );

			//UpdateData(false);			
			
		}

		if (dlgOptions._partition_based == true)
		{
			// format the open part of Usb drive
			if ( !FormatRealDrive(m_hWnd, "FAT32", dlgOptions._path) )
			{
				if ( !FormatRealDrive(m_hWnd, "FAT", dlgOptions._path) )
					AfxMessageBox("An Error occured while formatting Open partition on USB Drive. Please close programm, re-insert drive and Format by Windows.");
			}
			_theDiskPartition.CloseDisk();

			CreateBackupBlob();
			// create buckup real blob on drive  dlgOptions._path


		} else
		{
			CreateBackupBlob();
			CloseHandle(hFile);
			SetFileTime(hFile, NULL, NULL, &_modification_time);
		}

		
		AfxGetApp()->EndWaitCursor();

		CString s1 = LS(IDS_USB_PROTECT_OK_FS_INFO);
		int c = s1.Find("."); // вывести сообщение до точки.
		if (c > 0)
			s1 = s1.Mid(0, c);

		AfxMessageBox( s1, MB_ICONINFORMATION );		

		OnOK();
		return;

	} 
		
		
	
}

BOOL CDlg_NewDisk1::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetTimer(1, 500, NULL);
	SetWindowText( LS(IDS_NEWDISK_DLG) );
	SetDlgItemText(IDC_STATIC3, LS(IDS_L_PASSW) );
	SetDlgItemText(IDC_STATIC4, LS(IDS_L_CONFIRM_PASSW) );
	

	SetClassLong( GetDlgItem(IDC_STATIC6)->m_hWnd, GCL_HCURSOR, (LONG)LoadCursor(NULL,IDC_HAND) );
	
	//SetDlgItemText(IDC_STATIC2, LS(IDS_L_CONFIRM_PASSW) ); 
	//SetDlgItemText(IDC_BUTTON1, LS(IDS_OPTIONS) ); 

	CDC *dc = GetDC();
	int nHeight = -MulDiv(8, GetDeviceCaps(dc->m_hDC, LOGPIXELSY), 72);
	HFONT hBoldFont = CreateFont(nHeight, 0, 0, 0, FW_NORMAL, 0, 1, 0,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, VARIABLE_PITCH|FF_SWISS, TEXT("MS Shell Dlg") );		

	SendDlgItemMessage( IDC_STATIC6, WM_SETFONT , (WPARAM)hBoldFont, MAKELPARAM(1,0) );

	SetDlgItemText(IDC_STATIC5, LS(IDS_DISK_FOLDER) ); 

	// TODO:  Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

// change disk properties
void CDlg_NewDisk1::OnBnClickedButton1()
{
	if ( dlgOptions.DoModal() != IDOK)
		return; 

	UpdateNewDiskInfo();


	// TODO: Add your control notification handler code here
}

void CDlg_NewDisk1::AutoDetectUSBDrive()
{
	TCHAR path[500] = "F:\\";

	LPCTSTR s = GetUSB_Disk_old(true, 0);
	if (strlen(s))
		path[0] = *s;
	else
		GetPodNogamiPath(path, 0 );

	path[3]=0;

	dlgOptions._encrypt_USB_drive = path;
	dlgOptions.SetDefaultValues();
	UpdateNewDiskInfo();

}

void CDlg_NewDisk1::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default

	if ( nIDEvent ==  1)
	{
		KillTimer(1);
		AutoDetectUSBDrive();
		
		
	}

	CDialog::OnTimer(nIDEvent);
}


#define PD_BLOCK_SIZE		16

// 
int CDlg_NewDisk1::CreateBackupBlob()
{
	CString backup_path = dlgOptions._path;

	if (dlgOptions._partition_based == true)	
	{
		
		BYTE key[KEY_SIZE+16] = "";
		BYTE iv[IV_SIZE+16] = "";	

		memset( iv, 0, IV_SIZE );
		memset( key, 0, KEY_SIZE );	

		if ( !DeriveKeyFromPassw_v2((LPCTSTR)_password, key) ) {		
			return false;
		}	

		// for partition based - make back up 

		// backup partition size for emergency case.
		LARGE_INTEGER li = _theDiskPartition.GetPartitionedSpace();
		pKeyInfo.Flags[0] = li.LowPart;							
		pKeyInfo.Flags[1] = li.HighPart;						
																
		memcpy(file_blob, &pKeyInfo, sizeof(DISK_BLOB_INFO));	
		file_blob_size = sizeof(DISK_BLOB_INFO);				

		if( !EncryptBuffer( file_blob, &file_blob_size, sizeof(DISK_BLOB_INFO) + PD_BLOCK_SIZE ,key, iv ) ) {
			//setError( IDS_INT_ERR, IDS_ERR_CRYPTO_KEY_INFO);
			return FALSE;
		}	

		backup_path += "_backup.rdi1";
	}
	else 
		backup_path += ".rdi1";

	try{
		/*
		since 2.0 dont create backup files - since it may contain old password..s

		CFile f(backup_path, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary );
		f.Write(file_blob, file_blob_size);
		f.Write("ROHO", 4);
		f.Write(&file_blob_size, 4);
		f.Close();			
		*/
	}

	catch (...)
	{
	}

}

/** create disk-image file with 'dlgOptions.*' and writes disk image blog next to the image file (file_name.rdi1).
*/
int CDlg_NewDisk1::CreateFile_WriteBlob()
{

	BYTE key[KEY_SIZE+16] = "";
	BYTE iv[IV_SIZE+16] = "";	


	// generate randoms 
	HCRYPTPROV hProv = 0;
	if( !CryptAcquireContext( &hProv, NULL, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) ||		
		!CryptGenRandom( hProv, KEY_SIZE, key ) ||		
		!CryptGenRandom( hProv, IV_SIZE, iv ) )
	{
		WriteLog( "CreateFile_WriteBlob, failed to Crypt**, GLE:%X", GetLastError());
		
	}

	CryptGenRandom( hProv, 5000, file_blob );		
	memset( &pKeyInfo, 0, sizeof( DISK_BLOB_INFO ) );
	CryptGenRandom( hProv, sizeof(DISK_BLOB_INFO), (LPBYTE)&pKeyInfo );	

	
	BYTE Key1[KEY_SIZE]; DWORD i=0;
	DeriveKeyFromPassw_v2(dlgOptions._password, Key1);
	EncryptBuffer((LPBYTE)&pKeyInfo, &(i=0), sizeof( DISK_BLOB_INFO ), Key1, iv);
	EncryptBuffer((LPBYTE)key, &(i=0), KEY_SIZE, Key1, iv);


	// create disk blob

	pKeyInfo.Version = RH_BLOB_VERSION;
	memcpy( pKeyInfo.Key, key, KEY_SIZE );
	memcpy( pKeyInfo.IV, iv, IV_SIZE );

	CryptGenRandom( hProv, DISK_MAX_PATH, (BYTE*)pKeyInfo.FileName );
	strcpy( pKeyInfo.FileName, dlgOptions._path );

	CryptGenRandom( hProv, 12, (BYTE*)pKeyInfo.AlgorithmName );
	strcpy( pKeyInfo.AlgorithmName, dlgOptions._algoritm );
	pKeyInfo.DiskNumber = dlgOptions._name[0] - 'A';

	int disk_data_offset = 1; // disk data offset inside disk container (not the same as steganos offset)

	CryptGenRandom( hProv, sizeof(DWORD) * 30 + 600 + 1024, (BYTE*)pKeyInfo.Flags);

	pKeyInfo.Reserved1[5] = 0;
	pKeyInfo.Reserved1[6] = 0;

	if ( IsFileUseSteganos(dlgOptions._path) == false)
	{
		// new for rohos Disk 1.8 (partition based encryption, + hidden partitions inside blob)
		pKeyInfo.Reserved1[5] = LOWORD(disk_data_offset);
		pKeyInfo.Reserved1[6] = HIWORD(disk_data_offset);
	}

	dlgOptions._DiskSizeInBytes.QuadPart -= DEFAULT_DISK_DATA_GAP * 1048576;/* bytes in 1 mb*/;



	pKeyInfo.DiskSizeLow = dlgOptions._DiskSizeInBytes.u.LowPart;
	pKeyInfo.DiskSizeHigh = dlgOptions._DiskSizeInBytes.u.HighPart;
	pKeyInfo.BlockSize = 512;
	pKeyInfo.Reserved1[0] = 3; /*Use version 3 Encryption. encryption_mode setup*/
	pKeyInfo.DiskFlags = DISK_FLAGS_VISIBLE_IMAGE | DISK_FLAGS_FIXED_MEDIA;
	pKeyInfo.KeySizeInBytes =  KEY_SIZE;

	
	
	DWORD LastError = ERROR_SUCCESS;	

	memset( iv, 0, IV_SIZE );
	memset( key, 0, KEY_SIZE );	

	if ( !DeriveKeyFromPassw_v2((LPCTSTR)_password, key) ) {		
		return false;
	}	

	memcpy(file_blob, &pKeyInfo, sizeof(DISK_BLOB_INFO));
	file_blob_size = sizeof(DISK_BLOB_INFO);

	if( !EncryptBuffer( file_blob, &file_blob_size, sizeof(DISK_BLOB_INFO) + PD_BLOCK_SIZE ,key, iv ) ) {
		//setError( IDS_INT_ERR, IDS_ERR_CRYPTO_KEY_INFO);
		return FALSE;
	}	


	if (dlgOptions._partition_based == false)
	{
		HANDLE hFile;

		if ( IsFileUseSteganos(dlgOptions._path) == false)
		{
			// create SPARSE-FILE.  disk image file.
			
			if( ( hFile = CreateFile( dlgOptions._path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL ) ) == INVALID_HANDLE_VALUE ) {
				WriteLog(  "AddDiskBlobToFile. CreateFile %08X", GetLastError());

				if( ( hFile = CreateFile( dlgOptions._path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ) == INVALID_HANDLE_VALUE ) {
					WriteLog(  "AddDiskBlobToFile. CreateFile2 %08X", GetLastError());
					return FALSE;
				} 		
				//return FALSE;
			}

			// using sparce files
			DWORD tmp=0;
			if (!DeviceIoControl (hFile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &tmp, NULL))
			{
				WriteLog("Sparce ON failed", GetLastError() );
			}
		} else
		{
			if( ( hFile = CreateFile( dlgOptions._path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ) == INVALID_HANDLE_VALUE ) 
				WriteLog(  "AddDiskBlobToFile. CreateFile3 %08X", GetLastError());

		}

		GetFileTime(hFile, NULL, NULL, &_modification_time);

		CloseHandle(hFile);
	}


	
	AddDiskBlobToFile(dlgOptions._path, dlgOptions._DiskSizeInBytes); // write disk_blob to File or Partition 	

	// now we are ready to write FS format to the raw disk.

	int argc =4;
	char *argv = new char[100]; 

	//sprintf(argv, ""

	// initialize encryption models
	memcpy( IV, pKeyInfo.IV, 64 ); 
	memcpy( Key, pKeyInfo.Key, 32 );
	encryption_mode = pKeyInfo.Reserved1[0];

	/*if ( 0 == mkntfs_main((LPCTSTR)dlgOptions._path, dlgOptions._DiskSizeInBytes.QuadPart) )
	{		
		return 1;
	}*/

	return 1;

	
}


/** writes 'file_blob' (encrypted) into FileName.rdi1 file.
*/
BOOL CDlg_NewDisk1::AddDiskBlobToFile( LPCTSTR FileName, ULARGE_INTEGER DiskSizeInBytes)
{
	HANDLE hFile;
	//long hi;
	DWORD lo;
	DWORD LastError = ERROR_SUCCESS;

	if (dlgOptions._partition_based == true)
	{
		_theDiskPartition.SetDriveLetter(FileName[0]);

		_theDiskPartition.CheckDisk();

		LARGE_INTEGER liDriveSpace = _theDiskPartition.GetDriveSpace();

		/*if (liDriveSpace.QuadPart < DiskSizeInBytes.QuadPart)
		{
			WriteLog("not enought space on %s",FileName);
		}*/

		_theDiskPartition.partOffset.QuadPart = 0;
		_theDiskPartition.partOffset2.QuadPart =0; // clean this Offset to write disk blob to the begginnig.

		LARGE_INTEGER li; 
		li.QuadPart = DiskSizeInBytes.QuadPart;

		//_theDiskPartition.SetUnpartitionedSpace(li);

		if ( _theDiskPartition.RePartition(li) == false)
		{
			WriteLog("error repartition, %s, %X", FileName, DiskSizeInBytes.QuadPart / (1024*1024) );
			_theDiskPartition.CloseDisk();
			return false;
		}

		_theDiskPartition.CloseDisk();

		if (!_theDiskPartition.CheckDisk())
		{
			return false;
		}

		DWORD file_blob_size_512 = file_blob_size;

		if ( file_blob_size_512 % 512 ) 
			file_blob_size_512 += (512 - (file_blob_size_512 % 512)); // len should be equal to %512
		
		if( !_theDiskPartition.WriteData(file_blob, file_blob_size_512, &_theDiskPartition.partOffset) )		
		{
			WriteLog("error writting blob, %s, %X", FileName, DiskSizeInBytes.QuadPart / (1024*1024) );
			
			_theDiskPartition.CloseDisk();
			return FALSE;
		}
		int kbytes_offs = _theDiskPartition._partitioned_space.QuadPart / 1024;		
		WriteLog("WriteDiskBlob at %X %X", _theDiskPartition.partOffset, kbytes_offs);

		//_theDiskPartition.CloseDisk(); // do not close the disk , formating next...

		return true;
	}

	

	DWORD disk_size_mb = DiskSizeInBytes.QuadPart / 1048576;/* bytes in 1 mb*/

	DWORD disk_size_kb = DiskSizeInBytes.QuadPart / 1024;/* bytes in 1 mb*/

	WriteLog("CDiskComponent::AddDiskBlobToFile ... %d (%d kb)", disk_size_mb, disk_size_kb);

	if( !SetFileAttributes( FileName, 0 ) ){
		WriteLog(  "AddDiskBlobToFile. SetFileAttributes %08X", GetLastError());
		return FALSE;
	}

	if( ( hFile = CreateFile( FileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ) == INVALID_HANDLE_VALUE ) {
		WriteLog(  "AddDiskBlobToFile. CreateFile %08X", GetLastError());
		return FALSE;
	}

	LARGE_INTEGER i;
			DWORD move_at = FILE_END;
			i.QuadPart =0;

	if ( IsFileUseSteganos(FileName) ) {
		// using stegano's , store data inside AVI
		// äàííûé â AVI èäóò ÷åðåç 10Ìá ò.å. ÷òîáû ôèëüì ïîêàçûâàëñÿ.
		// êîíöîâêà ôàéëà ýòî ðàçìåð AVI - 50 êá. 

		DWORD begin_mb, last_mb;
		if ( GetSteganosOffset(FileName, &begin_mb, &last_mb) ) {			

			//use_steganos = true;
			move_at = FILE_END;
			i.QuadPart =0;
			i.QuadPart -= last_mb*1024*1024;
			i.QuadPart -= file_blob_size;


			if ( !SetFilePointerEx(hFile, i, NULL, move_at) ) {
				LastError = GetLastError();
				WriteLog(  "AddDiskBlobToFile. SetFilePointerEx %08X. %08X. %08X.", LastError, disk_size_mb, move_at);
				CloseHandle( hFile );
				SetLastError( LastError );
				return FALSE;
			}
		}
	} 	

	// since 2.8 write disk blob to the beggingin.

	if( !WriteFile( hFile, file_blob, file_blob_size, &( lo = 0L ), NULL ) || lo != file_blob_size )		
	{
		LastError = GetLastError();
		WriteLog(  "AddDiskBlobToFile. WriteFile %08X. %08X. %08X. %08X.", LastError, file_blob, file_blob_size, lo );
		CloseHandle( hFile );
		SetLastError( LastError );
		return FALSE;
	}

	/*if ( IsFileUseSteganos(FileName) == false) {
		// äëÿ steganos ýòî ìîæíî íå çàïèñûâàòü...
		WriteFile( hFile, "ROHO", 4, &(lo=0L), NULL );
		WriteFile( hFile, (BYTE*)(&file_blob_size), 4, &( lo = 0L ), NULL );
	}*/

	CloseHandle( hFile );
	return TRUE;
}

// clicked to change "Create disk within ..."
void CDlg_NewDisk1::OnBnClickedOption()
{	
	UpdateData();
	if (_option_based == 0) // file container
	{
		//AfxMessageBox("cont");
		//dlgOptions.
		AutoDetectUSBDrive();
		SetDlgItemText(IDC_STATIC6, "Change...");
		//enableItems(m_hWnd, 1, IDC_BUTTON1, 0);
		showItems(m_hWnd, 0, IDC_STATIC6, 0);
		dlgOptions._path = "";
		dlgOptions._partition_based = false;
		dlgOptions.SetDefaultValues();
		UpdateNewDiskInfo();

		return;
	}

	if (_option_based == 2) // partition 
	{
		//AfxMessageBox("part");
		AutoDetectUSBDrive();
		SetDlgItemText(IDC_STATIC6, "Change...");
		//enableItems(m_hWnd, 1, IDC_BUTTON1, 0);
		showItems(m_hWnd, 0, IDC_STATIC6, 0);
		dlgOptions._partition_based = true;
		dlgOptions.SetDefaultValues();
		UpdateNewDiskInfo();

		return;
	}

	if (_option_based == 1) // steganos
	{
		SetDlgItemText(IDC_STATIC6, "Choose media file (AVI, MP3, WMA,...)");

		//AfxMessageBox("part");
		//enableItems(m_hWnd, 0, IDC_BUTTON1, 0);
		showItems(m_hWnd, 1, IDC_STATIC6, 0);
		dlgOptions._path = "";
		dlgOptions._partition_based = false;
		dlgOptions._encrypt_USB_drive = "";
		//dlgOptions.SetDefaultValues();

		_disk_info = "";
		UpdateData(false);
		//UpdateNewDiskInfo();

		return;
	}
}


// update information about new disk to be created
// 
void CDlg_NewDisk1::UpdateNewDiskInfo()
{
	/*if (dlgOptions._partition_based == true)
		_disk_info = dlgOptions._ImagePathForDisplay;
	else
		_disk_info = dlgOptions._ImagePathForDisplay;

	
	_disk_info += "\n";
	_disk_info += dlgOptions._sizemb;
	_disk_info += " (" + dlgOptions._algoritm + ")";*/

	if (_option_based == 1) // steganos
	{
		_disk_info.Format("Encrypted container %s\nDisk size will be %s (%s)", dlgOptions._ImagePathForDisplay, dlgOptions._sizemb,dlgOptions._algoritm );

	} else
		_disk_info.Format("USB Device %s has been found\nEncrypted partition size will be %s (%s)", dlgOptions._encrypt_USB_drive, dlgOptions._sizemb, dlgOptions._algoritm );


		UpdateData(false);
}
HBRUSH CDlg_NewDisk1::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	int id = pWnd->GetDlgCtrlID();

	if (nCtlColor == CTLCOLOR_STATIC ) {		
		if (id == IDC_STATIC6 )
			pDC->SetTextColor( RGB(0,0,228) );			
	}

	// TODO:  Return a different brush if the default is not desired
	return hbr;
}

void CDlg_NewDisk1::OnStnClickedStatic6()
{
	static char *img_pth=NULL;
	
	

	TCHAR filter[100];
	memcpy(filter, LS(IDS_ALLFILES_FILTER), 90);
	img_pth = file_dlg(filter, (LPSTR)LS("txtChooseAVI"), 0, NULL, "" );

	if(!img_pth)
	{
		WriteLog("!img_pth");
		return;
	}

	if ( false == IsFileUseSteganos(img_pth) ) 
	{
		CString s1;
		s1.Format(LS(IDS_WRONG_FOR_STEGANOS), img_pth);
		AfxMessageBox(s1, MB_ICONINFORMATION);
		return;
	}

	DWORD begin_mb, last_mb, file_size_MB;
	if ( !GetSteganosOffset(img_pth, &begin_mb, &last_mb, &file_size_MB) ) {
		CString s1;
		s1.Format(LS(IDS_WRONG_FOR_STEGANOS), img_pth);
		AfxMessageBox(s1, MB_ICONINFORMATION);

		return ;
	}	

	dlgOptions._path = img_pth;			
	dlgOptions.SetDefaultValues();

	UpdateNewDiskInfo();

}

LRESULT CDlg_NewDisk1::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	// TODO: Add your specialized code here and/or call the base class

	if (message == WM_DEVICECHANGE && wParam == DBT_DEVICEARRIVAL )
	{
		TCHAR path[500] = "F:\\";

		LPCTSTR s = GetUSB_Disk_old(true, lParam);
		if (strlen(s))
			path[0] = *s;
		else
			GetPodNogamiPath(path, 0 );

		path[3]=0;

		dlgOptions._encrypt_USB_drive = path;
		dlgOptions.SetDefaultValues();
		UpdateNewDiskInfo();

	}

	return CDialog::WindowProc(message, wParam, lParam);
}
