/*=============================================================================
    FrKeyEdit.h: Keyframe editor dialog.
    Copyright Aug.2016 Vlad Gordienko.
=============================================================================*/

/*-----------------------------------------------------------------------------
    WKeyframeEditor.
-----------------------------------------------------------------------------*/

//
// Keyframe editor.
//
class WKeyframeEditor: public WForm, public CRefsHolder
{
public:
	// Variables.
	WLabel*				FrameLabel;
	WButton*			AddButton;
	WButton*			RemoveButton;
	WButton*			PrevButton;
	WButton*			NextButton;

	// Edit variables.
	FEntity*			Entity;
	FKeyframeComponent*	Keyframe;
	Integer				iFrame;

	// WKeyframeEditor.
	WKeyframeEditor( WContainer* InOwner, WWindow* InRoot )
		:	WForm( InOwner, InRoot ),
			Entity( nullptr ),
			Keyframe( nullptr ),
			iFrame( -1 )
	{
		// Initialize own fields.
		bCanClose			= false;
		bSizeableH			= false;
		bSizeableW			= false;
		Caption				= L"Keyframe";
		SetSize( 194, 98 );

		// Controls.
		AddButton						= new WButton( this, Root );
		AddButton->Caption				= L"+";
		AddButton->Tooltip				= L"Add New Key";									
		AddButton->Location				= TPoint( 85, 24 );
		AddButton->EventClick			= WIDGET_EVENT(WKeyframeEditor::ButtonAddClick);
		AddButton->SetSize( 30, 20 );

		RemoveButton					= new WButton( this, Root );
		RemoveButton->Caption			= L"-";
		RemoveButton->Tooltip			= L"Remove Key";
		RemoveButton->Location			= TPoint( 85, 74 );
		RemoveButton->EventClick		= WIDGET_EVENT(WKeyframeEditor::ButtonRemoveClick);
		RemoveButton->SetSize( 30, 20 );

		PrevButton						= new WButton( this, Root );
		PrevButton->Caption				= L"<<";
		PrevButton->Tooltip				= L"Goto Prev Key";
		PrevButton->Location			= TPoint( 4, 50 );
		PrevButton->EventClick			= WIDGET_EVENT(WKeyframeEditor::ButtonPrevClick);			
		PrevButton->SetSize( 30, 20 );

		NextButton						= new WButton( this, Root );
		NextButton->Caption				= L">>";
		NextButton->Tooltip				= L"Goto Next Key";
		NextButton->Location			= TPoint( 160, 50 );
		NextButton->EventClick			= WIDGET_EVENT(WKeyframeEditor::ButtonNextClick);
		NextButton->SetSize( 30, 20 );

		FrameLabel						= new WLabel( this, InRoot );
		FrameLabel->Caption				= L"XX/XX";
		FrameLabel->Location			= TPoint( 80, 50 );

		SetButtonEnabled( false );
		Hide();
	}
	void SetButtonEnabled( Bool InbEnabled )
	{
		AddButton->bEnabled		= InbEnabled;
		RemoveButton->bEnabled	= InbEnabled;
		PrevButton->bEnabled	= InbEnabled;
		NextButton->bEnabled	= InbEnabled;
	}
	void SetEntity( FEntity* InEntity )
	{
		if( InEntity )
		{
			// Find valid keyframe.
			Entity	= InEntity;
			Keyframe = nullptr;
			for( Integer i=0; i<InEntity->Components.Num(); i++ )
				if( InEntity->Components[i]->IsA(FKeyframeComponent::MetaClass) )
				{
					Keyframe	= (FKeyframeComponent*)InEntity->Components[i];
					break;
				}

			if( !Keyframe )
				goto Undefined;

			iFrame		= -1;
			Caption		= String::Format( L"Keyframe [%s]", *Entity->GetName() );
			UpdateLabel();
			SetButtonEnabled( true );
		}
		else
		{
			// Set undefined entity.
		Undefined:
			iFrame				= -1;
			Caption				= L"Keyframe";
			FrameLabel->Caption	= L"XX/XX";
			Entity				= nullptr;
			Keyframe			= nullptr;
			SetButtonEnabled( false );
		}
	}
	void UpdateLabel()
	{
		FrameLabel->Caption	= String::Format
										( 
											L"%02d/%02d", 
											iFrame+1, 
											Keyframe->Points.Num() 
										);
	}

	// Notifications.
	void ButtonAddClick( WWidget* Sender )
	{
		if( Keyframe )
		{
			// Add new key.
			Keyframe->Points.SetNum(Keyframe->Points.Num()+1);
			Keyframe->Points.Last().Location	= Entity->Base->Location;
			Keyframe->Points.Last().Rotation	= Entity->Base->Rotation;
			Keyframe->Points.Last().bCCW		= true;

			// Goto new key.
			iFrame	= Keyframe->Points.Num()-1;
			UpdateLabel();
		}
	}
	void ButtonNextClick( WWidget* Sender )
	{
		if( Keyframe )
		{
			if( iFrame >= Keyframe->Points.Num()-1 )
				return;
			iFrame++;
			if( iFrame > -1 )
			{
				// Goto valid frame.
				Entity->Base->Location	= Keyframe->Points[iFrame].Location;
				Entity->Base->Rotation	= Keyframe->Points[iFrame].Rotation;
			}
			else
			{
				// Goto special '0' key.
			}
			UpdateLabel();
		}
	}
	void ButtonPrevClick( WWidget* Sender )
	{
		if( Keyframe )
		{
			if( iFrame == -1 )
				return;
			iFrame--;
			if( iFrame > -1 )
			{
				// Goto valid frame.
				Entity->Base->Location	= Keyframe->Points[iFrame].Location;
				Entity->Base->Rotation	= Keyframe->Points[iFrame].Rotation;
			}
			else
			{
				// Goto special '0' key.
			}
			UpdateLabel();
		}
	}
	void ButtonRemoveClick( WWidget* Sender )
	{
		if( Keyframe )
		{
			if( Keyframe->Points.Num() == 0 )
				return;
			if( iFrame == -1 )
			{
				// Remove last frame.
				Keyframe->Points.SetNum(Keyframe->Points.Num()-1);
			}
			else
			{
				// Remove selected key.
				Keyframe->Points.RemoveShift(iFrame);
			}
			iFrame	= -1;
			UpdateLabel();
		}
	}

	// WForm interface.
	void Show( Integer X = 0, Integer Y = 0 )
	{
		WForm::Show( X, Y );
		SetEntity( nullptr );
	}
	void Hide()
	{
		WForm::Hide();
		SetEntity( nullptr );
	}

	// CRefsHolder interface.
	void CountRefs( CSerializer& S )
	{
		Serialize( S, Entity );
		Serialize( S, Keyframe );
		SetEntity( Entity );
	}
};


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/