/*=============================================================================
    FrSplash.h: Application splash dialog.
    Copyright Dec.2016 Vlad Gordienko.
=============================================================================*/

/*-----------------------------------------------------------------------------
    CSplash.
-----------------------------------------------------------------------------*/

//
// A splash.
//
class CSplash
{
public:
	// CSplash interface.
	CSplash( LPCTSTR BitmapID );
	~CSplash();

private:
	// Splash internal.
	HWND		hWnd;
	HBITMAP		hBitmap;
	Integer		XSize;
	Integer		YSize;
};


/*-----------------------------------------------------------------------------
    CSplash implementation.
-----------------------------------------------------------------------------*/

//
// Splash dialog WinProc.
//
LRESULT CALLBACK SplashWndProc( HWND, UINT, WPARAM, LPARAM )
{ 
	return 0; 
}


//
// Splash constructor.
//
CSplash::CSplash( LPCTSTR BitmapID )
{
	TStaticBitmap* Bitmap	= LoadBitmapFromResource(BitmapID);
	{
		BITMAPINFO*	Info	= (BITMAPINFO*)MemAlloc(sizeof(BITMAPINFO)+sizeof(RGBQUAD)*Bitmap->Palette.Colors.Num());
		HDC			hDc		= GetDC(nullptr);

		Info->bmiHeader.biBitCount		= 8;
		Info->bmiHeader.biClrImportant	= 
		Info->bmiHeader.biClrUsed		= Bitmap->Palette.Colors.Num();
		Info->bmiHeader.biCompression	= 0;
		Info->bmiHeader.biHeight		= Bitmap->VSize;
		Info->bmiHeader.biPlanes		= 1;
		Info->bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
		Info->bmiHeader.biSizeImage		= Bitmap->GetBlockSize();
		Info->bmiHeader.biWidth			= Bitmap->USize;
		Info->bmiHeader.biXPelsPerMeter	= 
		Info->bmiHeader.biYPelsPerMeter	= 2834;

		// Flip palette RGBA -> BGR.
		for( Integer i=0; i<Bitmap->Palette.Colors.Num(); i++ )
		{
			TColor Col = Bitmap->Palette.Colors[i];
			Info->bmiColors[i].rgbBlue		= Col.B;
			Info->bmiColors[i].rgbGreen		= Col.G;
			Info->bmiColors[i].rgbRed		= Col.R;
			Info->bmiColors[i].rgbReserved	= 0;
		}

		// Store size.
		XSize		= Bitmap->USize;
		YSize		= Bitmap->VSize;

		// Flip image.
		Byte*	Data	= (Byte*)Bitmap->GetData();
		Byte	Buffer[1024];
		for( Integer V=0; V<Bitmap->VSize/2; V++ )
		{
			MemCopy( Buffer, &Data[V*Bitmap->USize], Bitmap->USize );
			MemCopy( &Data[V*Bitmap->USize], &Data[(Bitmap->VSize-V-1)*Bitmap->USize], Bitmap->USize );
			MemCopy( &Data[(Bitmap->VSize-V-1)*Bitmap->USize], Buffer, Bitmap->USize );
		}

		// To hBitmap;
		hBitmap		= CreateDIBitmap
		(
			hDc,
			&Info->bmiHeader,
			CBM_INIT,
			Bitmap->GetData(),
			Info,
			DIB_RGB_COLORS
		);
		ReleaseDC( nullptr, hDc );
		MemFree( Info );
	}
	delete Bitmap;
	
	// Allocate dialog.
	hWnd	= CreateDialog
	(
		GEditor->hInstance,
		MAKEINTRESOURCE(IDD_SPLASH),
		nullptr,
		(DLGPROC)SplashWndProc
	);
		
	if( hWnd )
	{
		HWND hWndLogo = GetDlgItem( hWnd, IDC_LOGO );
		if( hWndLogo )
		{
			SetWindowPos
			( 
				hWnd, 
				HWND_TOPMOST, 
				(GetSystemMetrics(SM_CXSCREEN)-XSize)/2, 
				(GetSystemMetrics(SM_CYSCREEN)-YSize)/2, 
				XSize, YSize, 
				SWP_SHOWWINDOW 
			);
			SetWindowPos( hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
			SendMessage( hWndLogo, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap );
			UpdateWindow( hWnd );
		}
	}
}


//
// Splash destructor.
//
CSplash::~CSplash()
{
	DestroyWindow(hWnd);
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/