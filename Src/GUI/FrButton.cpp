/*=============================================================================
    FrButton.cpp: A clickable widgets.
    Copyright Jul.2016 Vlad Gordienko.
=============================================================================*/

#include "GUI.h"

/*-----------------------------------------------------------------------------
    WPictureButton implementation.
-----------------------------------------------------------------------------*/
	
//
// Picture button constructor.
//
WPictureButton::WPictureButton( WContainer* InOwner, WWindow* InRoot )
	:	WButton( InOwner, InRoot ),
		Picture( nullptr ),
		Offset( 0, 0 ), 
		Scale( 30, 30 )
{
	Caption		= L"";
	SetSize( 32, 32 );
}


//
// Picture button paint.
//
void WPictureButton::OnPaint( CGUIRenderBase* Render )
{
	// Draw button stuff.
	WButton::OnPaint( Render );
	TPoint	Base	= ClientToWindow(TPoint::Zero);


	// Draw picture.
	if( Picture )
		Render->DrawPicture
				( 
					TPoint( Base.X+1+(Size.Width-Scale.Width)/2, Base.Y+1+(Size.Height-Scale.Height)/2 ),
					TSize( Scale.Width, Scale.Height ), 
					bEnabled ? 
						Offset : TPoint( Offset.X, Offset.Y+16 ), 
					Scale, 
					Picture 
				);
}


/*-----------------------------------------------------------------------------
    WCheckBox implementation.
-----------------------------------------------------------------------------*/

//
// Check box constructor.
//
WCheckBox::WCheckBox( WContainer* InOwner, WWindow* InRoot )
	:	WWidget( InOwner, InRoot ),
		bChecked( false ),
		bHold( false )
{
	SetSize( CHECKBOX_SIDE, CHECKBOX_SIDE );
}


//
// Mouse press checkbox.
//
void WCheckBox::OnMouseDown( EMouseButton Button, Integer X, Integer Y )
{
	WWidget::OnMouseDown( Button, X, Y );

	if( Button == MB_Left )
		bHold = true;
}


//
// Mouse hover checkbox.
//
void WCheckBox::OnMouseMove( EMouseButton Button, Integer X, Integer Y )
{			
	WWidget::OnMouseMove( Button, X, Y );

	if( X > CHECKBOX_SIDE || Y > CHECKBOX_SIDE || X < 0 || Y < 0 )
		bHold = false;
}


//
// Mouse click checkbox.
//
void WCheckBox::OnMouseUp( EMouseButton Button, Integer X, Integer Y )
{
	WWidget::OnMouseUp( Button, X, Y );

	if( bHold )		
		OnClick();

	bHold = false;	
}


//
// Checkbox paint.
//
void WCheckBox::OnPaint( CGUIRenderBase* Render ) 
{
	WWidget::OnPaint( Render );

	TPoint Base = ClientToWindow(TPoint(0, 0));
	TSize  TextSize = TSize( Root->Font1->TextWidth(*Caption), Root->Font1->Height );

	Render->DrawRegion
			( 
				Base, 
				TSize( CHECKBOX_SIDE, CHECKBOX_SIDE ),
				bHold ^ bChecked ? GUI_COLOR_BUTTON_PRESS : Root->Highlight == this ? GUI_COLOR_BUTTON_HIGHLIGHT : GUI_COLOR_BUTTON,
				GUI_COLOR_BUTTON_BORDER,
				BPAT_Solid
			);

	if( bChecked )
		Render->DrawPicture
				( 
					TPoint( Base.X+1, Base.Y+1 ), 
					TSize( CHECKBOX_SIDE-2, CHECKBOX_SIDE-2 ), 
					TPoint(4, 0), 
					TSize(11, 11),
					Root->Icons 
				);


	Render->DrawText
			( 
				TPoint( Base.X + CHECKBOX_SIDE + 6, 
				Base.Y + (CHECKBOX_SIDE - TextSize.Height)/2 ),
				Caption,
				GUI_COLOR_TEXT, 
				Root->Font1 
			);
}


//
// Checkbox click notification.
//
void WCheckBox::OnClick()
{
	bChecked = !bChecked;
	EventClick( this );
}


/*-----------------------------------------------------------------------------
    WLinkLabel implementation.
-----------------------------------------------------------------------------*/

//
// Link label constructor.
//
WLinkLabel::WLinkLabel( WContainer* InOwner, WWindow* InRoot )
	:	WWidget( InOwner, InRoot ),
		Color( COLOR_SteelBlue )
{
	Caption = L"LinkLabel";
	Cursor	= CR_HandPoint;
	SetSize( 64, 16 );
}


//
// Redraw link label.
//
void WLinkLabel::OnPaint( CGUIRenderBase* Render ) 
{ 
	WWidget::OnPaint( Render );

	Render->DrawText
			( 
				ClientToWindow(TPoint(0, 0)),
				Caption,
				Root->Highlight == this ? Color + COLOR_Highlight1 : Color, 
				Root->Font1 
			);
}


//
// Mouse press link.
//
void WLinkLabel::OnMouseDown( EMouseButton Button, Integer X, Integer Y ) 
{ 
	WWidget::OnMouseDown( Button, X, Y );

	if( Button = MB_Left )
		OnClick();
}


//
// Link label click notification.
//
void WLinkLabel::OnClick()
{
	EventClick( this );
}


/*-----------------------------------------------------------------------------
    WButton implementation.
-----------------------------------------------------------------------------*/

//
// Button constructor.
//
WButton::WButton( WContainer* InOwner, WWindow* InRoot )
	:	WWidget( InOwner, InRoot ),
		bHold( false ),
		bDown( false ),
		bToggle( false )
{
	Caption = L"Button";
	SetSize( 60, 25 );
}


//
// Redraw button.
//
void WButton::OnPaint( CGUIRenderBase* Render ) 
{
	WWidget::OnPaint( Render );

	TPoint Base = ClientToWindow(TPoint(0, 0));
	TSize  TextSize = TSize( Root->Font1->TextWidth(*Caption), Root->Font1->Height );

	Render->DrawRegion
			( 
				Base, 
				Size,
				bHold || bDown ? GUI_COLOR_BUTTON_PRESS : Root->Highlight == this ? GUI_COLOR_BUTTON_HIGHLIGHT : GUI_COLOR_BUTTON,
				GUI_COLOR_BUTTON_BORDER,
				BPAT_Solid 
			);

	Render->DrawText
			( 
				TPoint(	Base.X + (Size.Width - TextSize.Width)/2, 
			            Base.Y + (Size.Height - TextSize.Height)/2 ),
				Caption,
				bEnabled ? GUI_COLOR_TEXT : GUI_COLOR_TEXT_OFF, 
				Root->Font1 
			);
}


//
// Mouse hover above button.
//
void WButton::OnMouseMove( EMouseButton Button, Integer X, Integer Y )
{		
	WWidget::OnMouseMove( Button, X, Y );

	if( X > Size.Width || Y > Size.Height || X < 0 || Y < 0 )
		bHold = false;
}


//
// Mouse press button.
//
void WButton::OnMouseDown( EMouseButton Button, Integer X, Integer Y )
{
	WWidget::OnMouseDown( Button, X, Y );

	if( Button == MB_Left )
		bHold = true;
}


//
// Mouse click button.
//
void WButton::OnMouseUp( EMouseButton Button, Integer X, Integer Y )
{
	WWidget::OnMouseUp( Button, X, Y );

	if( bHold )
		OnClick();
		
	bHold = false;	
}


//
// Button click notification.
//
void WButton::OnClick()
{
	if( bToggle )
		bDown = !bDown;

	EventClick( this );
}


/*-----------------------------------------------------------------------------
    WToolBar implementation.
-----------------------------------------------------------------------------*/

//
// Toolbar constructor.
//
WToolBar::WToolBar( WContainer* InOwner, WWindow* InRoot )
	:	WContainer( InOwner, InRoot ),
		Elements()
{
	Align	= AL_Top;
	SetSize( 150, 28 );
}


//
// Add a new button, u can pass nullptr,
// its will be separator.
//
void WToolBar::AddElement( WWidget* InElem )
{
	Elements.Push(InElem);
}


//
// Toolbar redraw.
//
void WToolBar::OnPaint( CGUIRenderBase* Render )
{
	WContainer::OnPaint( Render );

	TPoint Base = ClientToWindow(TPoint(0, 0));

	// Draw area.
	Render->DrawRegion( Base, Size, GUI_COLOR_PANEL, GUI_COLOR_PANEL, BPAT_Solid );

	// Set buttons locations & draw separators.
	Integer XWalk = 2;
	for( Integer i=0; i<Elements.Num(); i++ )
	{
		WWidget* W = Elements[i];

		if( W )
		{
			// Regular button.
			W->Location = TPoint( XWalk, (Size.Height - W->Size.Height) / 2 );
			XWalk += W->Size.Width - 1;
		}
		else
		{
			// Separator.
			Render->DrawRegion
						( 
							TPoint( Base.X+XWalk+2, Base.Y+2 ), 
							TSize( 2, Size.Height-4 ),
							GUI_COLOR_MENU_SELECTED, 
							GUI_COLOR_MENU_SELECTED, 
							BPAT_Solid 
						);

			XWalk += 5;
		}
	}
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/