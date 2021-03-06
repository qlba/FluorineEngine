/*=============================================================================
    FrPage.h: Abstract editor page.
    Copyright Jul.2016 Vlad Gordienko.
=============================================================================*/

/*-----------------------------------------------------------------------------
    Declaration.
-----------------------------------------------------------------------------*/

//
// An editor page type.
//
enum EPageType
{
	PAGE_None,			// Bad page type.
	PAGE_Hello,			// It's an intro page.
	PAGE_Bitmap,		// Bitmap edit page.
	PAGE_Level,			// Level edit page.
	PAGE_Animation,		// Animation edit page.
	PAGE_Script,		// Script edit page.
	PAGE_Play,			// Level testing page.
	PAGE_MAX
};


//
// Pages colors.
//
#define PAGE_COLOR_HELLO		TColor( 0x66, 0x66, 0x66, 0xff )
#define PAGE_COLOR_BITMAP		TColor( 0x00, 0x00, 0xcc, 0xff )
#define PAGE_COLOR_LEVEL		TColor( 0xcc, 0x80, 0x00, 0xff )
#define PAGE_COLOR_ANIMATION	TColor( 0xcc, 0xcc, 0x00, 0xff )
#define PAGE_COLOR_SCRIPT		TColor( 0x00, 0xcc, 0x00, 0xff )
#define PAGE_COLOR_PLAY			TColor( 0x00, 0xcc, 0xcc, 0xff )


/*-----------------------------------------------------------------------------
    WEditorPage.
-----------------------------------------------------------------------------*/

//
// An abstract editor page.
//
class WEditorPage: public WTabPage
{
public:
	// Variables.
	EPageType		PageType;

	// Constructors.
	WEditorPage( WContainer* InOwner, WWindow* InRoot )
		:	WTabPage( InOwner, InRoot ),
			PageType( PAGE_None )
	{}

	// WEditorPage interface.
	virtual void RenderPageContent( CCanvas* Canvas ){}
	virtual void TickPage( Float Delta ){}
	virtual void Undo(){}
	virtual void Redo(){}
	virtual FResource* GetResource(){ return nullptr; }
};


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/