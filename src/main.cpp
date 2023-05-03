


//BOOST
#include <boost/program_options.hpp>
#include <boost/progress.hpp>

//EPG
#include <epg/Context.h>
#include <epg/log/EpgLogger.h>
#include <epg/log/ShapeLogger.h>
#include <epg/tools/TimeTools.h>
#include <epg/tools/geometry/SegmentIndexedGeometry.h>
#include <epg/sql/tools/numFeatures.h>
#include <epg/params/EpgParameters.h>
#include <epg/params/tools/loadParameters.h>
#include <epg/sql/DataBaseManager.h>




namespace po = boost::program_options;

int main(int argc, char *argv[])
{
    // ign::geometry::PrecisionModel::SetDefaultPrecisionModel(ign::geometry::PrecisionModel(ign::geometry::PrecisionModel::FIXED, 1.0e5, 1.0e7) );
    epg::Context* context = epg::ContextS::getInstance();

    std::string     logDirectory = "";
    std::string     epgParametersFile = "";
    std::string     auTable = "";
    std::string     countryCode = "";
    bool            verbose = true;

// au_matching -c conf.json -t administrative_unit_area_w be

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("c" , po::value< std::string >(&epgParametersFile)     , "conf file" )
        ("t" , po::value< std::string >(&auTable)               , "table" )
        ("cc" , po::value< std::string >(&countryCode)          , "country code" )
    ;

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

        epg::params::EpgParameters& epgParams = epg::ContextS::getInstance()->getEpgParameters();

        std::string const idName = epgParams.getValue( ID ).toString();
		std::string const geomName = epgParams.getValue( GEOM ).toString();
        std::string const boundaryTableName = epgParams.getValue( TARGET_BOUNDARY_TABLE ).toString();
        std::string const landmaskTableName = epgParams.getValue( TARGET_COUNTRY_TABLE ).toString();


        ign::feature::sql::FeatureStorePostgis* fsLandmask = context->getDataBaseManager().getFeatureStore(landmaskTableName, idName, geomName);
		ign::feature::FeatureIteratorPtr itLandmask= fsLandmask->getFeatures(ign::feature::FeatureFilter("country = '"+countryCode+"'"));

        // ign::geometry::MultiLineString mlBoundary;
        ign::geometry::MultiPolygon mpLandmask;
        while (itLandmask->hasNext())
		{
            ign::feature::Feature const & fLandmask = itLandmask->next();
			mpLandmask = fLandmask.getGeometry().asMultiPolygon();

            // epg::tools::geometry::SegmentIndexedGeometry indexedBoundaries(mpLandmask);
            // for ( int i = 0 ; i < mpLandmask.numGeometries() ; ++i )
            // {
            //     for ( int j = 0 ; j < mpLandmask.polygonN(i).numRings() ; ++j)
            //     {
            //         mlBoundary.addGeometry(mpLandmask.polygonN(i).ringN(j));
            //     }
            // }
            break;
        }
        const epg::tools::geometry::SegmentIndexedGeometryInterface* indexedBoundaries = new epg::tools::geometry::SegmentIndexedGeometry( &mpLandmask );

        // Go through objects intersecting the boundary
        ign::feature::sql::FeatureStorePostgis* fsAu = context->getDataBaseManager().getFeatureStore(auTable, idName, geomName);
		ign::feature::FeatureIteratorPtr itAu = fsAu->getFeatures(ign::feature::FeatureFilter());

        epg::log::ShapeLogger* shapeLogger = epg::log::ShapeLoggerS::getInstance();
        shapeLogger->setDataDirectory(context->getLogDirectory());
        shapeLogger->addShape( "not_boundaries", epg::log::ShapeLogger::LINESTRING );
        shapeLogger->addShape( "contact_points", epg::log::ShapeLogger::POINT );

        //patience
        int numFeatures = epg::sql::tools::numFeatures( *fsAu, ign::feature::FeatureFilter());
		boost::progress_display display( numFeatures , std::cout, "[ au_matching  % complete ]\n") ;

        while (itAu->hasNext())
		{
			ign::feature::Feature const& fAu = itAu->next();
			ign::geometry::MultiPolygon const& mpAu = fAu.getGeometry().asMultiPolygon();

            logger->log(epg::log::DEBUG,fAu.getId());

            for ( int i = 0 ; i < mpAu.numGeometries() ; ++i )
            {
                ign::geometry::Polygon const & pAu = mpAu.polygonN(i);
                
                for ( int j = 0 ; j < pAu.numRings() ; ++j )                                                                                                                                                                                                                                                                              
                {
                    ign::geometry::LineString const & ring = pAu.ringN(j);
                    
                    // on ne prend pas en compte le dernier point car c'est le meme que le premier
                    int nbPoints = ring.numPoints()-1;
                    std::vector < bool > vIsBoundary(nbPoints, false);

                    for ( int k = 0 ; k < nbPoints ; ++k )
                    {
                        if ( indexedBoundaries->distance( ring.pointN(k), 0.1 ) < 0 ) continue;  
                        vIsBoundary[k] = true;
                    }

                    std::vector < ign::geometry::Point > vContactPoints;
                    // On cherche un point de départ
                    int start = -1;
                    for ( int k = 0 ; k < nbPoints ; ++k )
                    {
                        int next = (k == nbPoints-1) ? 0 : k+1;
                        int previous = (k == 0) ? nbPoints-1 : k-1;
                        if ( vIsBoundary[k] && !vIsBoundary[next] )
                        {
                            if (! vIsBoundary[previous]) {
                                vContactPoints.push_back(ring[k]);
                            }
                            else{
                                start = k;
                                break;
                            }
                        }
                    }
                    if ( start == -1 )
                    {
                        continue;
                    }

                    std::vector < ign::geometry::LineString > ringParts;
                    // on parcours le ring depuis le point de départ
                    int current = start;
                    do
                    {
                        int next = (current == nbPoints-1) ? 0 : current+1;
                        int previous = (current == 0) ? nbPoints-1 : current-1;
                        if( !vIsBoundary[current] ) 
                        {
                            ringParts.back().addPoint(ring[current]);
                        } 
                        else if ( !vIsBoundary[previous] && !vIsBoundary[next] )
                        {
                            // point de contact unique
                            ringParts.back().addPoint(ring[current]);
                        }
                        else if ( !vIsBoundary[previous] )
                        {
                            ringParts.back().addPoint(ring[current]);
                        }
                        else if ( !vIsBoundary[next] )
                        {
                            ringParts.push_back(ign::geometry::LineString(std::vector< ign::geometry::Point >(1, ring[current])));
                        }
                        current = next;
                    } while ( current != start );

                    for ( int i = 0 ; i < ringParts.size() ; ++i )
                    {
                        ign::feature::Feature feature = fAu;
                        feature.setGeometry( ringParts[i] );
                        shapeLogger->writeFeature( "not_boundaries", feature );
                    }
                    for ( int i = 0 ; i < vContactPoints.size() ; ++i )
                    {
                        ign::feature::Feature feature = fAu;
                        feature.setGeometry( vContactPoints[i] );
                        shapeLogger->writeFeature( "contact_points", feature );
                    }
                }
            }
            ++display;
        }
        shapeLogger->closeShape( "not_boundaries" );
        shapeLogger->closeShape( "contact_points" );

        delete indexedBoundaries;
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
    epg::log::EpgLoggerS::kill();;
    
    logFile.close();

    return returnValue ;
}