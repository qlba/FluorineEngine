/*=============================================================================
    FrWidges.h: Dump of widgets.
    Copyright Jul.2016 Vlad Gordienko.
=============================================================================*/

/*-----------------------------------------------------------------------------
    WPanel.
-----------------------------------------------------------------------------*/

//
// Simple panel.
//
class WPanel: public WContainer
{
public:
	// Variables.
	Bool		bDrawEdges;

	// WWidget interface.
	WPanel( WContainer* InOwner, WWindow* InRoot )
		:	WContainer( InOwner, InRoot ),
			bDrawEdges( true )
	{
		SetSize( 256, 256 );
	}
	void OnPaint( CGUIRenderBase* Render ) 
	{
		WWidget::OnPaint( Render );

		Render->DrawRegion
						( 
							ClientToWindow(TPoint::Zero),
			                Size,
							GUI_COLOR_PANEL,
							bDrawEdges ? GUI_COLOR_PANEL_BORDER : GUI_COLOR_PANEL,
							BPAT_Solid 
						);
	}
};


/*-----------------------------------------------------------------------------
    WStatusBar.
-----------------------------------------------------------------------------*/

// Status panel side.
enum EStatusPanelSide
{
	SPS_Left,
	SPS_Right
};


//
// A bottom status line.
//
class WStatusBar: public WWidget
{
public:
	// A status panel.
	struct TStatusPanel
	{
	public:
		EStatusPanelSide	Side;
		String				Text;
		Integer				Width;
	};

	// Variables.
	TArray<TStatusPanel>	Panels;
	TColor					Color;

	// WStatusBar interface.
	WStatusBar( WContainer* InOwner, WWindow* InRoot )
		:	WWidget( InOwner, InRoot ),
			Panels(),
			Color( COLOR_SteelBlue )
	{
		SetSize( 100, 23 );
		Align	= AL_Bottom;
	}

	// Add a new panel to the list.
	Integer AddPanel( String InText, Integer InWidth = 100, EStatusPanelSide InSide = SPS_Left )
	{
		TStatusPanel Panel;
		Panel.Side	= InSide;
		Panel.Text	= InText;
		Panel.Width	= InWidth;
		return Panels.Push(Panel);
	}

	// WWidget interface.
	void OnPaint( CGUIRenderBase* Render ) 
	{
		WWidget::OnPaint( Render );

		TPoint Base = ClientToWindow( TPoint( 0, 0 ) );
		Integer MinX = 5, MaxX = Size.Width;
		Render->DrawRegion( Base, Size, Color, Color, BPAT_Solid );

		for( Integer i=0; i<Panels.Num(); i++)
		{
			TStatusPanel& P	= Panels[i];
			TSize  TextSize = TSize( Root->Font1->TextWidth(*P.Text), Root->Font1->Height );

			if( P.Side == SPS_Left )
			{
				Render->DrawText
							(
								TPoint( MinX, Base.Y+(Size.Height-TextSize.Height)/2 ), 
								P.Text, 
								GUI_COLOR_TEXT, 
								Root->Font1 
							);

				MinX += P.Width;
			}
			else
			{
				MaxX -= P.Width;

				Render->DrawText
							(	
								TPoint( MaxX, Base.Y+(Size.Height-TextSize.Height)/2 ), 
								P.Text, 
								GUI_COLOR_TEXT, 
								Root->Font1 
							);			
			}
		}
	}
};


/*-----------------------------------------------------------------------------
    WSlider.
-----------------------------------------------------------------------------*/

// A slider orientation.
enum ESliderOrientation
{
	SLIDER_Horizontal,
	SLIDER_Vertical
};


//
// A slider or scrollbar.
//
class WSlider: public WWidget
{
public:
	// Variables.
	TNotifyEvent        EventChange;
	ESliderOrientation	Orientation;
	Integer				Value;

	// WWidget interface.
	WSlider( WContainer* InOwner, WWindow* InRoot )
		:	WWidget( InOwner, InRoot ),
			Value(0),
			Orientation( SLIDER_Horizontal )
	{
		SetSize( 160, 12 );
	}
	void OnMouseDown( EMouseButton Button, Integer X, Integer Y ) 
	{ 
		WWidget::OnMouseDown( Button, X, Y );

		if( Button == MB_Left )
		{	
			Value = Orientation == SLIDER_Horizontal ? (X * 100 / Size.Width) : (Y * 100 / Size.Height);
			Value = Clamp( Value, 0, 100 );
			OnChange();
		}
	}
	void OnMouseMove( EMouseButton Button, Integer X, Integer Y ) 
	{ 
		WWidget::OnMouseMove( Button, X, Y );

		if( Button == MB_Left )
		{	
			Value = Orientation == SLIDER_Horizontal ? (X * 100 / Size.Width) : (Y * 100 / Size.Height);
			Value = Clamp( Value, 0, 100 );
			OnChange();
		}
	}
	void OnPaint( CGUIRenderBase* Render ) 
	{
		WWidget::OnPaint( Render );
		TPoint Base = ClientToWindow( TPoint(0, 0) );

		Render->DrawRegion
						( 
							Base,
			                Size,
							GUI_COLOR_SLIDER_BG,
							GUI_COLOR_SLIDER_BORDER, 
							BPAT_Solid 
						);

		if( Orientation == SLIDER_Horizontal )
			Render->DrawRegion
							( 
								TPoint( Base.X + Value*(Size.Width-Size.Height)/100, Base.Y ),
			                    TSize( Size.Height, Size.Height ),
								GUI_COLOR_SLIDER,
								GUI_COLOR_SLIDER,
								BPAT_Solid 
							);
		else
			Render->DrawRegion
							( 
								TPoint( Base.X, Base.Y + Value*(Size.Height-Size.Width)/100 ),
			                    TSize( Size.Width, Size.Width ),
								GUI_COLOR_SLIDER,
								GUI_COLOR_SLIDER,
								BPAT_Solid 
							);
	}

	// WSlider interface.
	void SetOrientation( ESliderOrientation Or )
	{
		Orientation = Or;
	}
	void SetValue( Integer NewValue )
	{
		Value = Clamp( NewValue, 0, 100 );
		OnChange();
	}
	virtual void OnChange()
	{
		EventChange( this );
	}
};


/*-----------------------------------------------------------------------------
    WLabel.
-----------------------------------------------------------------------------*/

//
// A simple label.
//
class WLabel: public WWidget
{
public:
	// WWidget interface.
	WLabel( WContainer* InOwner, WWindow* InRoot )
		:	WWidget( InOwner, InRoot )
	{
		Caption = L"Label";
		SetSize( 80, 25 );
	}
	void OnPaint( CGUIRenderBase* Render ) 
	{ 
		WWidget::OnPaint( Render );		

		Render->DrawText
					( 
						ClientToWindow(TPoint::Zero),
						Caption,
						bEnabled ? GUI_COLOR_TEXT : GUI_COLOR_TEXT_OFF, 
						Root->Font1 
					);
	}
};


/*-----------------------------------------------------------------------------
    WSeparator.
-----------------------------------------------------------------------------*/

//
// A separator - used to separate widgets. Just
// set-up appropriate size and align side.
//
class WSeparator: public WWidget
{
public:
	// WWidget interface.
	WSeparator( WContainer* InOwner, WWindow* InRoot )
		:	WWidget( InOwner, InRoot )
	{
		SetSize( 10, 10 );
	}
	void OnPaint( CGUIRenderBase* Render ) 
	{
		WWidget::OnPaint( Render );

		Render->DrawRegion
						( 
							ClientToWindow(TPoint::Zero),
			                Size,
							GUI_COLOR_PANEL,
							GUI_COLOR_PANEL,
							BPAT_Solid 
						);
	}
};


/*-----------------------------------------------------------------------------
    WProgressBar.
-----------------------------------------------------------------------------*/

//
// A bar used to show some progress.
//
class WProgressBar: public WWidget
{
public:
	// Variables.
	Integer		Value;
	TColor		Color;

	// WProgressBar interface.
	WProgressBar( WContainer* InOwner, WWindow* InRoot )
		:	WWidget( InOwner, InRoot ),
			Value( 0 ),
			Color(GUI_COLOR_PROGBAR_VAL)
	{
		SetSize( 300, 11 );
	}
	void SetValue( Integer NewValue )
	{
		Value	= Clamp( NewValue, 0, 100 );
	}
	void ZeroValue()
	{
		SetValue( 0 );
	}

	// WWidgget interface.
	void OnPaint( CGUIRenderBase* Render )
	{
		WWidget::OnPaint( Render );
		TPoint	Base	= ClientToWindow(TPoint::Zero);

		Render->DrawRegion
		(
			Base,
			Size,
			GUI_COLOR_PROGBAR_BG,
			GUI_COLOR_PROGBAR_BORDER,
			BPAT_Solid
		);
		Render->DrawRegion
		(
			TPoint( Base.X+2, Base.Y+2 ),
			TSize( (Size.Width-4)*Value/100, Size.Height-4 ),
			Color,
			Color,
			BPAT_Solid
		);
	}
};


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/