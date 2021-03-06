/*=============================================================================
    FrPortal.cpp: Portals.
    Copyright Aug.2016 Vlad Gordienko.
=============================================================================*/

#include "Engine.h"

/*-----------------------------------------------------------------------------
    FPortalComponent implementation.
-----------------------------------------------------------------------------*/

//
// Portal constructor.
//
FPortalComponent::FPortalComponent()
	:	CRenderAddon( this ),
		Width( 16.f )
{
	Size		= TVector( 0.f, 0.f );
	bFixedAngle	= false;
}


//
// Portal destructor.
//
FPortalComponent::~FPortalComponent()
{
	// Remove from level's list.
	com_remove(Portals);
}


//
// Transfer a point through the portal.
//
TVector FPortalComponent::TransferPoint( TVector P )
{
	return P;
}


//
// Transfer a vector through the portal.
//
TVector FPortalComponent::TransferVector( TVector V )
{
	return V;
}


//
// Compute render info to render the scene through the
// portal, return true, if computed successfully.
//
Bool FPortalComponent::ComputeViewInfo( const TViewInfo& Parent, TViewInfo& Result )
{
	return false;
}


//
// Initialize portal for entity.
//
void FPortalComponent::InitForEntity( FEntity* InEntity )
{
	FBaseComponent::InitForEntity( InEntity );
	com_add(RenderObjects);
	com_add(Portals);
}


//
// When some field changed in portal.
//
void FPortalComponent::EditChange()
{
	FBaseComponent::EditChange();
	Width	= Clamp( Width, 2.f, 64.f );
}


//
// Serialize portal.
//
void FPortalComponent::SerializeThis( CSerializer& S )
{
	FBaseComponent::SerializeThis( S );
	CRenderAddon::SerializeAddon( S );
	Serialize( S, Width );
}


//
// Import the portal.
//
void FPortalComponent::Import( CImporterBase& Im )
{
	FBaseComponent::Import( Im );
	CRenderAddon::ImportAddon( Im );
	IMPORT_FLOAT( Width );
}


//
// Export the portal.
//
void FPortalComponent::Export( CExporterBase& Ex )
{
	FBaseComponent::Export( Ex );
	CRenderAddon::ExportAddon( Ex );
	EXPORT_FLOAT( Width );
}


//
// Render the portal.
//
void FPortalComponent::Render( CCanvas* Canvas )
{
}


/*-----------------------------------------------------------------------------
    FMirrorComponent implementation.
-----------------------------------------------------------------------------*/

//
// Mirror constructor.
//
FMirrorComponent::FMirrorComponent()
	:	FPortalComponent()
{
	bFixedAngle	= true;
	Color		= COLOR_DodgerBlue;
}


//
// Transfer point through the mirror.
//
TVector FMirrorComponent::TransferPoint( TVector P )
{
	return TVector( P.X + (Location.X - P.X)*2.f, P.Y );
}


//
// Transfer vector through the mirror.
//
TVector FMirrorComponent::TransferVector( TVector V )
{
	return TVector( -V.X, V.Y );
}


//
// Compute TViewInfo for other side of the mirror.
//
Bool FMirrorComponent::ComputeViewInfo( const TViewInfo& Parent, TViewInfo& Result )
{
	Result.X				= Parent.X;
	Result.Y				= Parent.Y;
	Result.Width			= Parent.Width;
	Result.Height			= Parent.Height;
	Result.bMirage			= true;
	Result.Coords.Origin	= TransferPoint( Parent.Coords.Origin );
	Result.Coords.XAxis		= -Parent.Coords.XAxis;
	Result.Coords.YAxis		= Parent.Coords.YAxis;
	Result.UnCoords			= Result.Coords.Transpose();
	Result.FOV				= Parent.FOV;
	Result.Zoom				= Parent.Zoom;

	// Mirror bounds.
	if( Location.X > Parent.Coords.Origin.X )
	{
		// Parent lie on the left half-plane.
		Result.Bounds.Min.X	= Parent.Bounds.Min.X;
		Result.Bounds.Max.X	= Location.X + (Location.X-Parent.Bounds.Min.X);
	}
	else
	{
		// Parent lie on the right half-plane.
		Result.Bounds.Min.X	= Location.X - (Parent.Bounds.Max.X - Location.X);
		Result.Bounds.Max.X	= Parent.Bounds.Max.X;
	}

	Result.Bounds.Min.Y	= Parent.Bounds.Min.Y;
	Result.Bounds.Max.Y	= Parent.Bounds.Max.Y;

	return true;
}


//
// Render the mirror.
//
void FMirrorComponent::Render( CCanvas* Canvas )
{
	// Never draw fake mirror.
	if( Canvas->View.bMirage || !(Level->RndFlags & RND_Other) )
		return;

	TColor DrawColor = Color;
	if( bSelected )
		DrawColor *= 1.5f;

	// Master line.
	TVector V1 = TVector( Location.X, Location.Y - 0.5f * Width );
	TVector V2 = TVector( Location.X, Location.Y + 0.5f * Width );

	Canvas->DrawLine( V1, V2, DrawColor, false );

	// Tips.
	Canvas->DrawCoolPoint( V1, 5.f, DrawColor );
	Canvas->DrawCoolPoint( V2, 5.f, DrawColor );

	// Centroid.
	Canvas->DrawPoint( Location, 4.f, DrawColor );
}


void FMirrorComponent::nativeMirrorPoint( CFrame& Frame )
{
	TVector P = POP_VECTOR;
	*POPA_VECTOR = TransferPoint( P );
}


void FMirrorComponent::nativeMirrorVector( CFrame& Frame )
{
	TVector V = POP_VECTOR;
	*POPA_VECTOR = TransferVector( V );
}


/*-----------------------------------------------------------------------------
    FWarpComponent implementation.
-----------------------------------------------------------------------------*/

//
// Warp constructor.
//
FWarpComponent::FWarpComponent()
	:	FPortalComponent(),
		Other( nullptr )
{
	bFixedAngle	= false;
}


//
// Transfer a point through the warp portal. 
// Caution: Destination should be specified.
//
TVector FWarpComponent::TransferPoint( TVector P )
{
	assert(Other);
	return TransformPointBy( TransformPointBy(P, ToLocal()), Other->Base->ToWorld() );
}


//
// Transfer a vector through the warp portal. 
// Caution: Destination should be specified.
//
TVector FWarpComponent::TransferVector( TVector V )
{
	assert(Other);
	return TransformVectorBy( TransformVectorBy(V, ToLocal()), Other->Base->ToWorld() );
}


//
// Compute TViewInfo for the other side of the warp
// portal.
//
Bool FWarpComponent::ComputeViewInfo( const TViewInfo& Parent, TViewInfo& Result )
{
	if( !Other )
		return false;

	Result.X				= Parent.X;
	Result.Y				= Parent.Y;
	Result.Width			= Parent.Width;
	Result.Height			= Parent.Height;
	Result.bMirage			= true;
	Result.Coords.Origin	= TransferPoint( Parent.Coords.Origin );
	Result.Coords.XAxis		= TransferVector( Parent.Coords.XAxis );
	Result.Coords.YAxis		= TransferVector( Parent.Coords.YAxis );
	Result.UnCoords			= Result.Coords.Transpose();
	Result.FOV				= Parent.FOV;
	Result.Zoom				= Parent.Zoom;
	Result.Bounds			= TRect
								( 
									Result.Coords.Origin, 
									FastSqrt( Sqr(Result.FOV.X)+Sqr(Result.FOV.Y)*Result.Zoom  )
								);

	return true;
}


//
// When some field changed in warp.
//
void FWarpComponent::EditChange()
{
	FPortalComponent::EditChange();

	if( Other && !Other->Base->IsA(FWarpComponent::MetaClass) )
		Other = nullptr;
}


//
// Warp serialization.
//
void FWarpComponent::SerializeThis( CSerializer& S )
{
	FPortalComponent::SerializeThis( S );
	Serialize( S, Other );
}


//
// Import the warp.
//
void FWarpComponent::Import( CImporterBase& Im )
{
	FPortalComponent::Import( Im );
	IMPORT_OBJECT( Other );
}


//
// Export the warp.
//
void FWarpComponent::Export( CExporterBase& Ex )
{
	FPortalComponent::Export( Ex );
	EXPORT_OBJECT( Other );
}


//
// Render the warp.
//
void FWarpComponent::Render( CCanvas* Canvas )
{
	// Don't draw fake.
	if( Canvas->View.bMirage || !(Level->RndFlags & RND_Other) )
		return;

	TCoords C = ToWorld();
	TVector V1 = TransformPointBy( TVector( 0.f, -Width*0.5f ), C );
	TVector V2 = TransformPointBy( TVector( 0.f, +Width*0.5f ), C );

	TColor DrawColor = Other ? COLOR_Green : COLOR_Crimson;
	if( bSelected )
		DrawColor *= 2.f;

	Canvas->DrawLine( V1, V2, DrawColor, false );

	// Tips.
	Canvas->DrawCoolPoint( V1, 6.f, DrawColor );
	Canvas->DrawCoolPoint( V2, 6.f, DrawColor );

	// Centroid.
	Canvas->DrawPoint( Location, 4.f, DrawColor );
}


void FWarpComponent::nativeWarpPoint( CFrame& Frame )
{
	TVector P = POP_VECTOR;
	*POPA_VECTOR = Other ? TransferPoint( P ) : P;
}


void FWarpComponent::nativeWarpVector( CFrame& Frame )
{
	TVector V = POP_VECTOR;
	*POPA_VECTOR = Other ? TransferVector( V ) : V;
}


/*-----------------------------------------------------------------------------
    Registration.
-----------------------------------------------------------------------------*/

REGISTER_CLASS_CPP( FPortalComponent, FBaseComponent, CLASS_Abstract )
{
	ADD_PROPERTY( Width,		TYPE_Float,		1,					PROP_Editable,	nullptr );

	return 0;
}


REGISTER_CLASS_CPP( FMirrorComponent, FPortalComponent, CLASS_None )
{
	ADD_PROPERTY( Color,		TYPE_Color,		1,					PROP_Editable,	nullptr );

	DECLARE_METHOD( nativeMirrorPoint,	TYPE_Vector,	TYPE_Vector, TYPE_None, TYPE_None, TYPE_None );
	DECLARE_METHOD( nativeMirrorVector,	TYPE_Vector,	TYPE_Vector, TYPE_None, TYPE_None, TYPE_None );
	return 0;
}


REGISTER_CLASS_CPP( FWarpComponent, FPortalComponent, CLASS_None ) 
{
	ADD_PROPERTY( Other,		TYPE_Entity,	1,					PROP_Editable,	nullptr );

	DECLARE_METHOD( nativeWarpPoint,	TYPE_Vector,	TYPE_Vector, TYPE_None, TYPE_None, TYPE_None );
	DECLARE_METHOD( nativeWarpVector,	TYPE_Vector,	TYPE_Vector, TYPE_None, TYPE_None, TYPE_None );
	return 0;
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/