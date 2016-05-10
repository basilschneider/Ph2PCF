#include <cstring>

#include "../Utils/Utilities.h"
#include "../tools/Commissioning.h"
#include "../tools/PedeNoise.h"

#include "../Utils/argvparser.h"
#include "TROOT.h"
#include "TApplication.h"


using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

using namespace CommandLineProcessing;


int main( int argc, char* argv[] )
{
	ArgvParser cmd;

	// init
	cmd.setIntroductoryDescription( "CMS Ph2_ACF  Commissioning tool to perform the following procedures:\n-Timing / Latency scan\n-Threshold Scan\n-Stub Latency Scan" );
	// error codes
	cmd.addErrorCode( 0, "Success" );
	cmd.addErrorCode( 1, "Error" );
	// options
	cmd.setHelpOption( "h", "help", "Print this help page" );

	cmd.defineOption( "file", "Hw Description File . Default value: settings/Commission_2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
	cmd.defineOptionAlternative( "file", "f" );

	cmd.defineOption( "latency", "scan the trigger latency", ArgvParser::NoOptionAttribute );
	cmd.defineOptionAlternative( "latency", "l" );

	cmd.defineOption( "stublatency", "scan the stub latency", ArgvParser::NoOptionAttribute );
	cmd.defineOptionAlternative( "stublatency", "s" );

	cmd.defineOption( "noise", "scan the CBC noise per strip", ArgvParser::NoOptionAttribute );
	cmd.defineOptionAlternative( "noise", "n" );

	cmd.defineOption( "signal", "Scan the threshold using physics triggers", ArgvParser::OptionRequiresValue );
	cmd.defineOptionAlternative( "signal", "S" );

	cmd.defineOption( "minimum", "minimum value for latency scan", ArgvParser::OptionRequiresValue );
	cmd.defineOptionAlternative( "minimum", "m" );

	cmd.defineOption( "range", "range in clock cycles for latency scan", ArgvParser::OptionRequiresValue );
	cmd.defineOptionAlternative( "range", "r" );

	cmd.defineOption( "output", "Output Directory . Default value: Results/", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
	cmd.defineOptionAlternative( "output", "o" );

	cmd.defineOption( "batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute );
	cmd.defineOptionAlternative( "batch", "b" );

	int result = cmd.parse( argc, argv );
	if ( result != ArgvParser::NoParserError )
	{
		std::cout << cmd.parseErrorDescription( result );
		exit( 1 );
	}

	// now query the parsing results
	std::string cHWFile = ( cmd.foundOption( "file" ) ) ? cmd.optionValue( "file" ) : "settings/Commissioning.xml";
	bool cLatency = ( cmd.foundOption( "latency" ) ) ? true : false;
	bool cStubLatency = ( cmd.foundOption( "stublatency" ) ) ? true : false;
	bool cSignal = ( cmd.foundOption( "signal" ) ) ? true : false;
	bool cNoise = ( cmd.foundOption( "noise" ) ) ? true : false;
	std::string cDirectory = ( cmd.foundOption( "output" ) ) ? cmd.optionValue( "output" ) : "Results/";
	if ( !cNoise )cDirectory += "Commissioning";
	else if ( cNoise ) cDirectory += "NoiseScan";
	bool batchMode = ( cmd.foundOption( "batch" ) ) ? true : false;

	uint8_t cStartLatency = ( cmd.foundOption( "minimum" ) ) ? convertAnyInt( cmd.optionValue( "minimum" ).c_str() ) :  0;
	uint8_t cLatencyRange = ( cmd.foundOption( "range" ) )   ?  convertAnyInt( cmd.optionValue( "range" ).c_str() ) :  10;
	int     cSignalRange  = ( cmd.foundOption( "signal" ) )  ?  convertAnyInt( cmd.optionValue( "signal" ).c_str() ) :  30;


	TApplication cApp( "Root Application", &argc, argv );
	if ( batchMode ) gROOT->SetBatch( true );
	else TQObject::Connect( "TCanvas", "Closed()", "TApplication", &cApp, "Terminate()" );


		std::string cResultfile;
		if ( cLatency || cStubLatency ) cResultfile = "Latency";
    		else if ( cSignal ) cResultfile = "SignalScan";
		else cResultfile = "Commissioning";

    Tool cTool;
    cTool.InitializeHw ( cHWFile );
    cTool.InitializeSettings ( cHWFile );
    cTool.CreateResultDirectory ( cDirectory );
    cTool.InitResultFile ( cResultfile );
    cTool.StartHttpServer();
    cTool.ConfigureHw();

	if ( !cNoise )
	{
		Commissioning cCommissioning;
        cCommissioning.Inherit(&cTool);


    if ( cLatency || cStubLatency )
    {
      cCommissioning.Initialize(cStartLatency, cLatencyRange );
      // Here comes our Part:
      if ( cLatency ) cCommissioning.ScanLatency( cStartLatency, cLatencyRange );
      if ( cStubLatency ) cCommissioning.ScanStubLatency( cStartLatency, cLatencyRange );
    }
    if ( cSignal ) {
      cCommissioning.Initialize(cStartLatency, cLatencyRange );
      cCommissioning.SignalScan( cSignalRange );
    }
	}

	if ( cNoise )
	{
		PedeNoise cPedeNoise;
        	cPedeNoise.Inherit(&cTool);
    		cPedeNoise.ConfigureHw();
		cPedeNoise.Initialise(); // canvases etc. for fast calibration
		cPedeNoise.measureNoise();
		cPedeNoise.Validate();
		cPedeNoise.SaveResults( );
        	cPedeNoise.dumpConfigFiles();
	}

    cTool.SaveResults();
    cTool.CloseResultFile();
    cTool.Destroy();

	if ( !batchMode ) cApp.Run();
	return 0;

}

