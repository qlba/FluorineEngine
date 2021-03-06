/*=============================================================================
    FrBlock.cpp: A large data manager.
    Copyright Nov.2016 Vlad Gordienko.
=============================================================================*/

#include "Engine.h"

/*-----------------------------------------------------------------------------
    CBlockManager implementation.
-----------------------------------------------------------------------------*/

//
// An initial block cost, by this value set how long block
// will stay in game, without using in seconds.
//
#define BLOCK_BASE_COST		60.0


//
// Data block in-editor constructor.
//
CBlockManager::CBlockManager()
	:	Blocks(),
		FileName(),
		ResFile( nullptr )
{
	assert(GIsEditor);
}


//
// Data block in-game constructor.
//
CBlockManager::CBlockManager( String InFileName )
	:	Blocks(),
		FileName( InFileName )
{
	// Allocate loader.
	if( !GPlat->FileExists(InFileName) )
		error( L"Resource file '%s' not found", *FileName );

	ResFile	= new CFileLoader( InFileName );
}


//
// Manager destructor.
//
CBlockManager::~CBlockManager()
{
	// Close file if any.
	if( !GIsEditor )
		freeandnil(ResFile);

	// Destroy blocks.
	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] )
			freeandnil(Blocks[i]);
}


//
// Return pointer to the block's data.
// If iBlock is not loaded, block will be loaded.
//
void* CBlockManager::GetBlock( Integer iBlock )
{
	assert(iBlock>=0 && iBlock<Blocks.Num());
	assert(Blocks[iBlock]);
	TDataBlock*	Block = Blocks[iBlock];

	// Load block if, it not loaded.
	if( !(Block->Flags & BLOCK_Loaded) )
		UploadBlock( iBlock );

	// Let popular block live longer.
	Block->Cost	= (Block->Flags & BLOCK_Longevity) ? BLOCK_BASE_COST*3.0 : BLOCK_BASE_COST;

	// Return actual data.
	return Block->Data;
}


//
// Return the size of block, even if it not actually
// loaded. This routine doesn't load block no way.
//
DWord CBlockManager::GetBlockSize( Integer iBlock )
{
	assert(iBlock>=0 && iBlock<Blocks.Num());
	assert(Blocks[iBlock]);
	
	return Blocks[iBlock]->Size;
}


/*-----------------------------------------------------------------------------
    Editor routines.
-----------------------------------------------------------------------------*/

//
// Allocate new data block, used only for editor.
//
Integer CBlockManager::AllocateBlock( Integer BytesCount, DWord ExtraFlags )
{
	assert(GIsEditor);
	assert(BytesCount>0);

	// Allocate and initialize block.
	TDataBlock*	B	= new TDataBlock( BytesCount );
	B->Flags		= ExtraFlags | BLOCK_Loaded;
	B->Cost			= 0.0;
	B->FileRecord	= 0;

	Integer iSlot = -1;

	// Try find available slot.
	for( Integer i=0; i<Blocks.Num(); i++ )
		if( !Blocks[i] )
		{
			iSlot	= i;
			break;
		}

	// Add to the list.
	if( iSlot != -1 )
	{
		Blocks[iSlot]	= B;
		return iSlot;
	}
	else
	{
		return Blocks.Push(B);
	}
}


//
// Release the block. Editor only.
//
void CBlockManager::ReleaseBlock( Integer iBlock )
{
	assert(iBlock>=0 && iBlock<Blocks.Num());

	TDataBlock*	B	= Blocks[iBlock];
	assert(B && B->Size>0 && B->Data);

	// Release it.
	delete B;
	Blocks[iBlock]	= nullptr;
}


//
// Save all resources to the file.
//
void CBlockManager::SaveAllBlocks( String InFileName )
{
	// Don't save resources in game.
	assert(GIsEditor);

	CFileSaver Saver(InFileName);

	// Count resources info.
	{
		Integer NumRes = 0, DbSize = 0;
		for( Integer i=0; i<Blocks.Num(); i++ )
			if( Blocks[i] )
			{
				NumRes++;
				DbSize	= i+1;
			}
		Serialize( Saver, DbSize );
		Serialize( Saver, NumRes );
	}

	// Save header of each resource, and store
	// position of data offset in file.
	TArray<Integer> OffsetMap(Blocks.Num());

	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] )
		{
			// Valid block.
			TDataBlock*	B	= Blocks[i];

			// Select best compression for resource.
			{
				B->Flags	&= ~(BLOCK_LZW | BLOCK_RLE);

				Integer RealSize, RLESize, LZWSize;
				CRLECompressor RLE;
				CLZWCompressor LZW;
				
				RealSize	= B->Size;
				RLESize		= RLE.ForecastSize( B->Data, B->Size );
				LZWSize		= LZW.ForecastSize( B->Data, B->Size );
				
				// Make decision.
				if( RealSize-Min(RLESize, LZWSize) <= 8*1024 )
				{
					// Uncompressed data is smaller than compressed, yes
					// this can happen, but really seldom.
				}
				else
				{
					// Select better compression, but we prefer RLE, since it much faster.
					// 16 Kb is not too much.
					B->Flags	|= ( RLESize-LZWSize <= 16*1024 ) ? BLOCK_RLE : BLOCK_LZW;
				}

#if 1
				// Dbg.
				log
				( 
					L"ResMan: %i's resource used %s (%i -> %i)kB", 
					i,  
					(B->Flags&BLOCK_LZW)?L"LZW":(B->Flags&BLOCK_RLE)?L"RLE":L"None",
					RealSize/1024,
					((B->Flags&BLOCK_LZW)?LZWSize:(B->Flags&BLOCK_RLE)?RLESize:RealSize)/1024		
				);
#endif
			}

			// Save block info.
			Serialize( Saver, i );
			Serialize( Saver, B->Flags );
			Serialize( Saver, B->Size );

			// Temporary serialize stuff, next time it will be 
			// offset to block's data.
			B->FileRecord	= -1;
			OffsetMap[i]	= Saver.Tell();
			Serialize( Saver, B->FileRecord );
		}

	// Save all block's data.
	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] )
		{
			// Valid block.
			TDataBlock*	B	= Blocks[i];

			// Update FileRecord.
			B->FileRecord = Saver.Tell();
			Saver.Seek( OffsetMap[i] );
			Serialize( Saver, B->FileRecord );
			Saver.Seek( B->FileRecord );

			if( B->Flags & BLOCK_LZW )
			{
				// Apply LZW compression.
				CLZWCompressor LZW;
				void* OutBuffer;
				DWord OutSize;
				LZW.Encode( B->Data, B->Size, OutBuffer, OutSize );		
				Serialize( Saver, OutSize );
				Saver.SerializeData( OutBuffer, OutSize );
				MemFree( OutBuffer );
			}
			else if( B->Flags & BLOCK_RLE )
			{
				// Apply RLE compression.
				CRLECompressor RLE;
				void* OutBuffer;
				DWord OutSize;
				RLE.Encode( B->Data, B->Size, OutBuffer, OutSize );		
				Serialize( Saver, OutSize );
				Saver.SerializeData( OutBuffer, OutSize );
				MemFree( OutBuffer );
			}
			else
			{
				// Save without compression.
				Saver.SerializeData( B->Data, B->Size );
			}
		}
}


//
// Load all resources from the file.
//
void CBlockManager::LoadAllBlocks( String InFileName )
{
	// Don't load all resources in game.
	assert(GIsEditor);

	CFileLoader Loader(InFileName);

	// Resources count.
	Integer NumRes, DbSize;
	Serialize( Loader, DbSize );
	Serialize( Loader, NumRes );

	// Allocate slots for blocks.
	Blocks.SetNum( DbSize );

	// Load main information about each block.
	for( Integer i=0; i<NumRes; i++ )
	{
		TDataBlock* B	= new TDataBlock();
		Integer iSlot;
		
		// Load.
		Serialize( Loader, iSlot );
		Serialize( Loader, B->Flags );
		Serialize( Loader, B->Size );
		Serialize( Loader, B->FileRecord );

		// Add to list.
		Blocks[iSlot]	= B;
	}

	// Load all blocks data.
	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] )
		{
			TDataBlock* B	= Blocks[i];

			// Goto data.
			Loader.Seek( B->FileRecord );
			
			// Load and uncompress data.
			if( B->Flags & BLOCK_LZW )
			{
				// Load LZW data and uncompress it.
				DWord InSize;
				Serialize( Loader, InSize );
				void* InBuffer	= MemMalloc( InSize );
				Loader.SerializeData( InBuffer, InSize );
				CLZWCompressor LZW;
				LZW.Decode( InBuffer, InSize, B->Data, B->Size );
				MemFree( InBuffer );
				B->Data	= InBuffer;
			}
			else if( B->Flags & BLOCK_RLE )
			{
				// Load RLE data and uncompress it.
				DWord InSize;
				Serialize( Loader, InSize );
				void* InBuffer	= MemMalloc( InSize );
				Loader.SerializeData( InBuffer, InSize );
				CRLECompressor RLE;
				RLE.Decode( InBuffer, InSize, B->Data, B->Size );
				MemFree( InBuffer );
			}
			else
			{
				// Load not compressed data.
				B->Data	= MemMalloc(B->Size);
				Loader.SerializeData( B->Data, B->Size );
			}
		}

	// Mark each resource as loaded.
	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] )
			Blocks[i]->Flags	|= BLOCK_Loaded;
}


/*-----------------------------------------------------------------------------
    Game routines.
-----------------------------------------------------------------------------*/

//
// Unload all blocks, except persistent of course.
// For game only.
//
void CBlockManager::Flush()
{
	assert(!GIsEditor);

	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] )
		{
			TDataBlock* B	= Blocks[i];

			if( (B->Flags & BLOCK_Loaded) && !(B->Flags & BLOCK_Persistent) )
			{
				MemFree( B->Data );
				B->Data		= nullptr;
				B->Flags	&= ~BLOCK_Loaded;
			}
		}
}


//
// Preload all information about blocks. But
// Don't load actual large data blocks.
//
void CBlockManager::LoadMetadata()
{
	assert(!GIsEditor);

	// Resources count.
	Integer NumRes, DbSize;
	Serialize( *ResFile, DbSize );
	Serialize( *ResFile, NumRes );

	// Allocate slots for blocks.
	Blocks.SetNum( DbSize );

	// Load main information about each block.
	for( Integer i=0; i<NumRes; i++ )
	{
		TDataBlock* B	= new TDataBlock();
		Integer iSlot;
		
		// Load.
		Serialize( *ResFile, iSlot );
		Serialize( *ResFile, B->Flags );
		Serialize( *ResFile, B->Size );
		Serialize( *ResFile, B->FileRecord );

		// Add to list.
		Blocks[iSlot]	= B;
	}

	// Mark each resource as not loaded.
	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] )
			Blocks[i]->Flags	&= ~BLOCK_Loaded;

	// Load all persistent blocks.
	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] && (Blocks[i]->Flags & BLOCK_Persistent) )
			UploadBlock( i );
}


//
// Upload single resource. Used to dynamic load
// blocks during playing.
//
void CBlockManager::UploadBlock( Integer iBlock )
{  
	assert(!GIsEditor);
	assert(iBlock>=0 && iBlock<Blocks.Num());
	assert(Blocks[iBlock]);
	assert(!(Blocks[iBlock]->Flags & BLOCK_Loaded));

	// Load it.
	TDataBlock* B		= Blocks[iBlock];
	ResFile->Seek(B->FileRecord);

	// Load and uncompress data.
	if( B->Flags & BLOCK_LZW )
	{
		// Load LZW data and uncompress it.
		DWord InSize;
		Serialize( *ResFile, InSize );
		void* InBuffer	= MemMalloc( InSize );
		ResFile->SerializeData( InBuffer, InSize );
		CLZWCompressor LZW;
		LZW.Decode( InBuffer, InSize, B->Data, B->Size );
		MemFree( InBuffer );
	}
	else if( B->Flags & BLOCK_RLE )
	{
		// Load RLE data and uncompress it.
		DWord InSize;
		Serialize( *ResFile, InSize );
		void* InBuffer	= MemMalloc( InSize );
		ResFile->SerializeData( InBuffer, InSize );
		CRLECompressor RLE;
		RLE.Decode( InBuffer, InSize, B->Data, B->Size );
		MemFree( InBuffer );
	}
	else
	{
		// Load not compressed data.
		B->Data	= MemMalloc(B->Size);
		ResFile->SerializeData( B->Data, B->Size );
	}

	// Mark block as loaded. And let it live 
	// about one minute.
	B->Flags		|= BLOCK_Loaded;
	B->Cost			= (B->Flags & BLOCK_Longevity) ? BLOCK_BASE_COST*3.0 : BLOCK_BASE_COST;

	// Dbg.
	log( L"ResMan: Block %i has been upload", iBlock );
}


//
// Update the block manager.
//
void CBlockManager::Tick( Float Delta )
{
	assert(!GIsEditor);

	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] )
		{
			TDataBlock* B	= Blocks[i];

			if( (B->Flags & BLOCK_Loaded) && !(B->Flags & BLOCK_Persistent) )
			{
				B->Cost -= Delta;

				if( B->Cost <= 0.0 )
				{
					// Block expired.
					MemFree( B->Data );
					B->Data		= nullptr;
					B->Flags	&= ~BLOCK_Loaded;

					// Dbg.
					log( L"ResMan: Block %i expired. Size=%i Kb", i, B->Size/1024 );
				}
			}
		}
}


/*-----------------------------------------------------------------------------
    Utility.
-----------------------------------------------------------------------------*/

//
// Output debug information about the manager.
//
void CBlockManager::DebugManager()
{
	Integer	NumBytes		= 0, 
			NumAvailable	= 0, 
			NumLoaded		= 0;

	// Count statistic.
	for( Integer i=0; i<Blocks.Num(); i++ )
		if( Blocks[i] )
		{
			TDataBlock* B = Blocks[i];

			if( B->Flags & BLOCK_Loaded )
			{
				NumBytes += B->Size;
				NumLoaded++;
			}
		}
		else
			NumAvailable++;

	// Put to the console.
	log( L"**DataBlock manage info:" );
	log( L"   Total used %i slots",			Blocks.Num() );
	log( L"   Used %i kB",					NumBytes / 1024 );
	log( L"   %i slots available",			NumAvailable );
	log( L"   %i blocks are loaded",		NumLoaded );
}


/*-----------------------------------------------------------------------------
    TDataBlock implementation.
-----------------------------------------------------------------------------*/

//
// Block constructor when it loading from
// the file.
//
TDataBlock::TDataBlock()
{
	Data		= nullptr;
	Flags		= BLOCK_None;
	Size		= 0;
	Cost		= 0.0;
	FileRecord	= 0;
}


//
// In editor block constructor.
//
TDataBlock::TDataBlock( Integer InSize )
{
	assert(GIsEditor);

	Data		= MemAlloc(align(InSize, 16));
	Flags		= BLOCK_Loaded;
	Size		= InSize;
	Cost		= 0.0;
	FileRecord	= 0;
}


//
// Data block destructor.
//
TDataBlock::~TDataBlock()
{
	if( Data )
	{
		MemFree( Data );
		Data	= nullptr;
	}
}


/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/