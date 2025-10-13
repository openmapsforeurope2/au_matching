
//BOOST
#include <boost/program_options.hpp>

//EPG
#include <epg/Context.h>
#include <epg/tools/TimeTools.h>
#include <epg/params/tools/loadParameters.h>

//OME2
#include <ome2/utils/setTableName.h>

//APP
#include <app/params/ThemeParameters.h>
#include <app/step/tools/initSteps.h>


namespace po = boost::program_options;

int main(int argc, char *argv[])
{
    // ign::geometry::PrecisionModel::SetDefaultPrecisionModel(ign::geometry::PrecisionModel(ign::geometry::PrecisionModel::FIXED, 1.0e5, 1.0e7) );
    epg::Context* context = epg::ContextS::getInstance();

    std::string     logDirectory = "";
    std::string     epgParametersFile = "";
    std::string     themeParametersFile = "";
    std::string     stepCode = "";
    std::string     countryCode = "";
    bool            verbose = true;

    epg::step::StepSuite< app::params::ThemeParametersS > stepSuite;
    app::step::tools::initSteps(stepSuite);

    std::ostringstream OperatorDetail;
	OperatorDetail << "set step :" << std::endl
		<< stepSuite.toString();

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("c", po::value< std::string >(&epgParametersFile)     , "conf file" )
        ("cc", po::value< std::string >(&countryCode)          , "country code" )
        ("sp", po::value< std::string >(&stepCode), OperatorDetail.str().c_str())
    ;
    stepCode = stepSuite.getStepsRange();

    //main log
    std::string     logFileName = "log.txt";
    std::ofstream   logFile( logFileName.c_str() ) ;

    logFile << "[START] " << epg::tools::TimeTools::getTime() << std::endl;

    int returnValue = 0;
    try{

        po::variables_map vm;
        int style = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;
        po::store( po::parse_command_line( argc, argv, desc, style ), vm );
        po::notify( vm );    

        if ( vm.count( "help" ) ) {
            std::cout << desc << std::endl;
            return 1;
        }

        //parametres EPG
		context->loadEpgParameters( epgParametersFile );

        //Initialisation du log de prod
        logDirectory = context->getConfigParameters().getValue( LOG_DIRECTORY ).toString();

        //test si le dossier de log existe sinon le creer
        boost::filesystem::path logDir(logDirectory);
        if (!boost::filesystem::is_directory(logDir))
        {
            if (!boost::filesystem::create_directory(logDir))
            {
                std::string mError = "le dossier " + logDirectory + " ne peut être cree";
                IGN_THROW_EXCEPTION(mError);
            }
        }

        epg::log::EpgLogger* logger = epg::log::EpgLoggerS::getInstance();
        // logger->setProdOfstream( logDirectory+"/au_matching.log" );
        logger->setDevOfstream( logDirectory+"/au_matching.log" );
        
        //repertoire de travail
        context->setLogDirectory( logDirectory );

        //theme parameters
        themeParametersFile = context->getConfigParameters().getValue( THEME_PARAMETER_FILE ).toString();
		app::params::ThemeParameters* themeParameters = app::params::ThemeParametersS::getInstance();
        epg::params::tools::loadParams( *themeParameters, themeParametersFile, countryCode );
        if ( themeParameters->getValue(COAST_TABLE).toString() == "" )
            themeParameters->setParameter(COAST_TABLE, ign::data::String(themeParameters->getValue(AREA_TABLE_INIT).toString() + themeParameters->getValue(COAST_TABLE_SUFFIX).toString()));
        if ( themeParameters->getValue(NOCOAST_TABLE).toString() == "" ) 
            themeParameters->setParameter(NOCOAST_TABLE, ign::data::String(themeParameters->getValue(AREA_TABLE_INIT).toString() + themeParameters->getValue(NOCOAST_TABLE_SUFFIX).toString()));

        //info de connection db
        context->loadEpgParameters( themeParameters->getValue(DB_CONF_FILE).toString() );
        
        //set BDD search path
        context->getDataBaseManager().setSearchPath(themeParameters->getValue(WORKING_SCHEMA).toString());
        ome2::utils::setTableName<app::params::ThemeParametersS>(AREA_TABLE_INIT);
        ome2::utils::setTableName<app::params::ThemeParametersS>(LANDMASK_TABLE);
        ome2::utils::setTableName<epg::params::EpgParametersS>(TARGET_BOUNDARY_TABLE);

        logger->log(epg::log::INFO, "[START AU-MATCHING PROCESS ] " + epg::tools::TimeTools::getTime());

        //lancement du traitement
		stepSuite.run(stepCode, verbose);

		logger->log(epg::log::INFO, "[END AU-MATCHING PROCESS ] " + epg::tools::TimeTools::getTime());

    }
    catch( ign::Exception &e )
    {
        std::cerr<< e.diagnostic() << std::endl;
        epg::log::EpgLoggerS::getInstance()->log( epg::log::ERROR, std::string(e.diagnostic()));
        logFile << e.diagnostic() << std::endl;
        returnValue = 1;
    }
    catch( std::exception &e )
    {
        std::cerr << e.what() << std::endl;
        epg::log::EpgLoggerS::getInstance()->log( epg::log::ERROR, std::string(e.what()));
        logFile << e.what() << std::endl;
        returnValue = 1;
    }

    logFile << "[END] " << epg::tools::TimeTools::getTime() << std::endl;

    epg::ContextS::kill();
    epg::log::EpgLoggerS::kill();
    epg::log::ShapeLoggerS::kill();
    app::params::ThemeParametersS::kill();
    
    logFile.close();

    return returnValue ;
}