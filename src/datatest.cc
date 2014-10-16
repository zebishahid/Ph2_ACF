#include <cstring>
#include "../HWDescription/Cbc.h"
#include "../HWDescription/Module.h"
#include "../HWDescription/BeBoard.h"
#include "../HWInterface/CbcInterface.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWDescription/Definition.h"
#include "../tools/Calibration.h"
#include "../HWInterface/Timer.h"
#include <TApplication.h>
#include <inttypes.h>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

void syntax( int argc )
{
	if ( argc > 4 ) std::cerr << RED << "ERROR: Syntax: calibrationtest VCth NEvents (HWDescriptionFile)" << std::endl;
	else if ( argc < 3 ) std::cerr << RED << "ERROR: Syntax: calibrationtest VCth NEvents (HWDescriptionFile)" << std::endl;
	else return;
}

int main( int argc, char* argv[] )
{
	syntax( argc );
	
	int pEventsperVcth;
	int cVcth;

	if ( sscanf( argv[1], "%xu", &cVcth ) != 1 )
		printf( "ERROR: not an integer" );
	if ( sscanf( argv[2], "%i", &pEventsperVcth ) != 1 )
		printf( "ERROR: not an integer" );
	std::cout << "Taking " << pEventsperVcth << " Events @ VCth of " << cVcth << std::endl;
	std::string cHWFile = argv[3];
//	if ( argc > 1 && !strcmp( argv[3], "8CBC" ) ) cHWFile = "settings/HWDescription_8CBC.xml";
//	else cHWFile = "settings/HybridTest2CBC.xml";

	std::cout << "cHWFile = " << cHWFile << std::endl;
	// TApplication cApp( "Root Application", &argc, argv );
	// TQObject::Connect( "TCanvas", "Closed()", "TApplication", &cApp, "Terminate()" );


        Timer t;
        t.start();
	SystemController cSystemController;

	cSystemController.InitializeHw( cHWFile );
	cSystemController.ConfigureHw();
        t.stop();
        t.show("Time to Initialize/configure the system: ");

        t.start();

	for ( auto cShelve : cSystemController.fShelveVector )
	{
		for ( auto cBoard : ( cShelve )->fBoardVector )
		{
			for ( auto cFe : cBoard.fModuleVector )
			{
				for ( auto cCbc : cFe.fCbcVector )
					cSystemController.fCbcInterface->WriteCbcReg( &cCbc, "VCth", uint8_t( cVcth ) );
			}
		}
	}

        t.stop();
        t.show("Time for changing VCth on all CBCs:");

	uint32_t cN = 0;
	uint32_t cNthAcq = 0;

	while ( cN < pEventsperVcth )
	{
		if(cN == pEventsperVcth) break;
		BeBoard pBoard = cSystemController.fShelveVector.at( 0 )->fBoardVector.at( 0 );
		cSystemController.Run( &pBoard, cNthAcq );

		const Event* cEvent = cSystemController.fBeBoardInterface->GetNextEvent( &pBoard );

		while ( cEvent )
		{
			std::cout << " cVcth = " << uint32_t( cVcth ) << std::endl;
			std::cout << ">>> Event #" << cN << std::endl;
			std::cout << *cEvent << std::endl;
			if ( cN == pEventsperVcth )
				break;
			cN++;

			if ( cN < pEventsperVcth )
				cEvent = cSystemController.fBeBoardInterface->GetNextEvent( &pBoard );
			else break;
		}
		cNthAcq++;
	}


}
