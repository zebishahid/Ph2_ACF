/*

        FileName :                     CbcInterface.cc
        Content :                      User Interface to the Cbcs
        Programmer :                   Lorenzo BIDEGAIN, Nicolas PIERRE, Georg AUZINGER
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : lorenzo.bidegain@gmail.com, nico.pierre@icloud.com

 */

#include "CbcInterface.h"
#include "../Utils/ConsoleColor.h"

#define DEV_FLAG 0

namespace Ph2_HwInterface
{

	CbcInterface::CbcInterface( const BeBoardFWMap& pBoardMap ) :
		fBoardMap( pBoardMap ),
		fBoardFW( nullptr ),
		prevBoardIdentifier( 65535 )
	{
	}

	CbcInterface::~CbcInterface()
	{

	}

	void CbcInterface::setBoard( uint16_t pBoardIdentifier )
	{
		if ( prevBoardIdentifier != pBoardIdentifier )
		{
			BeBoardFWMap::iterator i = fBoardMap.find( pBoardIdentifier );
			if ( i == fBoardMap.end() )
				std::cout << "The Board: " << +( pBoardIdentifier >> 8 ) <<  " in Shelve " << +( static_cast<uint8_t>( pBoardIdentifier ) ) << "  doesn't exist" << std::endl;
			else
			{
				fBoardFW = i->second;
				prevBoardIdentifier = pBoardIdentifier;
			}
		}
	}


	void CbcInterface::ConfigureCbc( const Cbc* pCbc, bool pVerifLoop, uint32_t pBlockSize )
	{
		this->CbcFastReset(pCbc);
		
		setBoard( pCbc->getBeBoardIdentifier() );

#ifdef __CBCDAQ_DEV__
		static long min( 0 ), sec( 0 );
		struct timeval start0, end;
		long seconds( 0 ), useconds( 0 );

		if ( DEV_FLAG )
			gettimeofday( &start0, 0 );
#endif


		CbcRegMap cCbcRegMap = pCbc->getRegMap();
		CbcRegMap::iterator cIt = cCbcRegMap.begin();

		while ( cIt != cCbcRegMap.end() )
		{
			std::vector<uint32_t> cVecWrite;
			std::vector<uint32_t> cVecRead;

			uint32_t cCounter = 0;

			for ( cIt; cIt != cCbcRegMap.end(); cIt++ )
			{

				if ( cCounter >= pBlockSize ) break;

				EncodeReg( cIt->second, pCbc->getCbcId(), cVecWrite );

				if ( pVerifLoop )
				{
					CbcRegItem cItem = cIt->second;
					cItem.fValue = 0;

					EncodeReg( cItem, pCbc->getCbcId(), cVecRead );
				}
				cCounter++;
			}

			fBoardFW->WriteCbcBlockReg( pCbc->getFeId(), cVecWrite );

			if ( pVerifLoop )
			{
				uint8_t cCbcId = pCbc->getCbcId();

				fBoardFW->ReadCbcBlockReg( pCbc->getFeId(), cVecRead );

				// only if I have a mismatch will i decode word by word and compare
				if ( cVecWrite != cVecRead )
				{

					auto cMismatchWord = std::mismatch( cVecWrite.begin(), cVecWrite.end(), cVecRead.begin() );
					while ( cMismatchWord.first != cVecWrite.end() )
					{
						CbcRegItem cRegItemWrite;
						DecodeReg( cRegItemWrite, cCbcId, *cMismatchWord.first );
						CbcRegItem cRegItemRead;
						DecodeReg( cRegItemRead, cCbcId, *cMismatchWord.second );

						// uint32_t index = std::distance( cVecWrite.end(), cMismatchWord.first );
						// std::advance( cIt, index );
						// std::string cMismatchName = cIt->first;

						std::cout << RED << "\nERROR !!!\nReadback value not the same for Register @ Page: "  << int( cRegItemWrite.fPage ) << " Address: " << int( cRegItemWrite.fAddress ) << "\n" << std::hex << "Written Value : 0x" << int( cRegItemWrite.fValue ) << "\nReadback Value : 0x" << int( cRegItemRead.fValue ) << std::dec << std::endl;
						std::cout << "Cbc Id : " << int( pCbc->getCbcId() ) << RESET << std::endl << std::endl;
						cMismatchWord = std::mismatch( ++cMismatchWord.first, cVecWrite.end(), ++cMismatchWord.second );
						// mypause();
					}
				}

			}
		}

#ifdef __CBCDAQ_DEV__
		if ( DEV_FLAG )
		{
			gettimeofday( &end, 0 );
			seconds = end.tv_sec - start0.tv_sec;
			useconds = end.tv_usec - start0.tv_usec;
			min += ( seconds + useconds / 1000000 ) / 60;
			sec += ( seconds + useconds / 1000000 ) % 60;
			std::cout << "Time took for Cbc register configuration so far = " << min << " min " << sec << " sec." << std::endl;
		}
#endif

	}



	bool CbcInterface::WriteCbcReg( Cbc* pCbc, const std::string& pRegNode, uint8_t pValue, bool pVerifLoop )
	{

#ifdef __CBCDAQ_DEV__
		static long min( 0 ), sec( 0 );
		struct timeval start0, end;
		long seconds( 0 ), useconds( 0 );

		if ( DEV_FLAG )
			gettimeofday( &start0, 0 );
#endif

		CbcRegItem cRegItem = ( pCbc->getRegMap() )[pRegNode];
		std::vector<uint32_t> cVecWrite;
		std::vector<uint32_t> cVecRead;

		cRegItem.fValue = pValue;

		setBoard( pCbc->getBeBoardIdentifier() );

		EncodeReg( cRegItem, pCbc->getCbcId(), cVecWrite );

		fBoardFW->WriteCbcBlockReg( pCbc->getFeId(), cVecWrite );

		pCbc->setReg( pRegNode, pValue );

		if ( pVerifLoop )
		{
			uint8_t cCbcId = pCbc->getCbcId();

			cRegItem.fValue = 0;

			EncodeReg( cRegItem, pCbc->getCbcId(), cVecRead );


			fBoardFW->ReadCbcBlockReg( pCbc->getFeId(), cVecRead );

			if ( cVecWrite != cVecRead )
			{

				CbcRegItem cReadItem;
				CbcRegItem cWriteItem;
				DecodeReg( cWriteItem, cCbcId, cVecWrite.at( 0 ) );
				DecodeReg( cReadItem, cCbcId, cVecRead.at( 0 ) );
				pCbc->setReg( pRegNode, cReadItem.fValue );

				std::cout << RED <<  "ERROR !!!\nReadback Value different for Register : " << pRegNode << "\n" << std::hex << "Written Value : 0x" << +pValue << "\nReadback Value : 0x" << int( cRegItem.fValue ) << std::dec << std::endl;
				std::cout << "Register Adress : " << int( cRegItem.fAddress ) << std::endl;
				std::cout << "Cbc Id : " << +cCbcId << RESET << std::endl << std::endl;
				// mypause();
				return false;
			}

			else return true;
		}


#ifdef __CBCDAQ_DEV__
		if ( DEV_FLAG )
		{
			gettimeofday( &end, 0 );
			seconds = end.tv_sec - start0.tv_sec;
			useconds = end.tv_usec - start0.tv_usec;
			min += ( seconds + useconds / 1000000 ) / 60;
			sec += ( seconds + useconds / 1000000 ) % 60;
			std::cout << "Time took for Cbc register update so far = " << min << " min " << sec << " sec." << std::endl;
		}
		return true;
#endif
	}

	void CbcInterface::WriteCbcMultReg( Cbc* pCbc, const std::vector< std::pair<std::string, uint8_t> >& pVecReq, bool pVerifLoop )
	{

#ifdef __CBCDAQ_DEV__
		static long min( 0 ), sec( 0 );
		struct timeval start0, end;
		long seconds( 0 ), useconds( 0 );

		if ( DEV_FLAG )
			gettimeofday( &start0, 0 );
#endif

		setBoard( pCbc->getBeBoardIdentifier() );

		std::vector<uint32_t> cVecWrite;
		std::vector<uint32_t> cVecRead;

		CbcRegItem cRegItemWrite;
		CbcRegItem cRegItemRead;

		for ( const auto& v : pVecReq )
		{
			cRegItemWrite = ( pCbc->getRegMap() )[v.first];
			cRegItemWrite.fValue = v.second;

			EncodeReg( cRegItemWrite, pCbc->getCbcId(), cVecWrite );

			pCbc->setReg( v.first, cRegItemWrite.fValue );

			if ( pVerifLoop )
			{
				cRegItemRead = ( pCbc->getRegMap() )[v.first];
				cRegItemRead.fValue = 0;

				EncodeReg( cRegItemRead, pCbc->getCbcId(), cVecRead );
			}
		}

		fBoardFW->WriteCbcBlockReg( pCbc->getFeId(), cVecWrite );

		if ( pVerifLoop )
		{
			uint8_t cCbcId = pCbc->getCbcId();

			fBoardFW->ReadCbcBlockReg( pCbc->getFeId(), cVecRead );

			// only if I have a mismatch will i decode word by word and compare
			if ( cVecWrite != cVecRead )
			{

				auto cMismatchWord = std::mismatch( cVecWrite.begin(), cVecWrite.end(), cVecRead.begin() );
				while ( cMismatchWord.first != cVecWrite.end() )
				{
					CbcRegItem cRegItemWrite;
					DecodeReg( cRegItemWrite, cCbcId, *cMismatchWord.first );
					CbcRegItem cRegItemRead;
					DecodeReg( cRegItemRead, cCbcId, *cMismatchWord.second );

					uint32_t index = std::distance( cVecWrite.begin(), cMismatchWord.first );
					std::string cMismatchName = pVecReq.at( index ).first;

					std::cout << RED << "\nERROR !!!\nReadback value not the same for Register : " << cMismatchName << " @ Page: " << int( cRegItemWrite.fPage ) << " Address: " << int( cRegItemWrite.fAddress ) << "\n"  << std::hex << "Written Value : 0x" << int( cRegItemWrite.fValue ) << "\nReadback Value : 0x" << int( cRegItemRead.fValue ) << std::dec << std::endl;
					std::cout << "Cbc Id : " << int( pCbc->getCbcId() ) << RESET << std::endl << std::endl;
					cMismatchWord = std::mismatch( ++cMismatchWord.first, cVecWrite.end(), ++cMismatchWord.second );
					// mypause();
				}
			}
		}

#ifdef __CBCDAQ_DEV__
		if ( DEV_FLAG )
		{
			gettimeofday( &end, 0 );
			seconds = end.tv_sec - start0.tv_sec;
			useconds = end.tv_usec - start0.tv_usec;
			min += ( seconds + useconds / 1000000 ) / 60;
			sec += ( seconds + useconds / 1000000 ) % 60;
			std::cout << "Time took for Cbc register update so far = " << min << " min " << sec << " sec." << std::endl;
		}
#endif


	}

	void CbcInterface::ReadCbcReg( Cbc* pCbc, const std::string& pRegNode )
	{

#ifdef __CBCDAQ_DEV__
		static long min( 0 ), sec( 0 );
		struct timeval start0, end;
		long seconds( 0 ), useconds( 0 );

		if ( DEV_FLAG )
			gettimeofday( &start0, 0 );
#endif

		uint8_t cCbcId;
		CbcRegItem cRegItem = ( pCbc->getRegMap() )[pRegNode];
		std::vector<uint32_t> cVecReq;

		setBoard( pCbc->getBeBoardIdentifier() );

		EncodeReg( cRegItem, pCbc->getCbcId(), cVecReq );

		fBoardFW->ReadCbcBlockReg( pCbc->getFeId(), cVecReq );

		DecodeReg( cRegItem, cCbcId, cVecReq[0] );

#ifdef __CBCDAQ_DEV__
		std::cout << "CbcId : " << +cCbcId << std::endl;
		std::cout << "Value read : " << int( cRegItem.fValue ) << std::endl;
#endif

		pCbc->setReg( pRegNode, cRegItem.fValue );

#ifdef __CBCDAQ_DEV__
		if ( DEV_FLAG )
		{
			gettimeofday( &end, 0 );
			seconds = end.tv_sec - start0.tv_sec;
			useconds = end.tv_usec - start0.tv_usec;
			min += ( seconds + useconds / 1000000 ) / 60;
			sec += ( seconds + useconds / 1000000 ) % 60;
			std::cout << "Time took for Cbc register refresh so far = " << min << " min " << sec << " sec." << std::endl;
		}
#endif


	}


	void CbcInterface::ReadCbcMultReg( Cbc* pCbc, const std::vector<std::string>& pVecReg )
	{

#ifdef __CBCDAQ_DEV__
		static long min( 0 ), sec( 0 );
		struct timeval start0, end;
		long seconds( 0 ), useconds( 0 );

		if ( DEV_FLAG )
			gettimeofday( &start0, 0 );
#endif

		uint8_t cCbcId;
		CbcRegItem cRegItem;
		std::vector<uint32_t> cVecReq;

		setBoard( pCbc->getBeBoardIdentifier() );

		for ( const auto& v : pVecReg )
		{
			cRegItem = ( pCbc->getRegMap() )[v];

			EncodeReg( cRegItem, pCbc->getCbcId(), cVecReq );

			fBoardFW->ReadCbcBlockReg( pCbc->getFeId(), cVecReq );

			DecodeReg( cRegItem, cCbcId, cVecReq[0] );

#ifdef __CBCDAQ_DEV__
			std::cout << "CbcId : " << +cCbcId << std::endl;
			std::cout << "Value read : " << int( cRegItem.fValue ) << std::endl;
#endif

			pCbc->setReg( v, cRegItem.fValue );
		}

#ifdef __CBCDAQ_DEV__
		if ( DEV_FLAG )
		{
			gettimeofday( &end, 0 );
			seconds = end.tv_sec - start0.tv_sec;
			useconds = end.tv_usec - start0.tv_usec;
			min += ( seconds + useconds / 1000000 ) / 60;
			sec += ( seconds + useconds / 1000000 ) % 60;
			std::cout << "Time took for Cbc register refresh so far = " << min << " min " << sec << " sec." << std::endl;
		}
#endif


	}


	void CbcInterface::ReadAllCbc( const Module* pModule )
	{

#ifdef __CBCDAQ_DEV__
		static long min( 0 ), sec( 0 );
		struct timeval start0, end;
		long seconds( 0 ), useconds( 0 );

		if ( DEV_FLAG )
			gettimeofday( &start0, 0 );
#endif
		CbcRegItem cRegItem;
		uint8_t cCbcId;
		std::vector<uint32_t> cVecReq;
		std::vector<std::string> cVecRegNode;

		int cMissed = 0;

		setBoard( pModule->getBeBoardIdentifier() );

		for ( uint8_t i = 0; i < pModule->getNCbc(); i++ )
		{

			if ( pModule->getCbc( i + cMissed ) == nullptr )
			{
				i--;
				cMissed++;
			}

			else
			{

				Cbc* cCbc = pModule->getCbc( i + cMissed );

				const CbcRegMap& cCbcRegMap = cCbc->getRegMap();

				for ( auto& it : cCbcRegMap )
				{
					EncodeReg( it.second, cCbc->getCbcId(), cVecReq );
					cVecRegNode.push_back( it.first );
				}

				fBoardFW->ReadCbcBlockReg( cCbc->getFeId(), cVecReq );

				for ( uint32_t j = 0; j < cVecReq.size(); j++ )
				{
					DecodeReg( cRegItem, cCbcId, cVecReq[j] );

#ifdef __CBCDAQ_DEV__
					std::cout << "CbcId : " << +cCbcId << std::endl;
					std::cout << "Value read : " << int( cRegItem.fValue ) << std::endl;
#endif

					cCbc->setReg( cVecRegNode.at( j ), cRegItem.fValue );
				}
			}
		}

#ifdef __CBCDAQ_DEV__
		if ( DEV_FLAG )
		{
			gettimeofday( &end, 0 );
			seconds = end.tv_sec - start0.tv_sec;
			useconds = end.tv_usec - start0.tv_usec;
			min += ( seconds + useconds / 1000000 ) / 60;
			sec += ( seconds + useconds / 1000000 ) % 60;
			std::cout << "Time took for reading all Cbcs so far = " << min << " min " << sec << " sec." << std::endl;
		}
#endif

	}


	void CbcInterface::WriteBroadcast( const Module* pModule, const std::string& pRegNode, uint32_t pValue )
	{
		setBoard( pModule->getBeBoardIdentifier() );

#ifdef __CBCDAQ_DEV__
		static long min( 0 ), sec( 0 );
		struct timeval start0, end;
		long seconds( 0 ), useconds( 0 );

		if ( DEV_FLAG )
			gettimeofday( &start0, 0 );
#endif

		uint8_t cCbcId = 0xFF;
		std::vector<uint32_t> cVecReq;

		int cMissed = 0;

		for ( uint8_t i = 0; i < pModule->getNCbc(); i++ )
		{
			if ( pModule->getCbc( i + cMissed ) == nullptr )
			{
				i--;
				cMissed++;
			}

			/*
			   It makes the broadcast only the first time it finds an
			   existing Cbc and then update all the other Cbcs.
			 */
			else if ( i == 0 && pModule->getCbc( i + cMissed ) != nullptr )
			{
				Cbc* cCbc = pModule->getCbc( i + cMissed );
				CbcRegItem cRegItem = ( cCbc->getRegMap() )[pRegNode];
				cRegItem.fValue = pValue;

				EncodeReg( cRegItem, cCbcId, cVecReq );

				fBoardFW->WriteCbcBlockReg( cCbc->getFeId(), cVecReq );

				ReadCbcReg( pModule->getCbc( i + cMissed ), pRegNode );
			}

			else
				ReadCbcReg( pModule->getCbc( i + cMissed ), pRegNode );
		}

#ifdef __CBCDAQ_DEV__
		if ( DEV_FLAG )
		{
			gettimeofday( &end, 0 );
			seconds = end.tv_sec - start0.tv_sec;
			useconds = end.tv_usec - start0.tv_usec;
			min += ( seconds + useconds / 1000000 ) / 60;
			sec += ( seconds + useconds / 1000000 ) % 60;
			std::cout << "Time took for writing all Cbcs so far = " << min << " min " << sec << " sec." << std::endl;
		}
#endif

	}

	void CbcInterface::CbcHardReset( const Cbc* pCbc )
	{
		setBoard( pCbc->getBeBoardIdentifier() );

		fBoardFW->WriteReg( CBC_HARD_RESET, 1 );

		usleep( 200000 );

		fBoardFW->WriteReg( CBC_HARD_RESET, 0 );

		usleep( 200000 );
	}

	void CbcInterface::CbcFastReset( const Cbc* pCbc )
	{
		setBoard( pCbc->getBeBoardIdentifier() );

		fBoardFW->WriteReg( CBC_FAST_RESET, 1 );

		usleep( 200000 );

		fBoardFW->WriteReg( CBC_FAST_RESET, 0 );
	}


	void CbcInterface::EncodeReg( const CbcRegItem& pRegItem, uint8_t pCbcId, std::vector<uint32_t>& pVecReq )
	{
		fBoardFW->EncodeReg( pRegItem, pCbcId, pVecReq );
	}


	void CbcInterface::DecodeReg( CbcRegItem& pRegItem, uint8_t pCbcId, uint32_t pWord )
	{
		fBoardFW->DecodeReg( pRegItem, pCbcId, pWord );
	}

}
