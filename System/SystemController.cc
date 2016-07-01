/*

        FileName :                    SystemController.cc
        Content :                     Controller of the System, overall wrapper of the framework
        Programmer :                  Nicolas PIERRE
        Version :                     1.0
        Date of creation :            10/08/14
        Support :                     mail to : nicolas.pierre@cern.ch

 */

#include "SystemController.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

namespace Ph2_System {

    SystemController::SystemController()
        : fFileHandler (nullptr)
    {
    }

    SystemController::~SystemController()
    {
    }
    void SystemController::Destroy()
    {
        if (fFileHandler) delete fFileHandler;

        delete fBeBoardInterface;
        delete fCbcInterface;
        fBeBoardFWMap.clear();
        fSettingsMap.clear();

        for ( auto& el : fBoardVector )
            delete el;

        fBoardVector.clear();
    }

    void SystemController::addFileHandler ( const std::string& pFilename , char pOption )
    {
        //if the opion is read, create a handler object and use it to read the
        //file in the method below!
        if (pOption = 'r')
            fFileHandler = new FileHandler ( pFilename, pOption );
        //if the option is w, remember the filename and construct a new
        //fileHandler for every Interface
        else if (pOption = 'w')
        {
            fRawFileName = pFilename;
            fWriteHandlerEnabled = true;
        }
    }

    void SystemController::readFile ( std::vector<uint32_t>& pVec, uint32_t pNWords32 )
    {
        if (pNWords32 == 0) pVec = fFileHandler->readFile( );
        else pVec = fFileHandler->readFileChunks (pNWords32);
    }

    void SystemController::InitializeHw ( const std::string& pFilename, std::ostream& os )
    {
        if ( pFilename.find ( ".xml" ) != std::string::npos )
            parseHWxml ( pFilename, os );
        else
            std::cerr << "Could not parse settings file " << pFilename << " - it is neither .xml nor .json format!" << std::endl;

        this->initializeFileHandler();
    }

    void SystemController::InitializeSettings ( const std::string& pFilename, std::ostream& os )
    {
        if ( pFilename.find ( ".xml" ) != std::string::npos )
            parseSettingsxml ( pFilename, os );
        else
            std::cerr << "Could not parse settings file " << pFilename << " - it is neither .xml nor .json format!" << std::endl;
    }

    void SystemController::ConfigureHw ( std::ostream& os , bool bIgnoreI2c )
    {
        os << std::endl << BOLDBLUE << "Configuring HW parsed from .xml file: " << RESET << std::endl;

        bool cHoleMode, cCheck;

        if ( !fSettingsMap.empty() )
        {
            SettingsMap::iterator cSetting = fSettingsMap.find ( "HoleMode" );

            if ( cSetting != fSettingsMap.end() )
                cHoleMode = cSetting->second;

            cCheck = true;
        }
        else cCheck = false;

        for (auto& cBoard : fBoardVector)
        {
            fBeBoardInterface->ConfigureBoard ( cBoard );
            fBeBoardInterface->CbcHardReset ( cBoard );

            if ( cCheck && cBoard->getBoardType() == "GLIB")
            {
                fBeBoardInterface->WriteBoardReg ( cBoard, "pc_commands2.negative_logic_CBC", ( ( cHoleMode ) ? 0 : 1 ) );
                os << GREEN << "Overriding GLIB register values for signal polarity with value from settings node!" << RESET << std::endl;
            }

            os << GREEN << "Successfully configured Board " << int ( cBoard->getBeId() ) << RESET << std::endl;

            for (auto& cFe : cBoard->fModuleVector)
            {
                for (auto& cCbc : cFe->fCbcVector)
                {
                    if ( !bIgnoreI2c )
                    {
                        fCbcInterface->ConfigureCbc ( cCbc );
                        os << GREEN <<  "Successfully configured Cbc " << int ( cCbc->getCbcId() ) << RESET << std::endl;
                    }
                }
            }

            //CbcFastReset as per recommendation of Mark Raymond
            fBeBoardInterface->CbcFastReset ( cBoard );
        }
    }

    //void SystemController::Run ( BeBoard* pBeBoard )
    //{
    //fBeBoardInterface->Start ( pBeBoard );
    //fBeBoardInterface->ReadData ( pBeBoard, true );
    //fBeBoardInterface->Stop ( pBeBoard );
    //}

    void SystemController::parseHWxml ( const std::string& pFilename, std::ostream& os )
    {
        pugi::xml_document doc;
        uint32_t cBeId, cModuleId, cCbcId;
        uint32_t cNBeBoard = 0;
        int i, j;

        pugi::xml_parse_result result = doc.load_file ( pFilename.c_str() );

        if ( !result )
        {
            os << "ERROR :\n Unable to open the file : " << pFilename << std::endl;
            os << "Error description : " << result.description() << std::endl;
            return;
        }

        os << "\n\n\n";

        for ( i = 0; i < 80; i++ )
            os << "*";

        os << "\n";

        for ( j = 0; j < 40; j++ )
            os << " ";

        os << BOLDRED << "HW SUMMARY: " << RESET << std::endl;

        for ( i = 0; i < 80; i++ )
            os << "*";

        os << "\n";
        os << "\n";
        const std::string strUhalConfig = expandEnvironmentVariables (doc.child ( "HwDescription" ).child ( "Connections" ).attribute ( "name" ).value() );

        // Iterate over the BeBoard Nodes
        for ( pugi::xml_node cBeBoardNode = doc.child ( "HwDescription" ).child ( "BeBoard" ); cBeBoardNode; cBeBoardNode = cBeBoardNode.next_sibling() )
        {
            BeBoard* cBeBoard = this->parseBeBoard (cBeBoardNode, os);
            std::string cBoardType = cBeBoardNode.attribute ( "boardType" ).value();
            cBeBoard->setBoardType (cBoardType);
            pugi::xml_node cBeBoardConnectionNode = cBeBoardNode.child ("connection");

            std::string cId = cBeBoardConnectionNode.attribute ( "id" ).value();
            std::string cUri = cBeBoardConnectionNode.attribute ( "uri" ).value();
            std::string cAddressTable = expandEnvironmentVariables (cBeBoardConnectionNode.attribute ( "address_table" ).value() );

            if (!strUhalConfig.empty() )
                RegManager::setDummyXml (strUhalConfig);

            std::cout << BOLDBLUE << "	" <<  "|"  << "----" << "Board Id: " << BOLDYELLOW << cId << BOLDBLUE << " URI: " << BOLDYELLOW << cUri << BOLDBLUE << " Address Table: " << BOLDYELLOW << cAddressTable;
            std::cout << BOLDBLUE << " Type: " << BOLDYELLOW << cBoardType << RESET << std::endl;

            //else std::cout << BOLDBLUE << "   " <<  "|"  << "----" << "Board Id: " << BOLDYELLOW << cId << BOLDBLUE << " Type: " << BOLDYELLOW << cBoardType << RESET << std::endl;

            // Iterate over the BeBoardRegister Nodes
            for ( pugi::xml_node cBeBoardRegNode = cBeBoardNode.child ( "Register" ); cBeBoardRegNode/* != cBeBoardNode.child( "Module" )*/; cBeBoardRegNode = cBeBoardRegNode.next_sibling() )
            {
                this->parseRegister (cBeBoardRegNode, cBeBoard, os);
                // os << BOLDCYAN << "|" << "  " << "|" << "_____" << cBeBoardRegNode.name() << "  " << cBeBoardRegNode.first_attribute().name() << " :" << cBeBoardRegNode.attribute( "name" ).value() << RESET << std:: endl;
            }

            if ( !cBoardType.compare ( std::string ( "GLIB" ) ) )
                fBeBoardFWMap[cBeBoard->getBeBoardIdentifier()] =  new GlibFWInterface ( cId.c_str(), cUri.c_str(), cAddressTable.c_str() );
            else if ( !cBoardType.compare ( std::string ( "ICGLIB" ) ) )
                fBeBoardFWMap[cBeBoard->getBeBoardIdentifier()] =  new ICGlibFWInterface ( cId.c_str(), cUri.c_str(), cAddressTable.c_str() );
            else if ( !cBoardType.compare ( std::string ( "CTA" ) ) )
                fBeBoardFWMap[cBeBoard->getBeBoardIdentifier()] =  new CtaFWInterface ( cId.c_str(), cUri.c_str(), cAddressTable.c_str() );
            else if ( !cBoardType.compare ( std::string ( "ICFC7" ) ) )
                fBeBoardFWMap[cBeBoard->getBeBoardIdentifier()] =  new ICFc7FWInterface ( cId.c_str(), cUri.c_str(), cAddressTable.c_str() );

            /*else
              cBeBoardFWInterface = new OtherFWInterface();*/

            // Iterate the module node
            for ( pugi::xml_node cModuleNode = cBeBoardNode.child ( "Module" ); cModuleNode; cModuleNode = cModuleNode.next_sibling() )
            {
                if ( static_cast<std::string> ( cModuleNode.name() ) == "Module" )
                {
                    bool cStatus = cModuleNode.attribute ( "Status" ).as_bool();

                    //std::cout << cStatus << std::endl;
                    if ( cStatus )
                    {
                        os << BOLDCYAN << "|" << "	" << "|" << "----" << cModuleNode.name() << "  "
                           << cModuleNode.first_attribute().name() << " :" << cModuleNode.attribute ( "ModuleId" ).value() << RESET << std:: endl;

                        cModuleId = cModuleNode.attribute ( "ModuleId" ).as_int();

                        Module* cModule = new Module ( cBeId, cModuleNode.attribute ( "FMCId" ).as_int(), cModuleNode.attribute ( "FeId" ).as_int(), cModuleId );
                        cBeBoard->addModule ( cModule );

                        this->parseCbc (cModuleNode, cModule, os);
                    }
                }
            }

        }

        cNBeBoard++;

        fBeBoardInterface = new BeBoardInterface ( fBeBoardFWMap );
        fCbcInterface = new CbcInterface ( fBeBoardFWMap );
        os << "\n";
        os << "\n";

        for ( i = 0; i < 80; i++ )
            os << "*";

        os << "\n";

        for ( j = 0; j < 40; j++ )
            os << " ";

        os << BOLDRED << "END OF HW SUMMARY: " << RESET << std::endl;

        for ( i = 0; i < 80; i++ )
            os << "*";

        os << "\n";
        os << "\n";
    }

    BeBoard* SystemController::parseBeBoard (pugi::xml_node pNode, std::ostream& os)
    {
        uint32_t cBeId = pNode.attribute ( "Id" ).as_int();
        BeBoard* cBeBoard = new BeBoard ( cBeId );

        os << BOLDCYAN << "|" << "----" << pNode.name() << "  " << pNode.first_attribute().name() << " :" << pNode.attribute ( "Id" ).value() << RESET << std:: endl;

        pugi::xml_node cBeBoardFWVersionNode = pNode.child ( "FW_Version" );
        uint16_t cNCbcDataSize = 0;
        cNCbcDataSize = static_cast<uint16_t> ( cBeBoardFWVersionNode.attribute ( "NCbcDataSize" ).as_int() );

        if ( cNCbcDataSize != 0 ) os << BOLDCYAN << "|" << "	" << "|" << "----" << cBeBoardFWVersionNode.name() << " NCbcDataSize: " << cNCbcDataSize  <<  RESET << std:: endl;

        cBeBoard->setNCbcDataSize ( cNCbcDataSize );
        fBoardVector.push_back ( cBeBoard );
        return cBeBoard;
    }

    void SystemController::parseRegister (pugi::xml_node pNode, BeBoard* pBoard, std::ostream& os)
    {
        if (std::string (pNode.name() ) == "Register")
        {
            // os << BOLDCYAN << "|" << "  " << "|" << "_____" << cBeBoardRegNode.name() << "  " << cBeBoardRegNode.first_attribute().name() << " :" << cBeBoardRegNode.attribute( "name" ).value() << RESET << std:: endl;
            pBoard->setReg ( static_cast<std::string> ( pNode.attribute ( "name" ).value() ), std::stoi ( pNode.first_child().value() ) );
        }
    }

    void SystemController::parseCbc (pugi::xml_node pModuleNode, Module* pModule, std::ostream& os )
    {
        pugi::xml_node cCbcPathPrefixNode = pModuleNode.child ( "CBC_Files" );
        std::string cFilePrefix = expandEnvironmentVariables (static_cast<std::string> ( cCbcPathPrefixNode.attribute ( "path" ).value() ) );

        if ( !cFilePrefix.empty() ) os << GREEN << "|" << "	" << "|" << "	" << "|" << "----" << "CBC Files Path : " << cFilePrefix << RESET << std::endl;

        // Iterate the CBC node
        for ( pugi::xml_node cCbcNode = pModuleNode.child ( "CBC" ); cCbcNode; cCbcNode = cCbcNode.next_sibling() )
        {
            os << BOLDCYAN << "|" << "	" << "|" << "	" << "|" << "----" << cCbcNode.name() << "  "
               << cCbcNode.first_attribute().name() << " :" << cCbcNode.attribute ( "Id" ).value()
               << ", File: " << expandEnvironmentVariables (cCbcNode.attribute ( "configfile" ).value() ) << RESET << std:: endl;


            std::string cFileName;

            if ( !cFilePrefix.empty() )
                cFileName = cFilePrefix + expandEnvironmentVariables (cCbcNode.attribute ( "configfile" ).value() );
            else cFileName = expandEnvironmentVariables (cCbcNode.attribute ( "configfile" ).value() );

            Cbc* cCbc = new Cbc ( pModule->getBeId(), pModuleNode.attribute ( "FMCId" ).as_int(), pModuleNode.attribute ( "FeId" ).as_int(), cCbcNode.attribute ( "Id" ).as_int(), cFileName );

            for ( pugi::xml_node cCbcRegisterNode = cCbcNode.child ( "Register" ); cCbcRegisterNode; cCbcRegisterNode = cCbcRegisterNode.next_sibling() )
                cCbc->setReg ( std::string ( cCbcRegisterNode.attribute ( "name" ).value() ), atoi ( cCbcRegisterNode.first_child().value() ) );

            for ( pugi::xml_node cCbcGlobalNode = pModuleNode.child ( "Global_CBC_Register" ); cCbcGlobalNode != pModuleNode.child ( "CBC" ) && cCbcGlobalNode != pModuleNode.child ( "CBC_Files" ) && cCbcGlobalNode != nullptr; cCbcGlobalNode = cCbcGlobalNode.next_sibling() )
            {

                if ( cCbcGlobalNode != nullptr )
                {
                    std::string regname = std::string ( cCbcGlobalNode.attribute ( "name" ).value() );
                    uint32_t regvalue = convertAnyInt ( cCbcGlobalNode.first_child().value() ) ;
                    cCbc->setReg ( regname, uint8_t ( regvalue ) ) ;

                    os << GREEN << "|" << "	" << "|" << "	" << "|" << "----" << cCbcGlobalNode.name()
                       << "  " << cCbcGlobalNode.first_attribute().name() << " :"
                       << regname << " =  0x" << std::hex << std::setw ( 2 ) << std::setfill ( '0' ) << regvalue << std::dec << RESET << std:: endl;
                }
            }

            pModule->addCbc (cCbc);
        }
    }

    void SystemController::parseSettingsxml ( const std::string& pFilename, std::ostream& os )
    {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file ( pFilename.c_str() );

        if ( !result )
        {
            os << "ERROR :\n Unable to open the file : " << pFilename << std::endl;
            os << "Error description : " << result.description() << std::endl;
            return;
        }

        for ( pugi::xml_node nSettings = doc.child ( "Settings" ); nSettings; nSettings = nSettings.next_sibling() )
        {
            for ( pugi::xml_node nSetting = nSettings.child ( "Setting" ); nSetting; nSetting = nSetting.next_sibling() )
            {
                fSettingsMap[nSetting.attribute ( "name" ).value()] = convertAnyInt ( nSetting.first_child().value() );
                os << RED << "Setting" << RESET << " --" << BOLDCYAN << nSetting.attribute ( "name" ).value() << RESET << ":" << BOLDYELLOW << convertAnyInt ( nSetting.first_child().value() ) << RESET << std:: endl;
            }
        }
    }

    void SystemController::initializeFileHandler()
    {
        if ( fWriteHandlerEnabled )
        {
            std::cout << BOLDBLUE << "Saving binary raw data to: " << fRawFileName << ".fedId" << RESET << std::endl;

            // here would be the ideal position to fill the file Header and call openFile when in read mode
            for (const auto& cBoard : fBoardVector)
            {
                char cBoardType[8];
                std::string cBoardTypeString = cBoard->getBoardType();
                std::copy (cBoardTypeString.begin(), cBoardTypeString.end(), cBoardType );
                uint32_t cBeId = cBoard->getBeId();
                uint32_t cNCbc = 0;

                for (const auto& cFe : cBoard->fModuleVector)
                    cNCbc += cFe->getNCbc();

                //nCbcDataSize is set to 0 if not explicitly set
                uint32_t cNEventSize32 = (cBoard->getNCbcDataSize() == 0 ) ? EVENT_HEADER_TDC_SIZE_32 + cNCbc * CBC_EVENT_SIZE_32 : EVENT_HEADER_TDC_SIZE_32 + cBoard->getNCbcDataSize() * CBC_EVENT_SIZE_32;

                uint32_t cFWWord = fBeBoardInterface->getBoardInfo (cBoard);
                uint32_t cFWMajor = (cFWWord & 0xFFFF0000) >> 16;
                uint32_t cFWMinor = (cFWWord & 0x0000FFFF);

                //with the above info fill the header
                FileHeader cHeader (cBoardType, cFWMajor, cFWMinor, cBeId, cNCbc, cNEventSize32);
                //construct a Handler
                std::stringstream cBeBoardString;
                cBeBoardString << "_fed" << std::setw (3) << cBeId;
                std::string cFilename = fRawFileName;
                cFilename.insert (fRawFileName.find (".raw"), cBeBoardString.str() );

                FileHandler* cHandler = new FileHandler (cFilename, 'w', cHeader);

                //finally set the handler
                fBeBoardInterface->SetFileHandler (cBoard, cHandler);
            }
        }
    }

    std::string SystemController::expandEnvironmentVariables ( std::string s )
    {
        if ( s.find ( "${" ) == std::string::npos ) return s;

        std::string pre  = s.substr ( 0, s.find ( "${" ) );
        std::string post = s.substr ( s.find ( "${" ) + 2 );

        if ( post.find ( '}' ) == std::string::npos ) return s;

        std::string variable = post.substr ( 0, post.find ( '}' ) );
        std::string value    = "";

        post = post.substr ( post.find ( '}' ) + 1 );

        if ( getenv ( variable.c_str() ) != NULL ) value = std::string ( getenv ( variable.c_str() ) );

        return expandEnvironmentVariables ( pre + value + post );
    }
}
