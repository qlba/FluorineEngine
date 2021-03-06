/*=============================================================================
    FrEmit.cpp: Particles emitters.
    Copyright Aug.2016 Vlad Gordienko.
=============================================================================*/

#include "Engine.h"

/*-----------------------------------------------------------------------------
    FEmitterComponent implementation.
-----------------------------------------------------------------------------*/

//
// Emitter constructor.
//
FEmitterComponent::FEmitterComponent()
	:	CRenderAddon( this ),
		CTickAddon( this ),
		MaxParticles( 100 ),
		SpawnArea( 0.f, 0.f ),
		SpawnOffset( 0.f, 0.f ),
		EmitPerSec( 10 ),
		SizeParam( PPT_Random ),
		Bitmap( nullptr ),
		bUnlit( true ),
		NumUTiles( 1 ),
		NumVTiles( 1 ),
		Particles(),
		NumPrts( 0 ),
		Accumulator( 0.f )
{
	LifeRange[0]		= 3.f;
	LifeRange[1]		= 5.f;

	SizeRange[0]		= 0.5f;
	SizeRange[1]		= 1.5f;

	Colors[0]			= TColor( 0xff, 0xff, 0xff, 0xff );
	Colors[1]			= TColor( 0xff, 0xff, 0xff, 0xff );
	Colors[2]			= TColor( 0xff, 0xff, 0xff, 0xff );

	SpinRange[0]		= 0.f;
	SpinRange[1]		= 0.f;

	// Force to be on top.
	DrawOrder			= 0.03;
}


//
// Emitter destructor.
//
FEmitterComponent::~FEmitterComponent()
{
}

//
// Initialize emitter for entity.
//
void FEmitterComponent::InitForEntity( FEntity* InEntity )
{
	FExtraComponent::InitForEntity( InEntity );
	com_add(RenderObjects);
	com_add(TickObjects);
}


//
// On some field changed in emitter.
//
void FEmitterComponent::EditChange()
{
	FExtraComponent::EditChange();

	// Clamp tiles count.
	NumUTiles	= Clamp<Byte>( NumUTiles, 1, 4 );
	NumVTiles	= Clamp<Byte>( NumVTiles, 1, 4 );

	// Clamp particles limit.
	MaxParticles	= Clamp<Integer>( MaxParticles, 1, MAX_PARTICLES );
}


//
// Initialize emitter after loading.
//
void FEmitterComponent::PostLoad()
{
	FExtraComponent::PostLoad();
}


//
// Update particles, should be implemented in
// subclasses.
//
void FEmitterComponent::UpdateEmitter( Float Delta )
{
}


//
// Return particle system cloud bound.
//
TRect FEmitterComponent::GetCloudRect()
{
	TRect Result;
	Result.Min		= Base->Location;
	Result.Max		= Base->Location;

	for( Integer i=0; i<NumPrts; i++ )
	{
		TParticle& P = Particles[i];

		// Update bounds.
		Result.Min.X	= Min( Result.Min.X, P.Location.X );
		Result.Min.Y	= Min( Result.Min.Y, P.Location.Y );
		Result.Max.X	= Max( Result.Max.X, P.Location.X );
		Result.Max.Y	= Max( Result.Max.Y, P.Location.Y );
	}

    // Cloud bound is compute, but necessary to consider
    // the particles size too.
	Result.Min.X	-= SizeRange[1];
	Result.Min.Y	-= SizeRange[1];
	Result.Max.X	+= SizeRange[1];
	Result.Max.Y	+= SizeRange[1];

	return Result;
}


//
// Tick in editor.
//
void FEmitterComponent::TickNonPlay( Float Delta )
{
	Tick( Delta );
}


//
// Tick in game.
//
void FEmitterComponent::Tick( Float Delta )
{
	// Reallocate list, if need.
	if( MaxParticles != Particles.Num() )
	{
		MaxParticles	= Clamp<Integer>( MaxParticles, 1, MAX_PARTICLES );
		NumPrts			= 0;
		Particles.SetNum( MaxParticles );
	}

	// Not really good way to determinate
	// is should update particles.
	TRect Camera = TRect( Level->Camera->Location, Level->Camera->FOV );
	TRect Cloud = GetCloudRect();
	if( !Camera.IsOverlap(Cloud) )
		return;

	// Sub-classes will process particles.
	UpdateEmitter( Delta );
}


//
// Serialize spark.
//
void Serialize( CSerializer& S, TParticle& V )
{
	Serialize( S, V.Location );
	Serialize( S, V.Rotation );
	Serialize( S, V.Speed );
	Serialize( S, V.SpinRate );
	Serialize( S, V.Size );
	Serialize( S, V.Life );
	Serialize( S, V.MaxLifeInv );
	Serialize( S, V.Phase );
	Serialize( S, V.iTile );
}


//
// Serialize emitter.
//
void FEmitterComponent::SerializeThis( CSerializer& S )
{
	FExtraComponent::SerializeThis( S );

	Serialize( S, MaxParticles );
	Serialize( S, LifeRange[0] );
	Serialize( S, LifeRange[1] );
	Serialize( S, SpawnArea );
	Serialize( S, EmitPerSec );
	SerializeEnum( S, SizeParam );
	Serialize( S, SizeRange[0] );
	Serialize( S, SizeRange[1] );
	Serialize( S, bUnlit );
	Serialize( S, Colors[0] );
	Serialize( S, Colors[1] );
	Serialize( S, Colors[2] );
	Serialize( S, SpinRange[0] );
	Serialize( S, SpinRange[1] );
	Serialize( S, Bitmap );
	Serialize( S, NumUTiles );
	Serialize( S, NumVTiles );
	Serialize( S, NumPrts );

	// Temporally shrink particles tables,
	// to store only "active" particles.
	if( S.GetMode() == SM_Save )
	{
		// Really reduce particles count, it's can crash
		// even CObjectDatabase!
		Integer NumToSave = Min( 100, NumPrts );
		if( NumToSave )
			S.SerializeData( &Particles[0], NumToSave*sizeof(TParticle) );
	}
	else if( S.GetMode() == SM_Load )
	{
		Integer NumToLoad = Min( 100, NumPrts );
		Particles.SetNum(NumToLoad);
		if( NumToLoad )
			S.SerializeData( &Particles[0], NumToLoad*sizeof(TParticle) );
	}
}


//
// Import emitter.
//
void FEmitterComponent::Import( CImporterBase& Im )
{
	FExtraComponent::Import( Im );

	IMPORT_INTEGER( MaxParticles );
	IMPORT_FLOAT( LifeRange[0] );
	IMPORT_FLOAT( LifeRange[1] );
	IMPORT_VECTOR( SpawnArea );
	IMPORT_INTEGER( EmitPerSec );
	IMPORT_BYTE( SizeParam );
	IMPORT_FLOAT( SizeRange[0] );
	IMPORT_FLOAT( SizeRange[1] );
	IMPORT_BOOL( bUnlit );
	IMPORT_COLOR( Colors[0] );
	IMPORT_COLOR( Colors[1] );
	IMPORT_COLOR( Colors[2] );
	IMPORT_FLOAT( SpinRange[0] );
	IMPORT_FLOAT( SpinRange[1] );
	IMPORT_OBJECT( Bitmap );
	IMPORT_BYTE( NumUTiles );
	IMPORT_BYTE( NumVTiles );

	// Don't store particles list, it's will
	// be respawn during playing.
}


//
// Export emitter.
//
void FEmitterComponent::Export( CExporterBase& Ex )
{
	FExtraComponent::Export( Ex );

	EXPORT_INTEGER( MaxParticles );
	EXPORT_FLOAT( LifeRange[0] );
	EXPORT_FLOAT( LifeRange[1] );
	EXPORT_VECTOR( SpawnArea );
	EXPORT_INTEGER( EmitPerSec );
	EXPORT_BYTE( SizeParam );
	EXPORT_FLOAT( SizeRange[0] );
	EXPORT_FLOAT( SizeRange[1] );
	EXPORT_BOOL( bUnlit );
	EXPORT_COLOR( Colors[0] );
	EXPORT_COLOR( Colors[1] );
	EXPORT_COLOR( Colors[2] );
	EXPORT_FLOAT( SpinRange[0] );
	EXPORT_FLOAT( SpinRange[1] );
	EXPORT_OBJECT( Bitmap );
	EXPORT_BYTE( NumUTiles );
	EXPORT_BYTE( NumVTiles );

	// Don't store particles list, it's will
	// be respawn during playing.
}


/*-----------------------------------------------------------------------------
    FLissajousEmitterComponent implementation.
-----------------------------------------------------------------------------*/

//
// Lissajous curve constructor.
//
FLissajousEmitterComponent::FLissajousEmitterComponent()
	:	FEmitterComponent(),
		Alpha( 1.5f ),
		Beta( 1.f ),
		Delta( 20.f ),
		X( 5.f ),
		Y( 5.f )
{
	LifeRange[0]	= 5.f;
	LifeRange[1]	= 5.f;

	SizeRange[0]	= 1.f;
	SizeRange[1]	= 5.f;

	MaxParticles	= 30;
	EmitPerSec		= 4;
	SizeParam		= PPT_Random;
}


void FLissajousEmitterComponent::SerializeThis( CSerializer& S )
{
	FEmitterComponent::SerializeThis( S );
	Serialize( S, Alpha );
	Serialize( S, Beta );
	Serialize( S, Delta );
	Serialize( S, X );
	Serialize( S, Y );
}


void FLissajousEmitterComponent::Import( CImporterBase& Im )
{
	FEmitterComponent::Import( Im );
	IMPORT_FLOAT( Alpha );
	IMPORT_FLOAT( Beta );
	IMPORT_FLOAT( Delta );
	IMPORT_FLOAT( X );
	IMPORT_FLOAT( Y );
}


void FLissajousEmitterComponent::Export( CExporterBase& Ex )
{
	FEmitterComponent::Export( Ex );
	EXPORT_FLOAT( Alpha );
	EXPORT_FLOAT( Beta );
	EXPORT_FLOAT( Delta );
	EXPORT_FLOAT( X );
	EXPORT_FLOAT( Y );
}


//
// Update Lissajous trajectories.
//
void FLissajousEmitterComponent::UpdateEmitter( Float DeltaTime )
{
	TVector Basis	= Base->Location + SpawnOffset;

	// Accumulate time.
	Accumulator += DeltaTime;

	// Spawn new particles.
	Integer NewPrts = Trunc( Accumulator * EmitPerSec );
	if( NewPrts > 0 )
		Accumulator = 0.f;
	while( NewPrts>0 && NumPrts<MaxParticles )
	{
		TParticle P;
		P.Location.X	= RandomRange( Basis.X-SpawnArea.X, Basis.X+SpawnArea.X );
		P.Location.Y	= RandomRange( Basis.Y-SpawnArea.Y, Basis.Y+SpawnArea.Y );

		P.Speed			= P.Location;		// Store spawn location.
		P.iTile			= Random(NumUTiles * NumVTiles);
		P.Phase			= RandomRange( 0.f, 2.f*PI );
		P.Life			= RandomRange( LifeRange[0], LifeRange[1] );
		P.MaxLifeInv	= 1.f / Max( 0.001f, P.Life );
		P.Size			= SizeParam == PPT_Random ? RandomRange( SizeRange[0], SizeRange[1] ) : SizeRange[0];

		if( SpinRange[0] == SpinRange[1] && SpinRange[0] == 0.f )
		{
			// No rotation.
			P.Rotation	= 0;
			P.SpinRate	= 0.f;
		}
		else
		{
			// Rotate.
			P.Rotation	= Random(0xffff);
			P.SpinRate	= RandomRange( SpinRange[0], SpinRange[1] );
		}

		// Add to list.
		Particles[NumPrts]	= P;
		NumPrts++;
		NewPrts--;
	}
	
	// Process Lissajous physics.
	for( Integer i=0; i<NumPrts;  )
	{
		TParticle& P = Particles[i];

		// Individual 'time' for each particle.
		Float T = P.Life * P.MaxLifeInv * (2.f*PI) + P.Phase;

		P.Location.X	= P.Speed.X + X * FastSinF( Alpha * T + Delta );
		P.Location.Y	= P.Speed.Y + Y * FastSinF( Beta * T );

		P.Rotation		+= DeltaTime * P.SpinRate;
		P.Life			-= DeltaTime;

		if( SizeParam == PPT_Linear )
		{
			Float Alpha = 1.f - P.Life * P.MaxLifeInv;
			P.Size		= SizeRange[0] + (SizeRange[1]-SizeRange[0]) * Alpha;
		}

		// Kill outlived.
		if( P.Life <= 0.f )
		{
			NumPrts--;
			Particles[i]	= Particles[NumPrts];
		}
		else
			i++;
	}
}


/*-----------------------------------------------------------------------------
	FWeatherEmitterComponent implementation.
-----------------------------------------------------------------------------*/

//
// Weather emitter constructor.
//
FWeatherEmitterComponent::FWeatherEmitterComponent()
	:	FEmitterComponent(),
		WeatherType( WEATHER_Snow )
{
	SpeedRange[0]	= 3.f;
	SpeedRange[1]	= 10.f;

	SpawnArea.X		= 32.f;
	SpawnArea.Y		= 10.f;
}


//
// Return the particles cloud bounds. But
// snowflakes or raindrops are fall in the entire world!
//
TRect FWeatherEmitterComponent::GetCloudRect()
{
	return TRect( TVector( 0.f, 0.f ), WORLD_SIZE );
}


//
// Render the weather particles.
//
void FWeatherEmitterComponent::Render( CCanvas* Canvas )
{
    // Don't draw snowflakes or raindrops in
    // a mirage, especially in sky-zone.
	if( !Canvas->View.bMirage )
		FEmitterComponent::Render( Canvas );
}


void FWeatherEmitterComponent::SerializeThis( CSerializer& S )
{
	FEmitterComponent::SerializeThis( S );
	SerializeEnum( S, WeatherType );
	Serialize( S, SpeedRange[0] );
	Serialize( S, SpeedRange[1] );
}


void FWeatherEmitterComponent::Import( CImporterBase& Im )
{
	FEmitterComponent::Import( Im );
	IMPORT_BYTE( WeatherType );
	IMPORT_FLOAT( SpeedRange[0] );
	IMPORT_FLOAT( SpeedRange[1] );
}


void FWeatherEmitterComponent::Export( CExporterBase& Ex )
{
	FEmitterComponent::Export( Ex );
	EXPORT_BYTE( WeatherType );
	EXPORT_FLOAT( SpeedRange[0] );
	EXPORT_FLOAT( SpeedRange[1] );
}


//
// Update the weather particles.
//
void FWeatherEmitterComponent::UpdateEmitter( Float Delta )
{
	// Precompute.
	Float ViewTop	= Level->Camera->FOV.Y*0.5f + Level->Camera->Location.Y;

	// Accumulate time.
	Accumulator += Delta;

	// Spawn new particles.
	Integer NewPrts = Trunc( Accumulator * EmitPerSec );
	if( NewPrts > 0 )
		Accumulator = 0.f;
	while( NewPrts>0 && NumPrts<MaxParticles )
	{
		TParticle P;
		P.Location.X	= RandomRange( -SpawnArea.X, SpawnArea.X ) + Level->Camera->Location.X;
		P.Location.Y	= ViewTop + SpawnArea.Y;

		if( WeatherType == WEATHER_Snow )
		{
			// Emit new snowflake.
			P.Speed.X	= P.Location.X;		// Store origin X, to apply jitter effect.
			P.Speed.Y	= RandomRange( SpeedRange[0], SpeedRange[1] );
		}
		else
		{
			// Emit new raindrop.
			P.Speed.X	= 0.f;		
			P.Speed.Y	= RandomRange( SpeedRange[0], SpeedRange[1] );
		}

		P.iTile			= Random(NumUTiles * NumVTiles);
		P.Phase			= RandomRange( 0.f, 2.f*PI );
		P.Life			= RandomRange( LifeRange[0], LifeRange[1] );
		P.MaxLifeInv	= 1.f / Max( 0.001f, P.Life );
		P.Size			= RandomRange( SizeRange[0], SizeRange[1] );

		if( SpinRange[0] == SpinRange[1] && SpinRange[0] == 0.f )
		{
			// No rotation.
			P.Rotation	= 0;
			P.SpinRate	= 0.f;
		}
		else
		{
			// Rotate.
			P.Rotation	= Random(0xffff);
			P.SpinRate	= RandomRange( SpinRange[0], SpinRange[1] );
		}
		
		// Add to list.
		Particles[NumPrts]	= P;
		NumPrts++;
		NewPrts--;
	}

	// Process weather physics.
	for( Integer i=0; i<NumPrts;  )
	{
		TParticle& P = Particles[i];

		if( WeatherType == WEATHER_Snow )
		{
			// Snowflake physics.
			P.Location.X	= P.Speed.X + FastSinF(P.Phase)*5.f;
			P.Location.Y	-= P.Speed.Y * Delta;
			P.Phase			+= Delta;
		}
		else
		{
			// Raindrop physics.
			P.Location.Y -= P.Speed.Y * Delta;
		}

		P.Rotation		+= Delta * P.SpinRate;
		P.Life			-= Delta;

		// Kill outlived.
		if( P.Life <= 0.f )
		{
			NumPrts--;
			Particles[i]	= Particles[NumPrts];
		}
		else
			i++;
	}
}


/*-----------------------------------------------------------------------------
	FPhysEmitterComponent implementation.
-----------------------------------------------------------------------------*/

//
// Physics emitter constructor.
//
FPhysEmitterComponent::FPhysEmitterComponent()
	:	FEmitterComponent()
{
	SpeedRange[0]	= TVector( -5.f, -5.f );
	SpeedRange[1]	= TVector( +5.f, +5.f );
	Acceleration	= TVector( 0.f, 0.f );
}


void FPhysEmitterComponent::SerializeThis( CSerializer& S )
{
	FEmitterComponent::SerializeThis( S );
	Serialize( S, SpeedRange[0] );
	Serialize( S, SpeedRange[1] );
	Serialize( S, Acceleration );
}


void FPhysEmitterComponent::Import( CImporterBase& Im )
{
	FEmitterComponent::Import( Im );
	IMPORT_VECTOR( SpeedRange[0] );
	IMPORT_VECTOR( SpeedRange[1] );
	IMPORT_VECTOR( Acceleration );
}


void FPhysEmitterComponent::Export( CExporterBase& Ex )
{
	FEmitterComponent::Export( Ex );
	EXPORT_VECTOR( SpeedRange[0] );
	EXPORT_VECTOR( SpeedRange[1] );
	EXPORT_VECTOR( Acceleration );
}


//
// Update physics.
//
void FPhysEmitterComponent::UpdateEmitter( Float Delta )
{
	// Let's velocity and acceleration in local coords.
	TCoords LocalToWorld = Base->ToWorld();
	TVector WorldAcc = TransformVectorBy( Acceleration, LocalToWorld );
	TVector Basis	= Base->Location + SpawnOffset;

	// Accumulate time.
	Accumulator += Delta;

	// Spawn new particles.
	Integer NewPrts = Trunc( Accumulator * EmitPerSec );
	if( NewPrts > 0 )
		Accumulator = 0.f;
	while( NewPrts>0 && NumPrts<MaxParticles )
	{
		TParticle P;
		P.Location.X	= RandomRange( Basis.X-SpawnArea.X, Basis.X+SpawnArea.X );
		P.Location.Y	= RandomRange( Basis.Y-SpawnArea.Y, Basis.Y+SpawnArea.Y );

		P.Speed.X		= RandomRange( SpeedRange[0].X, SpeedRange[1].X );
		P.Speed.Y		= RandomRange( SpeedRange[0].Y, SpeedRange[1].Y );
		P.Speed			= TransformVectorBy( P.Speed, LocalToWorld );

		P.Life			= RandomRange( LifeRange[0], LifeRange[1] );
		P.MaxLifeInv	= 1.f / Max( 0.001f, P.Life );
		P.iTile			= Random(NumUTiles * NumVTiles);
		P.Size			= SizeParam == PPT_Random ? RandomRange( SizeRange[0], SizeRange[1] ) : SizeRange[0];

		if( SpinRange[0] == SpinRange[1] && SpinRange[0] == 0.f )
		{
			// No rotation.
			P.Rotation	= 0;
			P.SpinRate	= 0.f;
		}
		else
		{
			// Rotate.
			P.Rotation	= Random(0xffff);
			P.SpinRate	= RandomRange( SpinRange[0], SpinRange[1] );
		}

		// Add to list.
		Particles[NumPrts]	= P;
		NumPrts++;
		NewPrts--;
	}

	// Process physics.
	for( Integer i=0; i<NumPrts;  )
	{
		TParticle& P = Particles[i];

		P.Location		+= P.Speed * Delta;
		P.Speed			+= WorldAcc * Delta;
		P.Life			-= Delta;
		P.Rotation		+= Delta * P.SpinRate;

		if( SizeParam == PPT_Linear )
		{
			Float Alpha = 1.f - P.Life * P.MaxLifeInv;
			P.Size		= SizeRange[0] + (SizeRange[1]-SizeRange[0]) * Alpha;
		}

		// Kill outlived.
		if( P.Life <= 0.f )
		{
			NumPrts--;
			Particles[i]	= Particles[NumPrts];
		}
		else
			i++;
	}
}


/*-----------------------------------------------------------------------------
	Particles rendering.
-----------------------------------------------------------------------------*/

//
// Color linear interpolation.
// Try to optimize. Probably I'm mad, just
// 3 multiplications it's not very expensive.
// I think via MMX it's will be super fast.
// Keep calm, this take about 8 cycles for 256 palette!!
// even table usage cost about 16 cycles.
//
inline TColor ColorLerp( TColor Color1, TColor Color2, Byte Alpha )
{
	TColor Result;
	Result.R = (Integer)(Color1.R) + (((Integer)(Color2.R)-(Integer)(Color1.R))*Alpha >> 8); 
	Result.G = (Integer)(Color1.G) + (((Integer)(Color2.G)-(Integer)(Color1.G))*Alpha >> 8); 
	Result.B = (Integer)(Color1.B) + (((Integer)(Color2.B)-(Integer)(Color1.B))*Alpha >> 8); 
	Result.A = (Integer)(Color1.A) + (((Integer)(Color2.A)-(Integer)(Color1.A))*Alpha >> 8); 
	return Result;
}


//
// Render particles.
//
void FEmitterComponent::Render( CCanvas* Canvas )
{
	// Particles render turn on?
	if( !(Level->RndFlags & RND_Particles) )
		return;

	// Render particles if they actually visible.
	TRect Cloud = GetCloudRect();
	if( !Canvas->View.Bounds.IsOverlap(Cloud) )
		return;

#if 0
	// Compute texture coords for each tile, will
	// be cool to cache it.
	TRect CoordTable[16];
	for( Integer V=0; V<NumVTiles; V++ )
		for( Integer U=0; U<NumUTiles; U++ )
		{
			CoordTable[V*NumUTiles+U].Min.X	= (Float)(U+0.f) / (Float)NumUTiles;
			CoordTable[V*NumUTiles+U].Max.X	= (Float)(U+1.f) / (Float)NumUTiles;

			CoordTable[V*NumUTiles+U].Min.Y	= 1.f - (Float)(V+0.f) / (Float)NumVTiles;
			CoordTable[V*NumUTiles+U].Max.Y	= 1.f - (Float)(V+1.f) / (Float)NumVTiles;
		}

	// Initialize shared fields of render rect.
	TRenderRect Rect;
	Rect.Flags			= POLY_Unlit*bUnlit | !(Level->RndFlags & RND_Lighting);
	Rect.Rotation		= 0;
	Rect.Bitmap			= Bitmap;
	
	// Render particles.
	for( Integer i=0; i<NumPrts; i++ )
	{
		TParticle&	P = Particles[i];
		Float		Side = P.Size * 0.5f;
		Float		Alpha = 1.f - P.Life * P.MaxLifeInv;

		// Interpolate color.
		if( Alpha < 0.5f )
			Rect.Color	= ColorLerp( Colors[0], Colors[1], (Byte)(Alpha*512) );
		else
			Rect.Color	= ColorLerp( Colors[1], Colors[2], (Byte)((Alpha-0.5f)*512) );

		// Draw.
		Rect.Bounds		= TRect( P.Location, Side );
		Rect.TexCoords	= CoordTable[P.iTile];
		Rect.Rotation	= P.Rotation;
		Canvas->DrawRect( Rect );
	}
#else
	// Compute texture coords for each tile, will
	// be cool to cache it.
	TVector CoordTable[16][4];
	for( Integer V=0; V<NumVTiles; V++ )
	for( Integer U=0; U<NumUTiles; U++ )
	{
		Integer iSlot	= V*NumUTiles+U;

		Float	X1		= (Float)(U+0.f) / (Float)NumUTiles;
		Float	X2		= (Float)(U+1.f) / (Float)NumUTiles;

		Float	Y1		= 1.f - (Float)(V+0.f) / (Float)NumVTiles;
		Float	Y2		= 1.f - (Float)(V+1.f) / (Float)NumVTiles;

		CoordTable[iSlot][0]	= TVector( X1, Y1 );
		CoordTable[iSlot][1]	= TVector( X1, Y2 );
		CoordTable[iSlot][2]	= TVector( X2, Y2 );
		CoordTable[iSlot][3]	= TVector( X2, Y1 );
	}

	// Initialize list.
	TRenderList List( NumPrts );
	List.Bitmap			= Bitmap;
	List.Flags			= POLY_Unlit*bUnlit | !(Level->RndFlags & RND_Lighting);

	// Initialize list.
	for( Integer i=0; i<NumPrts; i++ )
	{
		TParticle&	P = Particles[i];
		Float		Side = P.Size * 0.5f;
		Float		Alpha = 1.f - P.Life * P.MaxLifeInv;
		TColor		Color;

		// Interpolate color.
		if( Alpha < 0.5f )
			Color	= ColorLerp( Colors[0], Colors[1], (Byte)(Alpha*512) );
		else
			Color	= ColorLerp( Colors[1], Colors[2], (Byte)((Alpha-0.5f)*512) );

		// Particle color.
		List.Colors[i*4+0]	= Color;
		List.Colors[i*4+1]	= Color;
		List.Colors[i*4+2]	= Color;
		List.Colors[i*4+3]	= Color;

		// Texture coords.
		List.TexCoords[i*4+0]	= CoordTable[P.iTile][0];
		List.TexCoords[i*4+1]	= CoordTable[P.iTile][1];
		List.TexCoords[i*4+2]	= CoordTable[P.iTile][2];
		List.TexCoords[i*4+3]	= CoordTable[P.iTile][3];

		// Vertices.
		TCoords Coords		= TCoords( P.Location, P.Rotation );
		TVector XAxis = Coords.XAxis * Side,
				YAxis = Coords.YAxis * Side;

		List.Vertices[i*4+0]	= P.Location - YAxis - XAxis;
		List.Vertices[i*4+1]	= P.Location + YAxis - XAxis;
		List.Vertices[i*4+2]	= P.Location + YAxis + XAxis;
		List.Vertices[i*4+3]	= P.Location - YAxis + XAxis;
	}

	// Render particles.
	Canvas->DrawList( List );
#endif

	// Render cloud boundS if emitter are selected.
	if( Base->bSelected )
		Canvas->DrawLineRect
						( 
							Cloud.Center(), 
							Cloud.Size(), 
							0, 
							COLOR_Orange, 
							false 
						);
}


/*-----------------------------------------------------------------------------
    Registration.
-----------------------------------------------------------------------------*/

REGISTER_CLASS_CPP( FEmitterComponent, FExtraComponent, CLASS_Abstract )
{
	BEGIN_ENUM(EParticleParam)
		ENUM_ELEM(PPT_Random)
		ENUM_ELEM(PPT_Linear)
	END_ENUM;
	
	ADD_PROPERTY( MaxParticles,		TYPE_Integer,	1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( LifeRange,		TYPE_Float,		2,	PROP_Editable,	nullptr );
	ADD_PROPERTY( SpawnArea,		TYPE_Vector,	1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( SpawnOffset,		TYPE_Vector,	1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( EmitPerSec,		TYPE_Integer,	1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( SizeParam,		TYPE_Byte,		1,	PROP_Editable,	_EParticleParam );
	ADD_PROPERTY( SizeRange,		TYPE_Float,		2,	PROP_Editable,	nullptr );
	ADD_PROPERTY( bUnlit,			TYPE_Bool,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Colors,			TYPE_Color,		3,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Bitmap,			TYPE_Resource,	1,	PROP_Editable,	FBitmap::MetaClass );
	ADD_PROPERTY( SpinRange,		TYPE_Float,		2,	PROP_Editable,	nullptr );
	ADD_PROPERTY( NumUTiles,		TYPE_Byte,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( NumVTiles,		TYPE_Byte,		1,	PROP_Editable,	nullptr );
	return 0;
}


REGISTER_CLASS_CPP( FPhysEmitterComponent, FEmitterComponent, CLASS_None )
{
	ADD_PROPERTY( SpeedRange,		TYPE_Vector,	2,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Acceleration,		TYPE_Vector,	1,	PROP_Editable,	nullptr );
	return 0;
}


REGISTER_CLASS_CPP( FLissajousEmitterComponent, FEmitterComponent, CLASS_None )
{
	ADD_PROPERTY( Alpha,			TYPE_Float,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Beta,				TYPE_Float,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Delta,			TYPE_Float,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( X,				TYPE_Float,		1,	PROP_Editable,	nullptr );
	ADD_PROPERTY( Y,				TYPE_Float,		1,	PROP_Editable,	nullptr );
	return 0;
}


REGISTER_CLASS_CPP( FWeatherEmitterComponent, FEmitterComponent, CLASS_None )
{
	BEGIN_ENUM(EWeather)
		ENUM_ELEM(WEATHER_Snow);
		ENUM_ELEM(WEATHER_Rain);
	END_ENUM;

	ADD_PROPERTY( WeatherType,		TYPE_Byte,		1,	PROP_Editable,	_EWeather );
	ADD_PROPERTY( SpeedRange,		TYPE_Float,		2,	PROP_Editable,	nullptr );
	return 0;
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/