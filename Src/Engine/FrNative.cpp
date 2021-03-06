/*=============================================================================
    FrNative.cpp: Native script functions execution.
    Copyright Sep.2016 Vlad Gordienko.
=============================================================================*/

#include "Engine.h"

/*-----------------------------------------------------------------------------
    CFrame implementation.
-----------------------------------------------------------------------------*/

//
// Execute a native function.
//
void CFrame::ExecuteNative( FEntity* Context, EOpCode Code )
{
	CFrame& Frame = *this;
	switch(	Code )	
	{
		//
		// Engine functions.
		//
		case OP_PlaySoundFX:
		{
			FSound* S		= (FSound*)POP_RESOURCE;
			Float	Gain	= POP_FLOAT;
			Float	Pitch	= POP_FLOAT;
			if( S )
				GApp->GAudio->PlayFX( S, Gain, Pitch );
			break;
		}
		case OP_PlayMusic:
		{
			FMusic*		M		= (FMusic*)POP_RESOURCE;
			Float	FadeTime	= POP_FLOAT;
			GApp->GAudio->PlayMusic( M, FadeTime );
			break;
		}
		case OP_KeyIsPressed:
		{
			Integer iKey	= POP_INTEGER;
			*POPA_BOOL		= GApp->GInput->KeyIsPressed(iKey);
			break;
		}
		case OP_GetCamera:
		{
			*POPA_ENTITY	= This->Level->Camera->Entity;
			break;
		}
		case OP_GetScreenCursor:
		{
			*POPA_VECTOR	= TVector( GApp->GInput->MouseX, GApp->GInput->MouseY );
			break;
		}
		case OP_GetWorldCursor:
		{
			*POPA_VECTOR	= GApp->GInput->WorldCursor;
			break;
		}
		case OP_Localize:
		{
			String	Section	= POP_STRING;
			String	Key		= POP_STRING;
			*POPA_STRING	= GApp->Config->ReadString( *Section, *Key );
			break;
		}
		case OP_GetScript:
		{
			FEntity* Entity	= POP_ENTITY;
			*POPA_RESOURCE	= Entity ? Entity->Script : nullptr;
			break;
		}
		case OP_StaticPush:
		{
			String	Key		= POP_STRING,
					Value	= POP_STRING;
			GStaticBuffer.Put( Key, Value );
			break;
		}
		case OP_StaticPop:
		{
			String	Key		= POP_STRING,
					Default	= POP_STRING;
			String*	Value	= GStaticBuffer.Get( Key );
			*POPA_STRING	= Value ? *Value : Default;
			break;
		}
		case OP_TravelTo:
		{
			FLevel*	Level	= As<FLevel>(POP_RESOURCE);
			Bool	bCopy	= POP_BOOL;
			if( Level )
			{
				GIncomingLevel.Destination	= Level;
				GIncomingLevel.bCopy		= bCopy;
				GIncomingLevel.Teleportee	= This;
			}
			else
				ScriptError( L"An attempt to travel into nowhere." );
			break;
		}
		case OP_FindEntity:
		{
			FObject* Obj	= GObjectDatabase->FindObject( POP_STRING, FEntity::MetaClass, This->Level );
			*POPA_ENTITY	= As<FEntity>(Obj);
			break;
		}


		//
		// Math functions.
		//
		case OP_Abs:
		{
			Float A = POP_FLOAT;
			*POPA_FLOAT = Abs( A );
			break;
		}
		case OP_ArcTan:
		{
			Float A = POP_FLOAT;
			*POPA_FLOAT = ArcTan( A );
			break;
		}
		case OP_ArcTan2:
		{
			Float Y = POP_FLOAT;
			Float X = POP_FLOAT;
			*POPA_FLOAT = ArcTan2( Y, X );
			break;
		}
		case OP_Cos:
		{
			Float A = POP_FLOAT;
			*POPA_FLOAT = Cos( A );
			break;
		}
		case OP_Sin:
		{
			Float A = POP_FLOAT;
			*POPA_FLOAT = Sin( A );
			break;
		}
		case OP_Sqrt:
		{
			Float A = POP_FLOAT;
			if( A < 0.f ) ScriptError( L"Negative X in 'sqrt'" );
			*POPA_FLOAT = Sqrt( A );
			break;
		}
		case OP_Distance:
		{
			TVector A = POP_VECTOR;
			TVector B = POP_VECTOR;
			*POPA_FLOAT = Distance( A, B );
			break;
		}
		case OP_Exp:
		{
			Float A = POP_FLOAT;
			*POPA_FLOAT = exp( A );
			break;
		}
		case OP_Ln:
		{
			Float A = POP_FLOAT;
			*POPA_FLOAT = Ln( A );
			break;
		}
		case OP_Frac:
		{
			Float A = POP_FLOAT;
			*POPA_FLOAT = Frac( A );
			break;
		}
		case OP_Round:
		{
			Float A = POP_FLOAT;
			*POPA_INTEGER = Round( A );
			break;
		}
		case OP_Normalize:
		{
			TVector A = POP_VECTOR;
			A.Normalize();
			*POPA_VECTOR = A;
			break;
		}
		case OP_Random:
		{
			Integer A = POP_INTEGER;
			*POPA_INTEGER = Random( A );
			break;
		}
		case OP_RandomF:
		{
			*POPA_FLOAT = RandomF();
			break;
		}
		case OP_VectorSize:
		{
			TVector A = POP_VECTOR;
			*POPA_FLOAT = A.Size();
			break;
		}
		case OP_VectorToAngle:
		{
			TVector A = POP_VECTOR;
			*POPA_ANGLE = VectorToAngle(A);
			break;
		}
		case OP_AngleToVector:
		{
			TAngle A = POP_ANGLE;
			*POPA_VECTOR = AngleToVector(A);
			break;
		}
		case OP_RGBA:
		{
			Byte	R	= POP_BYTE;
			Byte	G	= POP_BYTE;
			Byte	B	= POP_BYTE;
			Byte	A	= POP_BYTE;
			*POPA_COLOR	= TColor( R, G, B, A );
			break;
		}
		case OP_IToS:
		{
			Integer i = POP_INTEGER;
			*POPA_STRING	= String::Format( L"%i", i );
			break;
		}
		case OP_CharAt:
		{
			String	S = POP_STRING;
			Integer i = Clamp( POP_INTEGER, 0, S.Len()-1 );
			Char Tmp[2] = { S[i], 0 };
			*POPA_STRING	= String(Tmp);
			break;
		}
		case OP_IndexOf:
		{
			String Needle	= POP_STRING;
			String HayStack	= POP_STRING;
			*POPA_INTEGER = String::Pos( Needle, HayStack );
			break;
		}
		case OP_Execute:
		{
			GApp->ConsoleExecute(POP_STRING);
			break;
		}
		case OP_Now:
		{
			*POPA_FLOAT	= GPlat->Now();
			break;
		}


		//
		// Iterators.
		//
		case IT_AllEntities:
		{
			FScript*	Script	= As<FScript>(POP_RESOURCE);
			FLevel*		Level	= This->Level;
			Foreach.Collection.Empty();
			if( Script )
			{
				for( Integer i=0; i<Level->Entities.Num(); i++ )
					if( Level->Entities[i]->Script == Script && !Level->Entities[i]->Base->bDestroyed )
						Foreach.Collection.Push(Level->Entities[i]);
			}
			else
			{
				for( Integer i=0; i<Level->Entities.Num(); i++ )
					if( !Level->Entities[i]->Base->bDestroyed )
						Foreach.Collection.Push(Level->Entities[i]);
			}
			break;
		}
		case IT_RectEntities:
		{
			FScript*		Script		= As<FScript>(POP_RESOURCE);
			TRect			Area		= POP_AABB;
			FLevel*			Level		= This->Level;
			Integer			NumBases	= 0;
			FBaseComponent*	Bases[MAX_COLL_LIST_OBJS];
			Foreach.Collection.Empty();		
			if( Script )
				Level->CollHash->GetOverlappedByScript( Area, Script, NumBases, Bases );
			else
				Level->CollHash->GetOverlapped( Area, NumBases, Bases );
			for( Integer i=0; i<NumBases; i++ )
				Foreach.Collection.Push(Bases[i]->Entity);
			break;
		}
		case IT_TouchedEntities:
		{
			Foreach.Collection.Empty();
			if( This->Base->IsA(FPhysicComponent::MetaClass) )
			{
				FPhysicComponent* Phys = (FPhysicComponent*)This->Base;
				for( Integer i=0; i<array_length(FPhysicComponent::Touched); i++ )
					if( Phys->Touched[i] )
						Foreach.Collection.Push(Phys->Touched[i]);
			}
			break;
		}


		//
		// Simple binary operators.
		//
		#define CASE_BINARY( icode, op, a1, a2, r ) case icode:{ Byte iReg=ReadByte(); *(r*)(Regs[iReg].Value) = *(a1*)(Regs[iReg].Value) op *(a2*)(Regs[ReadByte()].Value); break;}
		CASE_BINARY( BIN_Mult_Integer,		*,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Mult_Float,		*,	Float,		Float,		Float		)
		CASE_BINARY( BIN_Mult_Color,		*,	TColor,		TColor,		TColor		)
		CASE_BINARY( BIN_Mult_Vector,		*,	TVector,	Float,		TVector		)
		CASE_BINARY( BIN_Div_Integer,		/,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Div_Float,			/,	Float,		Float,		Float		)
		CASE_BINARY( BIN_Mod_Integer,		%,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Add_Integer,		+,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Add_Float,			+,	Float,		Float,		Float		)
		CASE_BINARY( BIN_Add_Color,			+,	TColor,		TColor,		TColor		)
		CASE_BINARY( BIN_Add_Vector,		+,	TVector,	TVector,	TVector		)
		CASE_BINARY( BIN_Sub_Integer,		-,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Sub_Float,			-,	Float,		Float,		Float		)
		CASE_BINARY( BIN_Sub_Color,			-,	TColor,		TColor,		TColor		)
		CASE_BINARY( BIN_Sub_Vector,		-,	TVector,	TVector,	TVector		)
		CASE_BINARY( BIN_Shr_Integer,		>>,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Shl_Integer,		<<,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Less_Integer,		<,	Integer,	Integer,	Bool		)
		CASE_BINARY( BIN_Less_Float,		<,	Float,		Float,		Bool		)
		CASE_BINARY( BIN_LessEq_Integer,	<=,	Integer,	Integer,	Bool		)
		CASE_BINARY( BIN_LessEq_Float,		<=,	Float,		Float,		Bool		)
		CASE_BINARY( BIN_Greater_Integer,	>,	Integer,	Integer,	Bool		)
		CASE_BINARY( BIN_Greater_Float,		>,	Float,		Float,		Bool		)
		CASE_BINARY( BIN_GreaterEq_Integer,	>=,	Integer,	Integer,	Bool		)
		CASE_BINARY( BIN_GreaterEq_Float,	>=,	Float,		Float,		Bool		)
		CASE_BINARY( BIN_And_Integer,		&,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Xor_Integer,		^,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Cross_Vector,		/,	TVector,	TVector,	Float		)
		CASE_BINARY( BIN_Or_Integer,		|,	Integer,	Integer,	Integer		)
		CASE_BINARY( BIN_Dot_Vector,		*,	TVector,	TVector,	Float		)
		#undef CASE_BINARY

		//
		// Assignment binary operators.
		//
		#define CASE_ASSIGN( icode, op, a1, a2, r ) case icode: { Byte iReg=ReadByte(); *(r*)Regs[iReg].Addr op *(a2*)Regs[ReadByte()].Value; break;}
		CASE_ASSIGN( BIN_AddEqual_Integer,	+=,		Integer,	Integer,	Integer )
		CASE_ASSIGN( BIN_AddEqual_Float,	+=,		Float,		Float,		Float )
		CASE_ASSIGN( BIN_AddEqual_Vector,	+=,		TVector,	TVector,	TVector )
		CASE_ASSIGN( BIN_AddEqual_Color,	+=,		TColor,		TColor,		TColor )
		CASE_ASSIGN( BIN_SubEqual_Integer,	-=,		Integer,	Integer,	Integer )
		CASE_ASSIGN( BIN_SubEqual_Float,	-=,		Float,		Float,		Float )
		CASE_ASSIGN( BIN_SubEqual_Vector,	-=,		TVector,	TVector,	TVector )
		CASE_ASSIGN( BIN_SubEqual_Color,	-=,		TColor,		TColor,		TColor )
		CASE_ASSIGN( BIN_MulEqual_Integer,	*=,		Integer,	Integer,	Integer )
		CASE_ASSIGN( BIN_MulEqual_Float,	*=,		Float,		Float,		Float )
		CASE_ASSIGN( BIN_MulEqual_Color,	*=,		TColor,		TColor,		TColor )
		CASE_ASSIGN( BIN_DivEqual_Integer,	/=,		Integer,	Integer,	Integer )
		CASE_ASSIGN( BIN_DivEqual_Float,	/=,		Float,		Float,		Float )
		CASE_ASSIGN( BIN_ModEqual_Integer,	%=,		Integer,	Integer,	Integer )
		CASE_ASSIGN( BIN_ShlEqual_Integer,	<<=,	Integer,	Integer,	Integer )
		CASE_ASSIGN( BIN_ShrEqual_Integer,	>>=,	Integer,	Integer,	Integer )
		CASE_ASSIGN( BIN_AndEqual_Integer,	&=,		Integer,	Integer,	Integer )
		CASE_ASSIGN( BIN_XorEqual_Integer,	^=,		Integer,	Integer,	Integer )
		CASE_ASSIGN( BIN_OrEqual_Integer,	|=,		Integer,	Integer,	Integer )
		#undef CASE_ASSIGN

		//
		// Simple unary operators.
		//
		#define CASE_UNARY( icode, op, type ) case icode:{Byte iReg=ReadByte(); *(type*)(Regs[iReg].Value) = op *(type*)(Regs[iReg].Value); break;}
		CASE_UNARY( UN_Plus_Integer,		+,		Integer );
		CASE_UNARY( UN_Plus_Float,			+,		Float );
		CASE_UNARY( UN_Plus_Vector,			+,		TVector );
		CASE_UNARY( UN_Plus_Color,			+,		TColor );
		CASE_UNARY( UN_Minus_Integer,		-,		Integer );
		CASE_UNARY( UN_Minus_Float,			-,		Float );
		CASE_UNARY( UN_Minus_Vector,		-,		TVector );
		CASE_UNARY( UN_Minus_Color,			-,		TColor );
		CASE_UNARY( UN_Not_Bool,			!,		Bool );
		CASE_UNARY( UN_Not_Integer,			~,		Integer );
		#undef CASE_UNARY

		//
		// Suffix / prefix operators.
		//
		#define CASE_PREFIX( icode, op, type ) case icode:{(*(type*)(Regs[ReadByte()].Addr))op; break;}
		CASE_PREFIX( UN_Inc_Integer,		++,		Integer );
		CASE_PREFIX( UN_Inc_Float,			++,		Float );
		CASE_PREFIX( UN_Dec_Integer,		--,		Integer );
		CASE_PREFIX( UN_Dec_Float,			--,		Float );
		#undef CASE_PREFIX

		//
		// Ugly string operators.
		//
		case BIN_AddEqual_String:
		{
			Byte iReg = ReadByte();
			*(String*)Regs[iReg].Addr += Regs[iReg].StrValue;
			break;
		}
		case BIN_Add_String:
		{
			Byte iReg=ReadByte();
			Regs[iReg].StrValue += Regs[ReadByte()].StrValue;
			break;
		}

		//
		// Invalid opcode.
		//
		default:
		{
			// Bad instruction.
			ScriptError( L"Unknown instruction '%i'", Code );
			break;
		}
	}
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/