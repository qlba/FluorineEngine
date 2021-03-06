/*=============================================================================
    FrResExp.cpp: Resource exporting.
    Copyright Dec.2016 Vlad Gordienko.
=============================================================================*/

#include "Editor.h"

/*-----------------------------------------------------------------------------
    Bitmap exporters.
-----------------------------------------------------------------------------*/

//
// Export image to bmp or tga file.
//
Bool ExportBitmap( FBitmap* Bitmap, String Directory )
{
	assert(Bitmap && Bitmap->IsValidBlock());

	if( String::Pos( L".bmp", String::LowerCase(Bitmap->FileName) ) != -1 )
	{
		//
		// Export bmp file.
		//
		CFileSaver Saver(Directory+L"\\"+Bitmap->FileName);

		BITMAPFILEHEADER BmpHeader;
		BITMAPINFOHEADER BmpInfo;

		BmpHeader.bfType		= 0x4d42;
		BmpHeader.bfSize		= sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+Bitmap->VSize*Bitmap->USize;
		BmpHeader.bfReserved1	= 0;
		BmpHeader.bfReserved2	= 0;
		BmpHeader.bfOffBits		= sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);

		BmpInfo.biSize			= sizeof(BITMAPINFOHEADER);
		BmpInfo.biWidth			= Bitmap->USize;
		BmpInfo.biHeight		= Bitmap->VSize;
		BmpInfo.biPlanes		= 1;
		BmpInfo.biBitCount		= 24;
		BmpInfo.biCompression	= BI_RGB;
		BmpInfo.biSizeImage		= 0;
		BmpInfo.biXPelsPerMeter	= 0;
		BmpInfo.biYPelsPerMeter	= 0;
		BmpInfo.biClrUsed		= 0;
		BmpInfo.biClrImportant	= 0;

		Saver.SerializeData( &BmpHeader, sizeof(BITMAPFILEHEADER) );
		Saver.SerializeData( &BmpInfo, sizeof(BITMAPINFOHEADER) );

		if( Bitmap->Format == BF_Palette8 )
		{
			// Unpack to save as RGB.
			Byte*	Data	= (Byte*)Bitmap->GetData();
			for( Integer V=0; V<Bitmap->VSize; V++ )
			{
				Byte* Line	= &Data[(Bitmap->VSize-V-1) << Bitmap->UBits];

				for( Integer U=0; U<Bitmap->USize; U++ )
				{
					TColor	C			= Bitmap->Palette.Colors[Line[U]];
					Byte	Buffer[3]	= { C.B, C.G, C.R };
					Saver.SerializeData( Buffer, 3 );
				}
			}
		}
		else
		{
			// Save as RGB.
			TColor* Data	= (TColor*)Bitmap->GetData();
			for( Integer V=0; V<Bitmap->VSize; V++ )
			{
				TColor* Line	= &Data[(Bitmap->VSize-V-1) << Bitmap->UBits];

				for( Integer U=0; U<Bitmap->USize; U++ )
				{
					Byte Buffer[3] = { Line[U].B, Line[U].G, Line[U].R };
					Saver.SerializeData( Buffer, 3 );
				}
			}
		}
	}
	else if( String::Pos( L".tga", String::LowerCase(Bitmap->FileName) ) != -1 )
	{
		//
		// Export tga file.
		//
#pragma pack(push,1)
	struct TGAHeader
	{
		Byte		FileType;
		Byte		ColorMapType;
		Byte		ImageType;
		Byte		ColorMapSpec[5];
		Byte		OrigX[2];
		Byte		OrigY[2];
		Byte		Width[2];
		Byte		Height[2];
		Byte		BPP;
		Byte		ImageInfo;
	};
#pragma pack(pop)
		CFileSaver Saver(Directory+L"\\"+Bitmap->FileName);
		TGAHeader	TgaHeader;

		TgaHeader.FileType			= 0;
		TgaHeader.ColorMapType		= 0;
		TgaHeader.ImageType			= 2;	// Uncompressed.
		TgaHeader.OrigX[0]			= 
		TgaHeader.OrigX[1]			= 
		TgaHeader.OrigY[0]			= 
		TgaHeader.OrigY[1]			= 0;
		TgaHeader.Width[0]			= Bitmap->USize;
		TgaHeader.Width[1]			= Bitmap->USize / 256;
		TgaHeader.Height[0]			= Bitmap->VSize;
		TgaHeader.Height[1]			= Bitmap->VSize / 256;
		TgaHeader.BPP				= 32;
		TgaHeader.ImageInfo			= 0;

		Saver.SerializeData( &TgaHeader, sizeof(TGAHeader) );

		// Save as RGBA.
		TColor* Data	= (TColor*)Bitmap->GetData();
		for( Integer V=0; V<Bitmap->VSize; V++ )
		{
			TColor* Line	= &Data[(Bitmap->VSize-V-1) << Bitmap->UBits];

			for( Integer U=0; U<Bitmap->USize; U++ )
			{
				Byte Buffer[4] = { Line[U].B, Line[U].G, Line[U].R, Line[U].A };
				Saver.SerializeData( Buffer, 4 );
			}
		}
	}
	else
		return false;
}


/*-----------------------------------------------------------------------------
    Script exporters.
-----------------------------------------------------------------------------*/

//
// Export to flu file.
//
Bool ExportScript( FScript* Script, String Directory )
{
	assert(Script);
	
	if( Script->bHasText )
	{
		CTextWriter	TextFile(Directory+L"\\"+Script->FileName);
		for( Integer iLine=0; iLine<Script->Text.Num(); iLine++ )
			TextFile.WriteString( Script->Text[iLine] );
		return true;
	}
	else
		return false;
}


/*-----------------------------------------------------------------------------
    Sound exporters.
-----------------------------------------------------------------------------*/

//
// Export to wav file.
//
Bool ExportSound( FSound* Sound, String Directory )
{
	assert(Sound && Sound->IsValidBlock());

	CFileSaver Saver(Directory+L"\\"+Sound->FileName);
	Saver.SerializeData( Sound->GetData(), Sound->GetBlockSize() );

	return true;
}


/*-----------------------------------------------------------------------------
    Editor functions.
-----------------------------------------------------------------------------*/

//
// Export resource to file, such as FBitmap -> .bmp, Sound -> .wav.
// Return true, if successfully saved. If some error occurs or
// resource type are unsupported return false.
// bOverride - should we override file if it exists? It's important
// for large resources.
//
Bool CEditor::ExportResource( FResource* Res, String Directory, Bool bOverride )
{
	assert(Res && Directory);

	// File already exists.
	if( !bOverride && GPlat->FileExists(Directory+L"\\"+Res->FileName) )
		return false;

	if( Res->IsA(FSound::MetaClass) )
	{
		// Export sound file.
		return ExportSound( As<FSound>(Res), Directory );
	}
	else if( Res->IsA(FScript::MetaClass) )
	{
		// Export script to file.
		return ExportScript( As<FScript>(Res), Directory );
	}
	else if( Res->IsA(FBitmap::MetaClass) )
	{
		// Export bitmap to file.
		return ExportBitmap( As<FBitmap>(Res), Directory );
	}
	else
	{
		// Unsupported type.
		warn( L"Failed save '%s'. Unsupported resource type '%s'", *Res->GetName(), *Res->GetClass()->Name );
		return false;
	}
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/