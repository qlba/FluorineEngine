/*=============================================================================
    FrAudio.h: Audio classes.
    Copyright Aug.2016 Vlad Gordienko.
=============================================================================*/

/*-----------------------------------------------------------------------------
    FMusic.
-----------------------------------------------------------------------------*/

//
// A level soundtrack.
//
class FMusic: public FResource
{
REGISTER_CLASS_H(FMusic);
public:
	// FMusic interface.
	FMusic();
	~FMusic();

	// FObject interface.
	void PostLoad();
};


/*-----------------------------------------------------------------------------
    FSound.
-----------------------------------------------------------------------------*/

//
// A FX or ambient sound.
//
class FSound: public FBlockResource
{
REGISTER_CLASS_H(FSound);
public:
	// Variables.
	Integer			AudioInfo;

	// FSound interface.
	FSound();
	~FSound();

	// FObject interface.
	void PostLoad();
};


/*-----------------------------------------------------------------------------
    CAudioBase.
-----------------------------------------------------------------------------*/

//
// An abstract audio system.
//
class CAudioBase
{
public:
	// Variables.
	Float			MasterVolume;
	Float			MusicVolume;
	Float			FXVolume;

	// CAudioBase interface.
	virtual void Flush() = 0;
	virtual void Tick( Float Delta, FLevel* Scene=nullptr ) = 0;
	virtual void PlayMusic( FMusic* Music, Float FadeTime ) = 0;
	virtual void PlayFX( FSound* Sound, Float Gain, Float Pitch ) = 0;
	virtual void PlayAmbient( FSound* Sound, TVector Location, Float Radius, Float Gain, Float Pitch, FObject* Owner ) = 0;
	virtual void StopAmbient( FObject* Owner ) = 0;
	virtual void FlushAmbients() = 0;
};


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/