/*=============================================================================
    FrThings.cpp: Various components implementation.
    Copyright Jun.2016 Vlad Gordienko.
=============================================================================*/

#include "Engine.h"

/*-----------------------------------------------------------------------------
    FLightComponent implementation.
-----------------------------------------------------------------------------*/

//
// Light constructor.
//
FLightComponent::FLightComponent()
	:	CRenderAddon( this ),
		FExtraComponent()
{
	bEnabled	= true;
	LightType	= LIGHT_Steady;
	LightFunc	= LF_Multiplicative;
	Radius		= 8.f;
	Brightness	= 1.f;
	Color		= COLOR_White;
}


//
// Light destructor.
//
FLightComponent::~FLightComponent()
{
	// Remove from lights db.
	com_remove(Lights);
}


//
// Initialize light for level.
//
void FLightComponent::InitForEntity( FEntity* InEntity )
{
	FExtraComponent::InitForEntity(InEntity);
	com_add(Lights);
	com_add(RenderObjects);
}


//
// When some field changed.
//
void FLightComponent::EditChange()
{
	FExtraComponent::EditChange();

	Radius		= Clamp( Radius, 1.f, MAX_LIGHT_RADIUS );
	Brightness	= Clamp( Brightness, 0.f, 10.f );
}


//
// Light serialization.
//
void FLightComponent::SerializeThis( CSerializer& S )
{
	FExtraComponent::SerializeThis( S );
	CRenderAddon::SerializeAddon( S );
	
	Serialize( S, bEnabled );
	SerializeEnum( S, LightType );
	SerializeEnum( S, LightFunc );
	Serialize( S, Radius );
	Serialize( S, Brightness );
}


//
// Light import.
//
void FLightComponent::Import( CImporterBase& Im )
{
	FExtraComponent::Import( Im );
	CRenderAddon::ImportAddon( Im );

	IMPORT_BOOL(bEnabled);
	IMPORT_BYTE(LightType);
	IMPORT_BYTE(LightFunc);
	IMPORT_FLOAT(Radius);
	IMPORT_FLOAT(Brightness);
}


//
// Light export.
//
void FLightComponent::Export( CExporterBase& Ex )
{
	FExtraComponent::Export( Ex );
	CRenderAddon::ExportAddon( Ex );

	EXPORT_BOOL(bEnabled);
	EXPORT_BYTE(LightType);
	EXPORT_BYTE(LightFunc);
	EXPORT_FLOAT(Radius);
	EXPORT_FLOAT(Brightness);
}


//
// Render light source.
//
void FLightComponent::Render( CCanvas* Canvas )
{
	if( Base->bSelected )
		Canvas->DrawCircle( Base->Location, Radius, COLOR_Yellow, false, 48 );
}


//
// Return light map bounds.
//
TRect FLightComponent::GetLightRect()
{
	return TRect( Base->Location, Radius*2.f );
}


/*-----------------------------------------------------------------------------
    FSkyComponent implementation.
-----------------------------------------------------------------------------*/

//
// Sky constructor.
//
FSkyComponent::FSkyComponent()
	:	Parallax( 0.05f, 0.05f ),
		Extent( 8.f ),
		Offset( 0.f, 0.f ),
		RollSpeed( 0.f )
{
	Size			= TVector( 24.f, 16.f );
	bFixedAngle		= true;
	bHashable		= false;
	Color			= COLOR_SkyBlue; // :3
}


//
// Sky initialization.
//
void FSkyComponent::InitForEntity( FEntity* InEntity )
{
	FZoneComponent::InitForEntity(InEntity);

	// If no sky, set self.
	if( !Level->Sky )
		Level->Sky	= this;
}


//
// Sky serialization.
//
void FSkyComponent::SerializeThis( CSerializer& S )
{
	FZoneComponent::SerializeThis(S);

	Serialize( S, Parallax );
	Serialize( S, Extent );
	Serialize( S, Offset );
	Serialize( S, RollSpeed );
}


//
// Sky import.
//
void FSkyComponent::Import( CImporterBase& Im )
{
	FZoneComponent::Import( Im );

	IMPORT_VECTOR( Parallax );
	IMPORT_FLOAT( Extent );
	IMPORT_VECTOR( Offset );
	IMPORT_FLOAT( RollSpeed );
}


//
// Sky export.
//
void FSkyComponent::Export( CExporterBase& Ex )
{
	FZoneComponent::Export( Ex );

	EXPORT_VECTOR( Parallax );
	EXPORT_FLOAT( Extent );
	EXPORT_VECTOR( Offset );
	EXPORT_FLOAT( RollSpeed );
}


//
// Validate sky parameters after edit in Object Inspector.
//
void FSkyComponent::EditChange()
{
	FZoneComponent::EditChange();
	Extent	= Clamp( Extent, 1.f, Size.X * 0.5f );
}


//
// Render sky zone.
//
void FSkyComponent::Render( CCanvas* Canvas )
{
	// Never render self in self.
	if( Canvas->View.bMirage )
		return;

	// Draw border.
	FZoneComponent::Render( Canvas );

	// Draw view area.
	if( bSelected )
	{
		TVector ViewArea, Eye;
		TRect Skydome;
		Float Side, Side2;

		ViewArea.X	= Extent;
		ViewArea.Y	= (Extent * Canvas->View.FOV.Y) / Canvas->View.FOV.X;

		Skydome		= TRect( Location, Size );
		Side		= FastSqrt( ViewArea.X*ViewArea.X + ViewArea.Y*ViewArea.Y );
		Side2		= Side * 0.5f;

		// Transform observer location and apply parallax.
		Eye.X	= Canvas->View.Coords.Origin.X * Parallax.X + Offset.X;
		Eye.Y	= Canvas->View.Coords.Origin.Y * Parallax.Y + Offset.Y;

		// Azimuth of sky should be wrapped.
		Eye.X	= Wrap( Eye.X, Skydome.Min.X, Skydome.Max.X );

		// Height of sky should be clamped.
		Eye.Y	= Clamp( Eye.Y, Skydome.Min.Y+Side2, Skydome.Max.Y-Side2 );

		TAngle Roll = TAngle(fmodf(RollSpeed*(Float)GPlat->Now(), 2.f*PI ));		
		Canvas->DrawLineRect( Eye, ViewArea, Roll, COLOR_Red, false );
		Canvas->DrawLineStar( Eye, Roll, 1.f * Canvas->View.Zoom, COLOR_Red, false );
	}
}	


/*-----------------------------------------------------------------------------
    FZoneComponent implementation.
-----------------------------------------------------------------------------*/

//
// Zone constructor.
//
FZoneComponent::FZoneComponent()
	:	FRectComponent()
{
	bFixedAngle		= true;
	bHashable		= true;
	Size			= TVector( 10.f, 10.f );
	Color			= COLOR_Crimson;
}


//
// Zone rendering.
//
void FZoneComponent::Render( CCanvas* Canvas )
{
	// Is visible?
	TRect Bounds = GetAABB();
	if( !Canvas->View.Bounds.IsOverlap(Bounds) || !(Level->RndFlags & RND_Other) )
		return;

	// Choose colors.
	TColor Color1 = Color;
	TColor Color2 = bSelected ? Color + COLOR_Highlight1 : Color;

	if( bFrozen )
		Color2	= COLOR_Gray + Color2 * 0.5f;

	// Draw wire bounds.
	Canvas->DrawLineRect
					( 
						Location, 
						Size, 
						TAngle(0), 
						Color2, 
						false
					);

	// If selected - draw stretch points and zone area.
	if( bSelected )
	{
		// Colored area.
		TRenderRect Rect;
		Rect.Flags		= POLY_Unlit | POLY_FlatShade | POLY_Ghost;
		Rect.Bounds		= Bounds;
		Rect.Rotation	= TAngle(0);
		Rect.Bitmap		= nullptr;
		Rect.Color		= Color1;

		Canvas->DrawRect(Rect);

		// Draw handles.
		FRectComponent::Render( Canvas );
	}
}


/*-----------------------------------------------------------------------------
    FRectComponent implementation.
-----------------------------------------------------------------------------*/

//
// Rectangular object constructor.
//
FRectComponent::FRectComponent()
	:	FBaseComponent(),
		CBitmapRenderAddon( this )
{
	bFixedAngle	= false;
	Size		= TVector( 2.f, 2.f );
}


//
// Initialize rect for entity.
//
void FRectComponent::InitForEntity( FEntity* InEntity )
{
	FBaseComponent::InitForEntity(InEntity);
	com_add(RenderObjects);
}


//
// Return rectangle AABB.
//
TRect FRectComponent::GetAABB()
{
	if( Rotation.Angle == 0 )
		return TRect( Location, Size );
	else
		return TRect( Location, FastSqrt(Sqr(Size.X)+Sqr(Size.Y)) );
}


//
// Game started.
//
void FRectComponent::BeginPlay()
{
	FBaseComponent::BeginPlay();

	// Normally everything is hidden.
	bShown		= false;
	Entity->CallEvent( EVENT_OnHide );
}


//
// Render rectangle border.
//
void FRectComponent::Render( CCanvas* Canvas )
{
	// Is visible?
	TRect Bounds = GetAABB();
	Bool bVisible	= Canvas->View.Bounds.IsOverlap(Bounds);

	// Send in game OnHide/OnShow notifications.
	if( Level->bIsPlaying && !Canvas->View.bMirage )
	{
		if( bVisible )
		{
			// Rect is visible.
			if( !bShown )
			{
				Entity->CallEvent( EVENT_OnShow );
				bShown	= true;
			}
		}
		else
		{
			// Rect is invisible.
			if( bShown )
			{
				Entity->CallEvent( EVENT_OnHide );
				bShown	= false;
			}
		}
	}

	// Don't actually render, if invisible.
	if( !bVisible )
		return;

	// Draw stretch handles.
	if( bSelected )
	{
		TVector Size2 = Size * 0.5f;
		TCoords	Coords	= TCoords( Location, Rotation );
		TVector XAxis = Coords.XAxis * Size2.X, 
				YAxis = Coords.YAxis * Size2.Y;

		// Draw wire border.
		Canvas->DrawLineRect( Location, Size, Rotation, Color, false );

		// Draw points.
		if( !bFixedSize )
		{
			TVector Points[8] = 
			{
				Coords.Origin - XAxis + YAxis,
				Coords.Origin + YAxis,
				Coords.Origin + XAxis + YAxis,
				Coords.Origin + XAxis,
				Coords.Origin + XAxis - YAxis,
				Coords.Origin - YAxis,
				Coords.Origin - XAxis - YAxis,
				Coords.Origin - XAxis,
			};

			for( Integer i=0; i<8; i++ )
				Canvas->DrawCoolPoint( Points[i], 8.f, Color );
		}
	}
}


//
// Return true, if rect appeared on screen.
//
void FRectComponent::nativeIsShown( CFrame& Frame )
{
	*POPA_BOOL	= bShown;
}


/*-----------------------------------------------------------------------------
    FBrushComponent implementation.
-----------------------------------------------------------------------------*/

//
// Brush constructor.
//
FBrushComponent::FBrushComponent()
	:	CBitmapRenderAddon( this ),
		Type( BRUSH_Solid ),
		Vertices(),
		NumVerts( 0 ),
		TexCoords( TVector( 0.f, 0.f ), TVector( 0.25f, 0.f ), TVector( 0.f, -0.25f ) ),
		Scroll( 0.f, 0.f )
{
	bHashable	= true;
	MemZero( Vertices, sizeof(Vertices) );
}


//
// Initialize the brush.
//
void FBrushComponent::InitForEntity( FEntity* InEntity )
{
	FBaseComponent::InitForEntity( InEntity );
	com_add(RenderObjects);
}


//
// Serialize brush.
//
void FBrushComponent::SerializeThis( CSerializer& S )
{
	FBaseComponent::SerializeThis( S );
	CBitmapRenderAddon::SerializeAddon( S );

	SerializeEnum( S, Type );
	Serialize( S, NumVerts );
	Serialize( S, TexCoords );
	Serialize( S, Scroll );

	for( Integer i=0; i<NumVerts; i++ )
		Serialize( S, Vertices[i] );
}


//
// Export brush.
//
void FBrushComponent::Export( CExporterBase& Ex )
{
	FBaseComponent::Export( Ex );
	CBitmapRenderAddon::ExportAddon( Ex );

	EXPORT_BYTE(Type);
	EXPORT_INTEGER(NumVerts);
	EXPORT_VECTOR(TexCoords.Origin);
	EXPORT_VECTOR(TexCoords.XAxis);
	EXPORT_VECTOR(TexCoords.YAxis);
	EXPORT_VECTOR(Scroll);

	for( Integer i=0; i<NumVerts; i++ )
		Ex.ExportVector( *String::Format( L"Vertices[%d]", i ), Vertices[i] );
}


//
// Import brush.
//
void FBrushComponent::Import( CImporterBase& Im )
{
	FBaseComponent::Import( Im );
	CBitmapRenderAddon::ImportAddon( Im );

	IMPORT_BYTE(Type);
	IMPORT_INTEGER(NumVerts);
	IMPORT_VECTOR(TexCoords.Origin);
	IMPORT_VECTOR(TexCoords.XAxis);
	IMPORT_VECTOR(TexCoords.YAxis);
	IMPORT_VECTOR(Scroll);

	for( Integer i=0; i<NumVerts; i++ )
		Vertices[i] = Im.ImportVector( *String::Format( L"Vertices[%d]", i ) );
}


//
// Return brush collision bounds.
//
TRect FBrushComponent::GetAABB()
{
	// Bounds in locals.
	TRect R( Vertices, NumVerts );

	// Simple transformation.
	R.Min += Location;
	R.Max += Location;

	return R;
}


//
// Render brush.
//
void FBrushComponent::Render( CCanvas* Canvas )
{
	// All brushes colors.
	static const TColor BrushColors[BRUSH_MAX] =
	{			
  		TColor( 0x40, 0x80, 0x00, 0xff ),	/* BRUSH_NotSolid */
  		TColor( 0x80, 0x60, 0x20, 0xff ),	/* BRUSH_SemiSolid */
  		TColor( 0x40, 0x40, 0x80, 0xff )	/* BRUSH_Solid */
	};

	// Is visible?
	TRect Bounds = GetAABB();
	if( !Canvas->View.Bounds.IsOverlap(Bounds) )
		return;

	// Pick wire color.
	TColor Color1, Color2;
	if( IsConvexPoly( Vertices, NumVerts ) )
	{
		// A valid brush.
		Color1 = BrushColors[Type];
		Color2 = Color1 * 1.5f;
	}
	else
	{
		// An invalid brush.
		Color1 = COLOR_Red;
		Color2 = COLOR_Red;
	}

	// Transform and store vertices in TRenderPoly.
	TRenderPoly Poly;
	Poly.NumVerts	= NumVerts;
	for( Integer i=0; i<NumVerts; i++ )
		Poly.Vertices[i]	= Vertices[i] + Location;

	// Draw textured surface.
	if( Bitmap )
	{
		Poly.Bitmap		= Bitmap;
		Poly.Flags		= POLY_Unlit*bUnlit | !(Level->RndFlags & RND_Lighting);
		Poly.Color		= Color;

		// Apply flips to matrix.
		TCoords	Coords = TexCoords;
		if( bFlipH )	Coords.XAxis	= -Coords.XAxis;
		if( bFlipV )	Coords.YAxis	= -Coords.YAxis;

		Coords.Origin	+= Scroll;

		// Compute texture coords.
		for( Integer i=0; i<NumVerts; i++ )
			Poly.TexCoords[i]	= TransformPointBy( Vertices[i], Coords );

		Canvas->DrawPoly( Poly );
	}

	// Draw a ghost highlight.
	if( !Bitmap && (Level->RndFlags & RND_Other) || bSelected )
	{
		Poly.Flags	= POLY_FlatShade | POLY_Ghost;
		Poly.Bitmap	= nullptr;
		Poly.Color	= bSelected ? Color2 : Color1;

		if( bFrozen )
			Poly.Color	= COLOR_Gray + Poly.Color*0.5f;

		Canvas->DrawPoly( Poly );
	}

	// Draw a wire.
	if( bSelected || (Level->RndFlags & RND_Other) )
	{
		TColor WireColor;
		TVector V1, V2;

		if( !Bitmap )
			WireColor = bSelected ? Color1 : Color2;
		else
			WireColor = bSelected ? Color2 : Color1;

		if( bFrozen )
			WireColor	= COLOR_Gray + WireColor*0.5f;

		V1 = Poly.Vertices[Poly.NumVerts-1];
		for( Integer i=0; i<Poly.NumVerts; i++ )
		{
			V2 = Poly.Vertices[i];
			Canvas->DrawLine( V1, V2, WireColor, false );
			V1 = V2;
		}
	}

	// Draw a texture coords marker.
	if( bSelected )
	{
		Canvas->DrawLineStar
						( 
							Location + TexCoords.Origin,
							VectorToAngle( TexCoords.XAxis ),
							2.5f * Canvas->View.Zoom,
							Color1,
							false
						);
		Canvas->DrawCoolPoint
							(
								Location + TexCoords.Origin,
								5.f,
								Color2
							);
	}

	// Draw a stretch points.
	if( bSelected )
		for( Integer i=0; i<NumVerts; i++ )
			Canvas->DrawCoolPoint( Poly.Vertices[i], 8.f, Color2 );
}


/*-----------------------------------------------------------------------------
    FCameraComponent implementation.
-----------------------------------------------------------------------------*/

//
// Camera constructor.
//
FCameraComponent::FCameraComponent()
	:	FBaseComponent(),
		FOV( 64.f, 32.f ),
		Zoom( 1.f )
{
}


//
// Camera destructor.
//
FCameraComponent::~FCameraComponent()
{
	if( Level && Level->Camera == this )
		Level->Camera = nullptr;
}


//
// Camera in level initialization.
//
void FCameraComponent::InitForEntity( FEntity* InEntity )
{
	FBaseComponent::InitForEntity(InEntity);

	// Setup level's camera.
	if( Level->Camera )
		log( L"Lev: Level '%s' camera changed", *Level->GetName() );

	// Kill old camera!
	if( Level->Camera && Level->Camera != this )
		Level->DestroyEntity(Level->Camera->Entity);

	// Make new camera active.
	Level->Camera	= this;
}


//
// Camera serialization.
//
void FCameraComponent::SerializeThis( CSerializer& S )
{
	FBaseComponent::SerializeThis( S );
	Serialize( S, FOV );
	Serialize( S, Zoom );
}


//
// Camera import.
//
void FCameraComponent::Import( CImporterBase& Im )
{
	FBaseComponent::Import( Im );
	IMPORT_VECTOR( FOV );
	IMPORT_FLOAT( Zoom );
}


//
// Camera export.
//
void FCameraComponent::Export( CExporterBase& Ex )
{
	FBaseComponent::Export( Ex );
	EXPORT_VECTOR( FOV );
	EXPORT_FLOAT( Zoom );
}


//
// Return a "good" camera FOV according to viewport
// resolution.
//
TVector FCameraComponent::GetFitFOV( Float ScreenX, Float ScreenY ) const
{
	TVector R;
	R.X = FOV.X;
	R.Y = ScreenY * Abs(FOV.X) / ScreenX;
	return R;
}


/*-----------------------------------------------------------------------------
    FInputComponent implementation.
-----------------------------------------------------------------------------*/

//
// Input component constructor.
//
FInputComponent::FInputComponent()
	:	FExtraComponent()
{
}


//
// Input component destructor.
//
FInputComponent::~FInputComponent()
{
	com_remove(Inputs);
}


//
// Initialize input for level.
// 
void FInputComponent::InitForEntity( FEntity* InEntity )
{
	FExtraComponent::InitForEntity(InEntity);
	com_add(Inputs);
}


/*-----------------------------------------------------------------------------
    Registration.
-----------------------------------------------------------------------------*/

REGISTER_CLASS_CPP( FLightComponent, FExtraComponent, CLASS_Sterile )
{
	BEGIN_ENUM(ELightType)
		ENUM_ELEM(LIGHT_Steady);
		ENUM_ELEM(LIGHT_Flicker);
		ENUM_ELEM(LIGHT_Pulse);
		ENUM_ELEM(LIGHT_SoftPulse);
		ENUM_ELEM(LIGHT_SlowWave);
		ENUM_ELEM(LIGHT_FastWave);
		ENUM_ELEM(LIGHT_SpotLight);
		ENUM_ELEM(LIGHT_Searchlight);
		ENUM_ELEM(LIGHT_Fan);
		ENUM_ELEM(LIGHT_Disco);
		ENUM_ELEM(LIGHT_Flower);
		ENUM_ELEM(LIGHT_Hypnosis);
		ENUM_ELEM(LIGHT_Whirligig);
	END_ENUM;

	BEGIN_ENUM(ELightFunc)
		ENUM_ELEM(LF_Additive);
		ENUM_ELEM(LF_Multiplicative);
	END_ENUM;

	ADD_PROPERTY( bEnabled,		TYPE_Bool,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( LightType,	TYPE_Byte,		1,	PROP_Editable,	_ELightType );
	ADD_PROPERTY( LightFunc,	TYPE_Byte,		1,	PROP_Editable,	_ELightFunc );
	ADD_PROPERTY( Color,		TYPE_Color,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Radius,		TYPE_Float,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Brightness,	TYPE_Float,		1,	PROP_Editable,	nullptr );

	return 0;
}


REGISTER_CLASS_CPP( FSkyComponent, FZoneComponent, CLASS_None )
{
	ADD_PROPERTY( Parallax,		TYPE_Vector,	1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Offset,		TYPE_Vector,	1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( RollSpeed,	TYPE_Float,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Extent,		TYPE_Float,		1,	PROP_Editable,	nullptr );

	return 0;
}


REGISTER_CLASS_CPP( FZoneComponent, FRectComponent, CLASS_None )
{
	ADD_PROPERTY( Color,	TYPE_Color,		1,					PROP_Editable,	nullptr );

	return 0;
}


REGISTER_CLASS_CPP( FRectComponent, FBaseComponent, CLASS_None )
{
	DECLARE_METHOD( nativeIsShown,		TYPE_Bool,		TYPE_None,		TYPE_None,		TYPE_None,	TYPE_None );
	  
	return 0;
}


REGISTER_CLASS_CPP( FCameraComponent, FBaseComponent, CLASS_None )
{
	ADD_PROPERTY( FOV,		TYPE_Vector,	1,					PROP_Editable,	nullptr );
	ADD_PROPERTY( Zoom,		TYPE_Float,		1,					PROP_Editable,	nullptr );

	return 0;
}


REGISTER_CLASS_CPP( FBrushComponent, FBaseComponent, CLASS_None )
{
	BEGIN_ENUM(EBrushType)
		ENUM_ELEM(BRUSH_NotSolid);
		ENUM_ELEM(BRUSH_SemiSolid);
		ENUM_ELEM(BRUSH_Solid);
	END_ENUM;

	ADD_PROPERTY( Type,		TYPE_Byte,		1,					PROP_Editable,	_EBrushType );
	ADD_PROPERTY( bUnlit,	TYPE_Bool,		1,					PROP_Editable,	nullptr );
	ADD_PROPERTY( bFlipH,	TYPE_Bool,		1,					PROP_Editable,	nullptr );
	ADD_PROPERTY( bFlipV,	TYPE_Bool,		1,					PROP_Editable,	nullptr );
	ADD_PROPERTY( Color,	TYPE_Color,		1,					PROP_Editable,	nullptr );
	ADD_PROPERTY( Bitmap,	TYPE_Resource,	1,					PROP_Editable,	FBitmap::MetaClass );
	ADD_PROPERTY( Scroll,	TYPE_Vector,	1,					PROP_None,		nullptr );
	ADD_PROPERTY( NumVerts, TYPE_Integer,	1,					PROP_None,		nullptr );
	ADD_PROPERTY( Vertices,	TYPE_Vector,	MAX_BRUSH_VERTS,	PROP_None,		nullptr );

	return 0;
}


REGISTER_CLASS_CPP( FInputComponent, FExtraComponent, CLASS_None )
{
	return 0;
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/