/*=============================================================================
    FrGLRender.cpp: OpenGL render class.
    Copyright Jun.2016 Vlad Gordienko.
=============================================================================*/

#include "OpenGLRend.h"
#include "FrGLExt.h"

/*-----------------------------------------------------------------------------
    COpenGLRender implementation.
-----------------------------------------------------------------------------*/

//
// OpenGL rendering constructor.
//
COpenGLRender::COpenGLRender( HWND InhWnd )
{
	// Prepare.
	hWnd	= InhWnd;
	hDc		= GetDC( hWnd );

	// Initialize OpenGL.
	PIXELFORMATDESCRIPTOR Pfd;
	Integer nPixelFormat;
	MemZero( &Pfd, sizeof(PIXELFORMATDESCRIPTOR) );
	Pfd.dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	nPixelFormat	= ChoosePixelFormat( hDc, &Pfd );
	SetPixelFormat( hDc, nPixelFormat, &Pfd );
	hRc				= wglCreateContext( hDc );
	wglMakeCurrent( hDc, hRc );

	// Initialize extensions.
	InitOpenGLext();

	// Allocate canvas.
	Canvas			= new COpenGLCanvas( this );

	// Notify.
	log( L"OpenGL: OpenGL render initialized" );
}


//
// Change the viewport resolution.
//
void COpenGLRender::Resize( Integer NewWidth, Integer NewHeight )
{
	// Clamp.
	NewWidth	= Clamp( NewWidth,  1, MAX_X_RES );
	NewHeight	= Clamp( NewHeight, 1, MAX_Y_RES );

	// Store resolution.
	WinWidth	= NewWidth;
	WinHeight	= NewHeight;

	// Update the OpenGL.
	glViewport( 0, 0, Trunc(WinWidth), Trunc(WinHeight) );
}


//
// Lock the render and start drawing.
//
CCanvas* COpenGLRender::Lock()
{
	// Setup OpenGL.
	glClear( GL_COLOR_BUFFER_BIT );	
	
	// Copy info to canvas.
	Canvas->ScreenWidth		= WinWidth;
	Canvas->ScreenHeight	= WinHeight;
	Canvas->LockTime		= fmod( GPlat->Now(), 1000.f*2.f*PI );

	return Canvas;
}


//
// Blit an image into screen, and 
// unlock the render.
//
void COpenGLRender::Unlock()
{
	SwapBuffers( hDc );
}


//
// Unload all temporal OpenGL stuff.
//
void COpenGLRender::Flush()
{
	TArray<GLuint>	List;

	// Kill all FBitmap's data.
	if( GProject )
		for( Integer i=0; i<GProject->GObjects.Num(); i++ )
			if( GProject->GObjects[i] && GProject->GObjects[i]->IsA(FBitmap::MetaClass) )
			{
				FBitmap* Bitmap	= As<FBitmap>(GProject->GObjects[i]);
				if( Bitmap->RenderInfo != -1 )
					List.Push(Bitmap->RenderInfo);

				Bitmap->RenderInfo	= -1;
			}

	if( List.Num() > 0 )
		glDeleteTextures( List.Num(), &List[0] );
}


//
// OpenGL render destructor.
//
COpenGLRender::~COpenGLRender()
{
	// Release canvas.
	delete Canvas;

	// Release OpenGL context.
	wglMakeCurrent( 0, 0 );
	wglDeleteContext( hRc );

	// Release Window DC.
	ReleaseDC( hWnd, hDc );
}


/*-----------------------------------------------------------------------------
    COpenGLCanvas implementation.
-----------------------------------------------------------------------------*/

//
// GFX effects set.
//
static TPostEffect	GNullEffect;
static TPostEffect	GWarpEffect;
static TPostEffect	GMirrorEffect;


//
// OpenGL canvas constructor.
//
COpenGLCanvas::COpenGLCanvas( COpenGLRender* InRender )
{
	// Initialize fields.
	Render			= InRender;
	StackTop		= 0;
	ScreenWidth		= ScreenHeight = 0.f;
	OldBlend		= BLEND_MAX;
	OldBitmap		= nullptr;
	OldColor		= COLOR_White;
	BitmapPan		= TVector( 0.f, 0.f );
	OldStipple		= POLY_None;

	// Allocate shader.
	Shader		= new COpenGLShader();

	// Set default OpenGL state.
	glActiveTexture( GL_TEXTURE0 );
	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	// Initialize gfx.
	GNullEffect.Highlights[0]	= 1.f;
	GNullEffect.Highlights[1]	= 1.f;
	GNullEffect.Highlights[2]	= 1.f;
	GNullEffect.MidTones[0]		= 1.f;
	GNullEffect.MidTones[1]		= 1.f;
	GNullEffect.MidTones[2]		= 1.f;
	GNullEffect.Shadows[0]		= 0.f;
	GNullEffect.Shadows[1]		= 0.f;
	GNullEffect.Shadows[2]		= 0.f;
	GNullEffect.BWScale			= 0.f;

	GWarpEffect.Highlights[0]	= 1.2f;
	GWarpEffect.Highlights[1]	= 1.f;
	GWarpEffect.Highlights[2]	= 1.f;
	GWarpEffect.MidTones[0]		= 0.9f;
	GWarpEffect.MidTones[1]		= 1.f;
	GWarpEffect.MidTones[2]		= 1.f;
	GWarpEffect.Shadows[0]		= -0.4f;
	GWarpEffect.Shadows[1]		= 0.f;
	GWarpEffect.Shadows[2]		= 0.f;
	GWarpEffect.BWScale			= 0.f;

	GMirrorEffect.Highlights[0]	= 1.f;
	GMirrorEffect.Highlights[1]	= 1.f;
	GMirrorEffect.Highlights[2]	= 1.2f;
	GMirrorEffect.MidTones[0]	= 1.f;
	GMirrorEffect.MidTones[1]	= 1.f;
	GMirrorEffect.MidTones[2]	= 0.9f;
	GMirrorEffect.Shadows[0]	= 0.f;
	GMirrorEffect.Shadows[1]	= 0.f;
	GMirrorEffect.Shadows[2]	= -0.4f;
	GMirrorEffect.BWScale		= 0.f;

	// Set default effect.
	Shader->SetPostEffect( GNullEffect );

	// Notify.
	log( L"OpenGL: OpenGL canvas initialized" );
}


//
// Set OpenGL's projection matrix for TViewInfo, fill
// the entire matrix. Used all parameters such as
// FOV, Zoom, Coords, and now even screen bounds.
//
void COpenGLCanvas::SetTransform( const TViewInfo& Info )
{
	GLfloat M[4][4];
	Float XFOV2, YFOV2;

	// Reset old matrix.
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	// Precompute.
	XFOV2	= 2.f / (Info.FOV.X * Info.Zoom);
	YFOV2	= 2.f / (Info.FOV.Y * Info.Zoom);

	// Screen computations.
	TVector SScale, SOffset;
	SScale.X	= (Info.Width / ScreenWidth);
	SScale.Y	= (Info.Height / ScreenHeight);
	SOffset.X	= (2.f/ScreenWidth) * (Info.X + (Info.Width/2.f)) - 1.f;
	SOffset.Y	= 1.f - (2.f/ScreenHeight) * (Info.Y + (Info.Height/2.f));

	// Compute cells.
    M[0][0]	= XFOV2 * +Info.Coords.XAxis.X * SScale.X;
    M[0][1] = YFOV2 * -Info.Coords.XAxis.Y * SScale.Y;
    M[0][2] = 0.f;
    M[0][3] = 0.f;

    M[1][0] = XFOV2 * -Info.Coords.YAxis.X * SScale.X;
    M[1][1] = YFOV2 * +Info.Coords.YAxis.Y * SScale.Y;
    M[1][2] = 0.f;
    M[1][3] = 0.f;

    M[2][0] = 0.f;
    M[2][1] = 0.f;
    M[2][2] = 1.f;
    M[2][3] = 0.f;

    M[3][0] = -(Info.Coords.Origin.X*M[0][0] + Info.Coords.Origin.Y*M[1][0]) + SOffset.X;
    M[3][1] = -(Info.Coords.Origin.X*M[0][1] + Info.Coords.Origin.Y*M[1][1]) + SOffset.Y;
    M[3][2] = -1.f;
    M[3][3] = 1.f;

    // Send this matrix to OpenGL.
    glLoadMatrixf( (GLfloat*)M );

	// Store this coords system.
	View			= Info;
}


//
// Canvas destructor.
//
COpenGLCanvas::~COpenGLCanvas()
{
	// Release shader.
	delete Shader;
}


/*-----------------------------------------------------------------------------
    Primitives drawing routines.
-----------------------------------------------------------------------------*/

//
// Draw a simple colored point.
//
void COpenGLCanvas::DrawPoint( const TVector& P, Float Size, TColor Color )
{
	glPointSize( Size );
	SetColor( Color );
	SetBitmap( nullptr );

	glBegin( GL_POINTS );
	{
		glVertex2fv( (GLfloat*)&P );
	}
	glEnd();
}


//
// Draw a simple colored line.
//
void COpenGLCanvas::DrawLine( const TVector& A, const TVector& B, TColor Color, Bool bStipple )
{
	SetColor( Color );
	SetBitmap( nullptr );

	if( bStipple )
	{
		// Stippled line.
		glEnable( GL_LINE_STIPPLE );
		glLineStipple( 1, 0x9292 );
	}
	else
	{
		// Solid line.
		glDisable( GL_LINE_STIPPLE );
	}

	glBegin( GL_LINES );
	{
		glVertex2fv( (GLfloat*)&A );
		glVertex2fv( (GLfloat*)&B );
	}
	glEnd();
}


//
// Draw rectangle.
//
void COpenGLCanvas::DrawRect( const TRenderRect& Rect )
{
	TVector Verts[4];

	if( Rect.Rotation.Angle == 0 )
	{
		// No rotation.
		Verts[0] = Rect.Bounds.Min;
		Verts[1] = TVector( Rect.Bounds.Min.X, Rect.Bounds.Max.Y );
		Verts[2] = Rect.Bounds.Max;
		Verts[3] = TVector( Rect.Bounds.Max.X, Rect.Bounds.Min.Y );
	}
	else
	{
		// Rotation.
		TVector Center		= Rect.Bounds.Center();
		TVector Size2		= Rect.Bounds.Size() * 0.5f;
		TCoords Coords		= TCoords( Center, Rect.Rotation );

		TVector XAxis = Coords.XAxis * Size2.X,
				YAxis = Coords.YAxis * Size2.Y;

		// World coords.
		Verts[0] = Center - YAxis - XAxis;
		Verts[1] = Center + YAxis - XAxis;
		Verts[2] = Center + YAxis + XAxis;
		Verts[3] = Center - YAxis + XAxis;
	}

	if( Rect.Flags & POLY_FlatShade )
	{
		// Draw colored rectangle.
		SetBitmap( nullptr );
		SetColor( Rect.Color );
		SetStipple( Rect.Flags );
		if( Rect.Flags & POLY_Ghost )
			SetBlend( BLEND_Translucent );

		glBegin( GL_POLYGON );
		{
			for( Integer i=0; i<4; i++ )
				glVertex2fv( (GLfloat*)&Verts[i] );
		}
		glEnd();
		SetStipple( POLY_None );
	}
	else
	{
		// Draw textured rectangle.
		SetColor( Rect.Color );
		SetBitmap( Rect.Bitmap ? Rect.Bitmap : FBitmap::Default, Rect.Flags & POLY_Unlit );
		if( Rect.Flags & POLY_Ghost )
			SetBlend( BLEND_Brighten );

		// Texture coords.
		TVector T1	= Rect.TexCoords.Min + BitmapPan;
		TVector T2	= Rect.TexCoords.Max + BitmapPan;
		TVector TexVerts[4];
		TexVerts[0]	= TVector( T1.X, T1.Y );
		TexVerts[1]	= TVector( T1.X, T2.Y );
		TexVerts[2]	= TVector( T2.X, T2.Y );
		TexVerts[3]	= TVector( T2.X, T1.Y );

		glBegin( GL_POLYGON );
		{
			for( Integer i=0; i<4; i++ )
			{
				glTexCoord2fv( (GLfloat*)&TexVerts[i] );
				glVertex2fv( (GLfloat*)&Verts[i] );
			}
		}
		glEnd();
	}
}


//
// Draw a list of rectangles.
//
void COpenGLCanvas::DrawList( const TRenderList& List )
{
	if( List.Flags & POLY_FlatShade )
	{
		// Draw a colored rectangles.
		SetBitmap( nullptr );
		if( List.Flags & POLY_Ghost )
			SetBlend( BLEND_Translucent );

		// Set color.
		if( List.Colors )
		{
			glEnableClientState( GL_COLOR_ARRAY );
			glColorPointer( 4, GL_UNSIGNED_BYTE, 0, List.Colors );
		}
		else
			SetColor( List.DrawColor );

		// Render it.
		glVertexPointer( 2, GL_FLOAT, 0, List.Vertices );
		glDrawArrays( GL_QUADS, 0, List.NumRects*4 );

		// Unset color.
		if( List.Colors )
			glDisableClientState( GL_COLOR_ARRAY );
	}
	else
	{
		// Draw textured rectangles.
		SetBitmap( List.Bitmap ? List.Bitmap : FBitmap::Default, List.Flags & POLY_Unlit );
		if( List.Flags & POLY_Ghost )
			SetBlend( BLEND_Brighten );

		// Set color.
		if( List.Colors )
		{
			glEnableClientState( GL_COLOR_ARRAY );
			glColorPointer( 4, GL_UNSIGNED_BYTE, 0, List.Colors );
		}
		else
			SetColor( List.DrawColor );

		// Apply panning if any.
		if( BitmapPan.SizeSquared() > 0.1f )
			for( Integer i=0; i<List.NumRects*4; i++ )
				List.TexCoords[i]	+= BitmapPan;

		// Render it.
		glVertexPointer( 2, GL_FLOAT, 0, List.Vertices );
		glTexCoordPointer( 2, GL_FLOAT, 0, List.TexCoords ); 
		glDrawArrays( GL_QUADS, 0, List.NumRects*4 );

		// Unset color.
		if( List.Colors )
			glDisableClientState( GL_COLOR_ARRAY );
	}
}


//
// Draw a convex polygon.
//
void COpenGLCanvas::DrawPoly( const TRenderPoly& Poly )
{
	if( Poly.Flags & POLY_FlatShade )
	{
		// Draw colored polygon.
		SetBitmap( nullptr );
		SetColor( Poly.Color );
		SetStipple( Poly.Flags );
		if( Poly.Flags & POLY_Ghost )
			SetBlend( BLEND_Translucent );

		glBegin( GL_POLYGON );
		{
			for( Integer i=0; i<Poly.NumVerts; i++ )
				glVertex2fv( (GLfloat*)&Poly.Vertices[i] );
		}
		glEnd();
		SetStipple( POLY_None );
	}
	else
	{
		SetColor( Poly.Color );
		SetBitmap( Poly.Bitmap ? Poly.Bitmap : FBitmap::Default, Poly.Flags & POLY_Unlit );
		if( Poly.Flags & POLY_Ghost )
			SetBlend( BLEND_Brighten );

		glBegin( GL_POLYGON );
		{
			for( Integer i=0; i<Poly.NumVerts; i++ )
			{
				TVector T = Poly.TexCoords[i] + BitmapPan;
				glTexCoord2fv( (GLfloat*)&T );
				glVertex2fv( (GLfloat*)&Poly.Vertices[i] );
			}
		}
		glEnd();
	}
}


/*-----------------------------------------------------------------------------
    Canvas support functions.
-----------------------------------------------------------------------------*/

//
// Convert a paletted image to 32-bit image.
//
static void* Palette8ToRGBA( Byte* SourceData, TColor* Palette, Integer USize, Integer VSize )
{
	static TColor Buffer512[512*512];
	Integer i, n;

    // Doesn't allow palette image with dimension > 512.
	if( USize*VSize > 512*512 )
		return nullptr;
	
// No asm required since VC++ generates well
// optimized code.
#if FLU_ASM && 0
	n = USize*VSize;
	__asm
	{
		mov	esi,	[Palette]
		mov ebx,	[SourceData]
		mov	ecx,	[n]

		align 16
	Reloop:
			movzx	eax,							byte ptr [ebx+ecx]
			mov		edx,							dword ptr [esi+eax*4]
			mov		dword ptr [Buffer512+ecx*4],	edx
			dec		ecx
			jnz Reloop
	}
#else	
	i	= USize * VSize;
	while( i-- > 0 )
	{
		Buffer512[i] = Palette[SourceData[i]];
	}
#endif

	return Buffer512;
}


//
// Set a bitmap for rendering.
// Here possible usage of function:
//  A. Bitmap!=nullptr && bUnlit  - Draw unlit texture.
//  B. Bitamp==nullptr && bUnlit  - Turn off shader, draw colored.
//  C. Bitmap!=nullptr && !bUnlit - Draw complex lit texture.
//  D. Bitamp==nullptr && !bUnlit - see case B.
//
void COpenGLCanvas::SetBitmap( FBitmap* Bitmap, Bool bUnlit )	
{
	if( Bitmap )
	{
		// Setup valid bitmap.
		if( Bitmap != OldBitmap )
		{
			// Load bitmap to OpenGL, if not loaded before.
			if( Bitmap->RenderInfo == -1 )
			{
				glGenTextures( 1, (GLuint*)&Bitmap->RenderInfo );
				glBindTexture( GL_TEXTURE_2D, Bitmap->RenderInfo );
				glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

				if( Bitmap->Format == BF_Palette8 )
				{             
					// Load a Palette image.
					glTexImage2D
							(
								GL_TEXTURE_2D,
								0,
								4,
								Bitmap->USize,
								Bitmap->VSize,
								0,
								GL_RGBA,
								GL_UNSIGNED_BYTE,
								Palette8ToRGBA
											( 
												(Byte*)Bitmap->GetData(), 
												&Bitmap->Palette.Colors[0], 
												Bitmap->USize, 
												Bitmap->VSize 
											)
							);
				}
				else
				{
					// Load a RGBA image.
					glTexImage2D
							( 
								GL_TEXTURE_2D,
								0,
								4,
								Bitmap->USize,
								Bitmap->VSize,
								0,
								GL_RGBA,
								GL_UNSIGNED_BYTE,
								Bitmap->GetData() 
							);
				}
			}

			// Make current.
			glBindTexture( GL_TEXTURE_2D, Bitmap->RenderInfo );

			// Update the dynamic bitmap, if required.
			if( Bitmap->bDynamic && Bitmap->bRedrawn )
			{
				if( Bitmap->Format == BF_Palette8 )
				{             
					// Load a dynamic palette image.
					glTexSubImage2D
								(
									GL_TEXTURE_2D,
									0,
									0,
									0,
									Bitmap->USize,
									Bitmap->VSize,
									GL_RGBA,
									GL_UNSIGNED_BYTE,
									Palette8ToRGBA
												( 
													(Byte*)Bitmap->GetData(), 
													&Bitmap->Palette.Colors[0], 
													Bitmap->USize, 
													Bitmap->VSize 
												)
								);
				}
				else
				{
					// Load a RGBA image.
					glTexSubImage2D
								( 
									GL_TEXTURE_2D,
									0,
									0,
									0,
									Bitmap->USize,
									Bitmap->VSize,
									GL_RGBA,
									GL_UNSIGNED_BYTE,
									Bitmap->GetData() 
								);
				}

				// Wait for the next redraw.
				Bitmap->bRedrawn = false;
			}

			// Set bitmap filter.
			if( Bitmap->Filter == BFILTER_Nearest )
			{
				// 8-bit style.
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			}
			else
			{
				// Smooth style.
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			}

			// Switch blending mode.
			SetBlend( Bitmap->BlendMode );

			// Update bitmap animation.
			Bitmap->Tick();

			// Bitmap panning.
			BitmapPan.X	= Bitmap->PanUSpeed * LockTime;
			BitmapPan.Y	= Bitmap->PanVSpeed * LockTime;
		}

		// Update lit/unlit, even for same bitmap.
		if( bUnlit )
			Shader->SetModeUnlit();
		else
			Shader->SetModeComplex();

		// Saturation scale.
		glUniform1f( Shader->idSaturation,	Bitmap->Saturation );
	}
	else
	{
		// Turn off bitmap.
		Shader->SetModeNone();
		SetBlend( BLEND_Regular );
		OldBitmap	= nullptr;
	}
}


//
// Set a blend mode for rendering.
//
void COpenGLCanvas::SetBlend( EBitmapBlend Blend )
{
	// Don't use OpenGL too often.
	if( Blend == OldBlend )
		return;

	switch( Blend )
	{
		case BLEND_Regular:
			// No blending.
			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			break;

		case BLEND_Masked:
			// Masked ( with rough edges ).
			glDisable( GL_BLEND );
			glEnable( GL_ALPHA_TEST );
			glAlphaFunc( GL_GREATER, 0.95f );
			break;

		case BLEND_Translucent:
			// Additive blending.
			glEnable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
			break;

		case BLEND_Modulated:
			// Modulation.
			glEnable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			break;

		case BLEND_Alpha:
			// Alpha mask.
			glEnable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			break;

		case BLEND_Darken:
			// Dark modulation.
			glEnable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );
			break;

		case BLEND_Brighten:
			// Extra bright addition.
			glEnable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE );
			break;

		case BLEND_FastOpaque:
			// Software rendering only.
			glEnable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
			break;

		default:
			log( L"OpenGL: Bad blend type %d", Blend );
			break;
	}
	OldBlend = Blend;
}


//
// Set color to draw objects.
//
void COpenGLCanvas::SetColor( TColor Color )
{
	// Don't overuse OpenGL.
	if( OldColor == Color )
		return;

	// Set it.
	glColor4ubv( (GLubyte*)&Color );
	OldColor = Color;
}


//
// Set a stipple pattern for poly render.
//
void COpenGLCanvas::SetStipple( DWord Stipple )
{
	Stipple	&= POLY_StippleI | POLY_StippleII;

	if( Stipple == OldStipple )
		return;

	if( Stipple & POLY_StippleI )
	{
		// Polka dot pattern.
		static const GLubyte PolkaDot[128] =
		{
			0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 
			0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44,
			0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 
			0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44,
			0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 
			0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44,
			0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 
			0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44,
			0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};	

		glEnable( GL_POLYGON_STIPPLE );
		glPolygonStipple( PolkaDot );
	}
	else if( Stipple & POLY_StippleII )
	{
		// Pin stripes pattern.
		static const GLubyte PinStripes[128] =
		{
			0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 
			0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 
			0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 
			0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 
			0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 
			0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 
			0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 
			0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 
			0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 
			0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 
			0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 
			0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 
			0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 
			0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 
			0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 
			0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88
		};

		glEnable( GL_POLYGON_STIPPLE );
		glPolygonStipple( PinStripes );
	}
	else
	{
		// No pattern.
		glDisable( GL_POLYGON_STIPPLE );
	}

	OldStipple = Stipple;
}


//
// Set a clipping area.
//
void COpenGLCanvas::SetClip( const TClipArea& Area )
{
	if	( 
			( Area.X == -1 ) && 
			( Area.Y == -1 ) && 
			( Area.Width == -1 ) && 
			( Area.Height == -1 )
		)
	{
		// Turn off clipping.
		glDisable( GL_SCISSOR_TEST );
	}
	else
	{
		// Turn on clipping.
		glEnable( GL_SCISSOR_TEST );
		glScissor
			( 
				Area.X,
				(Integer)ScreenHeight - Area.Y - Area.Height,
				Area.Width,
				Area.Height
			);
	}

	// Store clipping area.
	Clip	= Area;
}


/*-----------------------------------------------------------------------------
    COpenGLShader implementation.
-----------------------------------------------------------------------------*/

//
// Load shader source from the file. Please 'free'
// returned value after use. I'll use C style here.
//
GLchar* LoadShaderCode( String InFileName )
{
#if 0
	// Unicode to ANSI.
	char file_name[1024];
	wcstombs( file_name, *InFileName, array_length(file_name) );

	// Open file.
	FILE* file = fopen( file_name, "r" );
#else
	FILE* file = _wfopen( *InFileName, L"r" );
#endif
	if( !file )
		return 0;

	// Figure out file size.
	fseek( file, 0, SEEK_END );
	int file_size = ftell(file);	
	rewind(file);

	// Allocate buffer and load file.
	GLchar* buffer = (GLchar*)calloc( file_size+1, 1 );
	fread( buffer, 1, file_size, file );

	// Return it.
	return buffer;
}


//
// Shader constructor.
//
COpenGLShader::COpenGLShader()
{
	// Real shader filename.
	String VertShaderFile	= GDirectory + VERT_SHADER_DIR;
	String FragShaderFile	= GDirectory + FRAG_SHADER_DIR;

	// Test files.
	if( !GPlat->FileExists(VertShaderFile) )
		error( L"Vertex shader '%s' not found", *VertShaderFile );
	if( !GPlat->FileExists(FragShaderFile) )
		error( L"Fragment shader '%s' not found", *FragShaderFile );

	// Load vertex shader.
	{
		// Load file.
		GLchar* Text	= LoadShaderCode( VertShaderFile );
		assert(Text);
	
		// Compile shader.
		iglVertShader	= glCreateShader( GL_VERTEX_SHADER );
		glShaderSource( iglVertShader, 1, (const GLchar**)&Text, nullptr );
		glCompileShader( iglVertShader );

		// Test it.
		GLint	Success;
		glGetShaderiv( iglVertShader, GL_COMPILE_STATUS, &Success );
		if( Success != GL_TRUE )
		{
			Char	uError[2048];
			GLchar	cError[2048];

			glGetShaderInfoLog
							(
								iglVertShader,
								array_length(cError),
								nullptr,
								cError
							);

			mbstowcs( uError, cError, array_length(cError) );
			error( L"Failed compile vertex shader with message: %s", uError );
		}

		free(Text);
	}


	// Load fragment shader.
	{
		// Load file.
		GLchar* Text	= LoadShaderCode( FragShaderFile );
		assert(Text);

		// Compile shader.
		iglFragShader	= glCreateShader( GL_FRAGMENT_SHADER );
		glShaderSource( iglFragShader, 1, (const GLchar**)&Text, nullptr );
		glCompileShader( iglFragShader );

		// Test it.
		GLint	Success;
		glGetShaderiv( iglFragShader, GL_COMPILE_STATUS, &Success );
		if( Success != GL_TRUE )
		{
			Char	uError[2048];
			GLchar	cError[2048];

			glGetShaderInfoLog
							(
								iglFragShader,
								array_length(cError),
								nullptr,
								cError
							);

			mbstowcs( uError, cError, array_length(cError) );
			error( L"Failed compile fragment shader with message: %s", uError );
		}

		free(Text);
	}

	// Link program.
	iglProgram	= glCreateProgram();
	glAttachShader( iglProgram, iglVertShader );
	glAttachShader( iglProgram, iglFragShader );
	glLinkProgram( iglProgram );
	glUseProgram( iglProgram );

	// Bind to uniform variables.
	idUnlit				= glGetUniformLocation( iglProgram,	"bUnlit" );
	idRenderLightmap	= glGetUniformLocation( iglProgram, "bRenderLightmap" );
	idBitmap			= glGetUniformLocation( iglProgram,	"Bitmap" );
	idSaturation		= glGetUniformLocation( iglProgram, "Saturation" );
	idMNum				= glGetUniformLocation( iglProgram, "MNum" );
	idANum				= glGetUniformLocation( iglProgram,	"ANum" );
	idGameTime			= glGetUniformLocation( iglProgram, "Time" );
	idAmbientLight		= glGetUniformLocation( iglProgram, "AmbientLight" );

	idEffect.Highlights	= glGetUniformLocation( iglProgram,	"Effect.Highlights" );
	idEffect.MidTones	= glGetUniformLocation( iglProgram,	"Effect.MidTones" );
	idEffect.Shadows	= glGetUniformLocation( iglProgram,	"Effect.Shadows" );
	idEffect.BWScale	= glGetUniformLocation( iglProgram,	"Effect.BWScale" );

	// Bind all lights.
	for( Integer iLight=0; iLight<MAX_LIGHTS; iLight++ )
	{
		char Buffer[64];

		sprintf( Buffer, "ALights[%d].Effect", iLight );
		idALights[iLight].Effect		= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "ALights[%d].Color", iLight );
		idALights[iLight].Color			= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "ALights[%d].Brightness", iLight );
		idALights[iLight].Brightness	= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "ALights[%d].Radius", iLight );
		idALights[iLight].Radius		= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "ALights[%d].Location", iLight );
		idALights[iLight].Location		= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "ALights[%d].Rotation", iLight );
		idALights[iLight].Rotation		= glGetUniformLocation( iglProgram, Buffer );

		sprintf( Buffer, "MLights[%d].Effect", iLight );
		idMLights[iLight].Effect		= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "MLights[%d].Color", iLight );
		idMLights[iLight].Color			= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "MLights[%d].Brightness", iLight );
		idMLights[iLight].Brightness	= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "MLights[%d].Radius", iLight );
		idMLights[iLight].Radius		= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "MLights[%d].Location", iLight );
		idMLights[iLight].Location		= glGetUniformLocation( iglProgram, Buffer );
		sprintf( Buffer, "MLights[%d].Rotation", iLight );
		idMLights[iLight].Rotation		= glGetUniformLocation( iglProgram, Buffer );
	}

	// Only one bitmap supported.
	glUniform1i( idBitmap, 0 );

	// Turn off shader default.
	glUseProgram( 0 );
	bInUse		= false;
	bUnlit		= false;
	bLightmap	= false;

	// Notify.
	log( L"Rend: Shader initialized" );
}


//
// Shader destructor.
//
COpenGLShader::~COpenGLShader()
{
	glUseProgram(0);
	glDeleteShader( iglVertShader );
	glDeleteShader( iglFragShader );
	glDeleteProgram( iglProgram );
}		


//
// Set an post effect for rendering.
//
void COpenGLShader::SetPostEffect( const TPostEffect& InEffect )
{
	// Temporally turn on shader, if disables.
	Bool bWasInUse = bInUse;
	if( !bInUse )
		glUseProgram( iglProgram );

	glUniform3fv( idEffect.Highlights,	1,		InEffect.Highlights );	
	glUniform3fv( idEffect.MidTones,	1,		InEffect.MidTones );
	glUniform3fv( idEffect.Shadows,		1,		InEffect.Shadows );
	glUniform1f( idEffect.BWScale, InEffect.BWScale );

	// Turn off shader, if it was disabled.
	if( !bInUse )
		glUseProgram( 0 );
}


//
// Set an scene ambient color.
//
void COpenGLShader::SetAmbientLight( TColor InAmbient )
{
	// Really need to update?
	static TColor	OldAmbient = COLOR_AliceBlue;
	if( InAmbient == OldAmbient )
		return;
	OldAmbient	= InAmbient;

	// Temporally turn on shader, if disables.
	Bool bWasInUse = bInUse;
	if( !bInUse )
		glUseProgram( iglProgram );

	// Send data to shader.
	GLfloat Ambient[3] = { InAmbient.R/255.f, InAmbient.G/255.f, InAmbient.B/255.f };
	glUniform3fv( idAmbientLight,	1,	Ambient );

	// Turn off shader, if it was disabled.
	if( !bInUse )
		glUseProgram( 0 );
}


//
// Reset a lights lists. Prepare
// manager for lights collection.
//
void COpenGLShader::ResetLights()
{
	if( bInUse )
	{
		// Send to used shader.
		glUniform1i( idMNum, 0 );
		glUniform1i( idANum, 0 );
	}
	else
	{
		// Toggle shader.
		glUseProgram( iglProgram );
		{
			glUniform1i( idMNum, 0 );
			glUniform1i( idANum, 0 );
		}
		glUseProgram( 0 );
	}

	MNum	= 0;
	ANum	= 0;
}


//
// Add a new light to the shader. Return true, if added
// successfully, return false if list overflow, and light not
// added.
//
Bool COpenGLShader::AddLight( FLightComponent* Light, TVector Location, TAngle Rotation )
{
	// Don't add disabled light source.
	if( !Light->bEnabled )
		return true;

	// Temporally turn on shader, if disables.
	Bool bWasInUse = bInUse;
	if( !bInUse )
		glUseProgram( iglProgram );

	TUniformLightSource*	idSource;
	Bool Result		= false;

	// Add lightsource to appropriate lights list.
	if( Light->LightFunc == LF_Additive )
	{
		if( ANum >= MAX_LIGHTS )
			goto Rejected;

		ANum++;
		idSource	= &idALights[ANum-1];
		glUniform1i( idANum, ANum );
	}
	else
	{
		if( MNum >= MAX_LIGHTS )
			goto Rejected;

		MNum++;
		idSource	= &idMLights[MNum-1];
		glUniform1i( idMNum, MNum );
	}

	// Setup lightsource params.
	GLfloat	LightColor[4]	=
	{
		Light->Color.R / 255.f,
		Light->Color.G / 255.f,
		Light->Color.B / 255.f,
		1.f
	};
	
	// Compute brightness according to light type.
	GLfloat Scale;
	switch( Light->LightType )
	{
		case LIGHT_Flicker:		
			Scale	= RandomF();								
			break;

		case LIGHT_Pulse:		
			Scale	= 0.6f + 0.39f*Sin( GPlat->Now()*(2.f*PI)*35.f/60.f );								
			break;

		case LIGHT_SoftPulse:		
			Scale	= 0.9f + 0.09f*Sin( GPlat->Now()*(2.f*PI)*35.f/50.f );							
			break;

		default:
			Scale	= 1.f;
			break;
	}

	// Send information to shader.
	glUniform1i	( idSource->Effect,			(Integer)Light->LightType	);
	glUniform4fv( idSource->Color,		1,	LightColor					);
	glUniform1f	( idSource->Brightness,		Light->Brightness * Scale	);
	glUniform1f	( idSource->Radius,			Light->Radius				);
	glUniform2fv( idSource->Location,	1,	(GLfloat*)&Location			);
	glUniform1f	( idSource->Rotation,		Rotation.ToRads()			);

	// Everything fine.
	Result	 = true;

Rejected:

	// Turn off shader, if it was disabled.
	if( !bInUse )
		glUseProgram( 0 );

	return Result;
}


//
// Set a complex lit mode.
//
void COpenGLShader::SetModeComplex()
{
	if( !bInUse )
	{
		glUseProgram( iglProgram );
		bInUse	= true;
	}
	if( bUnlit )
	{
		glUniform1i( idUnlit, 0 );
		bUnlit	= false;
	}
	if( bLightmap )
	{
		glUniform1i( idRenderLightmap, 0 );
		bLightmap	= false;
	}
}


//
// Set unlit render mode. 
//
void COpenGLShader::SetModeUnlit()
{
	if( !bInUse )
	{
		glUseProgram( iglProgram );
		bInUse	= true;
	}
	if( !bUnlit )
	{
		glUniform1i( idUnlit, 1 );
		bUnlit	= true;
	}
	if( bLightmap )
	{
		glUniform1i( idRenderLightmap, 0 );
		bLightmap	= false;
	}
}


//
// Return to default OpenGL render
// mode. Turn off shader.
//
void COpenGLShader::SetModeNone()
{
	if( bInUse )
	{
		glUseProgram( 0 );
		bInUse	= false;
	}
}


//
// Set lightmap rendering mode.
//
void COpenGLShader::SetModeLightmap()
{
	if( !bInUse )
	{
		glUseProgram( iglProgram );
		bInUse	= true;
	}
	if( !bLightmap )
	{
		glUniform1i( idRenderLightmap, 1 );
		bLightmap	= true;
	}
}


/*-----------------------------------------------------------------------------
    Level rendering.
-----------------------------------------------------------------------------*/

//
// Draw an editor grid with cell size 1x1.
//
void drawGrid( COpenGLCanvas* Canvas )
{
	// Compute bounds.
	Integer CMinX = Trunc(Max<Float>( Canvas->View.Bounds.Min.X, -WORLD_HALF ));
	Integer CMinY = Trunc(Max<Float>( Canvas->View.Bounds.Min.Y, -WORLD_HALF ));
	Integer CMaxX = Trunc(Min<Float>( Canvas->View.Bounds.Max.X, +WORLD_HALF ));
	Integer CMaxY = Trunc(Min<Float>( Canvas->View.Bounds.Max.Y, +WORLD_HALF ));

	// Pick colors.
	TColor GridColor0 = TColor( 0x40, 0x40, 0x40, 0xff );
	TColor GridColor1 = TColor( 0x80, 0x80, 0x80, 0xff );
	
	// Vertical lines.
	for( Integer i=CMinX; i<=CMaxX; i++ )
	{
		TVector V1( i, -WORLD_HALF );
		TVector V2( i, +WORLD_HALF );
	
		if( !(i & 7) )
			Canvas->DrawLine( V1, V2, GridColor1, false );
		else if( !(i & 3) )
			Canvas->DrawLine( V1, V2, GridColor0, false );
		else
			Canvas->DrawLine( V1, V2, GridColor0, true );
	}

	// Horizontal lines.
	for( Integer i=CMinY; i<=CMaxY; i++ )
	{
		TVector V1( -WORLD_HALF, i );
		TVector V2( +WORLD_HALF, i );
	
		if( !(i & 7) )
			Canvas->DrawLine( V1, V2, GridColor1, false );
		else if( !(i & 3) )
			Canvas->DrawLine( V1, V2, GridColor0, false );
		else
			Canvas->DrawLine( V1, V2, GridColor0, true );
	}
}


//
// Render overlay lightmap, in the current 
// view info.
//
void COpenGLCanvas::RenderLightmap()
{
	Shader->SetModeLightmap();
	SetBlend( BLEND_Translucent );

	glBegin( GL_QUADS );
	{
		glVertex2f( View.Bounds.Min.X, View.Bounds.Min.Y );
		glVertex2f( View.Bounds.Min.X, View.Bounds.Max.Y );
		glVertex2f( View.Bounds.Max.X, View.Bounds.Max.Y );
		glVertex2f( View.Bounds.Max.X, View.Bounds.Min.Y );
	}
	glEnd();
}


//
// Draw safe frame for observer.
//
void drawSafeFrame( COpenGLCanvas* Canvas, FCameraComponent* Observer )
{
	// Draw simple editor bounds.	
	Canvas->DrawLineRect
	( 
		Observer->Location, 
		Observer->FOV, 
		Observer->Rotation, 
		COLOR_Peru, 
		false 
	);
}


//
// Render level's sky zone, if any.
//
void drawSkyZone( COpenGLCanvas* Canvas, FLevel* Level, const TViewInfo& Parent )
{
	// Prepare.
	FSkyComponent* Sky = Level->Sky;
	if( !Sky )
		return;

	// Compute sky frustum.
	TVector ViewArea, Eye;
	ViewArea.X	= Sky->Extent;
	ViewArea.Y	= Sky->Extent * Parent.FOV.Y / Parent.FOV.X;

	TRect	SkyDome	= TRect( Sky->Location, Sky->Size );
	Float	Side	= FastSqrt(Sqr(ViewArea.X) + Sqr(ViewArea.Y)),
			Side2	= Side * 0.5f;

	// Transform observer location and apply parallax.
	Eye.X	= Canvas->View.Coords.Origin.X * Sky->Parallax.X + Sky->Offset.X;
	Eye.Y	= Canvas->View.Coords.Origin.Y * Sky->Parallax.Y + Sky->Offset.Y;

	// Azimuth of sky should be wrapped.
	Eye.X	= Wrap( Eye.X, SkyDome.Min.X, SkyDome.Max.X );

	// Height of sky should be clamped.
	Eye.Y	= Clamp( Eye.Y, SkyDome.Min.Y+Side2, SkyDome.Max.Y-Side2 );

	// Sky roll angle.
	TAngle Roll = TAngle(fmodf( Sky->RollSpeed*(Float)GPlat->Now(), 2.f*PI ));	

	// Flags of sides renderings.
	Bool	bDrawWest	= false;
	Bool	bDrawEast	= false;

	// Setup main sky view.
	TViewInfo SkyView	= TViewInfo
	(
		Eye,
		Roll,
		ViewArea,
		/*1.f /*/ Parent.Zoom,
		true,
		Parent.X,
		Parent.Y,
		Parent.Width,
		Parent.Height			
	);
	TViewInfo WestView, EastView;

	// Compute wrapped pieces of sky zone.
	if( Eye.X-Side2 < SkyDome.Min.X )
	{
		// Draw also west piece.
		TVector WestEye = TVector( SkyDome.Max.X+(Eye.X-SkyDome.Min.X), Eye.Y );
		WestView		= TViewInfo
		(
			WestEye,
			Roll,
			ViewArea,
			SkyView.Zoom,
			true,
			SkyView.X,
			SkyView.Y,
			SkyView.Width,
			SkyView.Height	
		);
		bDrawWest	= true;
	}
	if( Eye.X+Side2 > SkyDome.Max.X )
	{
		// Draw also east piece.
		TVector EastEye = TVector( SkyDome.Min.X-(SkyDome.Max.X-Eye.X), Eye.Y );
		EastView		= TViewInfo
		(
			EastEye,
			Roll,
			ViewArea,
			SkyView.Zoom,
			true,
			SkyView.X,
			SkyView.Y,
			SkyView.Width,
			SkyView.Height	
		);
		bDrawEast	= true;
	}

	// Collect lightsource.
	if( Level->RndFlags & RND_Lighting )
	{
		// Clear lights list.
		Canvas->Shader->ResetLights();

		// Collect it.
		for( Integer l=0; l<Level->Lights.Num(); l++ )
		{
			FLightComponent*	Light	= Level->Lights[l];
			FBaseComponent*		Base	= Light->Base;

			// From master view.
			TRect LightRect = TRect( Base->Location, Light->Radius*2.f );
			if( SkyDome.IsOverlap(LightRect) )
			{
				if( !Canvas->Shader->AddLight( Light, Base->Location, Base->Rotation ) )
					break;
			}
			else
				continue;

			// Fake west side.
			LightRect	= TRect( Base->Location-TVector( Sky->Size.X, 0.f ), Light->Radius*2.f );
			if( SkyDome.IsOverlap(LightRect) )
				if( !Canvas->Shader->AddLight( Light, LightRect.Center(), Base->Rotation ) )
					break;

			// Fake east side.
			LightRect	= TRect( Base->Location+TVector( Sky->Size.X, 0.f ), Light->Radius*2.f );
			if( SkyDome.IsOverlap(LightRect) )
				if( !Canvas->Shader->AddLight( Light, LightRect.Center(), Base->Rotation ) )
					break;
		}
	}

	// Render west sky piece.
	if( bDrawWest )
	{
		Canvas->PushTransform( WestView );
		{
			for( Integer i=0; i<Level->RenderObjects.Num(); i++ )
				Level->RenderObjects[i]->Render( Canvas );
		}
		Canvas->PopTransform();
	}

	// Render east sky piece.
	if( bDrawEast )
	{
		Canvas->PushTransform( EastView );
		{
			for( Integer i=0; i<Level->RenderObjects.Num(); i++ )
				Level->RenderObjects[i]->Render( Canvas );
		}
		Canvas->PopTransform();
	}

	// Render central sky piece.
	Canvas->PushTransform( SkyView );
	{
		for( Integer i=0; i<Level->RenderObjects.Num(); i++ )
			Level->RenderObjects[i]->Render( Canvas );

		// Draw overlay lightmap, in sky coords!
		Canvas->RenderLightmap();
	}
	Canvas->PopTransform();		
}


//
// Draw all half-planes from all visible 
// mirrors.
//
void drawHalfPlaneMirror( COpenGLCanvas* Canvas, FLevel* Level, const TViewInfo& Parent )
{
	TRect Observer	= Parent.Bounds;

	for( Integer iPortal=0; iPortal<Level->Portals.Num(); iPortal++ )
	{
		FPortalComponent* Portal = Level->Portals[iPortal];
		if( !Portal->IsA(FMirrorComponent::MetaClass) )
			continue;

		Float HalfWidth = Portal->Width * 0.5f;

		// Fast X reject.
		if	( 
				Portal->Location.X < Observer.Min.X ||
				Portal->Location.X > Observer.Max.X	
			)
			continue;

		// Fast Y reject.
		if	(
				Portal->Location.Y+HalfWidth < Observer.Min.Y ||
				Portal->Location.Y-HalfWidth > Observer.Max.Y
			)
			continue;

		// Yes, mirror visible, render objects.
		TViewInfo MirrorView;
		FMirrorComponent* Mirror = (FMirrorComponent*)Portal;
		Mirror->ComputeViewInfo( Parent, MirrorView );
		Canvas->PushTransform( MirrorView );
		{
			// Collect mirror's light sources.
			if( Level->RndFlags & RND_Lighting )
			{
				Canvas->Shader->ResetLights();

				for( Integer l=0; l<Level->Lights.Num(); l++ )
				{
					FLightComponent* Light = Level->Lights[l];
		
					if( Canvas->View.Bounds.IsOverlap(Light->GetLightRect()) )
						Canvas->Shader->AddLight
						( 
							Light, 
							Light->Base->Location, 
							Light->Base->Rotation 
						);
				}
			}

			// Highlight portal.
			if( !Level->bIsPlaying )
				Canvas->Shader->SetPostEffect(GMirrorEffect);

			// Render level.
			for( Integer i=0; i<Level->RenderObjects.Num(); i++ )			
				Level->RenderObjects[i]->Render( Canvas );

			// Render overlay lightmap.
			if( Level->RndFlags & RND_Lighting )
				Canvas->RenderLightmap();
		}
		Canvas->PopTransform();
	}
}


//
// Draw all half-planes from all visible 
// warps.
//
void drawHalfPlaneWarp( COpenGLCanvas* Canvas, FLevel* Level, const TViewInfo& Parent )
{
	TRect Observer	= Parent.Bounds;

	for( Integer iPortal=0; iPortal<Level->Portals.Num(); iPortal++ )
	{
		FPortalComponent* Portal = Level->Portals[iPortal];
		if( !Portal->IsA(FWarpComponent::MetaClass) )
			continue;

		FWarpComponent* Warp	= (FWarpComponent*)Portal;
		if( !Warp->Other )
			continue;

		Float HalfWidth = Portal->Width * 0.5f;

		// Fast X reject.
		if	( 
				Portal->Location.X+HalfWidth < Observer.Min.X ||
				Portal->Location.X-HalfWidth > Observer.Max.X	
			)
			continue;

		// Fast Y reject.
		if	(
				Portal->Location.Y+HalfWidth < Observer.Min.Y ||
				Portal->Location.Y-HalfWidth > Observer.Max.Y
			)
			continue;

		// Compute warp bounding volume.
		TCoords TestToWorld = Warp->ToWorld();
		TVector V[2];
		V[0] = TransformPointBy( TVector(0.f, +HalfWidth), TestToWorld );
		V[1] = TransformPointBy( TVector(0.f, -HalfWidth), TestToWorld );
		if( !Observer.IsOverlap(TRect( V, 2 )) )
			continue;

		// Yes, warp is visible, render objects.
		TViewInfo WarpView;
		Warp->ComputeViewInfo( Parent, WarpView );

		Canvas->PushTransform( WarpView );
		{
			// Collect mirror's light sources.
			if( Level->RndFlags & RND_Lighting )
			{
				Canvas->Shader->ResetLights();
				
				for( Integer l=0; l<Level->Lights.Num(); l++ )
				{
					FLightComponent*	Light		= Level->Lights[l];
					//TVector				LightPos	= Warp->TransferPoint(Light->Base->Location);

					if( Canvas->View.Bounds.IsOverlap(Light->GetLightRect()) )
						Canvas->Shader->AddLight
						( 
							Light, 
							Light->Base->Location, 
							Light->Base->Rotation 
						);
				}
			}

			// Highlight portal.
			if( !Level->bIsPlaying )
				Canvas->Shader->SetPostEffect(GWarpEffect);

			// Render level.
			for( Integer i=0; i<Level->RenderObjects.Num(); i++ )			
				Level->RenderObjects[i]->Render( Canvas );

			// Render overlay lightmap.
			if( Level->RndFlags & RND_Lighting )
				Canvas->RenderLightmap();
		}
		Canvas->PopTransform();
	}
}


//
// Render objects comparison.
//
Bool RenderObjectCmp( CRenderAddon*const &A, CRenderAddon*const &B )
{
	return A->GetLayer() < B->GetLayer();
}


//
// Render entire level!
//
void COpenGLRender::RenderLevel( CCanvas* InCanvas, FLevel* Level, Integer X, Integer Y, Integer W, Integer H )
{
	// Check pointers.
	assert(Level);
	assert(InCanvas == this->Canvas);

	// Sort render objects according to it layer.
	if( GFrameStamp & 31 )
		Level->RenderObjects.Sort(RenderObjectCmp);
	
	// Update shader time.
	Canvas->Shader->SetModeComplex();
	glUniform1f( Canvas->Shader->idGameTime, Canvas->LockTime );
	Canvas->Shader->SetAmbientLight(COLOR_Black);

	// Clamp level scrolling when we play.
	if( Level->bIsPlaying )
	{
		FCameraComponent* Camera = Level->Camera;

		Camera->Location.X	= Clamp
		(
			Camera->Location.X,
			Level->ScrollClamp.Min.X + Camera->FOV.X*0.5f,
			Level->ScrollClamp.Max.X - Camera->FOV.X*0.5f
		);

		Camera->Location.Y	= Clamp
		(
			Camera->Location.Y,
			Level->ScrollClamp.Min.Y + Camera->FOV.Y*0.5f,
			Level->ScrollClamp.Max.Y - Camera->FOV.Y*0.5f
		);
	}

	// Compute master view.
	TViewInfo MasterView	= TViewInfo
	(
		Level->Camera->Location,
		Level->Camera->Rotation,
		Level->Camera->GetFitFOV( W, H ),
		Level->Camera->Zoom,
		false,
		X,
		Y, 
		W, 
		H
	);

	// Setup clipping area. In game only.
	if( Level->bIsPlaying )
	{
		TVector RealFOV		= MasterView.FOV;
		TVector CamFOV		= Level->Camera->FOV;

		Canvas->SetClip
		(
			TClipArea
			(
				X,
				Y+H*((RealFOV.Y-CamFOV.Y)/2.f)/RealFOV.Y,
				W,
				H*(CamFOV.Y/RealFOV.Y)
			)
		);
	}

	// Select master scene effect.
	TPostEffect MasterEffect;
	if( !(Level->RndFlags & RND_Effects) )
	{
		// Post-effects are turned off.
		MasterEffect	= GNullEffect;
	}
	else if( Level->bIsPlaying )
	{
		// Set effect in game, used interpolator.
		MasterEffect	= Level->GFXManager->GetResult();
	}
	else
	{
		// Default for editor
		MasterEffect	= Level->Effect;
	}

	// Draw scene.
	Canvas->PushTransform( MasterView );
	{
		// Set master effect.
		Canvas->Shader->SetPostEffect( MasterEffect );

		// Render sky zone if any.
		if( Level->Sky && (Level->RndFlags & RND_Backdrop) )
			drawSkyZone( Canvas, Level, MasterView );

		// Draw editor grid.
		if( Level->RndFlags & RND_Grid )
			drawGrid( Canvas );	

		// Set ambient light in level.
		if( Level->RndFlags & RND_Lighting )
			Canvas->Shader->SetAmbientLight(Level->AmbientLight);

		// Handle all portals.
		if( Level->RndFlags & RND_Portals )
		{
			drawHalfPlaneMirror( Canvas, Level, MasterView );
			drawHalfPlaneWarp( Canvas, Level, MasterView );
		}

		// Set master effect.
		Canvas->Shader->SetPostEffect( MasterEffect );

		// Reset lights. And prepare for their collection.
		if( Level->RndFlags & RND_Lighting )
			Canvas->Shader->ResetLights();

		// Collect all light sources from master view.
		if( Level->RndFlags & RND_Lighting )
			for( Integer l=0; l<Level->Lights.Num(); l++ )
			{
				FLightComponent* Light = Level->Lights[l];

				if( Canvas->View.Bounds.IsOverlap(Light->GetLightRect()) )
					Canvas->Shader->AddLight
					(
						Light,
						Light->Base->Location,
						Light->Base->Rotation
					);
			}

		// Render all objects in master view.
		for( Integer i=0; i<Level->RenderObjects.Num(); i++ )
			Level->RenderObjects[i]->Render( Canvas );

		// Draw addition-lighting lightmap.
		if( Level->RndFlags & RND_Lighting )
			Canvas->RenderLightmap();

		// Draw safe frame area.
		if( !Level->bIsPlaying )
			drawSafeFrame( Canvas, Level->Camera );
	}
	Canvas->PopTransform();

	// Render HUD in-game only.
	if( Level->bIsPlaying && (Level->RndFlags & RND_HUD) )
	{
		// Set screen coords.
		Canvas->PushTransform
		(
			TViewInfo
			( 
				Canvas->Clip.X, 
				Canvas->Clip.Y, 
				Canvas->Clip.Width, 
				Canvas->Clip.Height 
			)
		); 
		{
			// Draw each element.
			for( Integer p=0; p<Level->Painters.Num(); p++ )
				Level->Painters[p]->RenderHUD( Canvas );
		}
		Canvas->PopTransform();
	}

	// Turn off scene rendering stuff.
	Canvas->SetClip( CLIP_NONE );
	Canvas->Shader->SetPostEffect( GNullEffect );
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/