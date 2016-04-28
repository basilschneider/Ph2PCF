#include <cstring>
// #include "../Utils/Utilities.h"
// #include "../HWDescription/Cbc.h"
// #include "../HWDescription/Module.h"
// #include "../HWDescription/BeBoard.h"
// #include "../HWInterface/CbcInterface.h"
// #include "../HWInterface/BeBoardInterface.h"
// #include "../HWDescription/Definition.h"
//#include "../tools/Calibration.h"
#include "../Utils/Timer.h"
//#include <TApplication.h>
// #include <inttypes.h>
#include "../Utils/argvparser.h"
// #include "../Utils/ConsoleColor.h"
#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

using namespace CommandLineProcessing;

//Class used to process events acquired by a parallel acquisition


void syntax( int argc )
{
	if ( argc > 4 ) std::cerr << RED << "ERROR: Syntax: calibrationtest VCth NEvents (HWDescriptionFile)" << std::endl;
	else if ( argc < 3 ) std::cerr << RED << "ERROR: Syntax: calibrationtest VCth NEvents (HWDescriptionFile)" << std::endl;
	else return;
}

int main( int argc, char* argv[] )
{

	int cVcth;

	SystemController cSystemController;
	ArgvParser cmd;

	// init
	cmd.setIntroductoryDescription( "CMS Ph2_ACF  configuration binary" );
	// error codes
	cmd.addErrorCode( 0, "Success" );
	cmd.addErrorCode( 1, "Error" );
	// options
	cmd.setHelpOption( "h", "help", "Print this help page" );



	cmd.defineOption( "file", "Hw Description File . Default value: settings/Beamtest_Nov15.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
	cmd.defineOptionAlternative( "file", "f" );

	cmd.defineOption( "vcth", "Threshold in VCth units (hex (including 0x) or decimal) . Default values from HW description .XML file", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
	cmd.defineOptionAlternative( "vcth", "v" );


	int result = cmd.parse( argc, argv );
	if ( result != ArgvParser::NoParserError )
	{
		std::cout << cmd.parseErrorDescription( result );
		exit( 1 );
	}

	// now query the parsing results
	std::string cHWFile = ( cmd.foundOption( "file" ) ) ? cmd.optionValue( "file" ) : "settings/Beamtest_Nov15.xml";



	// std::string cOptionWrite = ( cmd.foundOption( "option" ) ) ? cmd.optionValue( "option" ) : "w+";
	cVcth = ( cmd.foundOption( "vcth" ) ) ? convertAnyInt( cmd.optionValue( "vcth" ).c_str() ) : 0;

	Timer t;
	t.start();

	cSystemController.InitializeHw( cHWFile );
	cSystemController.ConfigureHw( std::cout );

	t.stop();
	t.show( "Time to Initialize/configure the system: " );

	if ( cVcth != 0 )
	{
		t.start();


		CbcRegWriter cWriter( cSystemController.fCbcInterface, "VCth", uint8_t( cVcth ) );
		cSystemController.accept( cWriter );

		t.stop();
		t.show( "Time for changing VCth on all CBCs:" );
		CbcRegReader cReader( cSystemController.fCbcInterface, "VCth" );
		cSystemController.accept( cReader );
	}
}
