/*=============================================================================
    FrCompiler.cpp: FluScript compiler.
    Copyright Jul.2016 Vlad Gordienko.
=============================================================================*/

#include "Editor.h"

/*-----------------------------------------------------------------------------
    Compiler declarations.
-----------------------------------------------------------------------------*/

//
// An information about compiler
// fatal error.
//
struct TCompilerError
{
public:
	FScript*	Script;
	String		Message;
	Integer		ErrorPos;
	Integer		ErrorLine;
};


//
// Script fields alignment bounds.
//
#define SCRIPT_PROP_ALIGN	8


//
// An access modifier.
//
enum EAccessModifier
{
	ACC_Public,
	ACC_Private
};


//
// Keyword constants.
//
#define KEYWORD( word ) static const Char* KW_##word = L#word;
#include "FrKeyword.h"
#undef KEYWORD


//
// Events list.
//
#define SCRIPT_EVENT(name)	L#name ,
static const Char*	GEvents[_EVENT_MAX]	=
{
#include "..\Engine\FrEvent.h"
};
#undef SCRIPT_EVENT


//
// Temporal word variable.
//
static Word GTempWord	= 0xCAFE; 


//
// An current object context. It's should
// be tracked to reduce bytecode length.
//
enum EEntityContext
{
	CONT_This,
	CONT_Other,
	CONT_MAX
};


/*-----------------------------------------------------------------------------
    TToken.
-----------------------------------------------------------------------------*/

//
// Token type.
//
enum ETokenType
{
	TOK_None,
	TOK_Symbol,
	TOK_Const,
	TOK_Identifier
};


//
// A parsed lexeme.
//
struct TToken
{
public:
	// Text information.
	ETokenType		Type;
	String			Text;
	Integer			iLine;
	Integer			iPos;

	// An information about token
	// property type.
	CTypeInfo		TypeInfo;

	// Constant values if TOK_Const.
	union
	{
		Byte			cByte;
		Bool			cBool;
		Integer			cInteger;
		Float			cFloat;
		FResource*		cResource;
		FEntity*		cEntity;
	};
	String			cString;
	TVector			cVector;
	TAngle			cAngle;
	TColor			cColor;
	TRect			cAABB;
};


/*-----------------------------------------------------------------------------
    TStoredScript.
-----------------------------------------------------------------------------*/

//
// A stored script & entities properties
// to restore later. Since user don't want
// to set properties again and again after
// each compilation.
//
struct TStoredScript
{
public:
	FScript*					Script;			// Source script.
	Integer						InstanceSize;	// Stored properties instance size.
	TArray<CProperty*>			Properties;		// Old properties.
	TArray<CEnum*>				Enums;			// Old enumeration, still referenced by properties above.
	TArray<CInstanceBuffer*>	Buffers;		// All instance buffers of this script from the entities and script.
};


/*-----------------------------------------------------------------------------
    TExprResult.
-----------------------------------------------------------------------------*/

//
// An information about result type
// of the compiled expression.
//
struct TExprResult
{
public:
	// Variables.
	CTypeInfo	Type;
	Bool		bLValue;
	Byte		iReg;

	// Constructor.
	TExprResult()
		:	bLValue( false ),
			iReg( 0xff )
	{}
};


/*-----------------------------------------------------------------------------
    CCodeEmitter.
-----------------------------------------------------------------------------*/

//
// A bytecode emitter.
//
class CCodeEmitter: public CSerializer
{
public:
	// Variables.
	CBytecode*			Bytecode;

	// CCodeEmitter interface.
	CCodeEmitter();
	void SetBytecode( CBytecode* InBytecode );

	// CSerializer interface.
	void SerializeData( void* Mem, DWord Count );
	void SerializeRef( FObject*& Obj );
	DWord TotalSize();
	DWord Tell();
};


//
// Emitter macro.
//
#define emit(v)			Serialize( Emitter, v );
#define emit_const(v)	Serialize( Emitter, Script, v );


/*-----------------------------------------------------------------------------
    TNest.
-----------------------------------------------------------------------------*/

// Nesting type.
enum ENestType
{
	NEST_Leading,
	NEST_For,
	NEST_While,
	NEST_Do,
	NEST_If,
	NEST_Switch,
	NEST_Foreach
};


// Address replacement type.
enum EReplType
{
	REPL_Break,
	REPL_Continue,
	REPL_Return,
	REPL_MAX
};


//
// List of addresses in nest.
//
struct TNest
{
public:
	// Variables.
	ENestType		Type;
	TArray<Word>	Addrs[REPL_MAX];
};


/*-----------------------------------------------------------------------------
    CFamily.
-----------------------------------------------------------------------------*/

//
// A shared scripts interface.
//
class CFamily
{
public:
	// Variables.
	String				Name;		// An family name.
	Integer				iFamily;	// An unique family number.
	TArray<FScript*>	Scripts;	// List of family members.
	TArray<String>		VFNames;	// List of names of all virtual functions.
	TArray<CFunction*>	Proto;		// Signature for each function to match, order same as in VFNames.
};


/*-----------------------------------------------------------------------------
    CCompiler.
-----------------------------------------------------------------------------*/

//
// A script compiler.
//
class CCompiler
{
public:
	// CCompiler public interface.
	CCompiler( TArray<String>& OutWarnings, TCompilerError& OutFatalError );
	~CCompiler();
	Bool CompileAll( Bool bSilent );

private:
	// Errors.
	TArray<String>&						Warnings;		
	TCompilerError&						FatalError;			

	// Parsing.
	Integer								TextLine, TextPos;
	Integer								PrevLine, PrevPos;

	// General.
	FScript*							Script;			
	TArray<TStoredScript>				Storage;	
	TArray<FScript*>					AllScripts;
	TArray<CFamily*>					Families;

	// First pass variables.
	EAccessModifier						Access;
	TArray<TToken>						Constants;		

	// Second pass variables.
	CCodeEmitter						Emitter;
	CBytecode*							Bytecode;
	Integer								NestTop;
	TNest								Nest[16];
	Bool								bValidExpr;
	EEntityContext						Context;
	Bool								Regs[TRegister::NUM_REGS];
	
	// Top level functions.
	void StoreAllScripts();
	void RestoreAfterFailure();
	void RestoreAfterSuccess();
	void ParseHeader( FScript* InScript );
	void CompileFirstPass( FScript* InScript );
	void CompileSecondPass( FScript* InScript );

	// Errors & warnings.
	void Error( const Char* Fmt, ... );
	void Warn( const Char* Fmt, ... );

	// Expression compilation.
	TExprResult CompileExpr( const CTypeInfo& ReqType, Bool bForceR, Bool bAllowAssign = false, DWord InPri = 0 );
	Bool CompileEntityExpr( EEntityContext InContext, CTypeInfo Entity, Byte iConReg, TExprResult& Result );
	Byte GetCast( EPropType Src, EPropType Dst );
	TExprResult CompileProtoExpr( FScript* Prototype );
	
	// Second pass compilation.
	void CompileCode( CBytecode* InCode );
	void CompileStatement();
	void CompileIf();
	void CompileSwitch();
	void CompileWhile();
	void CompileFor();
	void CompileDo();
	void CompileForeach();
	void CompileCommand();

	// Nest.
	void PushNest( ENestType InType );
	void PopNest();
	void ReplNest( EReplType Repl, Word DestAddr );

	// Registers management.
	void ResetRegs();
	Byte GetReg();
	void FreeReg( Byte iReg );

	// Declarations compiling.
	void CompileDeclaration();
	CTypeInfo CompileVarType( Bool bNoArray = false );	
	void CompileEnumDecl();
	void CompileConstDecl();
	Bool CompileVarDecl( TArray<CProperty*>& Vars, DWord& VarsSize, Bool bDetectFunc = true );	
	void CompileFunctionDecl();	
	void CompileThreadDecl();
	
	// Lexical analysis.
	void GetToken( TToken& T, Bool bAllowNeg = false, Bool bAllowVect = false );
	void GotoToken( const TToken& T );
	Integer ReadInteger( const Char* Purpose );
	Float ReadFloat( const Char* Purpose );
	String ReadString( const Char* Purpose );
	void RequireIdentifier( const Char* Name, const Char* Purpose );
	void RequireSymbol( const Char* Name, const Char* Purpose );
	String GetIdentifier( const Char* Purpose );
	String PeekIdentifier();
	String PeekSymbol();	
	Bool MatchIdentifier( const Char* Name );
	Bool MatchSymbol( const Char* Name );

	// Characters parsing functions.
	Char _GetChar();
	Char GetChar();
	void UngetChar();
	
	// Searching.
	CFunction* FindFunction( FScript* InScript, String Name );
	CProperty* FindProperty( const TArray<CProperty*>& List, String Name );
	TToken* FindConstant( String Name );
	CEnum* FindEnum( FScript* Script, String Name, Bool bOwnOnly = false );
	FScript* FindScript( String Name );
	CFamily* FindFamily( String Name );
	Byte FindEnumElement( String Elem );
	CNativeFunction* FindUnaryOp( String Name, CTypeInfo ArgType = TYPE_None );
	CNativeFunction* FindBinaryOp( String Name, CTypeInfo Arg1 = TYPE_None, CTypeInfo Arg2 = TYPE_None );
	CNativeFunction* FindNative( String Name );
	Integer FindStaticEvent( String Name );
};


/*-----------------------------------------------------------------------------
    CCodeEmitter implementation.
-----------------------------------------------------------------------------*/

// 
// Code emitter constructor.
//
CCodeEmitter::CCodeEmitter()
	:	Bytecode(nullptr)
{
	Mode		= SM_Save;
}


//
// Set a bytecode to emitter code.
//
void CCodeEmitter::SetBytecode( CBytecode* InBytecode )
{
	assert(InBytecode);
	Bytecode	= InBytecode;
}


//
// Tell current location in bytecode.
//
DWord CCodeEmitter::Tell()
{
	return Bytecode->Code.Num();
}


//
// Return total size of the code.
//
DWord CCodeEmitter::TotalSize()
{
	return Bytecode->Code.Num();
}


//
// Push a data to the code.
//
void CCodeEmitter::SerializeData( void* Mem, DWord Count )
{
	Integer OldLoc = Tell();
	Bytecode->Code.SetNum( Bytecode->Code.Num() + Count );
	MemCopy( &Bytecode->Code[OldLoc], Mem, Count );
}


//
// Serialize reference to some object.
//
void CCodeEmitter::SerializeRef( FObject*& Obj )
{
	Integer iObj = Obj ? Obj->GetId() : -1;
	Serialize( *this, iObj );
}


//
// Opcode serialization.
//
static void Serialize( CSerializer& S, EOpCode V )
{
	// Force to be byte.
	assert(S.GetMode() == SM_Save);
	S.SerializeData( &V, sizeof(Byte) );
}


//
// Property type serialization.
//
static void Serialize( CSerializer& S, EPropType V )
{
	// Force to be byte.
	assert(S.GetMode() == SM_Save);
	S.SerializeData( &V, sizeof(Byte) );
}


//
// TToken's constant value serialization.
//
static void Serialize( CSerializer& S, FScript* Owner, TToken& Const )
{
	assert(Const.Type == TOK_Const);

	switch( Const.TypeInfo.Type )
	{
		case TYPE_Byte:
			Serialize( S, CODE_ConstByte );
			Serialize( S, Const.cByte );
			break;

		case TYPE_Bool:
			Serialize( S, CODE_ConstBool );
			Serialize( S, Const.cBool );
			break;

		case TYPE_Integer:
			Serialize( S, CODE_ConstInteger );
			Serialize( S, Const.cInteger );
			break;

		case TYPE_Float:
			Serialize( S, CODE_ConstFloat );
			Serialize( S, Const.cFloat );
			break;	

		case TYPE_Angle:
			Serialize( S, CODE_ConstAngle );
			Serialize( S, Const.cAngle );
			break;	

		case TYPE_Color:
			Serialize( S, CODE_ConstColor );
			Serialize( S, Const.cColor );
			break;	

		case TYPE_String:
			Serialize( S, CODE_ConstString );
			Serialize( S, Const.cString );
			break;	

		case TYPE_Vector:
			Serialize( S, CODE_ConstVector );
			Serialize( S, Const.cVector );
			break;	

		case TYPE_AABB:
			Serialize( S, CODE_ConstAABB );
			Serialize( S, Const.cAABB );
			break;	

		case TYPE_Entity:
			Serialize( S, CODE_ConstEntity );
			Serialize( S, Const.cEntity );
			break;

		case TYPE_Resource:
			Serialize( S, CODE_ConstResource );
			if( Const.cResource )
			{
				// Add to list.
				Byte iRes = Owner->ResTable.AddUnique( Const.cResource );
				assert(Owner->ResTable.Num() < 256);
				Serialize( S, iRes );
			}
			else
			{
				// null resource.
				Byte iRes = 0xff;
				Serialize( S, iRes );
			}
			break;

		default:
			error( L"Unsupported constant type %d", (Byte)Const.TypeInfo.Type );
	}
}


/*-----------------------------------------------------------------------------
    CCompiler implementation.
-----------------------------------------------------------------------------*/

//
// Compiler constructor.
//
CCompiler::CCompiler( TArray<String>& OutWarnings, TCompilerError& OutFatalError )
	:	Warnings( OutWarnings ),
		FatalError( OutFatalError ),
		Storage(),
		Emitter(),
		Families(),
		Bytecode( nullptr )
{
}


//
// Compiler destructor.
//
CCompiler::~CCompiler()
{
	// Destroy temporal families list.
	for( Integer i=0; i<Families.Num(); i++ )
		delete Families[i];
	Families.Empty();
}


//
// Compilation entry point.
// Return true, if compilation successfully,
// otherwise return false and FatalError will has 
// an information about error.
//
Bool CCompiler::CompileAll( Bool bSilent )
{
	try
	{
		// Prepare for compilation.
		Warnings.Empty();
		FatalError.ErrorLine	= -1;
		FatalError.ErrorPos		= -1;
		FatalError.Message		= L"Everything is fine";
		FatalError.Script		= nullptr;

		log( L"** COMPILATION BEGAN **" );

		// Perform compilation step by step.
		if( !bSilent ) GEditor->TaskDialog->UpdateSubtask(L"Collecting");
		StoreAllScripts();

		for( Integer i=0; i<Storage.Num(); i++ )
			ParseHeader( Storage[i].Script );

		if( !bSilent ) GEditor->TaskDialog->UpdateSubtask(L"First-Pass Compiling");
		for( Integer i=0; i<Storage.Num(); i++ )
		{
			if( !bSilent && !(i & 3) ) GEditor->TaskDialog->UpdateProgress( i, Storage.Num() );
			CompileFirstPass( Storage[i].Script );
		}

		if( !bSilent ) GEditor->TaskDialog->UpdateSubtask(L"Second-Pass Compiling");
		for( Integer i=0; i<Storage.Num(); i++ )
		{
			CompileSecondPass( Storage[i].Script );
			if( !bSilent && !(i & 3) ) GEditor->TaskDialog->UpdateProgress( i, Storage.Num() );
		}

		RestoreAfterSuccess();

		// Count lines.
		Integer  NumLines = 0;
		for( Integer i=0; i<Storage.Num(); i++ )
			NumLines += Storage[i].Script->Text.Num();

		// Everything ok, so notify and return.
		log( L"Compiler: COMPILATION SUCCESSFULLY" );
		log( L"Compiler: %d scripts compiled", Storage.Num() );
		log( L"Compiler: %d lines compiled", NumLines );

		// Add to compilation log.
		Warnings.Push( L"---" );
		Warnings.Push(String::Format( L"%d scripts compiled", Storage.Num() ));
		Warnings.Push(String::Format( L"%d lines compiled", NumLines ));

		return true;
	}
	catch( ... )
	{
		// Something horrible happened.
		RestoreAfterFailure();
		log( L"Compiler: COMPILATION ABORTED" );
		log( L"Compiler: %s", *FatalError.Message );

		return false;
	}
}


//
// Parse a script header and figure out it 
// name and family.
//
void CCompiler::ParseHeader( FScript* InScript )
{
	// Prepare for parsing.
	Script				= InScript;
	Script->iFamily		= -1;
	TextLine			= 0;
	TextPos				= 0;
	PrevLine			= 0;
	PrevPos				= 0;

	// Compile header.
	RequireIdentifier( KW_script, L"script declaration" );
	RequireIdentifier( *Script->GetName(), L"script declaration" );

	// Parse family.
	if( MatchSymbol(L":") )
	{
		// Add to family or create new one.
		RequireIdentifier( KW_family, L"script family" );
		String FamilyName	= GetIdentifier(L"script family");
		CFamily* MyFamily = FindFamily( FamilyName );

		if( !MyFamily )
		{
			// Create new one.
			MyFamily			= new CFamily();
			MyFamily->Name		= FamilyName;
			MyFamily->Scripts.Push(Script);
			MyFamily->iFamily	= Families.Push( MyFamily );
			log( L"Compiler: Created new family '%s'", *MyFamily->Name );
		}
		else
		{
			// Add to exist.
			assert(MyFamily->Scripts.FindItem(Script)==-1);
			MyFamily->Scripts.Push(Script);
		}

		Script->iFamily	= MyFamily->iFamily;
	}
}


//
// Compile script first pass.
//
void CCompiler::CompileFirstPass( FScript* InScript )
{
	// Prepare compiler.
	Access				= ACC_Public;
	Script				= InScript;
	TextLine			= 0;
	TextPos				= 0;
	PrevLine			= 0;
	PrevPos				= 0;	

	// Compile header.
	RequireIdentifier( KW_script, L"script header" );
	RequireIdentifier( *Script->GetName(), L"script header" );

	if( MatchSymbol(L":") )
	{
		// Family.
		RequireIdentifier( KW_family, L"script header" );
		/*Script->Family	=*/ GetIdentifier(L"script family");
	}

	RequireSymbol( L"{", L"script body" );
	
	// Compile declaration by declaration.
	for( ; ; )
	{
		if( MatchSymbol( L"}" ) )
		{
			// End of script.
			break;
		}
		else if( MatchIdentifier(KW_public) )
		{
			// Start public section.
			Access = ACC_Public;
			RequireSymbol( L":", L"public section" );
		}
		else if( MatchIdentifier(KW_private) )
		{
			// Start private section.
			Access = ACC_Private;
			RequireSymbol( L":", L"private section" );
		}
		else
		{
			// Any single declaration.
			CompileDeclaration();
		}
	}
}


//
// Compile script second pass.
//
void CCompiler::CompileSecondPass( FScript* InScript )
{
	// Set target.
	Script	= InScript;

	// Fill complete VF table for script, its ok
	// if some slots are nullptr.
	CFamily* Family = Script->iFamily != -1 ? Families[Script->iFamily] : nullptr;
	if( Family )
	{
		assert(Family->VFNames.Num()==Family->Proto.Num());
		Script->VFTable.SetNum( Family->VFNames.Num() );
		for( Integer i=0; i<Script->VFTable.Num(); i++ )
			Script->VFTable[i] = FindFunction( Script, Family->VFNames[i] );
	}
	else
		Script->VFTable.Empty();

	// Fill list of events.
	Script->Events.Empty();
	Script->Events.SetNum(_EVENT_MAX);
	for( Integer i=0; i<Script->Functions.Num(); i++ )
		if( Script->Functions[i]->Flags & FUNC_Event )
		{ 
			CFunction*	Func	= Script->Functions[i];
			Integer		iEvent	= FindStaticEvent(Func->Name);
			assert(iEvent != -1);
			Script->Events[iEvent]	= Func;
		}

	// Compile actor thread if it specified. It's important to compile the thread
	// before functions.
	if( Script->Thread )
		CompileCode( Script->Thread );

	// Compile all functions.
	for( Integer i=0; i<Script->Functions.Num(); i++ )
		CompileCode( Script->Functions[i] );
}


//
// Compile function or thread body.
//
void CCompiler::CompileCode( CBytecode* InCode )
{
	// Set compiling target.
	Emitter.SetBytecode( InCode );
	NestTop			= 0;
	Bytecode		= InCode;
	Context			= CONT_This;

	// Move caret to the function start.
	TextLine	=	PrevLine	= Bytecode->iLine;
	TextPos		=	PrevPos		= Bytecode->iPos;
	assert(PeekSymbol()==L"{");

	PushNest( NEST_Leading );

	// Compile function code.
	CompileStatement();

	ReplNest( REPL_Return, Emitter.Tell() );		
	PopNest();

	// Add special mark to the end of the bytecode.
	emit( CODE_EOC );

    // Check code size.
	if( Bytecode->Code.Num() >= 65535 )
		Error( L"Too large function >64kB of code" );

#if 0
	// Debug it!
	log( L"%s::%s", *Script->GetName(), Bytecode != Script->Thread ? *((CFunction*)Bytecode)->Name : L"Thread" );
	for( Integer i=0; i<Bytecode->Code.Num(); i++ )
		log( L"[%d]=%d", i, Bytecode->Code[i] );
#endif
}


/*-----------------------------------------------------------------------------
    Expression compiling.
-----------------------------------------------------------------------------*/

//
// Macro to convert L-value to the R-value.
//
#define emit_ltor( lval )\
if( lval.bLValue )\
{\
	if( lval.Type.Type == TYPE_String )\
	{\
		emit( CODE_LToRString );\
		emit( lval.iReg );\
	}\
	else if( lval.Type.TypeSize(true) == 4 )\
	{\
		emit( CODE_LToRDWord );\
		emit( lval.iReg );\
	}\
	else\
	{\
		Byte VarSize = lval.Type.TypeSize(true);\
		emit( CODE_LToR );\
		emit( lval.iReg );\
		emit( VarSize );\
	}\
	lval.bLValue = false;\
}\


//
// Macro to handle different assignments.
// lside - left side of assignment, should be l-value.
// rside - right side of assignment, should be r-value! 
//
#define emit_assign( lside, rside )\
{\
	if( lside.Type.Type == TYPE_String )\
	{\
		emit( CODE_AssignString );\
		emit( lside.iReg );\
		emit( rside.iReg );\
	}\
	else if( lside.Type.TypeSize(true) == 4 )\
	{\
		emit( CODE_AssignDWord );\
		emit( lside.iReg );\
		emit( rside.iReg );\
	}\
	else\
	{\
		Byte VarSize = lside.Type.TypeSize(true);\
		emit( CODE_Assign );\
		emit( lside.iReg );\
		emit( rside.iReg );\
		emit( VarSize );\
	}\
}\


//
// Compile a top-level expression. The most valuable function.
// Return information about compiled expression, its type and result register.
// If ReqType specified( not TYPE_None ), required to expression matched this type
// tries to apply type cast, if failed its abort compilation.
//
//	ReqType			- Required expression type, or TYPE_None, if doesn't really matter.
//	bForceR			- Force expression be an R-value and return value in R register.
//	bAllowAssign	- Whether allow assignment in expression.
//	InPri			- Binary operator priority, don't touch it, use just 0.
//
TExprResult CCompiler::CompileExpr( const CTypeInfo& ReqType, Bool bForceR, Bool bAllowAssign, DWord InPri )
{
	// Scrap.
	CProperty*			Prop;
	TToken*				Const;
	Byte				iEnumEl;
	CNativeFunction*	Native;
	FScript*			CastScript;
	CFamily*			CastFamily;

	// Prepare for expression compilation.
	TToken T;
	TExprResult ExprRes;
	GetToken( T, false, false );

	//
	// Step 1: Figure out the base expression.
	//
	if( T.Text == L"(" )
	{
		// Bracket, compile subexpression.
		ExprRes = CompileExpr( TYPE_None, false, false, 0 );
		RequireSymbol( L")", L"expression bracket" );
	}
	else if( T.Type == TOK_Const )
	{
		// Literal constant.
		ExprRes.bLValue		= false;
		ExprRes.iReg		= GetReg();
		ExprRes.Type		= T.TypeInfo;

		emit_const( T );
		emit( ExprRes.iReg );
	}
	else if( T.Text == KW_this )
	{
		// Reference to the self.
		ExprRes.bLValue			= false;
		ExprRes.iReg			= GetReg();
		ExprRes.Type			= CTypeInfo( TYPE_Entity, 1, Script );
		ExprRes.Type.iFamily	= Script->iFamily;

		emit( CODE_This );
		emit( ExprRes.iReg );
	}
	else if( T.Text == KW_assert )
	{
		// Assertion.
		Word Line = TextLine + 1;
		Byte iReg = CompileExpr( TYPE_Bool, true, false, 0 ).iReg;

		emit( CODE_Assert );
		emit( Line );
		emit( iReg );

		bValidExpr		= true;
		ExprRes.Type	= TYPE_None;
		FreeReg( iReg );
	}
	else if( T.Text == KW_log )
	{
		// Some kind of printf in C.
		RequireSymbol( L"(", L"log" );
		TArray<TExprResult> Args;

		String Text = ReadString( L"format string" );
		String Fmt = Text;

		// Parse each arg.
		do
		{
			Integer i = String::Pos( L"%", Text );
			if( i == -1 )
				break;

			RequireSymbol( L",", L"log arguments" );
			Text = String::Delete( Text, i, 1 );
			Char Symbol = Text(i);
			EPropType ArgType;

			switch( Symbol )
			{
				case 'i':
				case 'd':
					ArgType = TYPE_Integer;
					break;

				case 'b':
					ArgType = TYPE_Bool;
					break;

				case 'f':
					ArgType = TYPE_Float;
					break;

				case 's':
					ArgType	= TYPE_String;
					break;

				case 'v':
					ArgType	= TYPE_Vector;
					break;

				case 'r':
					ArgType	= TYPE_Resource;
					break;

				case 'c':
					ArgType	= TYPE_Color;
					break;

				case 'a':
					ArgType	= TYPE_AABB;
					break;

				case 'x':
					ArgType	= TYPE_Entity;
					break;

				default:
					Char Str[2] = { Symbol, '\0' }; 
					Error( L"Unknown format symbol '%s' in log", Str );
					break;
			}

			// Collect registers.
			Args.Push( CompileExpr( ArgType, true, false, 0 ) );
		}
		while( true );

		RequireSymbol( L")", L"log" );

		emit( CODE_Log );
		emit( Fmt );
		for( Integer i=0; i<Args.Num(); i++ )
		{
			emit( Args[i].Type.Type );
			emit( Args[i].iReg );
			FreeReg( Args[i].iReg );
		}

		bValidExpr		= true;
		ExprRes.Type	= TYPE_None;
	}
	else if( T.Text == KW_length )
	{
		// Return the length of string or static array.
		TExprResult Val = CompileExpr( TYPE_None, false, false, 100 );

		if( Val.Type.Type != TYPE_None && Val.Type.ArrayDim != 1 )
		{
			// Array length, store as integer constant.
			ExprRes.bLValue			= false;
			ExprRes.iReg			= GetReg();
			ExprRes.Type			= CTypeInfo( TYPE_Integer );

			emit( CODE_ConstInteger );
			emit( Val.Type.ArrayDim );
			emit( ExprRes.iReg );
			FreeReg( Val.iReg );
		}
		else if( Val.Type.Type == TYPE_String )
		{
			// Length of string.
			emit_ltor( Val );
			ExprRes.bLValue			= false;
			ExprRes.iReg			= GetReg();
			ExprRes.Type			= CTypeInfo( TYPE_Integer );

			emit( CODE_Length );
			emit( Val.iReg );
			emit( ExprRes.iReg );
			FreeReg( Val.iReg );
		}
		else
			Error( L"Bad argument in 'length'" );
	}
	else if( T.Text == KW_new )
	{
		// Create a new entity.
		TToken S;
		GetToken( S );
		GotoToken( S );
		FScript* Known;
		TExprResult ScriptExpr;

		if( (Known = FindScript(GetIdentifier(L"script name"))) != nullptr  )
		{
			// Script known.
			ScriptExpr.bLValue		= false;
			ScriptExpr.iReg			= GetReg();
			ScriptExpr.Type			= TYPE_SCRIPT;
			ScriptExpr.Type.iFamily	= Known->iFamily;

			emit( CODE_ConstResource );
			Byte iRes = Script->ResTable.AddUnique( Known );
			assert(Script->ResTable.Num() < 256);
			emit( iRes );
			emit( ScriptExpr.iReg );

			ExprRes.bLValue			= false;
			ExprRes.iReg			= GetReg();
			ExprRes.Type			= CTypeInfo( TYPE_Entity, 1, Known );
			ExprRes.Type.iFamily	= Known->iFamily;
		}
		else
		{
			// Should use RTTI.
			GotoToken( S );
			ScriptExpr = CompileExpr( TYPE_SCRIPT, true, false, 0 );

			ExprRes.bLValue			= false;
			ExprRes.iReg			= GetReg();
			ExprRes.Type			= CTypeInfo( TYPE_Entity, 1 );
			ExprRes.Type.iFamily	= -1;
		}

		emit( CODE_New );
		emit( ScriptExpr.iReg );
		emit( ExprRes.iReg );

		bValidExpr				= true;
		FreeReg( ScriptExpr.iReg );
	}
	else if( T.Text == KW_delete )
	{
		// Delete an entity.
		TExprResult EntityExpr = CompileExpr( TYPE_Entity, true, false, 0 );
		emit( CODE_Delete );
		emit( EntityExpr.iReg );

		FreeReg( EntityExpr.iReg );
		ExprRes.bLValue		= false;
		ExprRes.Type		= TYPE_None;
		bValidExpr			= true;
	}
	else if( T.Text == KW_label )
	{
		// A current thread label.
		if( !Script->Thread )
			Error( L"Script has no thread" );

		ExprRes.bLValue			= false;
		ExprRes.iReg			= GetReg();
		ExprRes.Type			= TYPE_Integer;

		emit( CODE_Label );
		emit( ExprRes.iReg );
	}
	else if( T.Text == KW_proto )
	{
		// Prototype value.
		ExprRes = CompileProtoExpr( Script ); 
	}
	else if( T.Text == L"[" )
	{
		// Vector constructor.
		TExprResult	X	= CompileExpr( TYPE_Float, true, false, 0 );
		RequireSymbol( L",", L"vector constructor" );
		TExprResult Y	= CompileExpr( TYPE_Float, true, false, 0 );
		RequireSymbol( L"]", L"vector constructor" );

		ExprRes.bLValue		= false;
		ExprRes.iReg		= GetReg();
		ExprRes.Type.Type	= TYPE_Vector;

		emit( CODE_VectorCnstr );
		emit( ExprRes.iReg );
		emit( X.iReg );
		emit( Y.iReg );

		FreeReg( X.iReg );
		FreeReg( Y.iReg );
	}
	else if( T.Text == L"@" )
	{
		// Label constant.
		if( !Script->Thread )
			Error( L"Labels doesn't allowed, without thread" );

		String LabName = GetIdentifier( L"label" );
		Integer iLabel = Script->Thread->GetLabelId( *LabName );

		if( iLabel == -1 )
			Error( L"Label '@%s' not found", *LabName );

		// Store as integer constant.
		ExprRes.bLValue	= false;
		ExprRes.iReg	= GetReg();
		ExprRes.Type	= TYPE_Integer;

		emit( CODE_ConstInteger );
		emit( iLabel );
		emit( ExprRes.iReg );
	}
	else if	( 
				Bytecode != Script->Thread && 
				(Prop = FindProperty( ((CFunction*)Bytecode)->Locals, T.Text) ) 
			)
	{
		// A local variable.
		CFunction*	Owner	= (CFunction*)Bytecode;
		Word	Offset		= Prop->Offset;

		ExprRes.bLValue		= true;
		ExprRes.iReg		= GetReg();
		ExprRes.Type		= *Prop;

		emit( CODE_LocalVar );
		emit( ExprRes.iReg );
		emit( Offset );
	}
	else if( Native = FindUnaryOp( T.Text, TYPE_None ) )
	{
		// Unary operator.
		TExprResult Arg = CompileExpr( TYPE_None, false, false, 777 );
		if( Arg.Type.Type == TYPE_None )
			Error( L"Bad argument in unary operator '%s'", *T.Text );

		Native = FindUnaryOp( T.Text, Arg.Type );
		if( !Native )
			Error( L"Operator '%s' cannot be applied to operand of type %s", *T.Text, *Arg.Type.TypeName() );

		// It an inc/dec operator or regular.
		if( Native->Flags & NFUN_SuffixOp )
		{
			// L-value operator.
			if( !Arg.bLValue )
				Error( L"The right-hand side of an assignment must be a variable" );

			emit( Native->iOpCode );
			emit( Arg.iReg );

			ExprRes.bLValue		= true;
			ExprRes.iReg		= Arg.iReg;
			ExprRes.Type		= Native->ResultType;
			bValidExpr			= true;
		}
		else
		{
			// R-value operator.
			emit_ltor( Arg );

			// Types are matched?
			if( Native->ParamsType[0].Type != Arg.Type.Type )	// It's ok, since it's a simple types.
			{
				// Add cast.
				Byte Cast = GetCast( Arg.Type.Type, Native->ParamsType[0].Type );
				assert(Cast != 0x00);

				emit( Cast );
				emit( Arg.iReg );
			}

			emit( Native->iOpCode );
			emit( Arg.iReg );

			ExprRes.bLValue		= false;
			ExprRes.Type		= Native->ResultType;
			ExprRes.iReg		= Arg.iReg;
		}
	}
	else if( Native = FindNative(T.Text) )
	{
		// Native function call.
		TArray<Byte>	ArgRegs;

		if( Native->Flags & NFUN_Foreach )
			Error( L"Iteration function '%s' is not allowed here", *Native->Name );

		RequireSymbol( L"(", L"function call" );
		for( Integer i=0; (i<8)&&(Native->ParamsType[i].Type!=TYPE_None); i++ )
		{
			ArgRegs.Push( CompileExpr( Native->ParamsType[i], true, false, 0 ).iReg );

			if( i<7 && Native->ParamsType[i+1].Type != TYPE_None )
				RequireSymbol( L",", L"arguments" ); 
		}
		RequireSymbol( L")", L"function call" );

		emit( Native->iOpCode );
		for( Integer i=0; i<ArgRegs.Num(); i++ )
		{
			emit( ArgRegs[i] );
			FreeReg( ArgRegs[i] );
		}

		if( Native->ResultType.Type != TYPE_None )
		{
			// Has result.
			ExprRes.Type		= Native->ResultType;
			ExprRes.iReg		= GetReg();
			ExprRes.bLValue		= false;
			emit( ExprRes.iReg );
		}
		else
		{
			// No result.
			ExprRes.bLValue		= false;
			ExprRes.iReg		= -1;
			ExprRes.Type		= TYPE_None;
		}

		bValidExpr	= true;
	}
	else if( CastScript = FindScript( T.Text ) )
	{
		// Entity script cast or prototype value.
		if( MatchSymbol(L".") )
		{
			// Proto.
			RequireIdentifier( KW_proto, L"script prototype" );
			ExprRes = CompileProtoExpr( CastScript );
		}
		else
		{
			// Typecast.
			RequireSymbol( L"(", L"explicit cast" );
			TExprResult Ent	= CompileExpr( TYPE_Entity, true, false, 0 );
			RequireSymbol( L")", L"explicit cast" );

			ExprRes.bLValue			= false;
			ExprRes.iReg			= Ent.iReg;
			ExprRes.Type			= CTypeInfo( TYPE_Entity, 1, CastScript );
			ExprRes.Type.iFamily	= CastScript->iFamily;
	
			emit( CODE_EntityCast );
			emit( Ent.iReg );
			emit( ExprRes.Type.Script );
		}
	}
	else if( CastFamily = FindFamily( T.Text ) )
	{
		// Entity family cast.
		RequireSymbol( L"(", L"explicit cast" );
		TExprResult Ent	= CompileExpr( TYPE_Entity, true, false, 0 );
		RequireSymbol( L")", L"explicit cast" );

		ExprRes.bLValue			= false;
		ExprRes.iReg			= Ent.iReg;
		ExprRes.Type			= CTypeInfo( TYPE_Entity, 1, Ent.Type.Script );
		ExprRes.Type.iFamily	= CastFamily->iFamily;
		assert(CastFamily->Scripts.Num()>0 && CastFamily->iFamily!=-1);

		emit( CODE_FamilyCast );
		emit( Ent.iReg );
		emit( CastFamily->iFamily );
	}
	else if( Const = FindConstant(T.Text) )
	{
		// Named constant.
		ExprRes.bLValue		= false;
		ExprRes.Type.Type	= Const->TypeInfo.Type;
		ExprRes.iReg		= GetReg();

		emit_const( *Const );
		emit( ExprRes.iReg );
	}
	else if( (iEnumEl = FindEnumElement( T.Text )) != 0xff )
	{
		// Of desperation, search enum element.
		ExprRes.bLValue		= false;
		ExprRes.iReg		= GetReg();
		ExprRes.Type		= TYPE_Byte;

		emit( CODE_ConstByte );
		emit( iEnumEl );
		emit( ExprRes.iReg );
	}
	else
	{
		// Try last thing.
		GotoToken( T );
		if( !CompileEntityExpr( CONT_This, CTypeInfo( TYPE_Entity, 1, Script ), 0xff, ExprRes ) )
			Error( L"Expected expression, got '%s'", *T.Text );
	}


	//
	// Step 1.5: Check for none result, is it has sense to continue parse expression.
	//
	if( ExprRes.Type.Type == TYPE_None )
	{
		if( ReqType.Type != TYPE_None )
			Error( L"'%s' expected but 'void' found", ReqType.TypeName() );

		return ExprRes;
	}


	//
	// Step 2: Members.
	//
	for( ; ; )
	{
		if( MatchSymbol( L"[" ) )
		{
			// Get an array element.
			if( ExprRes.Type.ArrayDim <= 1 )
				Error( L"Array type excepting" );
	
			if( !ExprRes.bLValue )
				Error( L"Array variable excepting" );

			// Get index subexpression.
			TExprResult Index = CompileExpr( TYPE_Integer, true, false, 0 );
			RequireSymbol( L"]", L"array index" );

			Byte InnerSize	= ExprRes.Type.TypeSize(true);
			Byte ArrSize	= ExprRes.Type.ArrayDim;
			emit( CODE_ArrayElem );
			emit( ExprRes.iReg );
			emit( Index.iReg );
			emit( InnerSize );
			emit( ArrSize );

			// Replace with inner.
			FreeReg( Index.iReg );
			ExprRes.Type.ArrayDim	= 1;
		}
		else if( MatchSymbol( L"." ) )
		{
			Byte Offset = 0xff;
			if( ExprRes.Type.Type == TYPE_Vector )
			{
				// Vector member.
				String Elem = String::LowerCase(GetIdentifier( L"vector member" ));
				Offset = Elem == L"x" ? 0 : Elem == L"y" ? 4 : 0xff;
				if( Offset == 0xff )
					Error( L"Unknown vector member '%s'", *Elem );
				ExprRes.Type.Type	= TYPE_Float;
				emit( ExprRes.bLValue ? CODE_LMember : CODE_RMember );
				emit( ExprRes.iReg );
				emit( Offset );
			}
			else if( ExprRes.Type.Type == TYPE_AABB )
			{
				// TRect member.
				String Elem = String::LowerCase(GetIdentifier( L"aabb member" ));
				Offset = Elem == L"min" ? 0 : Elem == L"max" ? 8 : 0xff;
				if( Offset == 0xff )
					Error( L"Unknown aabb member '%s'", *Elem );
				ExprRes.Type.Type	= TYPE_Vector;
				emit( ExprRes.bLValue ? CODE_LMember : CODE_RMember );
				emit( ExprRes.iReg );
				emit( Offset );
			}
			else if( ExprRes.Type.Type == TYPE_Color )
			{
				// Color member.
				String Elem = String::LowerCase(GetIdentifier( L"color member" ));
				Offset = Elem == L"r" ? 0 : Elem == L"g" ? 1 :  Elem == L"b" ? 2 : Elem == L"a" ? 3 : 0xff;
				if( Offset == 0xff )
					Error( L"Unknown color member '%s'", *Elem );
				ExprRes.Type.Type	= TYPE_Byte;
				emit( ExprRes.bLValue ? CODE_LMember : CODE_RMember );
				emit( ExprRes.iReg );
				emit( Offset );
			}
			else if( ExprRes.Type.Type == TYPE_Resource )
			{
				// Get a resource property.
				if( !ExprRes.Type.Class )
					Error( L"Bad resource class" );

				String PropName = GetIdentifier( L"resource property" );
				CProperty* Prop = ExprRes.Type.Class->FindProperty( *PropName );
				if( !Prop )
					Error( L"Property '%s' not found in '%s'", *PropName, *ExprRes.Type.Class->Alt );

				if( !(Prop->Flags & PROP_Editable) )
					Error( L"Property '%s' is not editable", *PropName );

				Word Offset = Prop->Offset;
				emit_ltor( ExprRes );
				emit( CODE_ResourceProperty );
				emit( ExprRes.iReg );
				emit( Offset );

				ExprRes.bLValue	= true;
				ExprRes.Type	= *Prop;

				// If property are const, force it to be const.
				if( Prop->Flags & PROP_Const )
				{
					emit_ltor( ExprRes );
					ExprRes.bLValue	= false;
				}
			}
			else if( ExprRes.Type.Type == TYPE_Entity )
			{
				// Compile other entity subexpression.
				emit_ltor(ExprRes);
				if( !CompileEntityExpr( CONT_Other, ExprRes.Type, ExprRes.iReg, ExprRes ) )
					Error( L"Missing entity sub-expression" );
			}
			else
				Error( L"Struct or object type expected" );
		}
		else
			break;
	}


	//	
	// Step 3: Postfix operators.
	//
	if( PeekSymbol() == L"++" || PeekSymbol() == L"--" )
	{
		GetToken( T, false, false );
		CNativeFunction* SuffOp = FindUnaryOp( T.Text, ExprRes.Type );

		if( !SuffOp )
			Error
				( 
					L"Postfix operator '%s' not applicable to %s operand type", 
					*T.Text, 
					*ExprRes.Type.TypeName() 
				);

		if( !ExprRes.bLValue || ExprRes.Type.ArrayDim>1 )
			Error( L"The right-hand side of an assignment must be a variable" );

		assert(ExprRes.Type.Type != TYPE_String);
		
		// Sorry, its works wrong way!
		// Not real C-style rule :( But it not so bad.
		emit( SuffOp->iOpCode );
		emit( ExprRes.iReg );

		emit_ltor( ExprRes );

		ExprRes.Type	= SuffOp->ResultType;
		ExprRes.bLValue	= false;
		bValidExpr		= true;
	}

	//
	// Step 4: Assignment.
	//
	if( MatchSymbol( L"=" ) )
	{
		if( !bAllowAssign )
			Error( L"The assignment doesn't allowed here" );

		if( !ExprRes.bLValue )
			Error( L"The left-hand side of an assignment must be a variable" );

		if( ExprRes.Type.ArrayDim != 1 )
			Error( L"An array assignment doesn't allowed" );

		// Compile right side of assignment.
		TExprResult Right = CompileExpr( ExprRes.Type, true, false, 0 );

		// Emit assignment.
		emit_assign( ExprRes, Right );

		// Finish him!
		FreeReg( ExprRes.iReg );
		FreeReg( Right.iReg );

		bValidExpr		= true;
		ExprRes.Type	= TYPE_None;
		return ExprRes;
	}


	//
	// Step 5: Binary operators.
	//
OperLoop:

	// Get operator symbol.
	GetToken( T, false, false );

	if( T.Text == KW_is )
	{
		// 'Is' operator.
		if( ExprRes.Type.Type != TYPE_Entity || ExprRes.Type.ArrayDim != 1 )
			Error( L"Entity type required in 'is' operator" );

		TExprResult ScriptExpr = CompileExpr( TYPE_SCRIPT, true, false );

		emit_ltor( ExprRes );
		emit( CODE_Is );
		emit( ExprRes.iReg );
		emit( ScriptExpr.iReg );

		FreeReg( ScriptExpr.iReg );

		ExprRes.bLValue		= false;
		ExprRes.Type.Type	= TYPE_Bool;
		goto OperLoop;
	}	
	else if( T.Text == KW_in )
	{
		// 'In' operator.
		if( ExprRes.Type.Type != TYPE_Entity || ExprRes.Type.ArrayDim != 1 )
			Error( L"Entity type required in 'in' operator" );

		String FamilyName = GetIdentifier( L"family name" );
		CFamily* Family = FindFamily( FamilyName );
		if( !Family )
			Error( L"Family '%s' not found", *FamilyName );
		assert(Family->Scripts.Num()>0);

		emit_ltor( ExprRes );
		emit( CODE_In );
		emit( ExprRes.iReg );
		emit( Family->iFamily );	

		ExprRes.bLValue		= false;
		ExprRes.Type.Type	= TYPE_Bool;
		goto OperLoop;
	}
	else if( ( T.Text == L"==" || T.Text == L"!=" ) && InPri < 7  )
	{
		// Comparison operators.
		if( ExprRes.Type.ArrayDim != 1 )
			Error( L"Array comparison doesn't allowed" );

		emit_ltor( ExprRes );

		TExprResult Other = CompileExpr( ExprRes.Type, true, false, 7 );
		Byte VarSize = ExprRes.Type.Type == TYPE_String ? 0 : ExprRes.Type.TypeSize(true);

		emit( T.Text == L"==" ? CODE_Equal : CODE_NotEqual );
		emit( ExprRes.iReg );
		emit( Other.iReg );
		emit( VarSize );

		FreeReg( Other.iReg );
		ExprRes.bLValue		= false;
		ExprRes.Type		= TYPE_Bool;
		goto OperLoop;
	}
	else if( ( T.Text == L"&&" && InPri < 3 ) || ( T.Text == L"||" && InPri < 2 ) )
	{
		// Short circuit operators.
		Bool	bAnd	= T.Text == L"&&";
		Integer Prior	= bAnd ? 3 : 2;

		if( ExprRes.Type.Type != TYPE_Bool || ExprRes.Type.ArrayDim != 1 )
			Error( L"Bool type expected in '%s' operator", *T.Text );

		emit_ltor( ExprRes );

		if( bAnd )
		{
			// Logical 'and'.
			Word DstExpr1, DstExpr2, DstOut;
			TExprResult Result;
			Result.bLValue			= false;
			Result.iReg				= GetReg();
			Result.Type				= TYPE_Bool;

			emit( CODE_JumpZero );
			DstExpr1	 = Emitter.Tell();
			emit( GTempWord );
			emit( ExprRes.iReg );

			TExprResult Second = CompileExpr( TYPE_Bool, true, false, Prior );

			emit( CODE_JumpZero );
			DstExpr2	= Emitter.Tell();
			emit( GTempWord );
			emit( Second.iReg );

			// Emit true.
			Bool bTrue = true;
			emit( CODE_ConstBool );
			emit( bTrue );
			emit( Result.iReg );

			emit( CODE_Jump );
			DstOut	= Emitter.Tell();
			emit( GTempWord );

			*(Word*)&Bytecode->Code[DstExpr1]	= Emitter.Tell();
			*(Word*)&Bytecode->Code[DstExpr2]	= Emitter.Tell();

			// Emit false.
			Bool bFalse = false;
			emit( CODE_ConstBool );
			emit( bFalse );
			emit( Result.iReg );

			*(Word*)&Bytecode->Code[DstOut]	= Emitter.Tell();

			FreeReg( ExprRes.iReg );
			FreeReg( Second.iReg );
			ExprRes	= Result;
		}
		else
		{
			// Logical 'or'.
			Word DstOut1, DstOut2, DstExpr2, DstZr;
			Bool bTrue = true, bFalse = false;
			TExprResult Result;
			Result.bLValue			= false;
			Result.iReg				= GetReg();
			Result.Type				= TYPE_Bool;

			emit( CODE_JumpZero );
			DstExpr2	 = Emitter.Tell();
			emit( GTempWord );
			emit( ExprRes.iReg );

			emit( CODE_ConstBool );
			emit( bTrue );
			emit( Result.iReg );

			emit( CODE_Jump );
			DstOut1		= Emitter.Tell();
			emit( GTempWord );

			*(Word*)&Bytecode->Code[DstExpr2]	= Emitter.Tell();
			TExprResult Second = CompileExpr( TYPE_Bool, true, false, Prior );

			emit( CODE_JumpZero );
			DstZr	 = Emitter.Tell();
			emit( GTempWord );
			emit( Second.iReg );

			emit( CODE_ConstBool );
			emit( bTrue );
			emit( Result.iReg );

			emit( CODE_Jump );
			DstOut2		= Emitter.Tell();
			emit( GTempWord );

			*(Word*)&Bytecode->Code[DstZr]	= Emitter.Tell();
			emit( CODE_ConstBool );
			emit( bFalse );
			emit( Result.iReg );

			*(Word*)&Bytecode->Code[DstOut1]	= Emitter.Tell();
			*(Word*)&Bytecode->Code[DstOut2]	= Emitter.Tell();

			FreeReg( ExprRes.iReg );
			FreeReg( Second.iReg );
			ExprRes	= Result;
		}

		goto OperLoop;
	}
	else
	{
		// Try to find standard.
		CNativeFunction* Oper = FindBinaryOp( T.Text, TYPE_None, TYPE_None );

		// Test operator and priority.
		if( Oper && InPri < Oper->Priority )
		{
			// Compile second expression to figure out exactly operator
			// since Oper is wrong, due operator overloading.
			TExprResult Second = CompileExpr( TYPE_None, true, false, Oper->Priority );
			if( Second.Type.Type == TYPE_None )
				Error( L"Bad second argument in operator '%s'", *T.Text );

			// Search second time to find exactly.
			Oper	= FindBinaryOp( T.Text, ExprRes.Type, Second.Type );
			if( !Oper )
				Error( L"Operator '%s' not applicable to '%s' and '%s'", *T.Text, *ExprRes.Type.TypeName(), *Second.Type.TypeName() );

			if( Oper->Flags & NFUN_AssignOp )
			{
				// It's an assignment operator, such as += or <<=.
				if( !ExprRes.bLValue )
					Error( L"The left-hand side of an assignment must be a variable" );

				// No cast required for first.
				assert( Oper->ParamsType[0].Type == ExprRes.Type.Type );

				// Cast second if any.
				if( Second.Type.Type != Oper->ParamsType[1].Type )
				{
					Byte Cast = GetCast( Second.Type.Type, Oper->ParamsType[0].Type );
					emit( Cast );
					emit( Second.iReg );
				}

				emit( Oper->iOpCode );
				emit( ExprRes.iReg );
				emit( Second.iReg );
				FreeReg( Second.iReg );

				// Result are r value.
				ExprRes.bLValue	= false;
				ExprRes.Type	= Oper->ResultType;

				bValidExpr	= true;
			}
			else
			{
				// It's a regular operator.
				emit_ltor( ExprRes );

				// Apply cast if any.
				if( ExprRes.Type.Type != Oper->ParamsType[0].Type )
				{
					Byte Cast = GetCast( ExprRes.Type.Type, Oper->ParamsType[0].Type );
					emit( Cast );
					emit( ExprRes.iReg );
				}
				if( Second.Type.Type != Oper->ParamsType[1].Type )
				{
					Byte Cast = GetCast( Second.Type.Type, Oper->ParamsType[0].Type );
					emit( Cast );
					emit( Second.iReg );
				}

				// Emit operator.
				emit( Oper->iOpCode );
				emit( ExprRes.iReg );
				emit( Second.iReg );
				FreeReg( Second.iReg );

				// Store result.
				ExprRes.Type	= Oper->ResultType;
				ExprRes.bLValue	= false;
			}

			goto OperLoop;
		}
		else
		{
			// It's not an operator, or let over one CompileExpr handle it.
			GotoToken( T );
		}	
	}


	//
	// Step 6: Ternary conditional.
	//
	if( InPri == 0 && MatchSymbol(L"?") )
	{
		// Bool expression.
		emit_ltor( ExprRes );
		if( ExprRes.Type.Type != TYPE_Bool || ExprRes.Type.ArrayDim != 1 )
			Error( L"Bool type expected in '?' operator" );

		Word DstExpr2, DstOut;

		emit( CODE_JumpZero );
		DstExpr2		= Emitter.Tell();
		emit( GTempWord );
		emit( ExprRes.iReg );

		TExprResult First = CompileExpr( TYPE_None, true, false, 0 );
		if( First.Type.Type == TYPE_None || First.Type.ArrayDim != 1 )
			Error( L"Bad ternary operand type" );

		emit( CODE_Jump );
		DstOut			= Emitter.Tell();
		emit( GTempWord );

		RequireSymbol( L":", L"ternary operator" );
		*(Word*)&Bytecode->Code[DstExpr2]	= Emitter.Tell();
		TExprResult Second = CompileExpr( First.Type, true, false, 0 );

		*(Word*)&Bytecode->Code[DstOut]	= Emitter.Tell();

		TExprResult Result;
		Result.bLValue		= false;
		Result.iReg			= GetReg();
		Result.Type			= First.Type;

		emit( CODE_ConditionalOp );
		emit( Result.iReg );
		emit( First.iReg );
		emit( Second.iReg );
		emit( ExprRes.iReg );

		FreeReg( ExprRes.iReg );
		FreeReg( First.iReg );
		FreeReg( Second.iReg );
		ExprRes	= Result;
	}


	//
	// Step 7: Implicit type casting.
	//
	if( ExprRes.bLValue && bForceR )
	{
		// Force result to be r-value.
		emit_ltor( ExprRes );
	}

	if( ReqType.Type == TYPE_None || ExprRes.Type.MatchTypes(ReqType) )			
	{
		// Result type, doesn't really matter, or types are matched.
		// Expression compilation ok.
		return ExprRes;
	} 
	else
	{
		// Typecast required.
		emit_ltor( ExprRes );
		
		// Try to apply implicit typecast.
		Byte Cast = GetCast( ExprRes.Type.Type, ReqType.Type );

		if( Cast != 0x00 )
		{
			// Found cast.
			emit( Cast );
			emit( ExprRes.iReg );
			ExprRes.Type = ReqType;
			return ExprRes;
		}
		else
		{
			// Error in expression.
			Error( L"Incompatible types: '%s' and '%s'", *ReqType.TypeName(), *ExprRes.Type.TypeName() );
			return ExprRes;
		}
	}
}


//
// Compile prototype variable expression.
//
TExprResult CCompiler::CompileProtoExpr( FScript* Prototype )
{
	assert(Prototype);

	// Getting start.
	TToken T;
	CProperty* Property;
	TExprResult Result;
	RequireSymbol( L".", L"proto variable" );
	GetToken( T, false, false );

	if( Property = FindProperty( Prototype->Properties, T.Text ) )
	{
		// Instance buffer value.
		Byte iSource	= 0xff;
		Word Offset		= Property->Offset;

		Result.bLValue	= true;
		Result.iReg		= GetReg();
		Result.Type		= *Property;

		emit( CODE_ProtoProperty );
		emit( Prototype );
		emit( iSource );
		emit( Result.iReg );
		emit( Offset );
		emit_ltor(Result);
	}
	else if( Property = Prototype->Base->GetClass()->FindProperty(*T.Text) )
	{
		// Base property.
		Byte iSource	= 0xfe;
		Word Offset		= Property->Offset;

		Result.bLValue	= true;
		Result.iReg		= GetReg();
		Result.Type		= *Property;

		emit( CODE_ProtoProperty );
		emit( Prototype );
		emit( iSource );
		emit( Result.iReg );
		emit( Offset );
		emit_ltor(Result);
	}
	else if( T.Text==KW_base || T.Text==L"$" )
	{
		// Component property.
		FComponent* Component;
		Byte iSource = 0xfe;
		if( T.Text == L"$" )
		{
			// Extra component.
			String Name = GetIdentifier( L"component name" );
			Component	= Prototype->FindComponent( Name );
			if( !Component )
				Error( L"Component '%s' not found in '%s'", *Name, *Prototype->GetName() );
			iSource		= Prototype->Components.FindItem( (FExtraComponent*)Component );
		}
		else
		{
			// Base component, deprecated access.
			Component	= Prototype->Base;
			iSource		= 0xfe;
		}

		RequireSymbol( L".", L"component field" );
		CClass*	Class	= Component->GetClass();
		String Field	= GetIdentifier( L"component field" );

		if( Property = Class->FindProperty(*Field) )
		{
			Word Offset		= Property->Offset;

			Result.bLValue	= true;
			Result.iReg		= GetReg();
			Result.Type		= *Property;

			emit( CODE_ProtoProperty );
			emit( Prototype );
			emit( iSource );
			emit( Result.iReg );
			emit( Offset );
			emit_ltor(Result);
		}
		else
			Error( L"Unknown component '%s' property '%s'", *Class->Alt, *Field );
	}
	else
		Error( L"Proto property expected, got '%s'", *T.Text );

	return Result;
}


//
// Emit context switch if required.
//
#define emit_context( contype, conreg )\
if( contype == CONT_This )\
{\
	/* This expression in 'this' context.*/\
	if( Context != CONT_This )\
	{\
		Byte iReg = GetReg();\
		emit( CODE_This );\
		emit( iReg );\
		emit( CODE_Context );\
		emit( iReg );\
		FreeReg( iReg );\
		Context	= CONT_This;\
	}	\
}\
else\
{\
	/* Other context, switch it anyway.*/ \
	assert(conreg != 0xff);\
	emit( CODE_Context );\
	emit( conreg );\
	FreeReg( conreg );\
	Context	= CONT_Other;\
}\


//
// Compile an entity subexpression such as components
// relative and functions call. Return true, if parsing 
// was successfully.
//
// Here:
//	- InContext: an entity context, since about 90% of work in 'this' context it increase speed.
//	- Entity: type of entity script and/or family.
//	- iConReg: index of entity context register, if 'this' use 0xff.
//	- Result: gotten result type, it also possible TYPE_None in functions.
//
Bool CCompiler::CompileEntityExpr( EEntityContext InContext, CTypeInfo Entity, Byte iConReg, TExprResult& Result )
{
#if 0
	// Does we have something to start with?
	if( !Entity.Script && Entity.iFamily==-1 )
		return false;
#else
	// If we have untyped entity without event family.
	if( !Entity.Script && Entity.iFamily==-1 )
	{
		CProperty*			Property;
		CNativeFunction*	Native;

		TToken T;
		GetToken( T, false, false );

		// Skip 'base', if any.
		if( T.Text == KW_base )
		{
			RequireSymbol( L".", L"base" );
			GetToken( T, false, false );
		}

		// Decide what we got...
		if( Property = FBaseComponent::MetaClass->FindProperty(*T.Text) )
		{
			// An base property from the entity context. 
			// Kinda of symbiosis.
			Word Offset		= Property->Offset;

			Result.bLValue	= true;
			Result.iReg		= GetReg();
			Result.Type		= *Property;

			emit_context( InContext, iConReg );
			emit( CODE_BaseProperty );
			emit( Result.iReg );
			emit( Offset );

			// If property are const, force it to be const.
			if( Property->Flags & PROP_Const )
			{
				emit_ltor( Result );
				Result.bLValue	= false;
			}

			return true;
		}
		else if( Native = FBaseComponent::MetaClass->FindMethod(*T.Text) )
		{
			// Native method call from base component.
			Word iNative = CClassDatabase::GFuncs.FindItem( Native );
			TArray<Byte> ArgRegs;

			RequireSymbol( L"(", L"native method" );
			for( Integer i=0; (i<8)&&(Native->ParamsType[i].Type!=TYPE_None); i++ )
			{
				ArgRegs.Push( CompileExpr( Native->ParamsType[i], true, false, 0 ).iReg );

				if( i<7 && Native->ParamsType[i+1].Type != TYPE_None )
					RequireSymbol( L",", L"arguments" ); 
			}
			RequireSymbol( L")", L"native method" );

			emit_context( InContext, iConReg );
			emit( CODE_BaseMethod );
			emit( iNative );
			for( Integer i=0; i<ArgRegs.Num(); i++ )
			{
				emit( ArgRegs[i] );
				FreeReg( ArgRegs[i] );
			}

			if( Native->ResultType.Type != TYPE_None )
			{
				// Has result.
				Result.Type		= Native->ResultType;
				Result.iReg		= GetReg();
				Result.bLValue	= false;
				emit( Result.iReg );
			}
			else
			{
				// No result.
				Result.bLValue	= false;
				Result.iReg		= -1;
				Result.Type		= TYPE_None;
			}

			bValidExpr	= true;
			return true;
		}
		else
		{
			Error( L"Unknown field '%s' in abstract base", *T.Text );
			return false;
		}
	}
#endif

	// Grab a token to recognize and store code location
	// for rollback in case of failure.
	Word CodeStart = Emitter.Tell();
	TToken T;
	GetToken( T, false, false );
	/*
	// Switch context if required.
	if( InContext == CONT_This )
	{
		// This expression in 'this' context.
		if( Context != CONT_This )
		{
			Byte iReg = GetReg();
			emit( CODE_This );
			emit( iReg );
			emit( CODE_Context );
			emit( iReg );
			FreeReg( iReg );
		}
	}
	else
	{
		// Other context, switch it anyway.
		assert(iConReg != 0xff);
		emit( CODE_Context );
		emit( iConReg );
		FreeReg( iConReg );
	}		
	*/

	// Objects.
	FScript* ConScript = Entity.Script;
	CFamily* ConFamily = ConScript ? (ConScript->iFamily!=-1 ? Families[ConScript->iFamily] : nullptr) : (Entity.iFamily!=-1 ? Families[Entity.iFamily] : nullptr);
	CProperty* Property;
	CFunction* Function;
	CNativeFunction*	Native;
	Integer	 iUnified;

	if( ConScript && (Property = FindProperty( ConScript->Properties, T.Text )) )
	{
		// An entity property.
		Word Offset	= Property->Offset;

		Result.bLValue	= true;
		Result.iReg		= GetReg();
		Result.Type		= *Property;

		emit_context( InContext, iConReg );
		emit( CODE_EntityProperty );
		emit( Result.iReg );
		emit( Offset );

		return true;
	}
	else if( ConScript && (Property = ConScript->Base->GetClass()->FindProperty(*T.Text)) )
	{
		// An base property from the entity context. 
		// Kinda of symbiosis.
		Word Offset		= Property->Offset;

		Result.bLValue	= true;
		Result.iReg		= GetReg();
		Result.Type		= *Property;

		emit_context( InContext, iConReg );
		emit( CODE_BaseProperty );
		emit( Result.iReg );
		emit( Offset );

		// If property are const, force it to be const.
		if( Property->Flags & PROP_Const )
		{
			emit_ltor( Result );
			Result.bLValue	= false;
		}

		return true;
	}
	else if( ConScript && (Function = FindFunction(ConScript, T.Text)) )
	{
		// An script function, doesn't matter it regular or unified cause
		// we know actual script!
		Byte			iFunc = ConScript->Functions.FindItem(Function);
		TArray<Byte>	ArgRegs;

		RequireSymbol( L"(", L"method call" );
		for( Integer i=0; i<Function->ParmsCount; i++ )
		{
			CProperty* Param = Function->Locals[i];

			if( Param->Flags & PROP_OutParm )
			{
				// Output property.
				TExprResult Out = CompileExpr( *Param, false, false, 0 );
				if( !Out.bLValue )
					Error( L"The right-hand side of an assignment must be a variable" );
				ArgRegs.Push( Out.iReg );
			}
			else
			{
				// Regular property.
				ArgRegs.Push( CompileExpr( *Param, true, false, 0 ).iReg );
			}

			if( i != Function->ParmsCount-1 )
				RequireSymbol( L",", L"method arguments" );
		}
		RequireSymbol( L")", L"method call" );

		emit_context( InContext, iConReg );
		emit( CODE_CallFunction );
		emit( iFunc );

		for( Integer i=0; i<ArgRegs.Num(); i++ )
		{
			emit( ArgRegs[i] );
			FreeReg( ArgRegs[i] );
		}

		if( Function->ResultVar )
		{
			// Has result.
			Result.Type		= *Function->ResultVar;
			Result.iReg		= GetReg();
			Result.bLValue	= false;
			emit( Result.iReg );
		}
		else
		{
			// No result.
			Result.bLValue	= false;
			Result.iReg		= -1;
			Result.Type		= TYPE_None;
		}

		bValidExpr	= true;
		return true;
	}
	else if( ConScript && (Native = ConScript->Base->GetClass()->FindMethod(*T.Text)) )
	{
		// Native method call from base component.
		Word iNative = CClassDatabase::GFuncs.FindItem( Native );
		TArray<Byte> ArgRegs;

		RequireSymbol( L"(", L"native method" );
		for( Integer i=0; (i<8)&&(Native->ParamsType[i].Type!=TYPE_None); i++ )
		{
			ArgRegs.Push( CompileExpr( Native->ParamsType[i], true, false, 0 ).iReg );

			if( i<7 && Native->ParamsType[i+1].Type != TYPE_None )
				RequireSymbol( L",", L"arguments" ); 
		}
		RequireSymbol( L")", L"native method" );

		emit_context( InContext, iConReg );
		emit( CODE_BaseMethod );
		emit( iNative );
		for( Integer i=0; i<ArgRegs.Num(); i++ )
		{
			emit( ArgRegs[i] );
			FreeReg( ArgRegs[i] );
		}

		if( Native->ResultType.Type != TYPE_None )
		{
			// Has result.
			Result.Type		= Native->ResultType;
			Result.iReg		= GetReg();
			Result.bLValue	= false;
			emit( Result.iReg );
		}
		else
		{
			// No result.
			Result.bLValue	= false;
			Result.iReg		= -1;
			Result.Type		= TYPE_None;
		}

		bValidExpr	= true;
		return true;
	}
	else if( ConFamily && (iUnified = ConFamily->VFNames.FindItem(T.Text)) != -1 )
	{
		// VF function call.
		CFunction*		Proto = ConFamily->Proto[iUnified];
		Byte			Index = iUnified;
		TArray<Byte>	ArgRegs;

		RequireSymbol( L"(", L"method call" );
		for( Integer i=0; i<Proto->ParmsCount; i++ )
		{
			ArgRegs.Push( CompileExpr( *Proto->Locals[i], true, false, 0 ).iReg );

			if( i != Proto->ParmsCount-1 )
				RequireSymbol( L",", L"method arguments" );
		}
		RequireSymbol( L")", L"method call" );

		emit_context( InContext, iConReg );
		emit( CODE_CallVF );
		emit( Index );

		for( Integer i=0; i<ArgRegs.Num(); i++ )
		{
			emit( ArgRegs[i] );
			FreeReg( ArgRegs[i] );
		}

		if( Proto->ResultVar )
		{
			// Has result.
			Result.Type		= *Proto->ResultVar;
			Result.iReg		= GetReg();
			Result.bLValue	= false;
			emit( Result.iReg );
		}
		else
		{
			// No result.
			Result.bLValue	= false;
			Result.iReg		= -1;
			Result.Type		= TYPE_None;
		}

		bValidExpr	= true;
		return true;
	}
	else if( ConScript && (T.Text==KW_base || T.Text==L"$") )
	{
		// Component property or method call.
		FComponent* Component;
		Byte iCompon = 0xff;
		if( T.Text == L"$" )
		{
			// Extra component.
			String Name = GetIdentifier( L"component name" );
			Component	= ConScript->FindComponent( Name );

			if( !Component )
				Error( L"Component '%s' not found in '%s'", *Name, *ConScript->GetName() );

			iCompon		= ConScript->Components.FindItem( (FExtraComponent*)Component );
		}
		else
		{
			// Base component, deprecated access.
			Component	= ConScript->Base;
			iCompon		= 0xff;
		}

		RequireSymbol( L".", L"component field" );
		CClass*	Class	= Component->GetClass();
		String Field	= GetIdentifier( L"component field" );

		if( Property = Class->FindProperty(*Field) )
		{
			// Component property.
			Word Offset		= Property->Offset;

			Result.bLValue	= true;
			Result.iReg		= GetReg();
			Result.Type		= *Property;

			emit_context( InContext, iConReg );
			if( Class->IsA(FBaseComponent::MetaClass) )
			{
				emit( CODE_BaseProperty );
			}
			else
			{
				emit( CODE_ComponentProperty );
				emit( iCompon );
			}
			emit( Result.iReg );
			emit( Offset );

			// If property are const, force it to be const.
			if( Property->Flags & PROP_Const )
			{
				emit_ltor( Result );
				Result.bLValue	= false;
			}
		}
		else if( Native = Class->FindMethod(*Field) )
		{
			// Native function call.
			Word iNative = CClassDatabase::GFuncs.FindItem( Native );
			TArray<Byte> ArgRegs;

			RequireSymbol( L"(", L"native method" );
			for( Integer i=0; (i<8)&&(Native->ParamsType[i].Type!=TYPE_None); i++ )
			{
				ArgRegs.Push( CompileExpr( Native->ParamsType[i], true, false, 0 ).iReg );

				if( i<7 && Native->ParamsType[i+1].Type != TYPE_None )
					RequireSymbol( L",", L"arguments" ); 
			}
			RequireSymbol( L")", L"native method" );

			emit_context( InContext, iConReg );
			if( Class->IsA(FBaseComponent::MetaClass) )
			{
				emit( CODE_BaseMethod );
				emit( iNative );
			}
			else
			{
				emit( CODE_ComponentMethod );
				emit( iNative );
				emit( iCompon );
			}

			for( Integer i=0; i<ArgRegs.Num(); i++ )
			{
				emit( ArgRegs[i] );
				FreeReg( ArgRegs[i] );
			}

			if( Native->ResultType.Type != TYPE_None )
			{
				// Has result.
				Result.Type		= Native->ResultType;
				Result.iReg		= GetReg();
				Result.bLValue	= false;
				emit( Result.iReg );
			}	
			else
			{
				// No result.
				Result.bLValue	= false;
				Result.iReg		= -1;
				Result.Type		= TYPE_None;
			}

			bValidExpr	= true;
		}
		else
			Error( L"Field '%s' not found in '%s'", *Field, *Class->Alt );

		return true;
	}

	// Failed parse entity subexpression, rollback
	// and return false.
	Emitter.Bytecode->Code.SetNum( CodeStart );
	GotoToken( T );
	return false;
}


//
// Get implicit cast code, if types are matched or no cast return
// 0x00, otherwise return code of cast.
//
Byte CCompiler::GetCast( EPropType Src, EPropType Dst )
{
	if( Src == TYPE_Byte	&& Dst == TYPE_Integer )
			return CAST_ByteToInteger;

	if( Src == TYPE_Byte	&& Dst == TYPE_Float )
			return CAST_ByteToFloat;

	if( Src == TYPE_Byte	&& Dst == TYPE_Angle )
			return CAST_ByteToAngle;

	if( Src == TYPE_Integer	&& Dst == TYPE_Float )
			return CAST_IntegerToFloat;

	if( Src == TYPE_Integer	&& Dst == TYPE_Byte )
			return CAST_IntegerToByte;

	if( Src == TYPE_Integer	&& Dst == TYPE_Angle )
			return CAST_IntegerToAngle;

	if( Src == TYPE_Angle	&& Dst == TYPE_Integer )
			return CAST_AngleToInteger;

	return 0x00;
}


/*-----------------------------------------------------------------------------
    Statements.
-----------------------------------------------------------------------------*/

//
// Compile statement between {..}, or just 
// single operator.
//
void CCompiler::CompileStatement()
{
	// Reset current context, its messy a little, but
	// may cause crash in really rare cases.
	Context	= CONT_Other;

	Bool bSingleLine = !MatchSymbol(L"{");

	do 
	{
		if( MatchSymbol(L"}") )
		{
			if( !bSingleLine )
				break;
			else
				Error( L"Unexpected '}'" );
		}

		if( PeekIdentifier() == KW_if )
		{
			CompileIf();
		}
		else if( PeekIdentifier() == KW_for )
		{
			CompileFor();
		}
		else if( PeekIdentifier() == KW_foreach )
		{
			CompileForeach();
		}
		else if( PeekIdentifier() == KW_do )
		{
			CompileDo();
		}
		else if( PeekIdentifier() == KW_while )
		{
			CompileWhile();
		}
		else if( PeekIdentifier() == KW_switch )
		{
			CompileSwitch();
		}
		else if( PeekSymbol() == L"{" )
		{
			// Sub nest.
			CompileStatement();
		}
		else if(	PeekIdentifier() == KW_break ||
					PeekIdentifier() == KW_continue ||
					PeekIdentifier() == KW_return ||
					PeekIdentifier() == KW_wait ||
					PeekIdentifier() == KW_sleep ||	
					PeekIdentifier() == KW_interrupt ||	
					PeekIdentifier() == KW_goto ||
					PeekIdentifier() == KW_stop	)
		{
			// Flow command.
			CompileCommand();
		}
		else if( MatchSymbol(L"@") )
		{
			// A thread label.
			if( Bytecode != Script->Thread /*|| NestTop > 1*/ )
				Error( L"Labels is not allowed here" );

			CThreadCode* Thread = (CThreadCode*)Bytecode;
			String Name	= GetIdentifier( L"thread label" );
			Integer iLabel;

			if( (iLabel = Thread->GetLabelId(*Name)) == -1 )
				Error( L"Label '%s' not found", *Name );

			// Set address of label.
			Thread->Labels[iLabel].Address	= Emitter.Tell();
			RequireSymbol( L":", L"label" );
		}
		else if	( 
					Bytecode != Script->Thread && 
					CompileVarDecl
								( 
									((CFunction*)Bytecode)->Locals, 
									((CFunction*)Bytecode)->FrameSize, 
									false 
								) 
				)
		{
			// Local variables compiled.
		}
		else
		{
			// Just expression.
			bValidExpr	= false;
			ResetRegs();
			CompileExpr( TYPE_None, false, true, 0 );

			if( bValidExpr == false )
				Error( L"Bad expression" );

			RequireSymbol( L";", L"statement" );
		}
	} while( !bSingleLine );
}


//
// Compile 'if' statement.
//
void CCompiler::CompileIf()
{
	assert(MatchIdentifier(KW_if));
	PushNest(NEST_If);

	Word DstElse;

	// Logical condition.
	RequireSymbol( L"(", L"if" );
	{
		ResetRegs();
		Byte iReg = CompileExpr( TYPE_Bool, true, false, 0 ).iReg;

		// Emit header.
		emit( CODE_JumpZero );
		DstElse	= Emitter.Tell();
		emit( GTempWord );
		emit( iReg );	
	}
	RequireSymbol( L")", L"if" );

	// 'then' statement.
	CompileStatement();

	// Optional 'else' statement.
	if( MatchIdentifier(KW_else) )
	{
		// With 'else'.
		emit( CODE_Jump );
		Word DstEnd	= Emitter.Tell();
		emit( GTempWord );

		*(Word*)&Bytecode->Code[DstElse]	= Emitter.Tell();
		CompileStatement();
		*(Word*)&Bytecode->Code[DstEnd]		= Emitter.Tell();
	}
	else
	{
		// Without 'else'.
		*(Word*)&Bytecode->Code[DstElse]	= Emitter.Tell();
	}

	PopNest();
}


//
// Compile 'while' statement.
//
void CCompiler::CompileWhile()
{
	assert(MatchIdentifier(KW_while));
	PushNest(NEST_While);

	Word AddrStart, DstEnd;

	// Logical condition.
	RequireSymbol( L"(", L"while" );
	{
		AddrStart	= Emitter.Tell();

		ResetRegs();
		Byte iReg = CompileExpr( TYPE_Bool, true, false, 0 ).iReg;

		// Emit header.
		emit( CODE_JumpZero );
		DstEnd	= Emitter.Tell();
		emit( GTempWord );
		emit( iReg );	
	}
	RequireSymbol( L")", L"while" );

	// statement.
	CompileStatement();

	// Post statement stuff.
	emit( CODE_Jump );
	emit( AddrStart );
	*(Word*)&Bytecode->Code[DstEnd]	= Emitter.Tell();

	// Fix & pop nest.
	ReplNest( REPL_Break, Emitter.Tell() );
	ReplNest( REPL_Continue, AddrStart );
	PopNest();
}


//
// Compile 'do' statement.
//
void CCompiler::CompileDo()
{
	assert(MatchIdentifier(KW_do));
	PushNest(NEST_Do);

	Word AddrStart, AddrCond, DstEnd;
	AddrStart	= Emitter.Tell();

	// Statement.
	CompileStatement();

	RequireIdentifier( KW_while, L"do..while" );
	RequireSymbol( L"(", L"do loop" );
	{
		AddrCond	= Emitter.Tell();

		ResetRegs();
		Byte iReg = CompileExpr( TYPE_Bool, true, false, 0 ).iReg;

		// Emit footer.
		emit( CODE_JumpZero );
		DstEnd	= Emitter.Tell();
		emit( GTempWord );
		emit( iReg );	
	}
	RequireSymbol( L")", L"do loop" );

	// Post statement stuff.
	emit( CODE_Jump );
	emit( AddrStart );
	*(Word*)&Bytecode->Code[DstEnd]	= Emitter.Tell();

	// Fix & pop nest.
	ReplNest( REPL_Break, Emitter.Tell() );
	ReplNest( REPL_Continue, AddrCond );
	PopNest();
}


//
// Compile 'for' statement.
//
void CCompiler::CompileFor()
{
	assert(MatchIdentifier(KW_for));
	PushNest(NEST_For);

	Word AddrBegin, AddrContinue, DstEnd = 0xffff;

	// Init expression.
	RequireSymbol( L"(", L"for statement" );
	if( !MatchSymbol( L";" ) )
	{
		ResetRegs();
		CompileExpr( TYPE_None, false, true, 0 );
		RequireSymbol( L";", L"for statement" );
	}

	// Cond expression.
	AddrBegin	= Emitter.Tell();
	if( !MatchSymbol(L";") )
	{
		ResetRegs();
		Byte iReg = CompileExpr( TYPE_Bool, true, false, 0 ).iReg;

		// Emit header.
		emit( CODE_JumpZero );
		DstEnd	= Emitter.Tell();
		emit( GTempWord );
		emit( iReg );

		RequireSymbol( L";", L"for statement" );
	}

	// Skip loop expression, for now.
	TToken LoopExprTok;
	GetToken( LoopExprTok, false, false );
	GotoToken( LoopExprTok );
	{
		Integer NumBrk = 1;
		TToken T;
		while( NumBrk > 0 )
		{
			GetToken( T, false, false );
			if( T.Text == L"(" )
				NumBrk++;
			else if( T.Text == L")" )
				NumBrk--;
		}
	}

	// Statement.
	CompileStatement();

	// Post statement.
	TToken EndToken;
	GetToken( EndToken, false, false );
	AddrContinue	= Emitter.Tell();
	GotoToken( LoopExprTok );
	if( !MatchSymbol(L")") )
	{
		ResetRegs();
		CompileExpr( TYPE_None, false, true, 0 );
		RequireSymbol( L")", L"for statement" );
	}
	GotoToken( EndToken );

	emit( CODE_Jump );
	emit( AddrBegin );

	if( DstEnd != 0xffff )
		*(Word*)&Bytecode->Code[DstEnd]	= Emitter.Tell();

	// Fix break and pop nest.
	ReplNest( REPL_Continue, AddrContinue );
	ReplNest( REPL_Break, Emitter.Tell() );
	PopNest();
}


//
// Compile 'switch' statement.
//
void CCompiler::CompileSwitch()
{
	assert(MatchIdentifier(KW_switch));
	PushNest(NEST_Switch);

	TExprResult Expr;
	Word DstJmpTab, DstDefaultAddr, DstOut;
	TArray<Integer>	Labels;
	TArray<Word>	Addrs;
	Byte			Size;
	Bool			bDefaultFound = false;

	// Switch expression.
	RequireSymbol( L"(", L"switch" );
	{
		ResetRegs();
		Expr = CompileExpr( TYPE_None, true, false, 0 );
		Size = Expr.Type.TypeSize(true);

		if	( 
				Expr.Type.ArrayDim != 1 || 
				!( Expr.Type.Type==TYPE_Byte || Expr.Type.Type==TYPE_Integer ) 
			)
				Error( L"Integral expression type in 'switch'" );

		// Emit header.
		emit( CODE_Switch );
		emit( Size );
		emit( Expr.iReg );
		DstDefaultAddr	= Emitter.Tell();
		emit( GTempWord );
		DstJmpTab		= Emitter.Tell();
		emit( GTempWord );

		FreeReg(Expr.iReg);
	}
	RequireSymbol( L")", L"switch" );

	// Process switch body.
	RequireSymbol( L"{", L"switch" );
	for( ; ; )
	{
		if( MatchSymbol( L"}" ) )
		{
			// End of switch.
			break;
		}
		else if( MatchIdentifier( KW_default ) )
		{
			// Default label.
			if( bDefaultFound )
				Error( L"Default label already appeared in this switch" );

			bDefaultFound	= true;
			*(Word*)&Bytecode->Code[DstDefaultAddr]	= Emitter.Tell();
			RequireSymbol( L":", L"default" );
		}
		else if( MatchIdentifier( KW_case ) )
		{
			// Add a new label.
			if( bDefaultFound )
				Error( L"Case label follows after default label" );

			TToken T;
			Integer Value;
			GetToken( T, true, false );

			if( T.Type == TOK_Const )
			{
				// Literal value.
				if( T.TypeInfo.Type == TYPE_Integer )
					Value = T.cInteger;
				else if( T.TypeInfo.Type == TYPE_Byte )
					Value = T.cByte;
				else
					Error( L"Integral expression required in 'switch'" );		
			}
			else if( T.Type == TOK_Identifier )
			{
				// Enumeration name.
				Value	= FindEnumElement(T.Text);
				if( Value == 0xff )
					Error( L"Enumeration element '%s' not found", *T.Text );
			}
			else 
				Error( L"Missing label expression" );

			if( Labels.FindItem(Value) != -1 )
				Error( L"Case label value already appeared in this switch" );

			// Add to list.
			Labels.Push( Value );
			Addrs.Push( Emitter.Tell() );

			RequireSymbol( L":", L"case label" );
		}
		else
		{
			// CompileStatement handle it properly.
			CompileStatement();
		}
	}

	// Skip table.
	emit( CODE_Jump );
	DstOut	= Emitter.Tell();
	emit( GTempWord );

	// Emit jump table.
	*(Word*)&Bytecode->Code[DstJmpTab]	= Emitter.Tell();
	assert(Labels.Num() == Addrs.Num());
	Byte NumLabs = Labels.Num();
	emit( NumLabs );
	for( Integer i=0; i<Labels.Num(); i++ )
	{
		if( Expr.Type.Type == TYPE_Byte )
			emit( *(Byte*)&Labels[i] )
		else
			emit( *(Integer*)&Labels[i] );
	
		emit( Addrs[i] );
	}

	// Set out address.
	*(Word*)&Bytecode->Code[DstOut]	= Emitter.Tell();

	if( !bDefaultFound )
		*(Word*)&Bytecode->Code[DstDefaultAddr]	= Emitter.Tell();

	// Fix & pop nest.
	ReplNest( REPL_Break, Emitter.Tell() );
	PopNest();
}


//
// Compile command. Return, Break, Continue.
// Note: Bru-bru hate it!   :3
// Also it invoke thread's flow control.
//
void CCompiler::CompileCommand()
{
	if( MatchIdentifier( KW_return ) )
	{
		// Return from fn.
		if( Bytecode == Script->Thread )
			Error( L"'return' is no allowed here" );

		if( !MatchSymbol(L";") )
		{
			// Return with result.
			CFunction* Func = (CFunction*)Bytecode;
			if( !Func->ResultVar )
				Error( L"Return value allow only in functions" );

			ResetRegs();
			TExprResult Left;
			TExprResult Right = CompileExpr( *Func->ResultVar, true, false, 0 );

			Left.bLValue	= true;
			Left.Type		= *Func->ResultVar;
			Left.iReg		= GetReg();
			Word Offset		= Func->ResultVar->Offset;

			emit( CODE_LocalVar );
			emit( Left.iReg );
			emit( Offset );

			emit_assign( Left, Right );

			RequireSymbol( L";", L"return" );
		}

		emit( CODE_Jump );
		Nest[0].Addrs[REPL_Return].Push( Emitter.Tell() );
		emit( GTempWord );
	}
	else if( MatchIdentifier( KW_break ) )
	{
		// Break, doesn't handle 'switch' statement.
		Integer LoopNest = NestTop - 1;
		while(	( LoopNest > 0 )&&
				( Nest[LoopNest].Type != NEST_Do )&&
				( Nest[LoopNest].Type != NEST_For )&&
				( Nest[LoopNest].Type != NEST_Foreach )&&
				( Nest[LoopNest].Type != NEST_Switch )&&
				( Nest[LoopNest].Type != NEST_While ) )
					LoopNest--;

		if( LoopNest == 0 )
			Error( L"No enclosing loop out of which to break or continue" );

		emit( CODE_Jump );
		Nest[LoopNest].Addrs[REPL_Break].Push( Emitter.Tell() );
		emit( GTempWord );
		RequireSymbol( L";", L"break or continue" );
	}
	else if( MatchIdentifier( KW_continue ) )
	{
		// Continue.
		Integer LoopNest = NestTop - 1;
		while(	( LoopNest > 0 )&&
				( Nest[LoopNest].Type != NEST_Do )&&
				( Nest[LoopNest].Type != NEST_For )&&
				( Nest[LoopNest].Type != NEST_Foreach )&&
				( Nest[LoopNest].Type != NEST_While ) )
					LoopNest--;

		if( LoopNest == 0 )
			Error( L"No enclosing loop out of which to break or continue" );

		emit( CODE_Jump );
		Nest[LoopNest].Addrs[REPL_Continue].Push( Emitter.Tell() );
		emit( GTempWord );
		RequireSymbol( L";", L"break or continue" );
	}
	else if( MatchIdentifier( KW_stop ) )
	{
		// Stop thread execution.
		if( Bytecode != Script->Thread )
			Error( L"'stop' is not allowed here" );

		emit( CODE_Stop );
		RequireSymbol( L";", L"stop" );
	}
	else if( MatchIdentifier( KW_interrupt ) )
	{
		// Interrupt thread execution.
		if( Bytecode != Script->Thread )
			Error( L"'interrupt' is not allowed here" );

		emit( CODE_Interrupt );
		RequireSymbol( L";", L"interrupt" );
	}
	else if( MatchIdentifier( KW_wait ) )
	{
		// Make thread wait until expression become true.
		if( Bytecode != Script->Thread )
			Error( L"'wait' is not allowed here" );

		ResetRegs();
		Word WaitExpr = Emitter.Tell();		
		Byte iReg = CompileExpr( TYPE_Bool, true, false, 0 ).iReg;

		emit( CODE_Wait );
		emit( WaitExpr );
		emit( iReg );

		RequireSymbol( L";", L"wait" );
	}
	else if( MatchIdentifier( KW_sleep ) )
	{
		// Sleep the thread.
		// Zzzzzz....
		if( Bytecode != Script->Thread )
			Error( L"'sleep' is not allowed here" );

		ResetRegs();
		Byte iReg = CompileExpr( TYPE_Float, true, false, 0 ).iReg;
		emit( CODE_Sleep );
		emit( iReg );

		RequireSymbol( L";", L"sleep" );
	}
	else if( MatchIdentifier( KW_goto ) )
	{
		// Goto an label in the thread.
		if( !Script->Thread )
			Error( L"Script has no thread" );

		ResetRegs();
		Byte iReg = CompileExpr( TYPE_Integer, true, false, 0 ).iReg;
		emit( CODE_Goto );
		emit( iReg );

		RequireSymbol( L";", L"sleep" );
	}
	else
		assert(false);
}


//
// Compile 'foreach' statement.
//
void CCompiler::CompileForeach()
{
	assert(MatchIdentifier(KW_foreach));

	// Doesn't allowed nested foreach loops.
	for( Integer i=0; i<NestTop; i++ )
		if( Nest[i].Type == NEST_Foreach )
			Error( L"Nested 'foreach' is not allowed" );

	// Don't use foreach in threads. To predic some really
	// uncatchy gpf errors.
	if( Bytecode == Script->Thread )
		Error( L"'foreach' loop is not allowed in thread" );

	PushNest(NEST_Foreach);
	
	Word AddrStart, DstEnd;
	Word PropAddr;

	// Foreach header.
	RequireSymbol( L"(", L"foreach" );
	{
		// Get loop control variable.
		String PropName = GetIdentifier(L"'foreach' loop control");
		CProperty* Control = FindProperty( ((CFunction*)Bytecode)->Locals, PropName );
		if( !Control )
			Error( L"Loop control variable '%s' is not found", *PropName );
		if( Control->ArrayDim != 1 || Control->Type != TYPE_Entity )
			Error( L"'foreach' loop control variable must be simple local variable" );
		PropAddr	= Control->Offset;

		// Separation.
		RequireSymbol( L":", L"foreach" );

		// Our iteration function.
		{
			String IterName	= GetIdentifier( L"Iteration function" );
			CNativeFunction* Iter	= FindNative(IterName);
			if( !Iter )
				Error( L"Iteration function '%s' not found", *IterName );
			if( !(Iter->Flags & NFUN_Foreach) )
				Error( L"'%s' is no an iteration function", *IterName );

			TArray<Byte>	ArgRegs;
			ResetRegs();
			RequireSymbol( L"(", L"function call" );
			for( Integer i=0; (i<8)&&(Iter->ParamsType[i].Type!=TYPE_None); i++ )
			{
				ArgRegs.Push( CompileExpr( Iter->ParamsType[i], true, false, 0 ).iReg );

				if( i<7 && Iter->ParamsType[i+1].Type != TYPE_None )
					RequireSymbol( L",", L"arguments" ); 
			}
			RequireSymbol( L")", L"function call" );	

			// Emit call.
			emit( Iter->iOpCode );
			for( Integer i=0; i<ArgRegs.Num(); i++ )
			{
				emit( ArgRegs[i] );
				FreeReg( ArgRegs[i] );
			}
			assert(Iter->ResultType.Type == TYPE_None);
		}

		// Emit foreach header.
		AddrStart		= Emitter.Tell();
		emit(CODE_Foreach);
		emit(PropAddr);
		DstEnd			= Emitter.Tell();
		emit(GTempWord);
	}
	RequireSymbol( L")", L"foreach" );

	// statement.
	CompileStatement();

	// Post statement stuff.
	emit( CODE_Jump );
	emit( AddrStart );
	*(Word*)&Bytecode->Code[DstEnd]	= Emitter.Tell();

	// Fix & pop nest.
	ReplNest( REPL_Break, Emitter.Tell() );
	ReplNest( REPL_Continue, AddrStart );
	PopNest();
}


/*-----------------------------------------------------------------------------
    Declaration compiling.
-----------------------------------------------------------------------------*/

//
// Compile a single declaration.
//
void CCompiler::CompileDeclaration()
{
	if( PeekIdentifier() == KW_enum )
	{
		// Enumeration.
		CompileEnumDecl();
	}
	else if( PeekIdentifier() == KW_const )
	{
		// Constant.
		CompileConstDecl(); 
	}
	else if( PeekIdentifier() == KW_thread )
	{
		// Thread.
		CompileThreadDecl();
	}
	else if( PeekIdentifier() == KW_fn ||
			 PeekIdentifier() == KW_event )
	{
		// Function.
		CompileFunctionDecl();
	}
	else if( CompileVarDecl( Script->Properties, Script->InstanceSize, true ) )
	{
		// Compile property, or function.
		// C style syntax, need to parse more to figure it out.
	}
	else
		Error( L"Expected declaration, got '%s'", *PeekIdentifier() );
}


//
// Compile a variable type as a CTypeInfo. 
// If it's not a type, return TYPE_None and return to the previous location.
// bNoArray - arrays a restricted here, so don't allowed in parameters.
//
CTypeInfo CCompiler::CompileVarType( Bool bNoArray )
{
	TToken TypeName;
	CClass* ResClass;
	CTypeInfo TypeInfo;
	GetToken( TypeName, false, false );

	// Figure out property type.
	if( TypeName.Text == KW_byte )
	{
		// Simple byte.
		TypeInfo.Type	= TYPE_Byte;
		TypeInfo.Enum	= nullptr;
	}
	else if( TypeName.Text == KW_bool )
	{
		// Bool.
		TypeInfo.Type	= TYPE_Bool;
	}
	else if( TypeName.Text == KW_integer )
	{
		// Integer variable.
		TypeInfo.Type	= TYPE_Integer;
	}
	else if( TypeName.Text == KW_float )
	{
		// Float.
		TypeInfo.Type	= TYPE_Float;
	}
	else if( TypeName.Text == KW_angle )
	{
		// Angle.
		TypeInfo.Type	= TYPE_Angle;
	}
	else if( TypeName.Text == KW_color )
	{
		// Color.
		TypeInfo.Type	= TYPE_Color;
	}
	else if( TypeName.Text == KW_string )
	{
		// String.
		TypeInfo.Type	= TYPE_String;
	}
	else if( TypeName.Text == KW_vector )
	{
		// Vector.
		TypeInfo.Type	= TYPE_Vector;
	}
	else if( TypeName.Text == KW_aabb )
	{
		// AABB.
		TypeInfo.Type	= TYPE_AABB;
	}
	else if( TypeName.Text == KW_entity )
	{
		// Void entity.
		TypeInfo.Type		= TYPE_Entity;
		TypeInfo.iFamily	= -1;
		TypeInfo.Script		= nullptr;
	}
	else if( TypeInfo.Enum = FindEnum( Script, TypeName.Text, false ) )
	{
		// Enumeration byte.
		TypeInfo.Type	= TYPE_Byte;
	}
	else if( ResClass = CClassDatabase::StaticFindClass( *(String(L"F")+TypeName.Text )) )
	{
		// Resource.
		if( !ResClass->IsA(FResource::MetaClass) )
			Error( L"'%s' is not resource derived class", *ResClass->Alt );

		TypeInfo.Type	= TYPE_Resource;
		TypeInfo.Class	= ResClass;
	}
	else if( TypeInfo.Script = FindScript( TypeName.Text ) )
	{
		// Typed entity.
		TypeInfo.Type		= TYPE_Entity;
		TypeInfo.iFamily	= TypeInfo.Script->iFamily;
	}
	else if( (TypeInfo.iFamily = FindFamily(TypeName.Text) ? FindFamily(TypeName.Text)->iFamily : -1) != -1 )
	{
		// Family entity.
		TypeInfo.Type	= TYPE_Entity;
		TypeInfo.Script	= nullptr;
	}
	else
	{
		// Not a type, give up.
		TypeInfo.Type	= TYPE_None;
		GotoToken( TypeName );
		return TypeInfo;
	}

	// Array size.
	if( MatchSymbol(L"[") )
	{
		if( bNoArray )
			Error( L"Array variables is not allowed here" );

		TypeInfo.ArrayDim = ReadInteger( L"array size" );
		if( TypeInfo.ArrayDim <=0 || TypeInfo.ArrayDim > STATIC_ARR_MAX )
			Error( L"Bad array size" );

		RequireSymbol( L"]", L"array" );
	}
	else
		TypeInfo.ArrayDim = 1;

	return TypeInfo;
}


//
// Compile an enumeration declaration.
//
void CCompiler::CompileEnumDecl()
{
	assert(MatchIdentifier(KW_enum));

	// Read and test enumeration name.
	String Name		= GetIdentifier( L"enumeration name" );
	if( FindEnum( Script, Name, false ) )
		Error( L"Enumeration '%s' already exists", *Name );

	RequireSymbol( L"{", L"enumeration" );

	// Allocate enumeration.
	CEnum*	Enum	= new CEnum( *Name, Script );
	Script->Enums.Push(Enum);

	// Parse elements.
	do 
	{
		String Elem	= GetIdentifier( L"enumeration" );
		//log( L"%s::%s", *Enum->Name, *Elem );

		if( Enum->Elements.FindItem(Elem) != -1 )
			Error( L"Enumeration element '%s' redeclarated", *Elem );

		if( Enum->AddElement(Elem) > 255 )
			Error( L"Too many enumeration elements" );
		
	} while( MatchSymbol(L",") );

	RequireSymbol( L"}", L"enumeration" );
}


//
// Compile a constant declaration.
//
void CCompiler::CompileConstDecl()
{
	assert(MatchIdentifier(KW_const));

	// Read and test constant name.
	String Name		= GetIdentifier( L"constant name" );
	if( FindConstant( Name ) )
		Error( L"Constant '%s' already exists", *Name );

	RequireSymbol( L"=", L"constant" );

	// Get constant value.
	TToken Const;
	GetToken( Const, true, true );

	if( Const.Type == TOK_Identifier )
	{
		// Reference other?
		TToken* Other = FindConstant( Const.Text );

		if( Other )
			Const = *Other;
		else
			Error( L"Constant '%s' not found", *Const.Text );
	}
	else if( Const.Type != TOK_Const )
		Error( L"Missing constant value" );

	// Complete constant and store.
	Const.Text	= Name;
	Constants.Push(Const);		

	// Close the line.
	RequireSymbol( L";", L"constant" );
}


//
// Compile a property declaration. 
// bDetectFunc - used in global scope, in case possible function declaration.
//
Bool CCompiler::CompileVarDecl( TArray<CProperty*>& Vars, DWord& VarsSize, Bool bDetectFunc )
{
	// Store source location, maybe its a function.
	TToken SourceLoc;
	GetToken( SourceLoc );
	GotoToken( SourceLoc );

	CTypeInfo TypeInfo = CompileVarType( false );
	if( TypeInfo.Type == TYPE_None )
		return false;

	// Parse variables.
	do 
	{
		String PropName	= GetIdentifier( L"property name" );

		// Check maybe it a part of function declaration.
		if( bDetectFunc && MatchSymbol( L"(" ) )
		{
			// YES! Its a function.
			GotoToken( SourceLoc );
			CompileFunctionDecl();
			return true;
		}
	
		// Check new name.
		if( FindProperty( Vars, PropName ) )
			Error( L"Property '%s' redeclarated", *PropName );

		// Allocate new property.
		CProperty* Property = new CProperty
										(
											TypeInfo,
											PropName,
											Access == ACC_Public ? PROP_Editable : PROP_None,
											VarsSize
										);

		// Add property to the list.
		Vars.Push( Property );
		VarsSize = align( VarsSize + TypeInfo.TypeSize(), SCRIPT_PROP_ALIGN );

		// Not really good place for it, but its works well.
		// Compile variable initialization, but for locals only.
		if( Bytecode && (&(((CFunction*)Bytecode)->Locals) == &Vars) )
		{
			if( MatchSymbol( L"=" ) )
			{
				// Compile expr.
				if( Property->ArrayDim != 1 )
					Error( L"An array initialization doesn't allowed" );

				ResetRegs();

				TExprResult Left;
				Left.bLValue		= true;
				Left.iReg			= GetReg();
				Left.Type			= *Property;

				TExprResult Right	= CompileExpr( *Property, true, false, 0 );

				// Emit local variable.
				Word Offset = Property->Offset;
				emit( CODE_LocalVar );
				emit( Left.iReg );
				emit( Offset );

				// Emit assignment.
				emit_assign( Left, Right );

				// Finish.
				FreeReg( Left.iReg );
				FreeReg( Right.iReg );
				bValidExpr	= true;
			}
		}

	} while( MatchSymbol(L",") );

	// End of declaration.
	RequireSymbol( L";", L"property" );
	return true;
}


//
// Compile thread declaration.
//
void CCompiler::CompileThreadDecl()
{
	assert(MatchIdentifier(KW_thread));

	// Check, maybe script already has thread.
	if( Script->Thread )
		Error( L"Thread already declared in '%s'", *Script->GetName() );

	// Allocate thread.
	CThreadCode* Thread = new CThreadCode();
	Script->Thread = Thread;

	// Store thread location.
	TToken T;

	if( PeekSymbol() == L"{" )
	{
		GetToken( T );
		Thread->iLine	= T.iLine;
		Thread->iPos	= T.iPos;
	}
	else
		Error( L"Thread body not defined" );

	// Skip body.
	Integer Level = 1;
	do 
	{
		GetToken( T );
		if( T.Text == L"{")
		{
			// Push nest level.
			Level++;
		}
		else if( T.Text == L"}" )
		{
			// Pop nest level.
			Level--;
		}
		else if( T.Text == L"@" )
		{
			// Possible label decl.
			String Name	= GetIdentifier( L"thread label" );
			if( MatchSymbol(L":") )
			{
				// Yes! Its a declaration.
				if( Thread->GetLabelId(*Name) != -1 )
					Error( L"Label '%s' redefined", *Name );

				CThreadCode::TLabel Label;
				Label.Address	= 0xffff;
				Label.Name		= Name;
				Thread->Labels.Push( Label );
			}
		}
	} while( Level != 0 );
}


//
// Compile function declaration.
//
void CCompiler::CompileFunctionDecl()
{
	// Allocate function.
	CFunction* Function = new CFunction();
	Script->Functions.Push( Function );

	if( MatchIdentifier(KW_event) )
	{
		// Its an event.
		Function->ResultVar	= nullptr;
		Function->Flags		= FUNC_Event;

		// Add to appropriate list.
		Script->Events.Push(Function);
	}
	else if( MatchIdentifier(KW_fn) )
	{
		// Its a simple function, without result.
		Function->ResultVar	= nullptr;
		Function->Flags		= FUNC_None;
	}
	else  
	{
		// With result.
		CTypeInfo ResType = CompileVarType( true );
		if( ResType.Type == TYPE_None )
			return;

		Function->ResultVar	= new CProperty( ResType, KW_result, PROP_None, Function->FrameSize );
		Function->FrameSize = align( Function->FrameSize + ResType.TypeSize(), SCRIPT_PROP_ALIGN );
		Function->Flags		= FUNC_HasResult;
		// Warning: Result not added to list of locals, now, since parameters
		// should be before the result variable.
	}

	// Get function name.
	Function->Name = GetIdentifier( L"function name" );
	if( FindFunction( Script, Function->Name ) != Function && FindFunction( Script, Function->Name ) != nullptr )
		Error( L"Function '%s' redeclarated", *Function->Name );

	// Test event name.
	if( Function->Flags & FUNC_Event )
		if( FindStaticEvent(Function->Name) == -1 )
			Error( L"Native event '%s' is not defined", *Function->Name );			

	RequireSymbol( L"(", L"function parameters" );

	// Parse parameters.
	if( !MatchSymbol(L")") )
	{
		for( ; ; )
		{
			// Output modifier.
			DWord ParmFlags = MatchIdentifier(KW_out) ? PROP_OutParm : 0;

			// Param type.
			CTypeInfo ParamType = CompileVarType( true );
			if( ParamType.Type == TYPE_None )
				Error( L"Unknown parameter %d type", Function->Locals.Num() );

			// Param name.
			String ParamName = GetIdentifier( L"parameter name" );
			if( FindProperty( Function->Locals, ParamName ) )
				Error( L"Parameter '%s' redeclarated", *ParamName );

			// Allocate parameter.
			CProperty*	Property	= new CProperty( ParamType, ParamName, ParmFlags, Function->FrameSize );
			Function->Locals.Push( Property );
			Function->FrameSize = align( Function->FrameSize + ParamType.TypeSize(), SCRIPT_PROP_ALIGN );
			Function->ParmsCount++;

			if( MatchSymbol( L"," ) )
			{
				// Parse more.
			}
			else
			{
				// No more parameters.
				RequireSymbol( L")", L"parameters list" );
				break;
			}
		}
	}

	// Insert result property after the parameters.
	if( Function->ResultVar )
		Function->Locals.Push( Function->ResultVar );

	// Function is unified?
	if( MatchIdentifier(KW_unified) )
	{
		if( Script->iFamily == -1 )
			Error( L"Script '%s' without family", *Script->GetName() );

		CFamily* Family = Families[Script->iFamily];
		Integer iProto = Family->VFNames.FindItem(Function->Name);
		if( iProto == -1 )
		{
			// Add a new function and it signature.
			Family->VFNames.Push(Function->Name);
			Family->Proto.Push(Function);
			assert(Family->VFNames.Num() == Family->Proto.Num());
		}
		else
		{
			// Test signature.
			CFunction* Other = Family->Proto[iProto];
			assert(Other->Name==Function->Name);

			// Match it.
			if( Other->ParmsCount != Function->ParmsCount )
				Error( L"Signature '%s' parameters count mismatched", *Other->Name );

			// Results.
			if( Function->ResultVar )
			{
				if( Other->ResultVar )
				{
					if( !Function->ResultVar->MatchTypes(*Other->ResultVar) )
						Error( L"Signature '%s' result type mismatched", *Other->Name );
				}
				else
					Error( L"Signature '%s' result type missing", *Other->Name );
			}
			else if( Other->ResultVar )
				Error( L"Signature '%s' result type missing", *Other->Name );

			// Match parameters.
			for( Integer i=0; i<Function->ParmsCount; i++ )
				if( !Function->Locals[i]->MatchTypes(*Other->Locals[i]) )
					Error
						( 
							L"Signature '%s' parameter %d types mismatched '%s' and '%s'", 
							*Function->Name, 
							i+1,
							*Function->Locals[i]->TypeName(), 
							*Other->Locals[i]->TypeName() 
						);
		}
	}

	// Store function location.
	TToken T;

	if( PeekSymbol() == L"{" )
	{
		GetToken( T );
		Function->iLine	= T.iLine;
		Function->iPos	= T.iPos;
	}
	else
		Error( L"Function body not defined" );

	// Skip body.
	Integer Level = 1;
	do 
	{
		GetToken( T );
		if( T.Text == L"{")
		{
			// Push nest level.
			Level++;
		}
		else if( T.Text == L"}" )
		{
			// Pop nest level.
			Level--;
		}
	} while( Level != 0 );	
}	


/*-----------------------------------------------------------------------------
    Nest stack functions.
-----------------------------------------------------------------------------*/

//
// Push a new stack.
//
void CCompiler::PushNest( ENestType InType )
{
	// Check for overflow.
	if( NestTop >= array_length(Nest) )
		Error( L"Nest level exceed maximum" );

	// Reset the nest.
	Nest[NestTop].Type	= InType;
	Nest[NestTop].Addrs[REPL_Break].Empty();
	Nest[NestTop].Addrs[REPL_Return].Empty();
	Nest[NestTop].Addrs[REPL_Continue].Empty();

	NestTop++;
}


//
// Pop top nest from the stack.
//
void CCompiler::PopNest()
{
	NestTop--;
	assert(NestTop >= 0);

	Nest[NestTop].Addrs[REPL_Break].Empty();
	Nest[NestTop].Addrs[REPL_Return].Empty();
	Nest[NestTop].Addrs[REPL_Continue].Empty();
}


//
// Replace an addresses at the top nest level.
//
void CCompiler::ReplNest( EReplType Repl, Word DestAddr )
{
	TNest& N = Nest[NestTop-1];

	for( Integer i=0; i<N.Addrs[Repl].Num(); i++ )
		*(Word*)&Bytecode->Code[N.Addrs[Repl][i]]	= DestAddr;
}


/*-----------------------------------------------------------------------------
    Registers management.
-----------------------------------------------------------------------------*/

//
// Reset all registers, mark them as
// unused.
//
void CCompiler::ResetRegs()
{
	MemZero( Regs, sizeof(Regs) );
}


//
// Get an available register index, return
// it's index, if registers are overflow,
// it's stops compilation.
//
Byte CCompiler::GetReg()
{
	Integer iAvail = -1;
	for( Integer i=0; i<array_length(Regs); i++ )
		if( Regs[i] == false )
		{
			iAvail = i;
			break;
		}

	if( iAvail == -1 )
		Error( L"Internal compiler error. Try to reorganize your expression." );

	// Mark gotten register as 'in use'.
	Regs[iAvail] = true;
	return iAvail;
}


//
// Release a register, mark it as unused.
//
void CCompiler::FreeReg( Byte iReg )
{
	assert(iReg>=0 && iReg<array_length(Regs));
	Regs[iReg] = false;
}


/*-----------------------------------------------------------------------------
    Objects search.
-----------------------------------------------------------------------------*/

//
// Find an enumeration, if enumeration not found return null.
// bOwnOnly - is should search only in the script, or in native 
// database too.
//
CEnum* CCompiler::FindEnum( FScript* Script, String Name, Bool bOwnOnly )
{
	// Searching in script.
	for( Integer i=0; i<Script->Enums.Num(); i++ )
		if( Name == Script->Enums[i]->Name )
			return Script->Enums[i];

	// Native database.
	if( !bOwnOnly )
		for( Integer i=0; i<CClassDatabase::GEnums.Num(); i++ )
			if( Name == CClassDatabase::GEnums[i]->Name )
				return CClassDatabase::GEnums[i];

	// Not found.
	return nullptr;
}


//
// Find a script by name.
//
FScript* CCompiler::FindScript( String Name )
{
	for( Integer i=0; i<AllScripts.Num(); i++ )
		if( Name == AllScripts[i]->GetName() )
			return AllScripts[i];

	return nullptr;
}


//
// Find a family by name, if not found return nullptr.
//
CFamily* CCompiler::FindFamily( String Name )
{
	for( Integer i=0; i<Families.Num(); i++ )
		if( Name == Families[i]->Name )
			return Families[i];

	return nullptr;
}


//
// Find a named constant, if constant are not found return nullptr.
// Otherwise return pointer to the token constant.
// Warning: constant are shared object!
//
TToken* CCompiler::FindConstant( String Name )
{
	for( Integer i=0; i<Constants.Num(); i++ )
		if( Constants[i].Text == Name )
			return &Constants[i];

	return nullptr;
}


//
// Find a property in the list, if not found return null.
//
CProperty* CCompiler::FindProperty( const TArray<CProperty*>& List, String Name )
{
	for( Integer i=0; i<List.Num(); i++ )
		if( Name == List[i]->Name )
			return List[i];

	return nullptr;
}


//
// Find a function in the script.
//
CFunction* CCompiler::FindFunction( FScript* InScript, String Name )
{
	for( Integer i=0; i<InScript->Functions.Num(); i++ )
		if( Name == InScript->Functions[i]->Name )
			return InScript->Functions[i];

	return nullptr;
}


//
// Find an unary operator by name and argument type.
// U can use TYPE_None, just to figure out is it an unary operator.
// Also, it try to apply type cast.
// If no operator found, return nullptr.
//
CNativeFunction* CCompiler::FindUnaryOp( String Name, CTypeInfo ArgType )
{
	if( ArgType.Type == TYPE_None )
	{
		// Just figure it out.
		for( Integer i=0; i<CClassDatabase::GFuncs.Num(); i++ )
		{
			CNativeFunction* F = CClassDatabase::GFuncs[i];

			if( (F->Flags & NFUN_UnaryOp) && 
				(F->Name == Name) )
				return F;
		}
	}
	else
	{
		// Try to find without typecast.
		for( Integer i=0; i<CClassDatabase::GFuncs.Num(); i++ )
		{
			CNativeFunction* F = CClassDatabase::GFuncs[i];

			if( ( F->Flags & NFUN_UnaryOp ) && 
				( F->Name == Name ) && 
				( F->ParamsType[0].Type == ArgType.Type ) )
				return F;
		}

		// Try to find with cast.
		for( Integer i=0; i<CClassDatabase::GFuncs.Num(); i++ )
		{
			CNativeFunction* F = CClassDatabase::GFuncs[i];

			if( ( F->Flags & NFUN_UnaryOp ) && 
				( F->Name == Name ) && 
				( GetCast( ArgType.Type, F->ParamsType[0].Type ) != 0x00 ) &&
				( !(F->Flags & NFUN_SuffixOp) ) )
				return F;
		}
	}

	return nullptr;
}


//
// Find a binary operator by name and arguments type.
// U can use TYPE_None, just to figure out is binary operator
// are exists, also it try to apply type cast.
// If no operator found, return nullptr.
//
CNativeFunction* CCompiler::FindBinaryOp( String Name, CTypeInfo Arg1, CTypeInfo Arg2 )
{
	if( Arg1.Type == TYPE_None || Arg2.Type == TYPE_None )
	{
		// Just figure out.
		for( Integer i=0; i<CClassDatabase::GFuncs.Num(); i++ )
		{
			CNativeFunction* F = CClassDatabase::GFuncs[i];

			if( ( F->Flags & NFUN_BinaryOp ) && 
				( F->Name == Name ) )
				return F;
		}
	}
	else
	{
		// Try to find without typecast.
		for( Integer i=0; i<CClassDatabase::GFuncs.Num(); i++ )
		{
			CNativeFunction* F = CClassDatabase::GFuncs[i];

			if( ( F->Flags & NFUN_BinaryOp ) && 
				( F->Name == Name ) && 
				( F->ParamsType[0].Type == Arg1.Type ) &&
				( F->ParamsType[1].Type == Arg2.Type ) )
					return F;
		}

		// Try to find with cast.
		for( Integer i=0; i<CClassDatabase::GFuncs.Num(); i++ )
		{
			CNativeFunction* F = CClassDatabase::GFuncs[i];
			if( !(F->Flags & NFUN_BinaryOp) || ( F->Name != Name ) )
				continue;

			if( F->Flags & NFUN_AssignOp )
			{
				// Only second operand uses cast.
				if( ( F->ParamsType[0].Type == Arg1.Type ) && 
					( F->ParamsType[1].Type == Arg2.Type || GetCast( Arg2.Type, F->ParamsType[1].Type ) != 0x00 ) )
						return F;
			}
			else
			{
				// Both operands support cast.
				if( ( F->ParamsType[0].Type == Arg1.Type || GetCast( Arg1.Type, F->ParamsType[0].Type ) != 0x00 ) && 
					( F->ParamsType[1].Type == Arg2.Type || GetCast( Arg2.Type, F->ParamsType[1].Type ) != 0x00 ) )
						return F;
			}
		}
	}

	return nullptr;
}


//
// Find an enumeration element, search in own enums and
// native. If element not found return 0xff.
//
Byte CCompiler::FindEnumElement( String Elem )
{
	// Searching in script.
	for( Integer iEnum=0; iEnum<Script->Enums.Num(); iEnum++ )
	{
		CEnum* Enum = Script->Enums[iEnum];

		for( Integer i=0; i<Enum->Elements.Num(); i++ )
			if( Elem == Enum->Elements[i] )
				return i;
	}

	// Native database.
	for( Integer iEnum=0; iEnum<CClassDatabase::GEnums.Num(); iEnum++ )
	{
		CEnum* Enum = CClassDatabase::GEnums[iEnum];

		for( Integer i=0; i<Enum->Elements.Num(); i++ )
			if( Elem == Enum->Elements[i] )
				return i;
	}

	// Not found.
	return 0xff;
}


//
// Find an native function.
//
CNativeFunction* CCompiler::FindNative( String Name )
{
	for( Integer i=0; i<CClassDatabase::GFuncs.Num(); i++ )
	{
		CNativeFunction* F = CClassDatabase::GFuncs[i];

		if( ( !(F->Flags & (NFUN_BinaryOp | NFUN_UnaryOp | NFUN_Method)) ) && 
			( F->Name == Name ) )
			return F;
	}

	return nullptr;
}


//
// Get an index of script event in global table.
// If no event found, return -1.
//
Integer CCompiler::FindStaticEvent( String Name )
{
	for( Integer i=0; i<_EVENT_MAX; i++ )
		if( wcscmp( *Name, GEvents[i] ) == 0 )
			return i;
	return -1;
}


/*-----------------------------------------------------------------------------
    Lexemes functions.
-----------------------------------------------------------------------------*/

//
// Peek the next identifier and get back to
// the previous text position.
//
String CCompiler::PeekIdentifier()
{
	TToken T;
	GetToken( T );
	GotoToken( T );
	return T.Text;
}


//
// Peek the next symbol and return to
// the previous text position.
//
String CCompiler::PeekSymbol()
{
	TToken T;
	GetToken( T );
	GotoToken( T );
	return T.Text;
}


//
// Match the name with the next lexeme (should be an identifier),
// if they are matched return true
// otherwise return false and get back to the previous text position.
//
Bool CCompiler::MatchIdentifier( const Char* Name )
{
	TToken T;
	GetToken( T );

	if( T.Text == Name )
	{
		return true;
	}
	else
	{
		GotoToken( T );
		return false;
	}
}


//
// Match the name with the next lexeme (should be a symbol),
// if they are matched return true
// otherwise return false and get back to the previous text position.
//
Bool CCompiler::MatchSymbol( const Char* Name )
{
	TToken T;
	GetToken( T );

	if( T.Text == Name )
	{
		return true;
	}
	else
	{
		GotoToken( T );
		return false;
	}
}


//
// Goto to the token's location in the text.
//
void CCompiler::GotoToken( const TToken& T )
{
	TextLine	=	PrevLine	= T.iLine;
	TextPos		=	PrevPos		= T.iPos;
}


//
// Read an identifier, and return it name. If lexeme
// is not an identifier this cause error.
//
String CCompiler::GetIdentifier( const Char* Purpose )
{
	TToken T;
	GetToken( T, false, false );

	if( T.Type != TOK_Identifier )
		Error( L"Missing identifier for %s", Purpose );

	return T.Text;
}


//
// Require an identifier with the same name, if names are mismatched
// abort compilation.
//
void CCompiler::RequireIdentifier( const Char* Name, const Char* Purpose )
{
	TToken T;
	GetToken( T );

	if( T.Text != Name )
		Error( L"Expected '%s' for %s, got '%s'", Name, Purpose, *T.Text );
}


//
// Require a symbol with the same name, if names are mismatched
// abort compilation.
//
void CCompiler::RequireSymbol( const Char* Name, const Char* Purpose )
{
	TToken T;
	GetToken( T );

	if( T.Text != Name )
		Error( L"Expected '%s' for %s, got '%s'", Name, Purpose, *T.Text );
}


//
// Read a literal integer constant.
//
Integer CCompiler::ReadInteger( const Char* Purpose )
{
	TToken T;
	GetToken( T, true, true );

	if( T.Type != TOK_Const || T.TypeInfo.Type != TYPE_Integer )
		Error( L"Missing integer constant in %s", Purpose );

	return T.cInteger;
}


//
// Read a literal float constant.
//
Float CCompiler::ReadFloat( const Char* Purpose )
{
	TToken T;
	GetToken( T, true, true );

	if( T.Type != TOK_Const || T.TypeInfo.Type != TYPE_Float )
		Error( L"Missing float constant in %s", Purpose );

	return T.cFloat;
}


//
// Read a literal string constant.
//
String CCompiler::ReadString( const Char* Purpose )
{
	TToken T;
	GetToken( T, true, true );

	if( T.Type != TOK_Const || T.TypeInfo.Type != TYPE_String )
		Error( L"Missing string constant in %s", Purpose );

	return T.cString;
}


//
// Grab the next lexeme from the text.
//   T - Is a gotten token.
//   bAllowNeg - hint to parser, whether parse negative numbers if got '-'.
//   bAllowVect - hint to parser, whether parse '[' as symbol or vector?
//
void CCompiler::GetToken( TToken& T, Bool bAllowNeg, Bool bAllowVect )
{
	// Read characters to the buffer first,
	// to avoid multiple slow string reallocation.
	Char Buffer[128] = {};
	Char* Walk = Buffer;
	Char C;

	// Skip unused blank symbols before
	// lexeme.
	do 
	{
		C = GetChar();
	}while( ( C == 0x20 )||( C == 0x09 )||
			( C == 0x0d )||( C == 0x0a ) );

	// Preinitialize the token.
	MemZero( Buffer, sizeof(Buffer) );
	T.Text				= L"";
	T.Type				= TOK_None;
	T.iLine				= PrevLine;
	T.iPos				= PrevPos;
	T.TypeInfo.ArrayDim	= 1;

	if( IsDigit( C ) )
	{
		// It's a numeric constant.
		Bool bGotDot = false;
		while( IsDigit(C) || C == L'.' )
		{
			*Walk = C;
			Walk++;
			if( C == L'.' )
			{
				if( bGotDot )
					Error( L"Bad float constant" );
				bGotDot	= true;
			}
			C = GetChar();
		}
		UngetChar();

		// Setup token.
		T.Text = Buffer;
		if( bGotDot )
		{
			// Float constant.
			T.Type			= TOK_Const;
			T.TypeInfo.Type	= TYPE_Float;
			T.Text.ToFloat( T.cFloat, 0.f );	
		}
		else
		{
			// Integer constant.
			T.Type			= TOK_Const;
			T.TypeInfo.Type	= TYPE_Integer;
			T.Text.ToInteger( T.cInteger, 0 );		
		}
	}
	else if( IsLetter( C ) )
	{
		// Its an identifier or keyword.
		while( IsLetter(C) || IsDigit(C) )
		{
			*Walk = C;
			Walk++;
			C	= GetChar();
		}
		UngetChar();
		T.Text	= Buffer;

		if( T.Text == KW_null )
		{													
			// Null resource const.
			T.Type					= TOK_Const;
			T.TypeInfo.Type			= TYPE_Resource;
			T.TypeInfo.Class		= FResource::MetaClass;
			T.cResource	= nullptr;
		}
		else if( T.Text == KW_true ||
				 T.Text == KW_false )
		{
			// Bool const.
			T.Type					= TOK_Const;
			T.TypeInfo.Type			= TYPE_Bool;
			T.cBool					= T.Text == KW_true;				
		}
		else if( T.Text == KW_undefined )
		{
			// Null entity const.
			T.Type					= TOK_Const;
			T.TypeInfo.Type			= TYPE_Entity;
			T.cEntity				= nullptr;
			T.TypeInfo.Script		= nullptr;
			T.TypeInfo.iFamily		= -1;
		}
		else
		{
			// Regular identifier.
			T.Type					= TOK_Identifier;			
		}
	}
	else if( C != L'\0' )
	{
		// It's a symbol, or something starts with it.
		Buffer[0]		= C;
		
		// Figure out, maybe two symbols?
		Char D = GetChar();

		if(	(( C == '-' )&&( D == '-' )) ||
			(( C == '+' )&&( D == '+' )) ||
			(( C == ':' )&&( D == ':' )) ||
			(( C == '>' )&&( D == '>' )) ||
			(( C == '<' )&&( D == '<' )) ||
			(( C == '>' )&&( D == '=' )) ||
			(( C == '<' )&&( D == '=' )) ||
			(( C == '=' )&&( D == '=' )) ||
			(( C == '!' )&&( D == '=' )) ||
			(( C == '|' )&&( D == '|' )) ||
			(( C == '&' )&&( D == '&' )) ||
			(( C == '^' )&&( D == '^' )) ||
			(( C == '+' )&&( D == '=' )) ||
			(( C == '-' )&&( D == '=' )) ||
			(( C == '*' )&&( D == '=' )) ||
			(( C == '/' )&&( D == '=' )) ||
			(( C == '&' )&&( D == '=' )) ||
			(( C == '|' )&&( D == '=' )) ||
			(( C == '^' )&&( D == '=' )) ||
			(( C == '-' )&&( D == '>' )) 
		  )
			Buffer[1] = D; 
		else
			UngetChar();
			
		T.Text	= Buffer;

		if( T.Text == L"[" && bAllowVect )
		{
			// Vector constant.
			T.Type				= TOK_Const;
			T.TypeInfo.Type		= TYPE_Vector;
			T.cVector.X			= ReadFloat( L"vector X component" );
			RequireSymbol( L",", L"vector" );
			T.cVector.Y			= ReadFloat( L"vector Y component" );
			RequireSymbol( L"]", L"vector" );
		}
		else if( T.Text == L"-" && bAllowNeg )
		{
			// Negative value.
			TToken U;
			GetToken( U, false, false );

			if( U.Type != TOK_Const )
				Error( L"Constant expected" );

			if( U.TypeInfo.Type == TYPE_Integer )
			{
				// Negative integer.
				T.Text				= T.Text + U.Text;
				T.Type				= TOK_Const;
				T.TypeInfo.Type		= TYPE_Integer;
				T.cInteger			= -U.cInteger;						
			}
			else if( U.TypeInfo.Type == TYPE_Float )
			{
				// Negative float.
				T.Text				= T.Text + U.Text;
				T.Type				= TOK_Const;
				T.TypeInfo.Type		= TYPE_Float;
				T.cFloat			= -U.cFloat;			
			}
			else
				Error( L"Numeric constant excepted" );
		}
		else if( T.Text == L"#" )
		{
			// Resource reference.
			String ResName	= GetIdentifier( L"resource reference" );
			FResource* Res	= (FResource*)GObjectDatabase->FindObject( ResName );

			if( !Res )
				Warn( L"Resource '%s' not found. Reference will set null", *ResName );

			T.Type					= TOK_Const;
			T.TypeInfo.Type			= TYPE_Resource;
			T.TypeInfo.Class		= Res ? Res->GetClass() : FResource::MetaClass;
			T.cResource				= Res;										
		}
		else if( T.Text == L"\"" )
		{
			// Literal string constant.
			MemZero( Buffer, sizeof(Buffer) );
			Walk = Buffer;

			do 
			{
				C = _GetChar();		// Don't skip comment things in literal text.
				if( C == L'"' || C == L'\0' )
					break;
				*Walk	= C;
				Walk++;
			} while( true );

			// String constant.
			T.Text					= String::Format( L"\"%s\"", Buffer );
			T.cString				= Buffer;
			T.Type					= TOK_Const;
			T.TypeInfo.Type			= TYPE_String;				
		}
		else
		{
			// Regular symbol.
			T.Type			= TOK_Symbol;			
		}
	}
	else
	{	
		// Its a bad symbol, maybe end of code.
		Error( L"Expected lexeme, got end of script" );
	}
}


/*-----------------------------------------------------------------------------
    Characters functions.
-----------------------------------------------------------------------------*/

//
// Read and processed a character from the text,
// this function are skip any kinds of comments.
//
Char CCompiler::GetChar()
{
	Integer PL, PP;
	Char Result;

Loop:
	Result	= _GetChar();
	PL		= PrevLine;
	PP		= PrevPos;

	if( Result == L'/' )
	{
		// Probably it's comment.
		Char C	= _GetChar();
		UngetChar();

		if( C == L'/' )
		{
			// Single line comment.
			TextLine++;
			TextPos	= 0;
			goto Loop;
		}
		else if( C == L'*' )
		{
			// Multiline comment.
			while( true )
			{
				C = _GetChar();
				if( C == L'*' )
				{
					// Maybe end of comment detected?
					Char D	= _GetChar();
					UngetChar();
					if( D == L'/' )
					{
						_GetChar();
						goto Loop;
					}
				}
				else if( C == L'\0' )
				{
					// Error, comment are never ended.
					return C;
				}
			}
		}
		else
		{
			// Restore original location.
			PrevLine	= PL;
			PrevPos		= PP;
		}
	}

	return Result;
}


//
// Return to the previous position in the text.
// Only one level of undo are support, but it's enough.
//
void CCompiler::UngetChar()
{
	TextLine	= PrevLine;
	TextPos		= PrevPos;
}


//
// Read a character from the text, do not use it, use
// GetChar instead, since this routine doesn't skip
// any comments.
//
Char CCompiler::_GetChar()
{
	// Store old location.
	PrevLine	= TextLine;
	PrevPos		= TextPos;

	// If end of text - return \0 symbol.
	if( TextLine >= Script->Text.Num() )
		return L'\0';

	// If end of line, goto next line and return
	// separator whitespace.
	if( TextPos >= Script->Text[TextLine].Len() )
	{
		TextPos		= 0;
		TextLine++;
		return L' ';
	}

	// Regular character.
	return Script->Text[TextLine](TextPos++);
}


/*-----------------------------------------------------------------------------
    Compiler errors & warnings.
-----------------------------------------------------------------------------*/

//
// Abort compilation, because some fatal error
// happened.
//
void CCompiler::Error( const Char* Fmt, ... )
{
	Char Msg[1024] = {};
	va_list ArgPtr;
	va_start( ArgPtr, Fmt );
	_vsnwprintf( Msg, 1024, Fmt, ArgPtr );
	va_end( ArgPtr );

	// Init fatal error info.
	FatalError.ErrorLine	= TextLine + 1;
	FatalError.ErrorPos		= TextPos + 1;
	FatalError.Script		= Script;
	FatalError.Message		= String::Format
										( 
											L"[Error] %s(%d): %s", 
											Script ? *Script->GetName() : L"", 
											TextLine+1, 
											Msg 
										);

	// Abort it!
	throw nullptr;
}


//
// Add a new compiler warning.
//
void CCompiler::Warn( const Char* Fmt, ... )
{
	Char Msg[1024] = {};
	va_list ArgPtr;
	va_start( ArgPtr, Fmt );
	_vsnwprintf( Msg, 1024, Fmt, ArgPtr );
	va_end( ArgPtr );

	Warnings.Push( String::Format
							( 
								L"[Warning] %s(%d): %s", 
								*Script->GetName(), 
								TextLine+1, 
								Msg ) 
							);
}


/*-----------------------------------------------------------------------------
    Script storage.
-----------------------------------------------------------------------------*/

//
// Collect all script, store their values,
// and entities of course, and prepare for
// the script compilation.
//
void CCompiler::StoreAllScripts()
{
	// Walk through all scripts.
	for( Integer i=0; i<GObjectDatabase->GObjects.Num(); i++ )
		if( GObjectDatabase->GObjects[i] && GObjectDatabase->GObjects[i]->IsA(FScript::MetaClass) )
		{
			FScript* S = (FScript*)GObjectDatabase->GObjects[i];

			// Add any script to list, for searching.
			AllScripts.Push(S);

			// Add only script with the text.
			if( S->bHasText )
			{
				// Script should have an instance buffer.
				assert(S->InstanceBuffer);

				// Add to the storage.
				TStoredScript Stored;
				Stored.Script		= S;
				Stored.Properties	= S->Properties;
				Stored.Enums		= S->Enums;
				Stored.InstanceSize	= S->InstanceSize;
				Stored.Buffers.Push( S->InstanceBuffer );

				// Collect instance buffers from the entities.
				for( Integer i=0; i<GObjectDatabase->GObjects.Num(); i++ )
					if( GObjectDatabase->GObjects[i] && GObjectDatabase->GObjects[i]->IsA(FEntity::MetaClass) )
					{
						FEntity* Entity = (FEntity*)GObjectDatabase->GObjects[i];

						if( Entity->Script == S )
						{
							// Same as S.
							assert(Entity->InstanceBuffer);
							Stored.Buffers.Push( Entity->InstanceBuffer );
						}
					}

				// Add to list.
				Storage.Push(Stored);

				// Cleanup all script's objects.
				freeandnil(S->Thread);
				for( Integer f=0; f<S->Functions.Num(); f++ )
					freeandnil(S->Functions[f]);
				S->Enums.Empty();
				S->Properties.Empty();
				S->Functions.Empty();
				S->Events.Empty();
				S->VFTable.Empty();
				S->InstanceSize	= 0;
				S->ResTable.Empty();
			}
		}
}


//
// Restore all scripts after compilation
// failure.
//
void CCompiler::RestoreAfterFailure()
{
	for( Integer i=0; i<Storage.Num(); i++ )
	{
		TStoredScript&	Stored = Storage[i];
		FScript*		S		= Stored.Script;

		// Clean up all newly created script objects.
		freeandnil(S->Thread);
		for( Integer f=0; f<S->Functions.Num(); f++ )
			freeandnil( S->Functions[f] );

		for( Integer p=0; p<S->Properties.Num(); p++ )
			freeandnil( S->Properties[p] );

		for( Integer e=0; e<S->Enums.Num(); e++ )
			freeandnil( S->Enums[e] );

		S->Enums.Empty();
		S->Properties.Empty();
		S->Functions.Empty();
		S->Events.Empty();
		S->VFTable.Empty();
		S->InstanceSize		= 0;
		S->iFamily			= -1;
		S->ResTable.Empty();

		// Copy old properties and enumerations
		// from the storage.
		S->Properties	= Stored.Properties;
		S->Enums		= Stored.Enums;
		S->InstanceSize	= Stored.InstanceSize;

		// The information in all CInstanceBuffer are
		// still valid and well.
	}
}


//
// Restore old data after successful
// compilation.
//
void CCompiler::RestoreAfterSuccess()
{
	for( Integer iSlot=0; iSlot<Storage.Num(); iSlot++ )
	{
		TStoredScript& Stored = Storage[iSlot];
		FScript* Script	= Stored.Script;

		for( Integer iBuff=0; iBuff<Stored.Buffers.Num(); iBuff++ )
		{
			CInstanceBuffer* Buffer = Stored.Buffers[iBuff];
			TArray<Byte> NewData(Script->InstanceSize);

			for( Integer iNew=0; iNew<Script->Properties.Num(); iNew++ )
			{
				CProperty* NewProp = Script->Properties[iNew];

				for( Integer iOld=0; iOld<Stored.Properties.Num(); iOld++ )
				{
					CProperty* OldProp = Stored.Properties[iOld];

					// Match old and new property.
					if( NewProp->Name == OldProp->Name && NewProp->MatchTypes(*OldProp) )
					{
						// Copy value from old property to the new.
						NewProp->CopyValues
										( 
											&NewData[NewProp->Offset], 
											&Buffer->Data[OldProp->Offset] 
										);
					}
				}
			}

			// Destroy old instance buffer data.
			Exchange( Script->Properties, Stored.Properties );
			Buffer->DestroyValues();
			Exchange( Script->Properties, Stored.Properties );

			// Copy new data.
			Buffer->Data	= NewData;
		}

		// Destroy old storage data.
		for( Integer i=0; i<Stored.Properties.Num(); i++ )
			freeandnil(Stored.Properties[i]);

		for( Integer i=0; i<Stored.Enums.Num(); i++ )
			freeandnil(Stored.Enums[i]);

		Stored.Buffers.Empty();
		Stored.Enums.Empty();
		Stored.Properties.Empty();
		Stored.InstanceSize	= 0;
	}
}


/*-----------------------------------------------------------------------------
    Global editor functions.
-----------------------------------------------------------------------------*/

//
// Compile all scripts.
//
Bool CEditor::CompileAllScripts( Bool bSilent )
{
	// Don't compile if no project.
	if( !Project )
		return false;

	// Tell to user.
	log( L"Compiler: Compilation preparation" );
	if( !bSilent )
		TaskDialog->Begin(L"Compiling");

	// Shutdown all play-pages, since in play-time all used script
	// objects are turns invalid. In many cases script
	// threads crash application. 
	{
		Bool bAgain	= true;
		while( bAgain )
		{
			bAgain	= false;
			for( Integer i=0; i<EditorPages->Pages.Num(); i++ )
			{
				WEditorPage* EdPage = (WEditorPage*)EditorPages->Pages[i];
				if( EdPage->PageType == PAGE_Play )
				{
					EdPage->Close( true );
					bAgain	= true;
					break;
				}
			}
		}
	}

	// Reset Undo/Redo for each level, since hardcoded
	// instance buffer.
	for( Integer i=0; i<EditorPages->Pages.Num(); i++ )
	{
		WEditorPage* EdPage = (WEditorPage*)EditorPages->Pages[i];
		if( EdPage->PageType == PAGE_Level )
		{
			WLevelPage* Page	= (WLevelPage*)EdPage;
			Page->Transactor->Reset();
		}
	}

	// Collect the script pages.
	TArray<WScriptPage*>	Pages;
	for( Integer i=0; i<EditorPages->Pages.Num(); i++ )
	{
		WEditorPage* EdPage = (WEditorPage*)EditorPages->Pages[i];
		
		if( EdPage->PageType == PAGE_Script )
			Pages.Push( (WScriptPage*)EdPage );
	}

	// Save text of each text.
	for( Integer i=0; i<Pages.Num(); i++ )
		Pages[i]->SaveScriptText( false );

	// Reset inspector since it refer CProperty.
	TArray<FObject*> tmp;
	Inspector->SetEditObjects( tmp );

	// Launch the compiler.
	Bool Result;
	TArray<String> Warns;
	TCompilerError Err;
	CCompiler Compiler( Warns, Err );

	if( Result = Compiler.CompileAll( bSilent ) )		
	{
		// Compilation successfully.
		for( Integer i=0; i<Pages.Num(); i++ )
		{
			Pages[i]->Output->Empty();

			for( Integer w=0; w<Warns.Num(); w++ )
				Pages[i]->Output->AddMessage( Warns[w], COLOR_Goldenrod );

			Pages[i]->Output->AddMessage( L"Compilation successfully", COLOR_Green );
		}
	}
	else
	{
		// Compilation failed.

		// Find and open the problem page.
		WScriptPage* Problem = nullptr;
		for( Integer i=0; i<Pages.Num(); i++ )
			if( Pages[i]->Script == Err.Script )
			{
				Problem	= Pages[i];
				break;
			}

		// If problem page not found - open it!
		if( !Problem )
			Problem	= (WScriptPage*)GEditor->OpenPageWith( Err.Script );

		// Highlight the error.
		Problem->HighlightError( Err.ErrorLine-1 );
		Pages.Push( Problem );

		// Forced open problem page.
		GEditor->EditorPages->ActivateTabPage(Problem);

		// Output messages.
		for( Integer i=0; i<Pages.Num(); i++ )
		{
			Pages[i]->Output->Empty();

			for( Integer w=0; w<Warns.Num(); w++ )
				Pages[i]->Output->AddMessage( Warns[w], COLOR_Goldenrod );

			Pages[i]->Output->AddMessage( L"Compilation aborted", TColor( 0xdb, 0x20, 0x20, 0xff ) );
			Pages[i]->Output->AddMessage( Err.Message, TColor( 0xdb, 0x20, 0x20, 0xff ) );
		}
	}
	
	if( !bSilent )
		TaskDialog->End();

	// Notify.
	log( L"Compiler: Compilation finished." );
	return Result;
}


//
// Drop all project's scripts.
//
Bool CEditor::DropAllScripts()
{
	if( !Project )
		return false;

	// Shutdown all play-pages, since in play-time all used script
	// objects are turns invalid. In many cases script
	// threads crash application. 
	{
		Bool bAgain	= true;
		while( bAgain )
		{
			bAgain	= false;
			for( Integer i=0; i<EditorPages->Pages.Num(); i++ )
			{
				WEditorPage* EdPage = (WEditorPage*)EditorPages->Pages[i];
				if( EdPage->PageType == PAGE_Play )
				{
					EdPage->Close( true );
					bAgain	= true;
					break;
				}
			}
		}
	}

	// Walk through all objects.
	for( Integer i=0; i<GObjectDatabase->GObjects.Num(); i++ )
		if( GObjectDatabase->GObjects[i] && GObjectDatabase->GObjects[i]->IsA(FScript::MetaClass) )
		{
			FScript* Script	= As<FScript>(GObjectDatabase->GObjects[i]);

			// Only scripts with text.
			if( !Script->bHasText )
				continue;

			// Destroy bytecodes.
			freeandnil(Script->Thread);
			for( Integer iFunc=0; iFunc<Script->Functions.Num(); iFunc++ )
				freeandnil(Script->Functions[iFunc]);

			// Clear tables.
			Script->Functions.Empty();
			Script->Events.Empty();
			Script->VFTable.Empty();
			Script->ResTable.Empty();
		}

	// Notify.
	log( L"Compiler: Scripts are were dropped." );
	return true;
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/