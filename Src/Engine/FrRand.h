/*=============================================================================
    FrRand.h: Random numbers functions.
    Copyright Jul.2016 Vlad Gordienko.
=============================================================================*/

/*-----------------------------------------------------------------------------
    Random functions.
-----------------------------------------------------------------------------*/

//
// Random value in range [0.f .. 1.f]
//
inline Float RandomF()
{
	return (Float)rand() / (Float)RAND_MAX;
}


//
// Random value in range [0..Maximum-1]
//
inline Integer Random( Integer Maximum )
{
	return rand() % Maximum;
}


//
// Random value in range [From..To]
//
inline Integer RandomRange( Integer From, Integer To )
{
	return From + Random(To-From+1);
}


//
// Random value in range [From..To]
//
inline Float RandomRange( Float From, Float To )
{
	return From + (To-From)*RandomF();
}


//
// Random bool value, 50% for
// both.
//
inline Bool RandomBool()
{
	return rand() & 1;
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/