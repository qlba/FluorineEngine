/*=============================================================================
    FrProjFile.cpp: Project file functions.
    Copyright Jul.2016 Vlad Gordienko.
=============================================================================*/

#include "Editor.h"

/*-----------------------------------------------------------------------------
    Project shutdown.
-----------------------------------------------------------------------------*/

//
// Close the project.
//
Bool CEditor::CloseProject( Bool bAsk )
{
	// Test the system.
	assert((Project == GProject) && (Project == GObjectDatabase));
	if( !Project )
		return false;

	if( bAsk )
	{
		int S = MessageBox
		( 
			hWnd, 
			*String::Format( L"Save changes to project '%s'?", *Project->ProjName ), 
			L"Confirm", 
			MB_YESNOCANCEL | MB_ICONASTERISK | MB_TASKMODAL 
		);

		if( S == IDCANCEL )
			return false;

		if( S == IDYES )
		{
			if( !SaveProject() )
				return false;
		}
	}

	// Close all projects pages.
	for( Integer i=0; i<EditorPages->Pages.Num();  )
		if( ((WEditorPage*)EditorPages->Pages[i])->PageType != PAGE_Hello )
		{
			EditorPages->CloseTabPage( i, true );	
		}
		else
			i++;

	// Flash all resources.
	Flush();

	// Destroy the project.
	delete Project;
	GObjectDatabase = nullptr;
	GProject		= nullptr;
	Project			= nullptr;

	// Refresh an editor panels.
	TArray<FObject*> EmptyArr;
	Inspector->SetEditObjects( EmptyArr );
	Browser->RefreshList();
	Explorer->Refresh();

	// Update caption.
	SetCaption(L"Fluorine Engine");

	return true;
}


/*-----------------------------------------------------------------------------
    Project construction.
-----------------------------------------------------------------------------*/

//
// Create a new project.
//
Bool CEditor::NewProject()
{
	// Unload last project.
	if( Project && !CloseProject() )
		return false;

	// Allocate project and it objects.
	Project					= new CProject();
	Project->Info			= NewObject<FProjectInfo>();
	Project->BlockMan		= new CBlockManager();
	Project->FileName		= L"";
	Project->ProjName		= L"Unnamed";

	// Load default resources for every project.
	for( Integer iDef=0; true; iDef++ )
	{
		String ResName = Config->ReadString
		( 
			L"Project", 
			*String::Format( L"Res[%d]", iDef ) 
		);

		if( !ResName )
			break;

		PreloadResource( ResName );
	}

	// Update caption.
	SetCaption(String::Format(L"%s - Fluorine Engine", *Project->ProjName));

	// Refresh an editor panels.
	TArray<FObject*> EmptyArr;
	Inspector->SetEditObjects( EmptyArr );
	Browser->RefreshList();
	Explorer->Refresh();

	return true;
}


/*-----------------------------------------------------------------------------
    Project saving.
-----------------------------------------------------------------------------*/

//
// Text exporter.
//
class CExporter: public CExporterBase
{
public:
	// Variables.
	CTextWriter&	Writer;
	Char			Whitespace[64];
	
	// Exporter constructor.
	CExporter( CTextWriter&	InWriter )
		:	Writer( InWriter )
	{}

	// Exporter destructor.
	~CExporter()
	{}

	// Set nest level to save data as a tree.
	void SetNestLevel( Integer InNestLevel )
	{
		assert(InNestLevel*4 < 64);
		MemZero( Whitespace, sizeof(Whitespace) );
		for( Integer i=0; i<InNestLevel*4; i++ )
			Whitespace[i] = L' ';
	}

	// Byte export.
	void ExportByte( const Char* FieldName, Byte Value )
	{
		if( Value )
			Writer.WriteString(String::Format
			( 
				L"%s%s = %d", 
				Whitespace, 
				FieldName, 
				Value 
			));
	}

	// Integer Export.
	void ExportInteger( const Char* FieldName, Integer Value )
	{
		if( Value )
			Writer.WriteString(String::Format
			( 
				L"%s%s = %d", 
				Whitespace, 
				FieldName, 
				Value 
			));
	}

	// Float export.
	void ExportFloat( const Char* FieldName, Float Value )
	{
		if( Value != 0.f )
			Writer.WriteString(String::Format
			( 
				L"%s%s = %.4f", 
				Whitespace, 
				FieldName, 
				Value 
			));
	}

	// String export.
	void ExportString( const Char* FieldName, String Value )
	{
		if( Value )
			Writer.WriteString(String::Format
			( 
				L"%s%s = \"%s\"", 
				Whitespace, 
				FieldName, 
				*Value 
			));
	}

	// Bool export.
	void ExportBool( const Char* FieldName, Bool Value ) 
	{
		if( Value )
			Writer.WriteString(String::Format
			( 
				L"%s%s = %s", 
				Whitespace, 
				FieldName, 
				Value ? L"true" : L"false" 
			));
	}

	// Color export.
	void ExportColor( const Char* FieldName, TColor Value ) 
	{
		if( Value != COLOR_Black )
			Writer.WriteString(String::Format
			( 
				L"%s%s = #%02x%02x%02x%02x", 
				Whitespace, 
				FieldName, 
				Value.R, Value.G, Value.B, Value.A 
			));
	}

	// Vector export.
	void ExportVector( const Char* FieldName, TVector Value ) 
	{
		if( Value.X!=0.f || Value.Y!=0.f )
			Writer.WriteString(String::Format
			( 
				L"%s%s = [%.4f; %.4f]", 
				Whitespace, 
				FieldName, 
				Value.X, Value.Y 
			));
	}

	// Rect export.
	void ExportAABB( const Char* FieldName, TRect Value ) 
	{
		Writer.WriteString(String::Format
		( 
			L"%s%s = (%.4f; %.4f; %.4f; %.4f)", 
			Whitespace, 
			FieldName, 
			Value.Min.X, Value.Min.Y, 
			Value.Max.X, Value.Max.Y 
		));
	}

	// Angle export.
	void ExportAngle( const Char* FieldName, TAngle Value ) 
	{
		if( Value.Angle != 0 )
			Writer.WriteString(String::Format
			( 
				L"%s%s = %d", 
				Whitespace, 
				FieldName, 
				Value.Angle 
			));
	}

	// Object export.
	void ExportObject( const Char* FieldName, FObject* Value ) 
	{
		if( Value )
			Writer.WriteString(String::Format
			( 
				L"%s%s = %s::%s", 
				Whitespace, 
				FieldName, 
				*Value->GetClass()->Name, 
				*Value->GetFullName() 
			));
	}
};


//
// Single resource save.
//
void SaveResource( FResource* R, String Directory )	
{
	assert(R != nullptr);

	// Open file.
	CTextWriter Writer(String::Format( L"%s\\Objects\\%s.fr", *Directory, *R->GetName()));
	CExporter Exporter(Writer);
	Exporter.SetNestLevel(0);

	// File header.
	Writer.WriteString( String::Format( L"BEGIN_RESOURCE %s %s", *R->GetClass()->Name, *R->GetName() ) );
	{
		if( R->IsA(FLevel::MetaClass) )
		{
			//
			// Level.
			//
			FLevel* Level = (FLevel*)R;
			Exporter.SetNestLevel(1);
			Level->Export(Exporter);

			// Each entity.
			for( Integer iEntity=0; iEntity<Level->Entities.Num(); iEntity++ )
			{
				FEntity* Entity = Level->Entities[iEntity];
				Writer.WriteString( String::Format
				( 
					L"    BEGIN_ENTITY %s %s", 
					*Entity->Script->GetName(), 
					*Entity->GetName() 
				));
				Exporter.SetNestLevel(2);
				Entity->Export(Exporter);
				
				// Entity's components.
				{
					Exporter.SetNestLevel(3);

					// Base
					Writer.WriteString( String::Format
					( 
						L"        BEGIN_COMPONENT %s %s", 
						*Entity->Base->GetClass()->Name, 
						*Entity->Base->GetName() ) 
					);
					Entity->Base->Export( Exporter );
					Writer.WriteString( L"        END_COMPONENT" );

					// Extra.
					for( Integer iCom=0; iCom<Entity->Components.Num(); iCom++ )
					{
						FComponent* Component = Entity->Components[iCom];
						Writer.WriteString( String::Format
						( 
							L"        BEGIN_COMPONENT %s %s", 
							*Component->GetClass()->Name, 
							*Component->GetName() ) 
						);
						Component->Export( Exporter );
						Writer.WriteString( L"        END_COMPONENT" );
					}
				}

				// Entity instance buffer.
				if( Entity->Script->bHasText )
				{
					Writer.WriteString( String::Format( L"        BEGIN_INSTANCE" ) );
						Entity->InstanceBuffer->ExportValues( Exporter );
					Writer.WriteString( L"        END_INSTANCE" );
				}

				Writer.WriteString( L"    END_ENTITY" );
			}
		}
		else if( R->IsA(FScript::MetaClass) )
		{
			//
			// Script.
			//
			FScript* Script = (FScript*)R;
			Exporter.SetNestLevel(1);
			Script->Export(Exporter);

			// Script's components.
			{
				Exporter.SetNestLevel(3);

				// Base
				Writer.WriteString( String::Format
				( 
					L"    BEGIN_COMPONENT %s %s", 
					*Script->Base->GetClass()->Name, 
					*Script->Base->GetName() ) 
				);
				Script->Base->Export(Exporter);
				Writer.WriteString( L"    END_COMPONENT" );

				// Extra.
				for( Integer iCom=0; iCom<Script->Components.Num(); iCom++ )
				{
					FComponent* Component = Script->Components[iCom];
					Writer.WriteString( String::Format
					( 
						L"    BEGIN_COMPONENT %s %s", 
						*Component->GetClass()->Name, 
						*Component->GetName() ) 
					);
					Component->Export( Exporter );
					Writer.WriteString( L"    END_COMPONENT" );
				}
			}

			// Instance buffer.
			if( Script->bHasText )
			{
				Writer.WriteString( String::Format( L"    BEGIN_INSTANCE" ) );
				Script->InstanceBuffer->ExportValues( Exporter );
				Writer.WriteString( L"    END_INSTANCE" );
			}

			// Store script text.
			if( Script->bHasText )
				GEditor->ExportResource( Script, Directory+L"\\Scripts", true );
		}
		else
		{
			//
			// Resource.
			//
			Exporter.SetNestLevel(1);
			R->Export(Exporter);

			// Save also bitmaps and sounds.
			if( R->IsA(FBitmap::MetaClass) )
			{
				FBitmap* Bitmap	= As<FBitmap>(R);
				if( Bitmap->IsValidBlock() )
					GEditor->ExportResource( R, Directory+L"\\Bitmaps", false );			
			}
			else if( R->IsA(FSound::MetaClass) )
				GEditor->ExportResource( R, Directory+L"\\Sounds", false );
		}
	}
	// File footer.
	Writer.WriteString( L"END_RESOURCE" );
}


//
// Save entire project.
//
Bool CEditor::SaveProject()
{
	if( !Project )
		return false;

	if( !Project->FileName )
	{
		// Project has no FileName, so Save it As.
		return SaveAsProject();
	}

	// Use dialog to show processing.
	TaskDialog->Begin(L"Project Saving");

	String	Directory	= GetFileDir(Project->FileName);
	CTextWriter ProjFile( Project->FileName );

	// Create directories for imported resources.
	CreateDirectory( *(Directory+L"\\Objects"), nullptr );
	CreateDirectory( *(Directory+L"\\Bitmaps"), nullptr );
	CreateDirectory( *(Directory+L"\\Scripts"), nullptr );
	CreateDirectory( *(Directory+L"\\Sounds"), nullptr );
	CreateDirectory( *(Directory+L"\\Music"), nullptr );

	// Wrap list of files in block.
	ProjFile.WriteString( L"BEGIN_INCLUDE" );
	{
		// Save all scripts.
		TaskDialog->UpdateSubtask(L"Saving Scripts");
		TaskDialog->UpdateProgress( 1, 4 );
		for( Integer i=0; i<Project->GObjects.Num(); i++ )
			if( Project->GObjects[i] )
			{
				FObject* Obj = Project->GObjects[i];

				// Save to file.
				if( Obj->IsA(FScript::MetaClass) )
				{
					ProjFile.WriteString(String::Format( L"    %s.fr", *Obj->GetName() ));
					SaveResource( (FResource*)Obj, Directory ); 
				}
			}

		// Save all bitmaps, due thier nesting.
		TaskDialog->UpdateSubtask(L"Saving Bitmaps");
		TaskDialog->UpdateProgress( 2, 4 );
		for( Integer i=0; i<Project->GObjects.Num(); i++ )
			if( Project->GObjects[i] )
			{
				FObject* Obj = Project->GObjects[i];

				// Save to file.
				if( Obj->IsA(FBitmap::MetaClass) )
				{
					ProjFile.WriteString(String::Format( L"    %s.fr", *Obj->GetName() ));
					SaveResource( (FResource*)Obj, Directory ); 
				}
			}

		// Save all resources, except scripts and bitmaps.
		TaskDialog->UpdateSubtask(L"Saving Resources");
		TaskDialog->UpdateProgress( 3, 4 );
		for( Integer i=0; i<Project->GObjects.Num(); i++ )
			if( Project->GObjects[i] )
			{
				FObject* Obj = Project->GObjects[i];

				// Save to file.
				if	( 
						Obj->IsA(FResource::MetaClass) && 
						!Obj->IsA(FScript::MetaClass) && 
						!Obj->IsA(FBitmap::MetaClass) && 
						!(Obj->IsA(FLevel::MetaClass) && ((FLevel*)Obj)->IsTemporal()) 
					)
				{
					ProjFile.WriteString(String::Format( L"    %s.fr", *Obj->GetName() ));
					SaveResource( (FResource*)Obj, Directory ); 
				}
			}
	}
	ProjFile.WriteString( L"END_INCLUDE" );

	// Editor pages.
	TaskDialog->UpdateSubtask(L"Saving Pages");
	TaskDialog->UpdateProgress( 4, 4 );
	ProjFile.WriteString( L"BEGIN_PAGES" );
	{
		for( Integer iPage=0; iPage<EditorPages->Pages.Num(); iPage++ )
		{
			WEditorPage* Page	= (WEditorPage*)EditorPages->Pages[iPage];
			FObject* Res = nullptr;

			if( Page->PageType!=PAGE_Hello && Page->PageType!=PAGE_Play )
				Res	= Page->GetResource();

			if( Res )
				ProjFile.WriteString(String::Format( L"    %s", *Res->GetName() ) );
		}
	}
	ProjFile.WriteString( L"END_PAGES" );
	TaskDialog->End();

	return true;
}


//
// Save project with ask.
//
Bool CEditor::SaveAsProject()
{
	if( !Project )
		return false;

	// Ask file name.
	String FileName;
	if	(	!ExecuteSaveFileDialog
			( 
				FileName, 
				Project->FileName ? Project->FileName :	GDirectory, 
				L"Fluorine Project (*.fluproj)\0*.fluproj\0" 
			) 
		)
			return false;

	// Append extension, if it missing.
	if( String::Pos( L".fluproj", String::LowerCase(FileName) ) == -1 )
		FileName += L".fluproj";

	// Whether override?
	if( GPlat->FileExists(FileName) )
	{
		if( FileName != Project->FileName )
		{
			int	S	= MessageBox
			(
				hWnd,
				*String::Format( L"%s already exists.\nOverride?", *FileName ), 
				L"Saving", 
				MB_YESNO | MB_ICONWARNING | MB_TASKMODAL 
			);

			if( S == IDNO )
				return false;
		}
	}

	// Set as project file name and save it.
	Project->FileName		= FileName;
	Project->ProjName		= GetFileName(FileName);

	// Save to file.
	if( !SaveProject() )
		return false;

	// Update caption.
	SetCaption(String::Format(L"%s - Fluorine Engine", *Project->ProjName));

	return true;
}


/*-----------------------------------------------------------------------------
    Project opening helpers.
-----------------------------------------------------------------------------*/

//
// An information about just loaded 
// property.
//
struct TLoadProperty
{
public:
	// Used static arrays instead dynamic string, since
	// it's fast and used very often at load time.
	Char		Name[32];
	Char		Value[64];

	//
	// Parse a value from the 'Value' string,
	// anyway return something even in case of
	// failure. Make sure this function's doesn't 
	// crash the app.
	//

	// Parse byte value. 
	Byte ToByte()
	{
		return _wtoi(Value);
	}

	// Parse int value.
	Integer ToInteger()
	{
		return _wtoi(Value);
	}

	// Parse float value.
	Float ToFloat()
	{
		return _wtof(Value);
	}

	// Parse bool value.
	Bool ToBool()
	{
		return wcsstr( Value, L"true" ) != nullptr;
	}

	// Parse angle value.
	TAngle ToAngle()
	{
		return _wtoi(Value);
	}

	// Parse string value.
	String ToString()
	{
		if( Value[0] != '"' ) 
			return L"";

		Char	Buffer[32] = {}, *Walk = Buffer, 
				*ValWalk = &Value[1], *End = &Value[array_length(Value)-1];

		while( *ValWalk != '"' && ValWalk != End )
		{
			*Walk = *ValWalk;
			Walk++;
			ValWalk++;
		}
		return Buffer;
	}

	// Parse vector value.
	TVector ToVector()
	{
		Float X=0.f, Y=0.f;
		Char *Walk=Value, *End = &Value[array_length(Value)-1];
		Walk++;
		X = _wtof(Walk);
		while( *Walk != ';' )
		{
			Walk++;
			if( Walk > End ) 
				return TVector( X, Y );
		}
		Walk++;
		Y = _wtof(Walk);
		return TVector( X, Y );
	}

	// Parse color value.
	TColor ToColor()
	{
		Byte R, G, B, A;
		R = FromHex(Value[1])*16 + FromHex(Value[2]);
		G = FromHex(Value[3])*16 + FromHex(Value[4]);
		B = FromHex(Value[5])*16 + FromHex(Value[6]);
		A = FromHex(Value[7])*16 + FromHex(Value[8]);
		return TColor( R, G, B, A );
	}

	// Parse object value.
	FObject* ToObject()
	{
		FObject* Result = nullptr;
		Char *Walk=Value, *End = &Value[array_length(Value)-1];
		Char ClassName[32]={}, ObjName[32]={};
		CClass* ReqClass = nullptr;

		for( Char* C=ClassName; *Walk!=':' && C!=&ClassName[31]; C++, Walk++ )
			*C = *Walk;

		Walk += 2;
		ReqClass = CClassDatabase::StaticFindClass( ClassName );
		if( !ReqClass ) 
			return nullptr;

		while( true )
		{
			MemZero( ObjName, sizeof(ObjName) );
			for( Char* C=ObjName; C!=&ObjName[31] && *Walk>32 && *Walk!='.'; C++, Walk++ )
				*C = *Walk;
			Walk++;
			Result = GObjectDatabase->FindObject( ObjName, FObject::MetaClass, Result );
			if( !Result ) return nullptr;
			if( *Walk <= 32 )
			{
				if( Result->IsA(ReqClass) )
					return Result;
			}
		}

		return Result;
	}

	// Parse rect value.
	TRect ToAABB()
	{
		TRect Rect;
		Char *Walk=Value, *End = &Value[array_length(Value)-1];
		Walk++;
		Rect.Min.X = _wtof(Walk);
		while( *Walk != ';' )
		{
			Walk++;
			if( Walk > End ) return Rect;
		}
		Walk++;
		Rect.Min.Y = _wtof(Walk);
		while( *Walk != ';' )
		{
			Walk++;
			if( Walk > End ) return Rect;
		}
		Walk++;
		Rect.Max.X = _wtof(Walk);
		while( *Walk != ';' )
		{
			Walk++;
			if( Walk > End ) return Rect;
		}
		Walk++;
		Rect.Max.Y = _wtof(Walk);
		return Rect;
	}
};


//
// Parse word from the line, start from iFirst character.
//
String ParseWord( String Source, Integer& iFirst )
{
	while( Source[iFirst]==' ' && iFirst<Source.Len() )
		++iFirst;

	Char Buffer[64] = {};
	Char* Walk = Buffer;
	while( iFirst<Source.Len() && Source[iFirst]!=' ' )
	{
		*Walk = Source[iFirst];
		Walk++;
		iFirst++;
	}
	return Buffer;
}


//
// A loaded object type.
//
enum ELoadObject
{
	LOB_Invalid,		// Bad object.
	LOB_Resource,		// Resource object, its a root of the file.
	LOB_Entity,			// Entity object, in level.
	LOB_Component,		// Component object.
	LOB_Instance		// CInstanceBuffer.
};


//
// A just loaded object.
//
struct TLoadObject
{
	// Variables.
	ELoadObject				Type;
	CClass*					Class;		// Except 'LOB_Instance'.
	String					Name;		// Except 'LOB_Instance'.
	FScript*				Script;		// 'LOB_Entity' only.

	union 
	{
		FObject*			Object;
		CInstanceBuffer*	Instance;
	};

	// Tables.
	TArray<TLoadProperty>	Props;
	TArray<TLoadObject*>	Nodes;

	// Load object constructor.
	TLoadObject()
		:	Type( LOB_Invalid ),
			Class( FObject::MetaClass ),
			Name( L"" ),
			Script( nullptr ),
			Object( nullptr ),
			Props(),
			Nodes()
	{}

	// Load object destructor.
	~TLoadObject()
	{
		for( Integer i=0; i<Nodes.Num(); i++ )
			delete Nodes[i];
		Props.Empty();
		Nodes.Empty();
	}

	// Find property by it name.
	TLoadProperty* FindProperty( String PropName, Bool bMandatory=false )
	{
		for( Integer i=0; i<Props.Num(); i++ )
			if( PropName == Props[i].Name )
				return &Props[i];

		if( bMandatory )	
			throw String::Format( L"Property '%s' not found", *PropName );

		return nullptr;
	}

	// Parse property from the line and add it property
	// to list.
	void ParseProp( const String& Line )
	{
		TLoadProperty Prop;
		MemZero( &Prop, sizeof(TLoadProperty) );
		Integer iPos = 0;
		Char *NameWalk = Prop.Name, *ValueWalk = Prop.Value;

		while( Line[iPos]==' ' && iPos<Line.Len() )
			++iPos;

		while( Line[iPos]!=' ' && iPos<Line.Len() )
		{
			*NameWalk = Line[iPos];
			NameWalk++;
			iPos++;
		}

		while( Line[iPos]==' ' && iPos<Line.Len() )
			++iPos;

		if( Line[iPos++] != '=' )
			throw String(L"Missing assignment");

		while( Line[iPos]==' ' && iPos<Line.Len() )
			++iPos;

		while( iPos<Line.Len() )
		{
			*ValueWalk = Line[iPos];
			ValueWalk++;
			iPos++;
		}

		Props.Push( Prop );
#if 0
		// Dbg.
		log( L"Got property '%s' with value '%s'", Prop.Name, Prop.Value );
#endif
	}
};


/*-----------------------------------------------------------------------------
    Project opening.
-----------------------------------------------------------------------------*/

//
// Load an object info from the single file.
//
TLoadObject* LoadResource( String FileName, String Directory )
{
	TLoadObject* Resource = new TLoadObject();
	CTextReader Reader( FileName );	
	
	//
	// Load resource header.
	//
	{
		String Header = Reader.ReadLine();
		String ObjName, ObjClass;
		Integer iPos = 0;

		if( ParseWord( Header, iPos ) != L"BEGIN_RESOURCE" )
			throw String::Format( L"Bad resource file: '%s'", *FileName );

		ObjClass = ParseWord( Header, iPos );
		ObjName	= ParseWord( Header, iPos );

		// Preinitialize Resource.
		Resource->Type		= LOB_Resource;
		Resource->Class		= CClassDatabase::StaticFindClass( *ObjClass );
		Resource->Name		= ObjName;
		if( !Resource->Class )
			throw String::Format( L"Class '%s' not found", *ObjClass );
	}

	// Read resource data.
	if( Resource->Class->IsA(FBitmap::MetaClass) )
	{
		//
		// Bitmap.
		//
		for( ; ; )
		{
			String Line = Reader.ReadLine();

			if( String::Pos( L"END_RESOURCE", Line ) != -1 )
			{
				// End of text.
				break;
			}
			else
			{
				// Read property.
				Resource->ParseProp(Line);
			}
		}

		// Allocate bitmap.
		if( Resource->Class->IsA(FDemoBitmap::MetaClass) )
		{
			// Allocate new one.
			FBitmap* Bitmap;
			Integer UBits, VBits;
			UBits	= Resource->FindProperty( L"UBits", true )->ToInteger();
			VBits	= Resource->FindProperty( L"VBits", true )->ToInteger();

			Resource->Object	= Bitmap = NewObject<FBitmap>( Resource->Class, Resource->Name, nullptr );
			Bitmap->Init( 1 << UBits, 1 << VBits );
		}
		else
		{
			// Import bitmap from the file.
			String BitFile, FullFn;
			BitFile	= Resource->FindProperty( L"FileName", true )->ToString();
			FullFn	= String::Format( L"%s\\Bitmaps\\%s", *Directory, *BitFile );
			if( !GPlat->FileExists(FullFn) ) 
				throw String::Format( L"File '%s' not found", *FullFn );
			Resource->Object	= GEditor->ImportResource( FullFn );
		}
	}
	else if( Resource->Class->IsA(FSound::MetaClass) )
	{
		//
		// Sound.
		//
		for( ; ; )
		{
			String Line = Reader.ReadLine();

			if( String::Pos( L"END_RESOURCE", Line ) != -1 )
			{
				// End of text.
				break;
			}
			else
			{
				// Read property.
				Resource->ParseProp(Line);
			}
		}		

		{
			// Import sound from file.
			String SndFile, FullFn;
			SndFile = Resource->FindProperty( L"FileName", true )->ToString();
			FullFn	= String::Format( L"%s\\Sounds\\%s", *Directory, *SndFile );
			if( !GPlat->FileExists(FullFn) ) 
				throw String::Format( L"File '%s' not found", *FullFn );
			Resource->Object	= GEditor->ImportResource( FullFn );
		}
	}
	else if( Resource->Class->IsA(FScript::MetaClass) )
	{
		//
		// Script.
		//
		for( ; ; )
		{
			String Line = Reader.ReadLine();

			if( String::Pos( L"END_RESOURCE", Line ) != -1 )
			{
				// End of text.
				break;
			}
			else if( String::Pos( L"BEGIN_COMPONENT", Line ) != -1 )
			{
				// Read component.
				TLoadObject* Component = new TLoadObject();
				Resource->Nodes.Push(Component);

				// Load resource header.
				{
					String Header = Line;
					String ObjName, ObjClass;
					Integer iPos = 0;

					if( ParseWord( Header, iPos ) != L"BEGIN_COMPONENT" )
						throw String::Format( L"Bad resource file: '%s'", *FileName );

					ObjClass = ParseWord( Header, iPos );
					ObjName	= ParseWord( Header, iPos );

					// Preinitialize Component.
					Component->Type		= LOB_Component;
					Component->Class	= CClassDatabase::StaticFindClass( *ObjClass );
					Component->Name		= ObjName;
					if( !Component->Class )
						throw String::Format( L"Class '%s' not found", *ObjClass );
				}
				for( ; ; )
				{
					String Line = Reader.ReadLine();

					if( String::Pos( L"END_COMPONENT", Line ) != -1 )
					{
						// End of text.
						break;
					}
					else
					{
						// Read component property.
						Component->ParseProp(Line);
					}
				}
			}
			else if( String::Pos( L"BEGIN_INSTANCE", Line ) != -1 )
			{
				// Instance buffer.
				TLoadObject* Instance = new TLoadObject();
				Resource->Nodes.Push( Instance );
				Instance->Type	= LOB_Instance;
				if( !(Resource->FindProperty( L"bHasText" )? Resource->FindProperty( L"bHasText" )->ToBool() : false) )
					throw String::Format( L"Script '%s' has no instance buffer", *Resource->Name );

				for( ; ; )
				{
					String Line = Reader.ReadLine();

					if( String::Pos( L"END_INSTANCE", Line ) != -1 )
					{
						// End of text.
						break;
					}
					else
					{
						// Read instance property.
						Instance->ParseProp(Line);
					}
				}
			}
			else
			{
				// Read property.
				Resource->ParseProp(Line);
			}
		}

		// Allocate script.
		FScript* Script			= NewObject<FScript>( Resource->Name );
		Resource->Object		= Script;
		Script->bHasText		= Resource->FindProperty( L"bHasText" ) ? Resource->FindProperty( L"bHasText" )->ToBool() : false;
		Script->FileName		= Resource->FindProperty( L"FileName" ) ? Resource->FindProperty( L"FileName" )->ToString() : L"";
		Script->InstanceBuffer	= Script->bHasText ? new CInstanceBuffer(Script) : nullptr;

		// Load script text.
		if( Script->bHasText )
		{
			String FullFileName = String::Format( L"%s\\Scripts\\%s", *Directory, *Script->FileName );
			if( !GPlat->FileExists(FullFileName) )
				throw String::Format( L"File '%s' not found", *FullFileName );
			CTextReader TextReader(FullFileName);
			while( !TextReader.IsEOF() )
				Script->Text.Push( TextReader.ReadLine() );

			// Eliminate empty lines at the end of file.
			while( Script->Text.Num() && !Script->Text.Last() )
				Script->Text.Remove(Script->Text.Num()-1);
		}

		// Create components.
		for( Integer iNode=0; iNode<Resource->Nodes.Num(); iNode++ )
		{
			TLoadObject* Node = Resource->Nodes[iNode];

			if( Node->Type == LOB_Component )
			{
				FComponent*	Com = NewObject<FComponent>( Node->Class, Node->Name, Script );
				Com->InitForScript( Script );
				Node->Object	= Com;
			}
			else if( Node->Type == LOB_Instance )
			{
				Node->Instance	= Script->InstanceBuffer;
			}
		}
	}
	else if( Resource->Class->IsA(FLevel::MetaClass) )
	{
		//
		// Level.
		//
		for( ; ; )
		{
			String Line = Reader.ReadLine();

			if( String::Pos( L"END_RESOURCE", Line ) != -1 )
			{
				// End of text.
				break;
			}
			else if( String::Pos( L"BEGIN_ENTITY", Line ) != -1 )
			{
				// Read entity.
				TLoadObject* Entity = new TLoadObject();
				Resource->Nodes.Push( Entity );

				// Load entity header.
				{
					String Header = Line;
					String ObjName, ObjScript;
					Integer iPos = 0;

					if( ParseWord( Header, iPos ) != L"BEGIN_ENTITY" )
						throw String::Format( L"Bad resource file: '%s'", *FileName );

					ObjScript	= ParseWord( Header, iPos );
					ObjName		= ParseWord( Header, iPos );

					// Preinitialize Component.
					Entity->Type		= LOB_Entity;
					Entity->Class		= FEntity::MetaClass;
					Entity->Name		= ObjName;
					Entity->Script		= (FScript*)GObjectDatabase->FindObject( ObjScript, FScript::MetaClass, nullptr );
					if( !Entity->Script )
						throw String::Format( L"Script '%s' not found", *ObjScript );
				}
				for( ; ; )
				{
					String Line = Reader.ReadLine();

					if( String::Pos( L"END_ENTITY", Line ) != -1 )
					{
						// End of text.
						break;
					}
					else if( String::Pos( L"BEGIN_COMPONENT", Line ) != -1 )
					{
						// Read component.
						TLoadObject* Component = new TLoadObject();
						Entity->Nodes.Push( Component );

						// Load resource header.
						{
							String Header = Line;
							String ObjName, ObjClass;
							Integer iPos = 0;

							if( ParseWord( Header, iPos ) != L"BEGIN_COMPONENT" )
								throw String::Format( L"Bad resource file: '%s'", *FileName );

							ObjClass = ParseWord( Header, iPos );
							ObjName	= ParseWord( Header, iPos );

							// Preinitialize Component.
							Component->Type		= LOB_Component;
							Component->Class	= CClassDatabase::StaticFindClass( *ObjClass );
							Component->Name		= ObjName;
							if( !Component->Class )
								throw String::Format( L"Class '%s' not found", *ObjClass );
						}
						for( ; ; )
						{
							String Line = Reader.ReadLine();

							if( String::Pos( L"END_COMPONENT", Line ) != -1 )
							{
								// End of text.
								break;
							}
							else
							{
								// Read component property.
								Component->ParseProp(Line);
							}
						}
					}
					else if( String::Pos( L"BEGIN_INSTANCE", Line ) != -1 )
					{
						// Instance buffer.
						TLoadObject* Instance = new TLoadObject();
						Entity->Nodes.Push( Instance );
						Instance->Type	= LOB_Instance;
						if( !Entity->Script->bHasText )
							throw String::Format( L"Script '%s' has no instance buffer", *Resource->Name );

						for( ; ; )
						{
							String Line = Reader.ReadLine();

							if( String::Pos( L"END_INSTANCE", Line ) != -1 )
							{
								// End of text.
								break;
							}
							else
							{
								// Read instance property.
								Instance->ParseProp(Line);
							}
						}
					}
				}
			}
			else
			{
				// Read property.
				Resource->ParseProp(Line);
			}
		}

		// Allocate level.
		FLevel* Level = NewObject<FLevel>( Resource->Name );
		Resource->Object	= Level;

		// Create entities.
		for( Integer iEntity=0; iEntity<Resource->Nodes.Num(); iEntity++ )
		{
			TLoadObject* EntNode = Resource->Nodes[iEntity];
			FEntity* Entity = NewObject<FEntity>( EntNode->Name, Level );
			Entity->Level	= Level;
			Entity->Script	= EntNode->Script;
			Entity->InstanceBuffer	= Entity->Script->bHasText ? new CInstanceBuffer(Entity->Script) : nullptr;
			EntNode->Object	= Entity;

			// Create components.
			for( Integer iNode=0; iNode<EntNode->Nodes.Num(); iNode++ )
			{
				TLoadObject* Node = EntNode->Nodes[iNode];	

				if( Node->Type == LOB_Component )
				{
					FComponent*	Com = NewObject<FComponent>( Node->Class, Node->Name, Entity );
					Com->InitForEntity( Entity );
					Node->Object	= Com;
				}
				else if( Node->Type == LOB_Instance )
				{
					Node->Instance	= Entity->InstanceBuffer;
				}
			}

			Level->Entities.Push( Entity );
		}
	}
	else
	{
		//
		// Regular resource.
		//
		for( ; ; )
		{
			String Line = Reader.ReadLine();

			if( String::Pos( L"END_RESOURCE", Line ) != -1 )
			{
				// End of text.
				break;
			}
			else
			{
				// Read property.
				Resource->ParseProp(Line);
			}
		}

		// Allocate resource.
		Resource->Object = NewObject<FObject>( Resource->Class, Resource->Name, nullptr );
	}

#if 0
	// Dbg.
	log( L"Load '%s'", *FileName );
#endif
	return Resource;
}



//
// Text importer.
//
class CImporter: public CImporterBase
{
public:
	// Variables.
	TLoadObject*	Object;

	// Importer constructor.
	CImporter()
		:	Object( nullptr )
	{}

	// Importer destructor.
	~CImporter()
	{}

	// Set object to import properties from.
	void SetObject( TLoadObject* InObject )
	{
		assert(InObject);
		Object	= InObject;
	}

	// Byte import.
	Byte ImportByte( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToByte() : 0x00;
	}

	// Integer import.
	Integer	ImportInteger( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToInteger() : 0;
	}

	// Float import.
	Float ImportFloat( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToFloat() : 0.f;
	}

	// String import.
	String ImportString( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToString() : L"";
	}

	// Bool import.
	Bool ImportBool( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToBool() : false;
	}

	// Color import.
	TColor ImportColor( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToColor() : COLOR_Black;
	}

	// Vector import.
	TVector	ImportVector( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToVector() : TVector(0,0);
	}

	// Rect import.
	TRect ImportAABB( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToAABB() : TRect(TVector(0.f,0.f), TVector(0.f,0.f));
	}

	// Angle import.
	TAngle ImportAngle( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToAngle() : TAngle(0.f);
	}

	// Object import.
	FObject* ImportObject( const Char* FieldName )
	{
		TLoadProperty* Prop = Object->FindProperty( FieldName, false );
		return Prop ? Prop->ToObject() : nullptr;
	}
};


//
// Import object's fields. Warning
// this routine are recursive to handle
// objects nodes graph.
//
void ImportFields( CImporter& Importer, TLoadObject* Object )
{
	// Load own fields.
	if( Object->Type == LOB_Instance )
	{
		// Load instance buffer.
		assert(Object->Instance);
		Importer.SetObject( Object );
		Object->Instance->ImportValues( Importer );
	}
	else
	{
		// Load 'F' object.
		assert(Object->Object);
		Importer.SetObject( Object );
		Object->Object->Import( Importer );
	}

	// Load sub-objects.
	for( Integer i=0; i<Object->Nodes.Num(); i++ )
		ImportFields( Importer, Object->Nodes[i] );
}


//
// Open project from the file.
//
Bool CEditor::OpenProjectFrom( String FileName )
{
	// No opened project before.
	assert(!GProject);

	// Allocate the project.
	String	Directory	= GetFileDir( FileName );
	String	ProjName	= GetFileName( FileName );
	Project				= new CProject();
	Project->BlockMan	= new CBlockManager();
	Project->FileName	= FileName;
	Project->ProjName	= ProjName;

	// Load project file.
	Bool Result		= true;
	CTextReader ProjFile( FileName );
	TArray<TLoadObject*>	ResObjs;

	// Use loaging dialog, for coolness.
	TaskDialog->Begin(L"Project Loading");

	// Let loading wrap into try-catch.
	try
	{
		// Load all resources into temporal TLoadObject structs.
		// And allocate all objects. All fields will be loaded
		// later.
		if( ProjFile.ReadLine() != L"BEGIN_INCLUDE" )
			throw String(L"Bad project file");

		TaskDialog->UpdateSubtask(L"Loading Objects");
		TaskDialog->UpdateProgress( 0, 100 );
		String ObjName;
		while( (ObjName = ProjFile.ReadLine()) != L"END_INCLUDE" )
		{	
			// Delete leading whitespace.
			//!!Find other way!
			while( ObjName.Len()>0 && ObjName[0]==' ' )
				ObjName = String::Delete( ObjName, 0, 1 );

			if( ObjName )
			{
				TLoadObject* Obj = LoadResource( String::Format( L"%s\\Objects\\%s", *Directory, *ObjName ), Directory );
				ResObjs.Push( Obj );
			}
		}

		// Find project info.
		for( Integer iRes=0; iRes<ResObjs.Num(); iRes++ )
			if( ResObjs[iRes]->Object->IsA(FProjectInfo::MetaClass) )
			{
				GProject->Info	= As<FProjectInfo>(ResObjs[iRes]->Object);
				break;
			}
		if( !GProject->Info )
			throw String(L"Project info not found");

		// Compile scripts.
		TaskDialog->UpdateSubtask(L"Script Compiling");
		TaskDialog->UpdateProgress( 20, 100 );
		if( !CompileAllScripts(true) )
			throw String(L"Failed compile script at start time");

		// Setup all fields.
		TaskDialog->UpdateSubtask(L"Properties Import");
		TaskDialog->UpdateProgress( 50, 100 );
		CImporter Importer;
		for( Integer iRes=0; iRes<ResObjs.Num(); iRes++ )
			ImportFields( Importer, ResObjs[iRes] );

		Result	= true;
	}
	catch( String Message )
	{
		// Failure with message.
		warn( L"Failed load project '%s' with message: '%s'.", *ProjName, *Message );
		CloseProject( false );
		Result	= false;
	}
	catch( ... )
	{
		// Unhandled failure.
		warn( L"Failed load project '%s'.", *ProjName );
		CloseProject( false );
		Result	= false;
	}

	// Destroy temporal objects.
	for( Integer i=0; i<ResObjs.Num(); i++ )
		delete ResObjs[i];
	ResObjs.Empty();

	// Notify all objects about loading.
	TaskDialog->UpdateSubtask(L"Notification");
	TaskDialog->UpdateProgress( 75, 100 );
	for( Integer iObj=0; GObjectDatabase && iObj<GObjectDatabase->GObjects.Num(); iObj++ )
		if( GObjectDatabase->GObjects[iObj] )
			GObjectDatabase->GObjects[iObj]->PostLoad();

	// Update all editor panels.
	TArray<FObject*> EmptyArr;
	Inspector->SetEditObjects( EmptyArr );
	Browser->RefreshList();
	Explorer->Refresh();

	// Restore pages.
	TaskDialog->UpdateSubtask(L"Pages Restoring");
	TaskDialog->UpdateProgress( 90, 100 );
	if( ProjFile.ReadLine() == L"BEGIN_PAGES" )
	{
		String PageName;
		while( (PageName = ProjFile.ReadLine()) != L"END_PAGES" )
		{
			// Delete leading whitespace.
			//!!Find other way!
			while( PageName.Len()>0 && PageName[0]==' ' )
				PageName = String::Delete( PageName, 0, 1 );

			FResource* Res = nullptr;
			if( PageName && (Res = (FResource*)Project->FindObject( PageName, FResource::MetaClass )) )
			{
				this->OpenPageWith( Res );
			}
		}
	}

	// Update list of recent projects.
	{
		Integer i;
		for( i=0; i<5; i++ )
			if(	String::UpperCase(Config->ReadString( L"Recent", *String::Format(L"Recent[%i]", i), L"" )) == 
				String::UpperCase(FileName) )
			{
				// Yes, it found in list.
				break;
			}

		if( i == 5 )
		{
			// Project not found in list of recent.
			// So shift list and add new one.
			for( Integer j=4; j>0; j-- )
			{
				String Prev	= Config->ReadString( L"Recent", *String::Format(L"Recent[%i]", j-1), L"" );
				Config->WriteString( L"Recent", *String::Format(L"Recent[%i]", j), Prev );
			}

			// Add new.
			Config->WriteString( L"Recent", L"Recent[0]", FileName );
		}
	}

	// Update caption.
	if( Project )
		SetCaption(String::Format(L"%s - Fluorine Engine", *Project->ProjName));

	// Hide progress.
	TaskDialog->End();

	return Result;
}


//
// Open entire project.
//
Bool CEditor::OpenProject()
{
	// Unload last project.
	if( Project && !CloseProject() )
		return false;

	// Ask file name.
	String FileName;
	if	( 
			!ExecuteOpenFileDialog
			( 
				FileName, 
				GDirectory, 
				L"Fluorine Project (*.fluproj)\0*.fluproj\0" 
			) 
		)
			return false;

	// Open it.
	return OpenProjectFrom( FileName );
}


//
// Load external resource from the file.
//
FResource* CEditor::PreloadResource( String Name )
{
	// Don't load if no project.
	if( !Project )
		return nullptr;

	FResource*	Res			= nullptr;
	String		FileName	= GDirectory + Name;
	String		Directory	= GetFileDir(FileName);
	assert(GPlat->FileExists(FileName));

	// Let loading wrap into try-catch.
	try
	{
		// Load object.
		TLoadObject* Obj = LoadResource( FileName, Directory );
		
		// If it is script, compile script, to load instance buffer
		// properly.
		Res					= As<FResource>(Obj->Object);
		FScript*	Script	= As<FScript>(Res);
		if( Script && Script->bHasText )
			if( !CompileAllScripts(true) )
				error( L"Unable to load script '%s' with errors", *Name );

		// Import all fields.
		CImporter Importer;
		ImportFields( Importer, Obj );

		// Destroy temporal objects.
		delete Obj;
	}
	catch( ... )
	{
		error( L"Unable to load resource '%s'", *Name );
	}

	Res->PostLoad();
	return Res;
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/