/*=============================================================================
    FrFields.cpp: WObjectInspector.
    Copyright Jul.2016 Vlad Gordienko.
=============================================================================*/

#include "Editor.h"

/*-----------------------------------------------------------------------------
    Inspector colors.
-----------------------------------------------------------------------------*/

#define COLOR_INSPECTOR_BACKDROP		TColor( 0x3f, 0x3f, 0x3f, 0xff )
#define COLOR_INSPECTOR_BORDER			TColor( 0x66, 0x66, 0x66, 0xff )
#define COLOR_INSPECTOR_ITEM			TColor( 0x30, 0x30, 0x30, 0xff )
#define COLOR_INSPECTOR_ITEM_COMPON		TColor( 0x25, 0x25, 0x25, 0xff )
#define COLOR_INSPECTOR_ITEM_BORDER		TColor( 0x3f, 0x3f, 0x3f, 0xff )
#define COLOR_INSPECTOR_ITEM_SEL		TColor( 0x22, 0x22, 0x22, 0xff )
#define COLOR_INSPECTOR_ITEM_WAIT		COLOR_IndianRed
#define COLOR_INSPECTOR_ITEM_NOEDIT		TColor( 0x37, 0x37, 0x37, 0xff )


/*-----------------------------------------------------------------------------
    CInspectorItemBase implementation.
-----------------------------------------------------------------------------*/

//
// Base item constructor.
//
CInspectorItemBase::CInspectorItemBase( WObjectInspector* InInspector, DWord InDepth )
	:	bHidden( false ),
		bExpanded( false ),
		Caption( L"" ),
		Inspector( InInspector ),
		Children(),
		Objects(),
		Depth( 0 ),
		Top( 0 )
{
	assert(Inspector);
}


//
// Base item destructor.
//
CInspectorItemBase::~CInspectorItemBase()
{
	// Don't destroy children, they actually 
	// owned by inspector.
	Children.Empty();
	Objects.Empty();
}


//
// Collapse all children.
//
void CInspectorItemBase::CollapseAll()
{
	for( Integer i=0; i<Children.Num(); i++ )
	{
		Children[i]->CollapseAll();
		Children[i]->bHidden	= true;
	}

	bExpanded	= false;
	Inspector->UpdateChildren();
}


//
// Expand all children.
//
void CInspectorItemBase::ExpandAll()
{
	// Don't expand children of children.
	for( Integer i=0; i<Children.Num(); i++ )
		Children[i]->bHidden	= false;

	bExpanded	= true;
	Inspector->UpdateChildren();
}


//
// Draw the base item, should be implemented in
// derived classes.
//
void CInspectorItemBase::Paint( TPoint Base, CGUIRenderBase* Render )
{
}


//
// Mouse press base item.
//
void CInspectorItemBase::MouseDown( EMouseButton Button, Integer X, Integer Y )
{
}


//
// Mouse up base item.
//
void CInspectorItemBase::MouseUp( EMouseButton Button, Integer X, Integer Y )
{
}


//
// Mouse move base item.
//
void CInspectorItemBase::MouseMove( EMouseButton Button, Integer X, Integer Y )
{
}


//
// When user drag something above item.
//
void CInspectorItemBase::DragOver( void* Data, Integer X, Integer Y, Bool& bAccept )
{
	bAccept	= false;
}


//
// User drop something on item.
//
void CInspectorItemBase::DragDrop( void* Data, Integer X, Integer Y )
{
}


//
// Item has lost the focus.
//
void CInspectorItemBase::Unselect()
{
}


/*-----------------------------------------------------------------------------
    CPropertyItem.
-----------------------------------------------------------------------------*/

//
// Property item.
//
class CPropertyItem: public CInspectorItemBase
{
public:
	// Variables.
	CTypeInfo		TypeInfo;
	Bool			bExpandable;
	DWord			AddrOffset;
	DWord			Flags;

	// Temporal and optional widgets.
	WButton*		Button;
	WEdit*			Edit;
	WComboBox*		ComboBox;

	// CPropertyItem interface.
	CPropertyItem	(	WObjectInspector* InInspector, 
						DWord InDepth, 
						const TArray<FObject*>& InObjs, 
						String InCaption, 
						const CTypeInfo& InType, 
						DWord InAddrOffset, 
						DWord InFlags 
					);
	~CPropertyItem();
	Bool IsAtSeparator( Integer X ) const;
	Bool IsAtSign( Integer X ) const;
	inline void* GetAddress( Integer iObject );

	// Event from temporal widgets.
	void OnSomethingChange( WWidget* Sender );
	void OnPickClick( WWidget* Sender );

	// Events from Object Inspector.
	void Paint( TPoint Base, CGUIRenderBase* Render );
	void MouseDown( EMouseButton Button, Integer X, Integer Y );
	void MouseUp( EMouseButton Button, Integer X, Integer Y );
	void MouseMove( EMouseButton Button, Integer X, Integer Y );
	void DragOver( void* Data, Integer X, Integer Y, Bool& bAccept );
	void DragDrop( void* Data, Integer X, Integer Y );
	void Unselect();
};


//
// Property item constructor.
//
CPropertyItem::CPropertyItem(	WObjectInspector* InInspector, 
								DWord InDepth, 
								const TArray<FObject*>& InObjs, 
								String InCaption, 
								const CTypeInfo& InType, 
								DWord InAddrOffset, 
								DWord InFlags 
							)
	:	CInspectorItemBase( InInspector, InDepth ),
		Button( nullptr ),
		ComboBox( nullptr ),
		Edit( nullptr ),
		Flags( InFlags ),
		TypeInfo( InType ),
		AddrOffset( InAddrOffset ),
		bExpandable( false )
{
	Caption	= InCaption;
	Depth	= InDepth;

	// Add to inspector.
	Inspector->Children.Push(this);
	Objects = InObjs;

	if( TypeInfo.ArrayDim > 1 )
	{
		// Item is an array property.
		CTypeInfo InnerType = TypeInfo;
		InnerType.ArrayDim	= 1;

		// Add each time.
		for( Integer i=0; i<TypeInfo.ArrayDim; i++ )
			Children.Push( new CPropertyItem
										(
											Inspector,
											Depth + 1,
											InObjs,
											String::Format( L"[%d]", i ),
											InnerType,
											InAddrOffset + i*InnerType.TypeSize(),
											Flags
										) );

		bExpandable	= true;
	}
	else
	{
		// Scalar value.
		if( TypeInfo.Type == TYPE_Color )
		{
			// Color struct.
			static const Char* MemNames[4] = { L"R", L"G", L"B", L"A" };

			for( Integer i=0; i<4; i++ )
				Children.Push( new CPropertyItem
											(
												Inspector,
												Depth + 1,
												InObjs,
												MemNames[i],
												TYPE_Byte,
												InAddrOffset + (Integer)((Byte*)&(&((TColor*)nullptr)->R)[i] -(Byte*)nullptr),
												Flags
											) );

			bExpandable	= true;
		}
		else if( TypeInfo.Type == TYPE_Vector )
		{
			// Vector struct.
			static const Char* MemNames[2] = { L"X", L"Y" };

			for( Integer i=0; i<2; i++ )
				Children.Push( new CPropertyItem
											(
												Inspector,
												Depth + 1,
												InObjs,
												MemNames[i],
												TYPE_Float,
												InAddrOffset + (Integer)((Byte*)&(&((TVector*)nullptr)->X)[i] -(Byte*)nullptr),
												Flags
											) );

			bExpandable	= true;
		}
		else if( TypeInfo.Type == TYPE_AABB )
		{
			// AABB struct.
			static const Char* MemNames[2] = { L"Min", L"Max" };

			for( Integer i=0; i<2; i++ )
				Children.Push( new CPropertyItem
											(
												Inspector,
												Depth + 1,
												InObjs,
												MemNames[i],
												TYPE_Vector,
												InAddrOffset + (Integer)((Byte*)&(&((TRect*)nullptr)->Min)[i] -(Byte*)nullptr),
												Flags
											) );

			bExpandable	= true;
		}
	}

	bExpanded = false;
	CollapseAll();
}


//
// Property item destructor.
//
CPropertyItem::~CPropertyItem()
{
	// Release the controls.
	freeandnil( Button );
	freeandnil( Edit );
	freeandnil( ComboBox );
}


//
// Let user pick the entity.
//
void CPropertyItem::OnPickClick( WWidget* Sender )
{
	assert(TypeInfo.Type == TYPE_Entity);
	Inspector->BeginWaitForPick( this );

	if( Inspector->bWaitForPick )
	{
		Button->bEnabled	= false;
		Button->bDown		= true;
	}
}


//
// User drop resource here.
//
void CPropertyItem::DragDrop( void* Data, Integer X, Integer Y )
{
	FObject* Res = (FObject*)Data;
	assert( Res && Res->IsA(FResource::MetaClass) );

	if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackEnter();
	{
		// Set value.
		for( Integer i=0; i<Objects.Num(); i++ )
			*(FObject**)GetAddress(i) = Res;	

		// Notify.
		for( Integer i=0; i<Objects.Num(); i++ )
			Objects[i]->EditChange();
	}
	if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackLeave();
}


//
// User drag over something, decide it is correct object or not.
//
void CPropertyItem::DragOver( void* Data, Integer X, Integer Y, Bool& bAccept )
{
	bAccept =	!( Flags & PROP_Const )&& 
				( TypeInfo.ArrayDim == 1 )&& 
				( TypeInfo.Type == TYPE_Resource )&& 
				( Data )&&
				((FObject*)Data)->IsA(TypeInfo.Class);
}	


//
// Return absolute address of the property of this item for
// the i'th object.
//
inline void* CPropertyItem::GetAddress( Integer iObject )
{
	assert( iObject>=0 && iObject<Objects.Num() );

	// Hack a little.
	if( Objects[0]->IsA(FEntity::MetaClass) )
	{
		// Entity property from the CInstanceBuffer.
		FEntity* Entity = (FEntity*)Objects[iObject];
		return (void*)((Byte*)(&Entity->InstanceBuffer->Data[0]) + AddrOffset);
	}
	else if( Objects[0]->IsA(FScript::MetaClass) && !(Flags & PROP_Native) )
	{
		// Script property from the instance buffer.
		FScript* Script = (FScript*)Objects[iObject];
		return (void*)((Byte*)(&Script->InstanceBuffer->Data[0]) + AddrOffset);
	}
	else
	{
		// Regular property.
		return (void*)(((Byte*)Objects[iObject]) + AddrOffset);
	}
}


//
// Called when some fields editor control value has been modified.
//
void CPropertyItem::OnSomethingChange( WWidget* Sender )
{
	assert(TypeInfo.ArrayDim == 1);

	switch( TypeInfo.Type )
	{
		case TYPE_Bool:
		{
			// Boolean value.
			Integer iFlag = ComboBox->ItemIndex;

			if( iFlag != 0 && iFlag != 1 )
				return;

			if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackEnter();
			{
				for( Integer i=0; i<Objects.Num(); i++ )
					*(Bool*)GetAddress(i) = iFlag != 0;
			}
			if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackLeave();

			break;
		}
		case TYPE_Float:
		{
			// Float value.
			Float Value;

			if( Edit->Text.ToFloat( Value, 0.f ) )	
			{
				if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackEnter();
				{
					for( Integer i=0; i<Objects.Num(); i++ )
						*(Float*)GetAddress(i) = Value;
				}
				if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackLeave();
			}
			break;
		}
		case TYPE_Byte:
		{
			// Byte value.
			Integer Value;

			if( Edit )
				Edit->Text.ToInteger( Value, 0 );
			else
				Value = ComboBox->ItemIndex;

			if( Value > 255 || Value < 0 )
				return;

			if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackEnter();
			{
				for( Integer i=0; i<Objects.Num(); i++ )
					*(Byte*)GetAddress(i) = Value;
			}
			if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackLeave();

			break;
		}
		case TYPE_String:
		{
			// String value.
			if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackEnter();
			{
				for( Integer i=0; i<Objects.Num(); i++ )
					*(String*)GetAddress(i) = Edit->Text;
			}
			if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackLeave();
			break;
		}
		case TYPE_Integer:
		{
			// Integer value.
			Integer Value;

			if( Edit->Text.ToInteger( Value, 0 ) )	
			{
				if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackEnter();
				{
					for( Integer i=0; i<Objects.Num(); i++ )
						*(Integer*)GetAddress(i) = Value;
				}
				if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackLeave();
			}
			break;
		}
		case TYPE_Angle:
		{
			// Angle value.
			Float Value;

			if( Edit->Text.ToFloat( Value, 0.f ) )	
			{
				if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackEnter();
				{
					for( Integer i=0; i<Objects.Num(); i++ )
						*(TAngle*)GetAddress(i) = TAngle( Value * PI / 180.f );
				}
				if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackLeave();
			}
			break;
		}
		case TYPE_Resource:
		{
			// Resource ref.
			CClass* ResClass = TypeInfo.Class;
			assert(ResClass && ResClass->IsA(FResource::MetaClass));
			FObject* ResRef = GProject->FindObject( Edit->Text, ResClass, nullptr );

			if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackEnter();
			{
				for( Integer i=0; i<Objects.Num(); i++ )
					*(FResource**)GetAddress(i) = (FResource*)ResRef;
			}
			if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackLeave();
			break;
		}
		case TYPE_Entity:
		{
			// Entity ref.
			WEditorPage* Page = (WEditorPage*)GEditor->EditorPages->GetActivePage();
			if( Page && Page->PageType == PAGE_Level )
			{
				FLevel* Level = ((WLevelPage*)Page)->Level;
				assert(Level);
				FEntity* Gotten = Level->FindEntity( Edit->Text );
				FEntity* Result = nullptr;

				if( Gotten )
					Result = TypeInfo.Script ? ( TypeInfo.Script == Gotten->Script ? Gotten : nullptr ) : Gotten;
				else
					Result = nullptr;

				if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackEnter();
				{
					for( Integer i=0; i<Objects.Num(); i++ )
						*(FEntity**)GetAddress(i) = (FEntity*)Result;
				}
				if( Inspector->LevelPage )	Inspector->LevelPage->Transactor->TrackLeave();
			}
			break;
		}
		default:
			error( L"Unsupported property type %d", (Byte)TypeInfo.Type );
	}

	// Notify all objects.
	for( Integer i=0; i<Objects.Num(); i++ )
		Objects[i]->EditChange();
}


//
// Return true, if x value is at collapse/expand
// button.
//
Bool CPropertyItem::IsAtSign( Integer X ) const
{ 
	return Abs( X - (18+Depth*20) ) < 20;
}


//
// User has clicked item.
//
void CPropertyItem::MouseDown( EMouseButton MouseButton, Integer X, Integer Y )
{
	// Maybe just slide separator.
	if( IsAtSeparator(X) )
	{
		Inspector->bMoveSep	= true;
		return;
	}

	// Unselect all.
	Inspector->UnselectAll();

	if( bExpandable )
	{
		// A struct or array.
		if( IsAtSign(X) )
		{
			// Toggle children.
			if( bExpanded )
				CollapseAll();
			else
				ExpandAll();
		}
		else
		{
			// Color dialog.
			if( TypeInfo.Type == TYPE_Color && TypeInfo.ArrayDim==1 && X > Inspector->Size.Width-54 )
			{
				WColorChooser::SharedColor	= *(TColor*)GetAddress(0);
				Inspector->WaitColor		= this;
				new WColorChooser( Inspector->Root, TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::ColorSelected ) );
			}
		}
	}
	else if( TypeInfo.ArrayDim == 1 && !(Flags & PROP_Const) )
	{
		// Simple value.
		Inspector->Selected = this;

		// Test value in all object,s they are total matched?
		Bool bMatched = true;
		for( Integer i=1; i<Objects.Num(); i++ )
			if( !TypeInfo.CompareValues( GetAddress(0), GetAddress(i) ) )
			{
				bMatched	= false;
				break;
			}

		switch( TypeInfo.Type )
		{
			case TYPE_Bool:
			{
				// Boolean value.
				ComboBox		= new WComboBox( Inspector, Inspector->Root );
				ComboBox->SetSize( Inspector->Size.Width-Inspector->Separator-11, INSPECTOR_ITEM_HEIGHT );
				ComboBox->Location = TPoint( Inspector->Separator, Top );
				ComboBox->EventChange = TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::SomethingChange );
				ComboBox->AddItem( L"False", nullptr );
				ComboBox->AddItem( L"True" , nullptr );
				if( bMatched )
					ComboBox->SetItemIndex( *(Bool*)GetAddress(0) ? 1 : 0, false );
				else
					ComboBox->SetItemIndex( -1, false );
				break;
			}
			case TYPE_Byte:
			{
				// Byte value.
				if( TypeInfo.Enum )
				{
					// Enumeration.
					ComboBox		= new WComboBox( Inspector, Inspector->Root );
					ComboBox->SetSize( Inspector->Size.Width-Inspector->Separator-11, INSPECTOR_ITEM_HEIGHT );
					ComboBox->Location = TPoint( Inspector->Separator, Top );
					ComboBox->EventChange = TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::SomethingChange );

					for( Integer i=0; i<TypeInfo.Enum->Aliases.Num(); i++ )
						ComboBox->AddItem( TypeInfo.Enum->Aliases[i], nullptr );

					ComboBox->SetItemIndex( bMatched ? *(Byte*)GetAddress(0) : -1, false );
					break;
				}
				else
				{
					// Simple value.
					Edit		= new WEdit( Inspector, Inspector->Root );
					Edit->SetSize( Inspector->Size.Width-Inspector->Separator-14, INSPECTOR_ITEM_HEIGHT-2 );
					Edit->Location = TPoint( Inspector->Separator+1, Top+1 );
					Edit->EditType = EDIT_Integer;
					Edit->EventChange = TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::SomethingChange );
					if( bMatched )
						Edit->SetText( String::Format( L"%d", *(Byte*)GetAddress(0) ), false );
				}
				break;
			}
			case TYPE_Float:
			{
				// Float value.
				Edit		= new WEdit( Inspector, Inspector->Root );
				Edit->SetSize( Inspector->Size.Width-Inspector->Separator-14, INSPECTOR_ITEM_HEIGHT-2 );
				Edit->Location = TPoint( Inspector->Separator+1, Top+1 );
				Edit->EditType = EDIT_Float;
				Edit->EventChange = TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::SomethingChange );
				if( bMatched )
					Edit->SetText( String::Format( L"%.4f", *(Float*)GetAddress(0) ), false );
				break;
			}
			case TYPE_String:
			{
				// String value.
				Edit		= new WEdit( Inspector, Inspector->Root );
				Edit->SetSize( Inspector->Size.Width-Inspector->Separator-14, INSPECTOR_ITEM_HEIGHT-2 );
				Edit->Location = TPoint( Inspector->Separator+1, Top+1 );
				Edit->EditType = EDIT_String;
				Edit->EventChange = TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::SomethingChange );
				if( bMatched )
					Edit->SetText( *(String*)GetAddress(0), false );
				break;
			}
			case TYPE_Integer:
			{
				// Integer value.
				Edit		= new WEdit( Inspector, Inspector->Root );
				Edit->SetSize( Inspector->Size.Width-Inspector->Separator-14, INSPECTOR_ITEM_HEIGHT-2 );
				Edit->Location = TPoint( Inspector->Separator+1, Top+1 );
				Edit->EditType = EDIT_Integer;
				Edit->EventChange = TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::SomethingChange );
				if( bMatched )
					Edit->SetText( String::Format( L"%d", *(Integer*)GetAddress(0) ), false );
				break;
			}
			case TYPE_Angle:
			{
				// Angle value.
				Edit		= new WEdit( Inspector, Inspector->Root );
				Edit->SetSize( Inspector->Size.Width-Inspector->Separator-14, INSPECTOR_ITEM_HEIGHT-2 );
				Edit->Location = TPoint( Inspector->Separator+1, Top+1 );
				Edit->EditType = EDIT_Float;
				Edit->EventChange = TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::SomethingChange );
				if( bMatched )
					Edit->SetText( String::Format( L"%.2f", (*(TAngle*)GetAddress(0)).ToDegs() ), false );
				break;
			}
			case TYPE_Resource:
			{
				// Resource value.
				Edit		= new WEdit( Inspector, Inspector->Root );
				Edit->SetSize( Inspector->Size.Width-Inspector->Separator-14, INSPECTOR_ITEM_HEIGHT-2 );
				Edit->Location = TPoint( Inspector->Separator+1, Top+1 );
				Edit->EditType = EDIT_String;
				Edit->EventChange = TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::SomethingChange );
				if( bMatched )
					Edit->SetText( TypeInfo.ToString(GetAddress(0)) );
				break;
			}
			case TYPE_Entity:
			{
				// Entity value, available only for pure entity, not
				// scripts.
				if( !Inspector->Objects[0]->IsA(FEntity::MetaClass) )
					break;

				// Entity pick button.
				Button			= new WButton( Inspector, Inspector->Root );
				Button->SetSize( 50, INSPECTOR_ITEM_HEIGHT-1 );
				Button->Location = TPoint( Inspector->Size.Width - 11 - Button->Size.Width, Top+0 );
				Button->EventClick	= TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::PickClick );
				Button->Caption		= L"Pick";

				// Entity edit.
				Edit		= new WEdit( Inspector, Inspector->Root );
				Edit->SetSize( Inspector->Size.Width-Inspector->Separator-12 - Button->Size.Width, INSPECTOR_ITEM_HEIGHT-2 );
				Edit->Location = TPoint( Inspector->Separator+1, Top+1 );
				Edit->EditType = EDIT_String;
				Edit->EventChange = TNotifyEvent( Inspector, (TNotifyEvent::TEvent)&WObjectInspector::SomethingChange );
				if( bMatched )
					Edit->SetText( TypeInfo.ToString(GetAddress(0)) );
	
				break;
			}
			default:
				error( L"Unsupported property type %d", (Byte)TypeInfo.Type );
		}
	}
}


//
// Draw a property item.
//
void CPropertyItem::Paint( TPoint Base, CGUIRenderBase* Render )
{
	// Don't draw invisible.
	if( bHidden )
		return;
	/*
	// Draw background.
	Render->DrawRegion
				( 
					TPoint( Base.X+15, Base.Y+Top ), 
					TSize( Inspector->Size.Width-27, INSPECTOR_ITEM_HEIGHT ), 
					Inspector->WaitItem == this ?	COLOR_INSPECTOR_ITEM_WAIT : 
													Inspector->Selected == this ?	COLOR_INSPECTOR_ITEM_SEL :  
																					COLOR_INSPECTOR_ITEM, 
					COLOR_INSPECTOR_ITEM_BORDER, 
					BPAT_Solid 
				);
	*/

	// Draw background.
	Render->DrawRegion
				( 
					TPoint( Base.X+15, Base.Y+Top ), 
					TSize( Inspector->Size.Width-26, INSPECTOR_ITEM_HEIGHT ), 
					Inspector->WaitItem == this ?	COLOR_INSPECTOR_ITEM_WAIT : 
					Inspector->Selected == this ?	COLOR_INSPECTOR_ITEM_SEL :  
					Flags & PROP_Editable ?			COLOR_INSPECTOR_ITEM :
													COLOR_INSPECTOR_ITEM_NOEDIT,								 
					COLOR_INSPECTOR_ITEM_BORDER, 
					BPAT_Solid 
				);

	// Separator.
	Render->DrawRegion
				( 
					TPoint( Base.X + Inspector->Separator, Base.Y+Top ), 
					TSize( 0, INSPECTOR_ITEM_HEIGHT ),		
					COLOR_INSPECTOR_ITEM_BORDER, 
					COLOR_INSPECTOR_ITEM_BORDER, 
					BPAT_None 
				);

	// Sign.
	if( bExpandable )
		Render->DrawPicture
					( 
						TPoint( Base.X + 18 + Depth*20, Base.Y + Top + 6 ), 
						TSize( 9, 9 ), 
						TPoint( bExpanded ? 30 : 21, 0 ), 
						TSize( 9, 9 ), 
						Inspector->Root->Icons 
					);

	// Property name.
	Render->DrawText
				( 
					TPoint( Base.X + 30 + Depth*20, Base.Y+Top+3  ), 
					Caption, 
					GUI_COLOR_TEXT,  
					Inspector->Root->Font1 
				);

	// Draw property value, only
	// if not selected.
	if( Inspector->Selected != this )
	{
		String Value;

		if( TypeInfo.Type == TYPE_AABB )
		{
			// AABB value.
			Value	= L"(...)";
		}
		else if( TypeInfo.ArrayDim == 1 )
		{
			// Pick value from first.
			Value = TypeInfo.ToString( GetAddress(0) );

			// Check with others, if mismatched, nothing to put.
			for( Integer i=1; i<Objects.Num(); i++ )
				if( !TypeInfo.CompareValues( GetAddress(0), GetAddress(i) ) )
				{
					// Mismatched.
					Value = L"";
					break;
				}
		}
		else
		{
			// Array value.
			Value = L"[...]";
		}

		// Put it!
		Render->DrawText
					( 
						TPoint( Base.X + Inspector->Separator + 5, Base.Y + Top + 4 ),  
						Value, 
						GUI_COLOR_TEXT, 
						Inspector->Root->Font1  
					);

		// Draw color rect.
		if( TypeInfo.Type == TYPE_Color && TypeInfo.ArrayDim==1 && Value )
			Render->DrawRegion
						( 
							TPoint( Base.X + Inspector->Size.Width - 34, Top + Base.Y + 2 ),
							TSize( 20, INSPECTOR_ITEM_HEIGHT-4 ),
							*(TColor*)GetAddress(0),
							COLOR_Black,
							BPAT_Solid
						);
	}
}


//
// Called when item lost focus.
//
void CPropertyItem::Unselect()
{
	freeandnil( Edit );
	freeandnil( Button );
	freeandnil( ComboBox );
}


//
// Mouse up on this item.
//
void CPropertyItem::MouseUp( EMouseButton Button, Integer X, Integer Y )
{
	Inspector->bMoveSep	= false;
}


//
// Mouse hover item.
//
void CPropertyItem::MouseMove( EMouseButton Button, Integer X, Integer Y )
{
	if( Inspector->bMoveSep )
	{
		// Move separator.
		Inspector->Separator	= X;
		Inspector->Separator	= Clamp( Inspector->Separator, 100, Inspector->Size.Width-100 );
		
		Inspector->UnselectAll();
	}

	// Change cursor style.
	Inspector->Cursor = IsAtSeparator(X) ? CR_HSplit : CR_Arrow;
}


//
// Return true if cursor is near separator.
//
Bool CPropertyItem::IsAtSeparator( Integer X ) const
{
	return Abs(Inspector->Separator - X) < 5;
}


/*-----------------------------------------------------------------------------
    CComponentItem.
-----------------------------------------------------------------------------*/

//
// A component item.
//
class CComponentItem: public CInspectorItemBase
{
public:
	// CComponentItem interface.
	CComponentItem( WObjectInspector* InInspector, const TArray<FObject*>& InObjs );
	~CComponentItem();
	Bool IsAtSign( Integer X ) const;

	// Events from Object Inspector.
	void Paint( TPoint Base, CGUIRenderBase* Render );
	void MouseDown( EMouseButton Button, Integer X, Integer Y );
};


//
// Component item constructor.
//
CComponentItem::CComponentItem( WObjectInspector* InInspector, const TArray<FObject*>& InObjs )
	:	CInspectorItemBase( InInspector, 0 )
{
	// Add to inspector.
	Inspector->Children.Push( this );
	Objects = InObjs;
	Caption	= Objects[0]->GetName();

	// Check for problems.
	for( Integer i=1; i<Objects.Num(); i++ )
		assert(Objects[i]->GetClass() == Objects[0]->GetClass());

	// Add all children.
	Bool bScript = Inspector->Objects[0]->IsA(FScript::MetaClass);
	for( CClass* C = Objects[0]->GetClass(); C; C = C->Super )
	{
		for( Integer iProp=0; iProp<C->Properties.Num(); iProp++ )
		{
			CProperty* Prop = C->Properties[iProp];

			if( Prop->Flags & PROP_Editable || bScript )
				Children.Push( new CPropertyItem( Inspector, 1, Objects, bScript ? Prop->Name : Prop->Alias, *Prop, Prop->Offset, Prop->Flags ) );
		}
	}

	// Collapse by default.
	bExpanded = true;
	ExpandAll();
}


//
// Component item destructor.
//
CComponentItem::~CComponentItem()
{
}


//
// Draw component item. 
//
void CComponentItem::Paint( TPoint Base, CGUIRenderBase* Render )
{
	// Draw background.
	Render->DrawRegion
				( 
					TPoint( Base.X+15, Base.Y+Top ), 
					TSize( Inspector->Size.Width-26, INSPECTOR_ITEM_HEIGHT ), 
					COLOR_INSPECTOR_ITEM_COMPON,
					COLOR_INSPECTOR_ITEM_BORDER, 
					BPAT_Solid 
				);

	// Sign.
	Render->DrawPicture
				( 
					TPoint( Base.X + 3, Base.Y + Top + 6 ), 
					TSize( 9, 9 ), 
					TPoint( bExpanded ? 30 : 21, 0 ), 
					TSize( 9, 9 ), 
					Inspector->Root->Icons 
				);

	// Component name.
	Render->DrawText
				( 
					TPoint( Base.X + 30, Base.Y+Top+3  ), 
					Caption, 
					GUI_COLOR_TEXT,  
					Inspector->Root->Font1 
				);
}


//
// User has clicked item.
//
void CComponentItem::MouseDown( EMouseButton Button, Integer X, Integer Y )
{
	if( IsAtSign(X) )
	{
		// Toggle show children.
		if( bExpanded )
			CollapseAll();
		else
			ExpandAll();

		Inspector->UnselectAll();
	}
}


//
// Return true, if X is near component sign.
//
Bool CComponentItem::IsAtSign( Integer X ) const
{
	return X < 15;
}


/*-----------------------------------------------------------------------------
    WObjectInspector implementation.
-----------------------------------------------------------------------------*/

//
// Object inspector constructor.
//
WObjectInspector::WObjectInspector( WContainer* InOwner, WWindow* InRoot )
	:	WContainer( InOwner, InRoot ),
		Separator( 130 ),
		bMoveSep( false ),
		Children(),
		Objects(),
		Selected( nullptr ),
		TopClass( FObject::MetaClass ),
		bWaitForPick( false ),
		WaitItem( nullptr )
{
	// Allocate scrollbar.
	ScrollBar		= new WSlider( this, InRoot );
	ScrollBar->SetOrientation( SLIDER_Vertical );
	ScrollBar->EventChange	= TNotifyEvent( this, (TNotifyEvent::TEvent)&WObjectInspector::ScrollChange );
	ScrollBar->SetSize( 12, 50 );
	ScrollBar->Align	= AL_Right;

	// Set own default size.
	SetSize( 300, 300 );
	Padding	= TArea( INSPECTOR_HEADER_SIZE, 0, 0, 0 );

	Caption	= L"Inspector";
}


//
// Object inspector destructor.
//
WObjectInspector::~WObjectInspector()
{
	// Don't destroy scrollbar, 
	// WContainer do this job.

	// Destroy all children.
	for( Integer i=0; i<Children.Num(); i++ )
		delete Children[i];
}


//
// Set an objects list for edit.
//
void WObjectInspector::SetEditObjects( TArray<FObject*>& Objs )
{
	// Cleanup old stuff.
	ScrollBar->Value	= 0;
	Selected			= nullptr;
	WaitItem			= nullptr;
	UnselectAll();
	for( Integer i=0; i<Children.Num(); i++ )
		delete Children[i];
	Children.Empty();
	Objects.Empty();

	if( Objs.Num() > 0 )
	{
		// Maybe current page is level edit and selected entity, so
		// we should use transactor here.
		if	( 
				GEditor->GetActivePage() && 
				GEditor->GetActivePage()->PageType == PAGE_Level &&
				Objs[0]->IsA(FEntity::MetaClass)
			)
			LevelPage	= (WLevelPage*)GEditor->GetActivePage();
		else
			LevelPage	= nullptr;


		if( Objs[0]->IsA(FEntity::MetaClass) )				
		{
			// Its a list of entities.
			FEntity*	First = (FEntity*)Objs[0];
			FScript*	Script = First->Script;
			TArray<FBaseComponent*>	Bases;
			Bases.Push( First->Base );
			TopClass	= First->Base->GetClass();

			// Add similar objects to edit.
			for( Integer i=1; i<Objs.Num(); i++ )
			{
				assert(Objs[i]->IsA(FEntity::MetaClass));
				FEntity* Other = (FEntity*)Objs[i];

				if( Other->Script == Script )
				{
					assert(Other->Base->GetClass() == TopClass);
					Bases.Push( Other->Base );
				}
			}

			// Store list of source objects, doesn't full copy,
			// since some input objects are rejected due script.
			for( Integer i=0; i<Bases.Num(); i++ )
				Objects.Push(Bases[i]->Entity);

			// Own properties is owned by Base.
			for( CClass* C = First->Base->GetClass(); C; C = C->Super )
			{
				for( Integer iProp=0; iProp<C->Properties.Num(); iProp++ )
				{
					CProperty* Prop = C->Properties[iProp];

					// Property item will add to list its self, also
					// add children and hide them as default.
					if( Prop->Flags & PROP_Editable )
						new CPropertyItem( this, 0, *(TArray<FObject*>*)&Bases, Prop->Alias, *Prop, Prop->Offset, Prop->Flags );
				}
			}

			// InstanceBuffer properties.
			for( Integer i=0; i<First->Script->Properties.Num(); i++ )
			{
				CProperty*Prop = First->Script->Properties[i];

				if( Prop->Flags & PROP_Editable )
					new CPropertyItem( this, 0, *(TArray<FObject*>*)&Objects, Prop->Alias, *Prop, Prop->Offset, Prop->Flags );
			}

			// Add component items.
			for( Integer i=0; i<Bases[0]->Entity->Components.Num(); i++  )
			{
				TArray<FExtraComponent*> Comps;

				// Collect same components from different entities.
				for( Integer iOwner=0; iOwner<Bases.Num(); iOwner++ )
					Comps.Push( Bases[iOwner]->Entity->Components[i] );

				CComponentItem* Item =  new CComponentItem( this, *(TArray<FObject*>*)&Comps );

				// Don't add empty component item.
				if( Item->Children.Num() == 0 )
				{
					Children.RemoveShift(Children.FindItem(Item));
					delete Item;
				}
			}

			// Set caption.
			Caption	= String::Format( L"Inspector [%s]", Objects.Num()==1 ? *Bases[0]->Entity->GetName() : *Bases[0]->Entity->Script->GetName() );
		}
		else if( Objs[0]->IsA(FScript::MetaClass) )
		{
			// Script resource.
			assert(Objs.Num() == 1);
			FScript* Script = (FScript*)Objs[0];
			TArray<FComponent*> TmpArr;
			TmpArr.SetNum( 1 );

			for( Integer i=0; i<Objs.Num(); i++ )
				Objects.Push(Objs[i]);

			TopClass	= Objects[0]->GetClass();

			// FScript properties.
			for( CClass* C = TopClass; C; C = C->Super )
			{
				for( Integer iProp=0; iProp<C->Properties.Num(); iProp++ )
				{
					CProperty* Prop = C->Properties[iProp];

					// Property item will add to list its self, also
					// add children and hide them as default.
					new CPropertyItem( this, 0, Objects, Prop->Name, *Prop, Prop->Offset, Prop->Flags );
				}
			}

			// Base component properties.
			for( CClass* C = Script->Base->GetClass(); C; C = C->Super )
			{
				for( Integer iProp=0; iProp<C->Properties.Num(); iProp++ )
				{
					CProperty* Prop = C->Properties[iProp];

					// Property item will add to list its self, also
					// add children and hide them as default.
					TmpArr[0] = Script->Base;
					new CPropertyItem( this, 0, *(TArray<FObject*>*)&TmpArr, Prop->Name, *Prop, Prop->Offset, Prop->Flags );
				}
			}

			// InstanceBuffer properties.
			for( Integer i=0; i<Script->Properties.Num(); i++ )
			{
				CProperty* Prop = Script->Properties[i];

				if( Prop->Flags & PROP_Editable )
					new CPropertyItem( this, 0, *(TArray<FObject*>*)&Objects, Prop->Name, *Prop, Prop->Offset, Prop->Flags );
			}

			// Components.
			for( Integer i=0; i<Script->Components.Num(); i++ )
			{
				TmpArr[0] = Script->Components[i];
				CComponentItem* Item = new CComponentItem( this, *(TArray<FObject*>*)&TmpArr );

				// Don't add empty component item.
				if( Item->Children.Num() == 0 )
				{
					Children.RemoveShift(Children.FindItem(Item));
					delete Item;
				}
			}

			// Set caption.
			Caption	= String::Format( L"Inspector [%s: %s]", *TopClass->Alt, *Objects[0]->GetName() );
		}
		else if( Objs[0]->IsA(FResource::MetaClass) )
		{
			// Its a list of resources.
			assert(Objs.Num() == 1);	

			for( Integer i=0; i<Objs.Num(); i++ )
				Objects.Push( Objs[i] );

			TopClass	= Objects[0]->GetClass();

			// Own properties.
			for( CClass* C = TopClass; C; C = C->Super )
			{
				for( Integer iProp=0; iProp<C->Properties.Num(); iProp++ )
				{
					CProperty* Prop = C->Properties[iProp];

					// Property item will add to list its self, also
					// add children and hide them as default.
					new CPropertyItem( this, 0, Objects, Prop->Alias, *Prop, Prop->Offset, Prop->Flags );
				}
			}

			// Set caption.
			Caption	= String::Format( L"Inspector [%s: %s]", *TopClass->Alt, *Objects[0]->GetName() );
		}
		else
		{
			// Something unsupported.
			error( L"Unsupported class \"%s\" to edit.", Objs[0]->GetClass()->Name );
		}
	}
	else
	{
		// No objects to edit.
		// Object inspector will blank.
		Caption	= L"Inspector";
	}

	// Organize the items.
	UpdateChildren();
}


//
// User drag something, delegate decision to the item.
//
void WObjectInspector::OnDragOver( void* Data, Integer X, Integer Y, Bool& bAccept )
{
	bAccept	= false;
	if( !bWaitForPick && Y > INSPECTOR_HEADER_SIZE )
	{
		Integer NewY;
		CInspectorItemBase* Item = GetItemAt( Y, NewY );

		if( Item )
			Item->DragOver( Data, X, Y, bAccept );
	}
}


//
// User drop something, delegate decision to the item.
// Lets item deal with it :3
//
void WObjectInspector::OnDragDrop( void* Data, Integer X, Integer Y )
{
	if( !bWaitForPick && Y > INSPECTOR_HEADER_SIZE )
	{
		Integer NewY;
		CInspectorItemBase* Item = GetItemAt( Y, NewY );

		if( Item )
			Item->DragDrop( Data, X, Y );
	}
}


//
// Entity has been picked.
//
void WObjectInspector::ObjectPicked( FEntity* Picked )
{
	if( !bWaitForPick || !WaitItem )
		return;

	// Compare scripts.
	CPropertyItem* Waiter = (CPropertyItem*)WaitItem;
	if( Picked && Waiter->TypeInfo.Script && Picked->Script != Waiter->TypeInfo.Script )
		Picked	= nullptr;

	// Set value.
	if( LevelPage )	LevelPage->Transactor->TrackEnter();
	{
		for( Integer i=0; i<WaitItem->Objects.Num(); i++ )
			*(FEntity**)((CPropertyItem*)WaitItem)->GetAddress(i) = Picked;
	}
	if( LevelPage )	LevelPage->Transactor->TrackLeave();

	// Notify.
	for( Integer i=0; i<WaitItem->Objects.Num(); i++ )
		WaitItem->Objects[i]->EditChange();

	// Reset it.
	bWaitForPick		= false;
	WaitColor			= nullptr;
	WaitItem			= nullptr;
	ScrollBar->bEnabled	= true;
	UnselectAll();
}


//
// Begin wait for pick.
//
void WObjectInspector::BeginWaitForPick( CInspectorItemBase* Waiter )
{
	assert(!bWaitForPick)

	WEditorPage* Page = (WEditorPage*)GEditor->EditorPages->GetActivePage();
	if( Page && Page->PageType == PAGE_Level )
	{
		WLevelPage* LevPag = (WLevelPage*)Page;

		bWaitForPick		= true;
		WaitItem			= Waiter;
		ScrollBar->bEnabled	= false;

		LevPag->SetTool( LEV_PickEntity );
	}
}


//
// Recompute offset for each children.
//
void WObjectInspector::UpdateChildren()
{
	// Count non hidden, required for scroll.
	Integer NonHidden = 0;
	for( Integer i=0; i<Children.Num(); i++ )
		if( !Children[i]->bHidden )
			NonHidden++;

	// Compute scrolling.
	Integer	NumVisible	= Floor( (Float)(Size.Height-Padding.Top)  / (Float)INSPECTOR_ITEM_HEIGHT );
	Integer NumInvis	= Max( NonHidden-NumVisible, 0);
	Integer Scroll		= (NumInvis * ScrollBar->Value * INSPECTOR_ITEM_HEIGHT) / 100;

	Integer WalkY	= INSPECTOR_HEADER_SIZE + 1 - Scroll;

	for( Integer i=0; i<Children.Num(); i++ )
	{
		CInspectorItemBase* Item	= Children[i];

		if( !Item->bHidden )
		{
			// Item is visible.
			Item->Top	= WalkY;
			WalkY		+= INSPECTOR_ITEM_HEIGHT-1;
		}
		else
		{
			// Item  is invisible.
			Item->Top	= 0;
		}
	}
}


//
// Return item at specified location, if no item found here
// return nullptr, LocalY is an Y value in items local coords.
//
CInspectorItemBase* WObjectInspector::GetItemAt( Integer ParentY, Integer& LocalY )
{ 
	for( Integer i=0; i<Children.Num(); i++ )
	{
		if( Children[i]->bHidden ) 
			continue;

		Integer Y1 = Children[i]->Top;
		Integer Y2 = Y1 + INSPECTOR_ITEM_HEIGHT;

		if( ParentY >= Y1 && ParentY <= Y2 )
		{
			// Found.
			LocalY	= ParentY - Y1;
			return Children[i];
		}
	}

	// Nothing found.
	return nullptr;
}


//
// Mouse move on inspector.
//
void WObjectInspector::OnMouseMove( EMouseButton Button, Integer X, Integer Y )
{
	if( !bWaitForPick && Y > INSPECTOR_HEADER_SIZE )
	{
		Integer NewY;
		CInspectorItemBase* Item = GetItemAt( Y, NewY );

		if( Item )
			Item->MouseMove( Button, X, NewY );	
	}
}


//
// Mouse down on inspector.
//
void WObjectInspector::OnMouseDown( EMouseButton Button, Integer X, Integer Y )
{
	if( !bWaitForPick && Y > INSPECTOR_HEADER_SIZE )
	{
		Integer NewY;
		CInspectorItemBase* Item = GetItemAt( Y, NewY );

		if( Item )
			Item->MouseDown( Button, X, NewY );	
	}
}


//
// Mouse up on inspector.
//
void WObjectInspector::OnMouseUp( EMouseButton Button, Integer X, Integer Y )
{
	if( !bWaitForPick && Y > INSPECTOR_HEADER_SIZE )
	{
		Integer NewY;
		CInspectorItemBase* Item = GetItemAt( Y, NewY );

		if( Item )
			Item->MouseUp( Button, X, NewY );	
	}
}


//
// Scroll inspector via mouse wheel.
//
void WObjectInspector::OnMouseScroll( Integer Delta )
{
	ScrollBar->Value	= Clamp
							( 
								ScrollBar->Value-Delta/120, 
								0, 
								100 
							);
	ScrollChange( this );
}


//
// Draw inspector and its children.
//
void WObjectInspector::OnPaint( CGUIRenderBase* Render )
{
	WContainer::OnPaint( Render );

	TPoint Base = ClientToWindow(TPoint(0, 0));

	// Draw frame.
	Render->DrawRegion
				(
					Base,
					Size,
					COLOR_INSPECTOR_BACKDROP,
					COLOR_INSPECTOR_BORDER,
					BPAT_Solid
				);
	
	// Draw header.
	Render->DrawRegion
				(
					Base,
					TSize( Size.Width, INSPECTOR_HEADER_SIZE ),
					GUI_COLOR_FORM_HEADER,//TColor( 0x33, 0x33, 0x33, 0xff ),
					COLOR_INSPECTOR_BORDER,
					BPAT_Diagonal

				);
	Render->DrawText
				( 
					TPoint( Base.X + 5, Base.Y+(INSPECTOR_HEADER_SIZE-Root->Font1->Height)/2 ), 
					Caption, 
					GUI_COLOR_TEXT, 
					Root->Font1 
				);

	// Draw items.
	Render->SetClipArea( Base + TPoint( 0, Padding.Top+2 ), TSize( Size.Width-12, Size.Height-Padding.Top-3 ) );
	for( Integer i=0; i<Children.Num(); i++ )
	{
		CInspectorItemBase* Item = Children[i];

		if	(	!Item->bHidden && 
				( Item->Top + INSPECTOR_ITEM_HEIGHT ) > INSPECTOR_HEADER_SIZE 
			)
		{
			Item->Paint( Base, Render );
		}

		// End of visible.
		if( Item->Top > Size.Height )
			break;
	}
}


//
// When inspector scrolled.
//
void WObjectInspector::ScrollChange( WWidget* Sender )
{
	UnselectAll();
	UpdateChildren();
}


//
// Some of the item widget changed.
//
void WObjectInspector::SomethingChange( WWidget* Sender )
{
	//!! not really good solution :(
	assert(Selected);
	((CPropertyItem*)Selected)->OnSomethingChange( Sender );
}


//
// Pick button has been clicked.
//
void WObjectInspector::PickClick( WWidget* Sender )
{
	//!! not really good solution :(
	assert(Selected);
	((CPropertyItem*)Selected)->OnPickClick( Sender );
}


//
// User just pick color.
//
void WObjectInspector::ColorSelected( WWidget* Sender )
{
	assert(WaitColor);
	CPropertyItem* Item = (CPropertyItem*)WaitColor;

	// Set new color for all objects.
	if( LevelPage )	LevelPage->Transactor->TrackEnter();
	{
		for( Integer i=0; i<Item->Objects.Num(); i++ )
			*(TColor*)Item->GetAddress(i)	= WColorChooser::SharedColor;
	}
	if( LevelPage )	LevelPage->Transactor->TrackLeave();

	WaitColor	= nullptr;
}


//
// Inspector has been resized.
//
void WObjectInspector::OnResize()
{
	// Clamp separator.
	Separator	= Clamp( Separator, 100, Size.Width-100 );

	// Reset focus.
	UnselectAll();

	// Update scroll.
	UpdateChildren();
}


//
// Unselect all items.
//
void WObjectInspector::UnselectAll()
{
	for( Integer i=0; i<Children.Num(); i++ )
		Children[i]->Unselect();

	Selected	= nullptr;
}


//
// Set an object to edit.
//
void WObjectInspector::SetEditObject( FObject* Obj )
{
	assert(Obj);
	TArray<FObject*> Tmp;
	Tmp.Push(Obj);
	SetEditObjects( Tmp );
}


//
// Count all references in inspector.
//
void WObjectInspector::CountRefs( CSerializer& S )
{
	Serialize( S, Objects );
	CLEANUP_ARR_NULL(Objects);

	// Also cleanup in children.
	for( Integer i=0; i<Children.Num(); i++ )
	{
		CInspectorItemBase* Item = Children[i];

		Serialize( S, Item->Objects );
		CLEANUP_ARR_NULL(Item->Objects);
	}

	// Maybe cleanup all.
	if( Objects.Num() == 0 )
	{
		TArray<FObject*> Tmp;
		SetEditObjects( Tmp );
	}
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/