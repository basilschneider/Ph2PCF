#include <cstring>
#include "../HWDescription/Cbc.h"
#include "../HWDescription/Module.h"
#include "../HWDescription/BeBoard.h"
#include "../HWInterface/CbcInterface.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWDescription/Definition.h"
#include "../tools/Calibration.h"
#include "../tools/OldCalibration.h"
#include "../Utils/argvparser.h"
#include "TROOT.h"
#include "TApplication.h"
#include "../Utils/Timer.h"



using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

using namespace CommandLineProcessing;


int main( int argc, char* argv[] )
{
	ArgvParser cmd;

	// init
	cmd.setIntroductoryDescription( "CMS Ph2_ACF  calibration routine using K. Uchida's algorithm or a fast algoriithm" );
	// error codes
	cmd.addErrorCode( 0, "Success" );
	cmd.addErrorCode( 1, "Error" );
	// options
	cmd.setHelpOption( "h", "help", "Print this help page" );

	cmd.defineOption( "file", "Hw Description File . Default value: settings/Calibration8CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
	cmd.defineOptionAlternative( "file", "f" );

	cmd.defineOption( "output", "Output Directory . Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
	cmd.defineOptionAlternative( "output", "o" );

	cmd.defineOption( "skip", "skip scaning VCth vs Vplus", ArgvParser::NoOptionAttribute );

	cmd.defineOption( "old", "Use old calibration algorithm", ArgvParser::NoOptionAttribute );



	cmd.defineOption( "allChan", "Do calibration using all channels? Default: false", ArgvParser::NoOptionAttribute );
	cmd.defineOptionAlternative( "allChan", "a" );

	cmd.defineOption( "batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute );
	cmd.defineOptionAlternative( "batch", "b" );


	int result = cmd.parse( argc, argv );
	if ( result != ArgvParser::NoParserError )
	{
		std::cout << cmd.parseErrorDescription( result );
		exit( 1 );
	}

	// now query the parsing results
	std::string cHWFile = ( cmd.foundOption( "file" ) ) ? cmd.optionValue( "file" ) : "settings/Calibration8CBC.xml";
	std::string cDirectory = ( cmd.foundOption( "output" ) ) ? cmd.optionValue( "output" ) : "Results/";
	cDirectory += "Calibration";
	bool cVplus = ( cmd.foundOption( "skip" ) ) ? false : true;
	bool cOld = ( cmd.foundOption( "old" ) ) ? true : false;

	bool cCalibrateTGrp = ( cmd.foundOption( "allChan" ) ) ? true : false;
	bool batchMode = ( cmd.foundOption( "batch" ) ) ? true : false;


	TApplication cApp( "Root Application", &argc, argv );
	if ( batchMode ) gROOT->SetBatch( true );
	else TQObject::Connect( "TCanvas", "Closed()", "TApplication", &cApp, "Terminate()" );

	Timer t;

	if ( !cOld )
	{
		t.start();
		Calibration cCalibration;
		cCalibration.InitializeHw( cHWFile );
		cCalibration.InitializeSettings( cHWFile );
		cCalibration.CreateResultDirectory( cDirectory );
		cCalibration.InitResultFile( "CalibrationResults" );
		cCalibration.StartHttpServer();

		cCalibration.ConfigureHw();
		cCalibration.Initialise( false );
		if ( cVplus ) cCalibration.FindVplus();
		cCalibration.FindOffsets();
		cCalibration.SaveResults();
		t.stop();
		t.show( "Time to Calibrate the system: " );
	}
	else
	{
		t.start();
		OldCalibration cCalibration( cCalibrateTGrp );
		cCalibration.InitializeHw( cHWFile );
		cCalibration.InitializeSettings( cHWFile );
		cCalibration.CreateResultDirectory( cDirectory );
		cCalibration.InitResultFile( "CalibrationResults" );
		cCalibration.StartHttpServer();

		cCalibration.Initialise( ); // canvases etc. for fast calibration
		if ( cVplus ) cCalibration.ScanVplus();
		cCalibration.ScanOffset();
		cCalibration.SaveResults();

		t.stop();
		t.show( "Time to Calibrate the system: " );
	}

	if ( !batchMode ) cApp.Run();

	return 0;
}
