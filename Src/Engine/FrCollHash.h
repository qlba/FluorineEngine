/*=============================================================================
    FrCollHash.h: Grid-based collision hash.
    Copyright Aug.2016 Vlad Gordienko.
=============================================================================*/

/*-----------------------------------------------------------------------------
    CCollisionHash.
-----------------------------------------------------------------------------*/

// How many objects can be overlapped.
#define MAX_COLL_LIST_OBJS		32

// Hash size.
#define COLL_HASH_SIZE			1024
#define COLL_FACTOR				2

//
// Possible hash parameters:
//
//   A. Size = 256;   Factor = 4;
//   B. Size = 512;   Factor = 3;
//   C. Size = 1024;  Factor = 2;
//   D. Size = 2048;  Factor = 1;
//   E. Size = 4096;  Factor = 0;
//


//
// A collision hash.
//
class CCollisionHash
{
public:
	// CCollisionHash interface.
	CCollisionHash( FLevel* InLevel );
	~CCollisionHash();
	void AddToHash( FBaseComponent* Object );
	void RemoveFromHash( FBaseComponent* Object );
	void GetOverlapped( TRect Bounds, Integer& OutNumObjs, FBaseComponent** OutList );
	void GetOverlappedByClass( TRect Bounds, CClass* Class, Integer& OutNumObjs, FBaseComponent** OutList );
	void GetOverlappedByScript( TRect Bounds, FScript* Script, Integer& OutNumObjs, FBaseComponent** OutList );
	void DebugHash();

private:
	// Hash internal.
	struct THashItem
	{
	public:
		FBaseComponent*		Object;
		THashItem*			Next;
	};
	
	FLevel*				Level;
	CMemPool			Pool;
	THashItem*			Hash[COLL_HASH_SIZE];
	THashItem*			FirstAvail;
	DWord				Mark;

	void GetHashIndex( TVector V, Integer& iX, Integer& iY );

	// Stats.
	Integer				HashActivItems;
	Integer				HashObjects;
	Integer				HashNumItems;
};


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/