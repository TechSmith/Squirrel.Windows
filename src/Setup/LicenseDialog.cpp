#include "stdafx.h"
#include "LicenseDialog.h"
#include "resource.h"
#include <richedit.h>
#include <sstream>

bool LicenseDialog::AcceptLicense()
{
   HMODULE hRichEditModule = LoadLibrary( L"Msftedit.dll" );
   INT_PTR returnVal = DoModal();
   ::FreeLibrary( hRichEditModule );
   return returnVal == IDOK;
}

bool LicenseDialog::ShouldShowLicense()
{
   HRSRC hRes = FindResourceEx( NULL, TEXT( "LICENSE" ), MAKEINTRESOURCE( IDR_LICENSE_RTF ), MAKELANGID( LANG_NEUTRAL, SUBLANG_NEUTRAL ) );
   return hRes && SizeofResource( NULL, hRes ) > 0;
}

DWORD CALLBACK EditStreamCallback( DWORD_PTR dwCookie,
                                   LPBYTE lpBuff,
                                   LONG cb,
                                   PLONG pcb )
{
   std::stringstream* stream = ( std::stringstream* )dwCookie;   
   
   stream->read( (char*)lpBuff, cb );
   if ( stream->bad() )
   {
      return -1;
   }
   *pcb = (LONG)stream->gcount();
   return 0;   
}

LRESULT LicenseDialog::OnInitDialog( UINT, WPARAM, LPARAM, BOOL& )
{
   CenterWindow();
   GetDlgItem( IDC_CONTINUE ).EnableWindow( FALSE );
   m_licenseText.Attach( GetDlgItem( IDC_LICENSE_TEXT ) );
   m_licenseText.SetEventMask( m_licenseText.GetEventMask() | ENM_LINK );
   LoadLicenseFromResources();
   return TRUE;
}

void LicenseDialog::LoadLicenseFromResources()
{
   EDITSTREAM es;
   HRSRC hResource = FindResourceEx( NULL, TEXT( "LICENSE" ), MAKEINTRESOURCE( IDR_LICENSE_RTF ), MAKELANGID( LANG_NEUTRAL, SUBLANG_NEUTRAL ) );
   if ( hResource )
   {
      HGLOBAL hGlobal = LoadResource( NULL, hResource );
      if ( hGlobal )
      {
         LPVOID bytes = LockResource( hGlobal );
         if ( bytes )
         {
            std::stringstream resourceStream( static_cast<const char *>( bytes ) );
            es.dwCookie = (DWORD)&resourceStream;
            es.dwError = 0;
            es.pfnCallback = EditStreamCallback;
            m_licenseText.StreamIn( SF_RTF, es );
         }
         ::FreeResource( hGlobal );
      }
   }
}

LRESULT LicenseDialog::OnLink( int idCtrl, LPNMHDR pnmh, BOOL& )
{
   ENLINK* pnml = reinterpret_cast<ENLINK*>( pnmh );

   if ( pnml->msg == WM_LBUTTONDOWN ||
      ( pnml->msg == WM_KEYDOWN && pnml->wParam == VK_RETURN ) )
   {
      CString url;
      m_licenseText.GetTextRange( pnml->chrg.cpMin, pnml->chrg.cpMax, url.GetBuffer( pnml->chrg.cpMax- pnml->chrg.cpMin) );
      ShellExecute( NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL );
      return 1;
   }

   return 0;
}

LRESULT LicenseDialog::OnClose( UINT, WPARAM, LPARAM, BOOL& )
{
   EndDialog( IDCLOSE );
   return TRUE;
}

LRESULT LicenseDialog::OnAccept( WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled )
{
   if ( IsDlgButtonChecked( IDC_ACCEPT ) )
   {
      GetDlgItem( IDC_CONTINUE ).EnableWindow( TRUE );
   }   
   return 0;
}

LRESULT LicenseDialog::OnContinue( WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled )
{
   EndDialog( IDOK );
   return TRUE;
}

LRESULT LicenseDialog::OnDecline( WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled )
{  
   if ( !IsDlgButtonChecked( IDC_DECLINE ) )
   {
      return TRUE;
   }

   ::SetFocus( GetDlgItem( IDC_ACCEPT).m_hWnd );
   GetDlgItem( IDC_CONTINUE ).EnableWindow( FALSE );

   return TRUE;
}

LRESULT LicenseDialog::OnPrint( WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled )
{
   PRINTDLG pd = { sizeof(pd) };
   pd.lStructSize = sizeof( pd );
   pd.hInstance = NULL;
   pd.hwndOwner = ::GetActiveWindow();
   pd.hDevMode = NULL;
   pd.hDevNames = NULL;
   pd.Flags = PD_ALLPAGES | PD_HIDEPRINTTOFILE | PD_NOPAGENUMS | PD_RETURNDC;
   pd.nCopies = 1;

   if ( PrintDlg(&pd) )
   {
      HDC hdc = pd.hDC;
      PrintRTF( GetDlgItem( IDC_LICENSE_TEXT ).m_hWnd, hdc );
   }
   return TRUE;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/bb787875(v=vs.85).aspx
BOOL LicenseDialog::PrintRTF( HWND hwnd, HDC hdc )
{
   DOCINFO di = { sizeof( di ) };

   if ( !StartDoc( hdc, &di ) )
   {
      return FALSE;
   }

   int cxPhysOffset = GetDeviceCaps( hdc, PHYSICALOFFSETX );
   int cyPhysOffset = GetDeviceCaps( hdc, PHYSICALOFFSETY );

   int cxPhys = GetDeviceCaps( hdc, PHYSICALWIDTH );
   int cyPhys = GetDeviceCaps( hdc, PHYSICALHEIGHT );

   int dpiX = GetDeviceCaps( hdc, LOGPIXELSX );
   int dpiY = GetDeviceCaps( hdc, LOGPIXELSY );
   const int TWIPS_PER_INCH = 1440;
   SendMessage( hwnd, WM_SETREDRAW, FALSE, 0 );
   SendMessage( hwnd, EM_SETTARGETDEVICE, (WPARAM)hdc, cxPhys );

   FORMATRANGE fr;

   fr.hdc = hdc;
   fr.hdcTarget = hdc;

   // Set page rect to physical page size in twips.
   fr.rcPage.top = 0;
   fr.rcPage.left = 0;
   fr.rcPage.right = MulDiv( cxPhys, TWIPS_PER_INCH, dpiX );
   fr.rcPage.bottom = MulDiv( cyPhys, TWIPS_PER_INCH, dpiY );

   // Set the rendering rectangle to the printable area of the page.
   fr.rc.left = MulDiv( cxPhysOffset, TWIPS_PER_INCH, dpiX );
   fr.rc.right = fr.rc.left + MulDiv( cxPhys, TWIPS_PER_INCH, dpiX ) - 4 * MulDiv( cxPhysOffset, TWIPS_PER_INCH, dpiX );
   fr.rc.top = MulDiv( cyPhysOffset, TWIPS_PER_INCH, dpiY );
   fr.rc.bottom = fr.rc.top + MulDiv( cyPhys, TWIPS_PER_INCH, dpiY ) - 4 * MulDiv( cyPhysOffset, TWIPS_PER_INCH, dpiY );

   SendMessage( hwnd, EM_SETSEL, 0, (LPARAM)-1 );          // Select the entire contents.
   SendMessage( hwnd, EM_EXGETSEL, 0, (LPARAM)&fr.chrg );  // Get the selection into a CHARRANGE.

   BOOL fSuccess = TRUE;

   // Use GDI to print successive pages.
   while ( fr.chrg.cpMin < fr.chrg.cpMax && fSuccess )
   {
      fSuccess = StartPage( hdc ) > 0;

      if ( !fSuccess ) break;

      int cpMin = SendMessage( hwnd, EM_FORMATRANGE, TRUE, (LPARAM)&fr );

      if ( cpMin <= fr.chrg.cpMin )
      {
         fSuccess = FALSE;
         break;
      }

      fr.chrg.cpMin = cpMin;
      fSuccess = EndPage( hdc ) > 0;
   }
   SendMessage( hwnd, EM_SETSEL, 0, (LPARAM)0 );
   SendMessage( hwnd, EM_FORMATRANGE, FALSE, 0 );
   SendMessage( hwnd, EM_SETTARGETDEVICE, NULL, 0);
   SendMessage( hwnd, WM_SETREDRAW, TRUE, 0 );
   ::RedrawWindow( hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW );

   if ( fSuccess )
   {
      EndDoc( hdc );
   }
   else
   {
      AbortDoc( hdc );
   }

   return fSuccess;
}