/*=============================================================================
    FrFile.h: ANCI C style binary and text files loading & saving.
    Copyright Jul.2016 Vlad Gordienko.
=============================================================================*/

/*-----------------------------------------------------------------------------
    CFileLoader.
-----------------------------------------------------------------------------*/

//
// Serializer to load data from disc.
//
class CFileLoader: public CSerializer
{
public:
	String	FileName;
	FILE*	File;

	// CFileLoader interface.
	CFileLoader( String InFileName )
		: FileName( InFileName )
	{
		Mode	= SM_Load;
		File	= _wfopen( *FileName, L"rb" );
		if( !File )
			error( L"File \"%s\" not found", *FileName );
	}
	~CFileLoader()
	{
		fclose( File );
	}

	// CSerializer interface.
	void SerializeData( void* Mem, DWord Count )
	{
		fread( Mem, Count, 1, File );
	}
	void SerializeRef( FObject*& Obj )
	{
		Integer Id;
		Serialize( *this, Id );
		Obj	= Id != -1 ? GObjectDatabase->GObjects[Id] : nullptr;
	}
	DWord TotalSize()
	{
		DWord OldPos = ftell( File );
		fseek( File, 0, SEEK_END );
		DWord Size = ftell( File );
		fseek( File, OldPos, SEEK_SET );
		return Size;
	}
	void Seek( DWord NewPos )
	{
		if( fseek( File, NewPos, SEEK_SET ) )
			error( L"Seek failed %d in \"%s\"", NewPos, *FileName );
	}
	DWord Tell()
	{
		return ftell( File );
	}
};


/*-----------------------------------------------------------------------------
    CFileSaver.
-----------------------------------------------------------------------------*/

//
// Serializer to write data to disc.
//
class CFileSaver: public CSerializer
{
public:
	String	FileName;
	FILE*	File;

	// CFileSaver interface.
	CFileSaver( String InFileName )
		: FileName( InFileName )
	{
		Mode	= SM_Save;
		File	= _wfopen( *FileName, L"wb" );
		if( !File )
			error( L"File \"%s\" not found", *FileName );
	}
	~CFileSaver()
	{
		fclose( File );
	}

	// CSerializer interface.
	void SerializeData( void* Mem, DWord Count )
	{
		fwrite( Mem, Count, 1, File );
	}
	void SerializeRef( FObject*& Obj )
	{
		Integer Id	= Obj ? Obj->GetId() : -1;
		Serialize( *this, Id );
	}
	DWord TotalSize()
	{
		DWord OldPos = ftell( File );
		fseek( File, 0, SEEK_END );
		DWord Size = ftell( File );
		fseek( File, OldPos, SEEK_SET );
		return Size;
	}
	void Seek( DWord NewPos )
	{
		if( fseek( File, NewPos, SEEK_SET ) )
			error( L"Seek failed %d in \"%s\"", NewPos, *FileName );
	}
	DWord Tell()
	{
		return ftell( File );
	}
};


/*-----------------------------------------------------------------------------
    CTextReader.
-----------------------------------------------------------------------------*/

//
// Text file reader.
//
class CTextReader
{
public:
	String	FileName;
	FILE*	File;

	// CTextReader interface.
	CTextReader( String InFileName )
		: FileName( InFileName )
	{
		File	= _wfopen( *FileName, L"r" );
		if( !File )
			error( L"File \"%s\" not found", *FileName );
	}
	~CTextReader()
	{
		fclose( File );
	}
	String ReadLine()
	{
		Char Buffer[2048] = {};
		
		if( fgetws( Buffer, 2048, File ) )
		{
			// Very ugly.
			for( Integer i=0; i<2048; i++ )
				if( Buffer[i] == L'\n' )
				{
					Buffer[i] = L'\0';
					break;
				}

			return Buffer;
		}
		else
			return L"";
	}
	Bool IsEOF()
	{
		return feof( File );
	}
};


/*-----------------------------------------------------------------------------
    CTextWriter.
-----------------------------------------------------------------------------*/

//
// Text file writer.
//
class CTextWriter
{
public:
	String	FileName;
	FILE*	File;

	// CTextWriter interface.
	CTextWriter( String InFileName )
		: FileName( InFileName )
	{
		File	= _wfopen( *FileName, L"w" );
		if( !File )
			error( L"File \"%s\" not found", *FileName );
	}
	~CTextWriter()
	{
		fclose( File );
	}
	void WriteString( String Line )
	{
		fwprintf( File, L"%s\n", *Line );
	}
	Bool IsEOF()
	{
		return feof( File );
	}
};


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/