/*=============================================================================
    FrEdit.cpp: Text/Numbers edit classes.
    Copyright Jul.2016 Vlad Gordienko.
=============================================================================*/

#include "GUI.h"

/*-----------------------------------------------------------------------------
    WEdit implementation.
-----------------------------------------------------------------------------*/

//
// Edit box constructor.
//
WEdit::WEdit( WContainer* InOwner, WWindow* InRoot )
	:	WWidget( InOwner, InRoot ),
		bReadOnly( false ),
		EditType( EDIT_String ),		
		CaretBegin( 0 ),
		CaretEnd( 0 ),
		OldInteger( 0 ),
		OldFloat( 0.f ),
		ScrollX( 0 )
{
	// Precompute the size of single character.
	CharSize = TSize
				( 
					Root->Font2->TextWidth( L"A" ), 
					Root->Font2->Height 
				);
}


//
// Set a new text, and optional notify, via flag.
//
void WEdit::SetText( String NewText, Bool bNotify )
{
	Text	= NewText;
	
	if( bNotify )
		OnChange();

	CaretBegin	= CaretEnd	= 0;
	ScrollToCaret();
}


//
// Convert caret location to pixels.
//
Integer WEdit::CaretToPixel( Integer C )
{
	return (C-ScrollX) * CharSize.Width;	
}


//
// Convert pixel value to caret, result
// will be clamped to cover text.
//
Integer WEdit::PixelToCaret( Integer X )
{
	return Clamp
			( 
				Round((Float)X / (Float)CharSize.Width)+ScrollX, 
				0, 
				Text.Len() 
			);	
}


//
// Select the entire text.
//
void WEdit::SelectAll()
{
	CaretBegin = 0;
	CaretEnd = Text.Len();	
}


//
// Clear the selected chunk of text.
//
void WEdit::ClearSelected()
{
	if( CaretBegin == CaretEnd ) 
		return;

	Store();
	Text = String::Delete( Text, CaretBegin, CaretEnd-CaretBegin );
	CaretEnd = CaretBegin = Clamp( CaretBegin, 0, Text.Len() );
	//ScrollX -= 2;
	ScrollToCaret();
	OnChange();
}


//
// Edit lost focus.
//
void WEdit::OnDeactivate()
{
	WWidget::OnDeactivate();

	if( !bReadOnly )
	{
		// Fix numeric values, in case of bad input.
		if( EditType == EDIT_Integer )
		{
			// Fix integer.
			Integer V;
			if( !Text.ToInteger( V ) )
			{
				Text = String::Format( L"%d", OldInteger );
				OnChange();
			}
		}
		else if( EditType == EDIT_Float )
		{
			// Fix float.
			Float V;
			if( !Text.ToFloat( V ) )
			{
				Text = String::Format( L"%f", OldFloat );
				OnChange();	
			}
		}	
	}

	CaretBegin	= CaretEnd = 0;
}


//
// Mouse down edit.
//
void WEdit::OnMouseDown( EMouseButton Button, Integer X, Integer Y )
{
	WWidget::OnMouseDown( Button, X, Y );

	if( Button == MB_Left )
		CaretBegin = CaretEnd = PixelToCaret( X );
}


//
// Mouse button up.
//
void WEdit::OnMouseUp( EMouseButton Button, Integer X, Integer Y )
{
	WWidget::OnMouseUp( Button, X, Y );

	// Fix selection range order.
	if( CaretBegin > CaretEnd )
		Exchange( CaretBegin, CaretEnd );
}


//
// Mouse move.
//
void WEdit::OnMouseMove( EMouseButton Button, Integer X, Integer Y )
{
	WWidget::OnMouseMove( Button, X, Y );

	if( Button == MB_Left )
		CaretEnd = PixelToCaret( X );

	ScrollToCaret();
}


//
// Edit double click.
//
void WEdit::OnDblClick( EMouseButton Button, Integer X, Integer Y )
{
	WWidget::OnDblClick( Button, X, Y );

	// Select entire text.
	if( Button == MB_Left )
		SelectAll();
}


//
// User press button.
//
void WEdit::OnKeyDown( Integer Key )
{	
	WWidget::OnKeyDown( Key );	

	if( Key == 0x25 )
	{
		// <Left> button.
		CaretBegin = CaretEnd = Clamp
								( 
									CaretBegin == CaretEnd ? CaretBegin-1 : CaretBegin,
									0,
									Text.Len() 
								);
	} 
	else if( Key == 0x27 )
	{
		// <Right> button.
		CaretBegin = CaretEnd = Clamp
								( 
									CaretBegin == CaretEnd ? CaretEnd+1 : CaretEnd,
									0,
									Text.Len() 
								);
	}
	else if( Key == 0x2e )
	{
		// <Del> button.
		if( !bReadOnly )
		{
			if( CaretBegin != CaretEnd )
			{
				ClearSelected();
			}
			else if( CaretEnd < Text.Len() )
			{
				Store();
				Text = String::Delete( Text, CaretEnd, 1 );
				CaretBegin = CaretEnd = CaretEnd + 0;
				OnChange();
			}	
		}
	}
	else if( Root->bCtrl && Key == L'A' )
	{
		// <Ctrl> + <A>.
		SelectAll();
	}
	else if( Root->bCtrl && (Key == L'X' || Key == L'C' ) )
	{
		// Copy or Cut.
		Integer NumChars = CaretEnd - CaretBegin;
		if( NumChars )
		{
			Char* TxtToCpy = new Char[NumChars+1]();
			MemCopy( TxtToCpy, &Text[CaretBegin], NumChars*sizeof(Char) );
			GPlat->ClipboardCopy( TxtToCpy );
			delete[] TxtToCpy;
		}

		// Clear text after cut op.
		if( Key == L'X' )
		{
			ClearSelected();
			OnChange();
		}
	}
	else if( Root->bCtrl && Key == L'V' )
	{
		// Paste a text.
		ClearSelected();

		// Read text.
		String ClipTxt = GPlat->ClipboardPaste();
		if( !ClipTxt )
			return;

		// Store rest of the line.
		String Rest = String::Copy( Text, CaretBegin, Text.Len()-CaretBegin );
		Text	= String::Delete( Text, CaretBegin, Text.Len()-CaretBegin );

		// Insert it.
		for( Integer i=0; i<ClipTxt.Len(); i++ )
		{
			Char C[2] = { ClipTxt[i], 0 };

			// Skip some symbols.
			if( *C=='\r' || *C=='\n' || *C=='\t' )	
				continue;  

			Text += C;
		}

		CaretEnd	= CaretBegin	= Text.Len();

		// Insert the rest.
		Text += Rest;
		OnChange();
	}

	ScrollToCaret();
}


//
// User type char.
//
void WEdit::OnCharType( Char TypedChar )
{	
	WWidget::OnCharType( TypedChar );
	
	// don't type, if <Ctrl> used as part of command.
	if( Root->bCtrl )
		return;

	// Don't modify if not allowed.
	if( bReadOnly )
		return;

	if( TypedChar == 0x08 )
	{
		// <Backspace>.
		if( CaretBegin != CaretEnd )
		{
			ClearSelected();
		}
		else if( CaretBegin > 0 )
		{
			Store();
			Text = String::Delete( Text, CaretBegin-1, 1 );
			CaretBegin = CaretEnd = CaretBegin - 1;
			OnChange();

			if( Text.Len() >= Size.Width/CharSize.Width )
				ScrollX--;
		}	
	}
	else if( TypedChar == 0x0d )
	{
		// <Enter>.
		OnAccept();
	}
	else
	{
		// Any character.
		if( CaretBegin != CaretEnd )
			ClearSelected();

		// Filter a little.
		if( EditType == EDIT_Integer )
			if( !((TypedChar>=L'0' && TypedChar <= L'9')||(TypedChar==L'-')||(TypedChar==L'+')) )
				return;

		if( EditType == EDIT_Float )
			if( !((TypedChar>=L'0' && TypedChar <= L'9')||(TypedChar==L'-')||(TypedChar==L'+')||(TypedChar==L'.')) )
				return;

		Store();

		// Append.
		Char tmp[2] = { TypedChar, '\0' };
		Text = String::Copy( Text, 0, CaretBegin ) + tmp + String::Copy( Text, CaretBegin, Text.Len()-CaretBegin );
		CaretBegin = CaretEnd = CaretBegin + 1;

		OnChange();
	}

	ScrollToCaret();
}


//
// Draw edit box.
//
void WEdit::OnPaint( CGUIRenderBase* Render )
{
	WWidget::OnPaint( Render );

	// Pick cursor, not sure its should be here.
	Cursor = bReadOnly ? CR_Arrow : CR_IBeam;

	TPoint Base = ClientToWindow(TPoint(0, 0));
	Integer TextY = Base.Y + (Size.Height - CharSize.Height) / 2;

	Render->DrawRegion
				( 
					Base, 
					Size, 
					bEnabled ? GUI_COLOR_EDIT : GUI_COLOR_EDIT_OFF,
					bEnabled ? GUI_COLOR_EDIT : GUI_COLOR_EDIT_OFF,
					BPAT_Solid 
				);	

	// Clip text and selection.
	Render->SetClipArea
					( 
						TPoint( Base.X+1, TextY ), 
						TSize( Size.Width-2, CharSize.Height ) 
					);

	if( IsFocused() )
	{
		if( CaretBegin == CaretEnd )
			Render->DrawText
						( 
							TPoint( Base.X + CaretToPixel(CaretBegin) - 2, TextY ), 
							L"|", 
							1,
							GUI_COLOR_TEXT, 
							Root->Font2 
						);
		else
			Render->DrawRegion
						( 
							TPoint(Base.X+CaretToPixel(CaretBegin)+2 , Base.Y), 
							TSize((CaretToPixel(CaretEnd)-CaretToPixel(CaretBegin)), Size.Height ), 
							GUI_COLOR_EDIT_MARK, 
							GUI_COLOR_EDIT, 
							BPAT_Solid 
						);
	}

	// Draw text.
	Render->DrawText
				( 
					TPoint( Base.X + 2 - ScrollX*CharSize.Width, TextY ), 
					Text, 
					bEnabled ? GUI_COLOR_TEXT : GUI_COLOR_TEXT_OFF, 
					Root->Font2 
				);
}


//
// Text has been changed.
//
void WEdit::OnChange()
{
	// Notify.
	EventChange( this );
}	


//
// Lets accept this value.
//
void WEdit::OnAccept()
{
	EventAccept( this );
}


//
// Store values to restore when bad input.
//
void WEdit::Store()
{
	if( EditType == EDIT_Integer )
	{
		Integer V;
		if( Text.ToInteger( V, 0 ) )
			OldInteger	= V;
	}
	else if( EditType == EDIT_Float )
	{
		Float V;
		if( Text.ToFloat( V, 0.f ) )
			OldFloat = V;
	}
}


//
// Scroll text to fit caret.
//
void WEdit::ScrollToCaret()
{
	Integer NumVis	= Size.Width / CharSize.Width;

	// Scroll from the current location.
	while( CaretEnd < ScrollX )				ScrollX--;
	while( CaretEnd > ScrollX+NumVis )		ScrollX++;
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/