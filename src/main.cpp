


//BOOST
#include <boost/program_options.hpp>
#include <boost/progress.hpp>

//EPG
#include <epg/Context.h>
#include <epg/log/EpgLogger.h>
#include <epg/log/ShapeLogger.h>
#include <epg/tools/TimeTools.h>
#include <epg/tools/geometry/SegmentIndexedGeometry.h>
#include <epg/tools/geometry/LineStringSplitter.h>
#include <epg/tools/MultiLineStringTool.h>
#include <epg/sql/tools/numFeatures.h>
#include <epg/params/EpgParameters.h>
#include <epg/params/tools/loadParameters.h>
#include <epg/sql/DataBaseManager.h>

namespace po = boost::program_options;


void extractNotTouchingParts(
    const epg::tools::geometry::SegmentIndexedGeometryInterface* refGeom,
    const ign::geometry::LineString & ls, 
    std::vector<ign::geometry::LineString> & vNotTouchingParts,
    std::vector<ign::geometry::Point>* vTouchingPoints = 0
) 
{
    bool isRing = ls.isClosed();
    int nbPoints = isRing ? ls.numPoints()-1 : ls.numPoints();
    
    std::vector < bool > vIsBoundary(nbPoints, false);

    for ( int k = 0 ; k < nbPoints ; ++k )
    {
        if ( refGeom->distance( ls.pointN(k), 0.1 ) < 0 ) continue;  
        vIsBoundary[k] = true;
    }

    // On cherche un point de départ
    int start = -1;
    for ( int k = 0 ; k < nbPoints ; ++k )
    {
        int next = (k == nbPoints-1) ? 0 : k+1;
        int previous = (k == 0) ? nbPoints-1 : k-1;
        if ( vIsBoundary[k] && !vIsBoundary[next] )
        {
            if ((isRing || k != 0) && ! vIsBoundary[previous]) {
                if (vTouchingPoints != 0) vTouchingPoints->push_back(ls[k]);
            }
            else{
                start = k;
                break;
            }
        }
    }

    if ( start >= 0 )
    {
        int current = start;
        do
        {
            int next = (current == nbPoints-1) ? 0 : current+1;
            int previous = (current == 0) ? nbPoints-1 : current-1;
            if( !vIsBoundary[current] ) 
            {
                vNotTouchingParts.back().addPoint(ls[current]);
            } 
            else if ( ((!isRing && current == 0) || !vIsBoundary[previous]) && ((!isRing && current == nbPoints-1) || !vIsBoundary[next] ) )
            {
                // point de contact unique
                vNotTouchingParts.back().addPoint(ls[current]);
            }
            else if ( (isRing || current != 0) && !vIsBoundary[previous] )
            {
                vNotTouchingParts.back().addPoint(ls[current]);
            }
            else if ( (isRing || current != nbPoints-1) && !vIsBoundary[next] )
            {
                vNotTouchingParts.push_back(ign::geometry::LineString(std::vector< ign::geometry::Point >(1, ls[current])));
            }
            current = next;
        } while ( current != start );
    }
}

void extractNotTouchingParts(
    const epg::tools::geometry::SegmentIndexedGeometryInterface* refGeom,
    const ign::geometry::Polygon & p, 
    std::vector<ign::geometry::LineString> & vNotTouchingParts,
    std::vector<ign::geometry::Point>* vTouchingPoints = 0
) 
{
    for (int i = 0 ; i < p.numRings() ; ++i)
    {
        extractNotTouchingParts(refGeom, p.ringN(i), vNotTouchingParts, vTouchingPoints);
    }
}

void extractNotTouchingParts(
    const epg::tools::geometry::SegmentIndexedGeometryInterface* refGeom,
    const ign::geometry::MultiPolygon & mp, 
    std::vector<ign::geometry::LineString> & vNotTouchingParts,
    std::vector<ign::geometry::Point>* vTouchingPoints = 0
) 
{
    for (int i = 0 ; i < mp.numGeometries() ; ++i)
    {
        extractNotTouchingParts(refGeom, mp.polygonN(i), vNotTouchingParts, vTouchingPoints);
    }
}

int main(int argc, char *argv[])
{
    // ign::geometry::PrecisionModel::SetDefaultPrecisionModel(ign::geometry::PrecisionModel(ign::geometry::PrecisionModel::FIXED, 1.0e5, 1.0e7) );
    epg::Context* context = epg::ContextS::getInstance();

    double searchDistance = 10.;
    double snapDistOnVertex = 5.;
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

        epg::log::ShapeLogger* shapeLogger = epg::log::ShapeLoggerS::getInstance();
        shapeLogger->setDataDirectory(context->getLogDirectory());
        shapeLogger->addShape( "not_boundaries", epg::log::ShapeLogger::LINESTRING );
        shapeLogger->addShape( "contact_points", epg::log::ShapeLogger::POINT );
        shapeLogger->addShape( "not_boundaries_trim", epg::log::ShapeLogger::LINESTRING );
        shapeLogger->addShape( "not_boundaries_merged", epg::log::ShapeLogger::LINESTRING );
        shapeLogger->addShape( "coastline_path_not_found", epg::log::ShapeLogger::LINESTRING );
        shapeLogger->addShape( "coastline_path", epg::log::ShapeLogger::LINESTRING );
        shapeLogger->addShape( "boundary_not_touching_coast", epg::log::ShapeLogger::LINESTRING );
        shapeLogger->addShape( "resulting_polygons", epg::log::ShapeLogger::POLYGON );
        shapeLogger->addShape( "resulting_polygons_not_valid", epg::log::ShapeLogger::POLYGON );
        shapeLogger->addShape( "resulting_polygons_not_modified", epg::log::ShapeLogger::POLYGON );
        shapeLogger->addShape( "path", epg::log::ShapeLogger::LINESTRING );
        shapeLogger->addShape( "point", epg::log::ShapeLogger::POINT );

        epg::params::EpgParameters& epgParams = epg::ContextS::getInstance()->getEpgParameters();

        std::string const idName = epgParams.getValue( ID ).toString();
		std::string const geomName = epgParams.getValue( GEOM ).toString();
        std::string const countryCodeName = epgParams.getValue( COUNTRY_CODE ).toString();
        std::string const boundaryTableName = epgParams.getValue( TARGET_BOUNDARY_TABLE ).toString();
        std::string const landmaskTableName = epgParams.getValue( TARGET_COUNTRY_TABLE ).toString();
        std::string const boundaryTypeName = epgParams.getValue( BOUNDARY_TYPE ).toString();
        std::string const typeCostlineValue = epgParams.getValue( TYPE_COASTLINE ).toString();


        ign::feature::sql::FeatureStorePostgis* fsLandmask = context->getDataBaseManager().getFeatureStore(landmaskTableName, idName, geomName);
		ign::feature::FeatureIteratorPtr itLandmask= fsLandmask->getFeatures(ign::feature::FeatureFilter(countryCodeName+" = '"+countryCode+"'"));

        // ign::geometry::MultiLineString mlBoundary;
        ign::geometry::MultiPolygon mpLandmask;
        while (itLandmask->hasNext())
		{
            ign::feature::Feature const & fLandmask = itLandmask->next();
            ign::geometry::MultiPolygon const & mp = fLandmask.getGeometry().asMultiPolygon();
            for ( int i = 0 ; i < mp.numGeometries() ; ++i )
            {
                mpLandmask.addGeometry(mp.polygonN(i));
            }
        }

        // faire un MultiLineStringTool avec mpLandmask
        epg::tools::MultiLineStringTool mlsTool( ign::feature::FeatureFilter(countryCodeName+" = '"+countryCode+"'"), *fsLandmask );

        // faire FeatureStorePostgis avec "country = '"+countryCode+"'" + de type coastline
        ign::feature::sql::FeatureStorePostgis* fsCoast = context->getDataBaseManager().getFeatureStore(boundaryTableName, idName, geomName);
        ign::feature::FeatureIteratorPtr itCoast = fsCoast->getFeatures(ign::feature::FeatureFilter(countryCodeName+" = '"+countryCode+"' AND "+boundaryTypeName+" = '"+typeCostlineValue+"'"));

        // calculer tous les chemins sur le MultiLineStringTool en prenant pour source et target les extrémités des costlines
        ign::geometry::MultiLineString mlsPath;
        while (itCoast->hasNext())
        {
            ign::feature::Feature const & fCoast = itCoast->next();
            ign::geometry::LineString const& lsCoast = fCoast.getGeometry().asLineString();

            double searchDistance2 = 50.;
            std::pair< bool, ign::geometry::LineString > pathFound = mlsTool.getPath(
                lsCoast.startPoint(),
                lsCoast.endPoint(),
                searchDistance2,
                snapDistOnVertex
            );

            if ( !pathFound.first )
            {
                ign::feature::Feature feature = fCoast;
                feature.setGeometry( lsCoast );
                shapeLogger->writeFeature( "coastline_path_not_found", feature );
            }
            else
            {
                mlsPath.addGeometry(pathFound.second);

                ign::feature::Feature feature = fCoast;
                feature.setGeometry( lsCoast );
                shapeLogger->writeFeature( "coastline_path", feature );
            }
        }

        const epg::tools::geometry::SegmentIndexedGeometryInterface* indexedCoasts = new epg::tools::geometry::SegmentIndexedGeometry( &mlsPath );

        std::vector<ign::geometry::LineString> vBoundaryNotTouchingCoasts;
        extractNotTouchingParts( indexedCoasts, mpLandmask, vBoundaryNotTouchingCoasts );

        for ( int i = 0 ; i < vBoundaryNotTouchingCoasts.size() ; ++i )
        {
            ign::feature::Feature feature;
            feature.setGeometry( vBoundaryNotTouchingCoasts[i] );
            shapeLogger->writeFeature( "boundary_not_touching_coast", feature );
        }

        epg::tools::geometry::SegmentIndexedGeometryCollection* indexedBoundaries = new epg::tools::geometry::SegmentIndexedGeometryCollection();
        for (std::vector<ign::geometry::LineString>::const_iterator vit=vBoundaryNotTouchingCoasts.begin() ; vit!=vBoundaryNotTouchingCoasts.end() ; ++vit)
        {
            indexedBoundaries->addGeometry(&*vit);
        }

        ign::feature::sql::FeatureStorePostgis* fsBoundary = context->getDataBaseManager().getFeatureStore(boundaryTableName, idName, geomName);
        epg::tools::MultiLineStringTool* boundaryTool = new epg::tools::MultiLineStringTool( ign::feature::FeatureFilter("country LIKE '%"+countryCode+"%'"), *fsBoundary );

        // Go through objects intersecting the boundary
        ign::feature::sql::FeatureStorePostgis* fsAu = context->getDataBaseManager().getFeatureStore(auTable, idName, geomName);
		ign::feature::FeatureIteratorPtr itAu = fsAu->getFeatures(ign::feature::FeatureFilter());
        // ign::feature::FeatureIteratorPtr itAu = fsAu->getFeatures(ign::feature::FeatureFilter("inspireid='90e9e088-50d5-4d4c-9521-89a904928703'"));

        //patience
        int numFeatures = epg::sql::tools::numFeatures( *fsAu, ign::feature::FeatureFilter());
		boost::progress_display display( numFeatures , std::cout, "[ au_matching  % complete ]\n") ;        

        while (itAu->hasNext())
		{
			ign::feature::Feature const & fAu = itAu->next();
			ign::geometry::MultiPolygon const & mpAu = fAu.getGeometry().asMultiPolygon();
            ign::geometry::MultiPolygon newGeometry;

            logger->log(epg::log::DEBUG,fAu.getId());
            // std::cout << fAu.getId() << std::endl;

            bool bIsModified = false;
            for ( int i = 0 ; i < mpAu.numGeometries() ; ++i )
            {
                ign::geometry::Polygon const & pAu = mpAu.polygonN(i);
                
                std::vector<ign::geometry::LineString> vRings;
                for ( int j = 0 ; j < pAu.numRings() ; ++j )                                                                                                                                                                                                                                                                              
                {
                    ign::geometry::LineString const & ring = pAu.ringN(j);

                    std::vector < ign::geometry::LineString > ringParts;
                    std::vector < ign::geometry::Point > vContactPoints;
                    extractNotTouchingParts( indexedBoundaries, ring, ringParts, &vContactPoints);
                    if (!ringParts.empty()) {
                        bIsModified = true;
                    } else {
                        vRings.push_back(ring);
                        continue;
                    }

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
                   
                    for ( int i = 0 ; i < ringParts.size() ; ++i )
                    {
                        epg::tools::geometry::LineStringSplitter lsSplitter( ringParts[i], 1e-5 );

                        ign::geometry::MultiLineString mls;
                        boundaryTool->getLocal(ringParts[i].getEnvelope(), mls);
                        lsSplitter.addCuttingGeometry(mls);

                        ringParts[i] = lsSplitter.truncAtEnds();
                    }

                    for ( int i = 0 ; i < ringParts.size() ; ++i )
                    {
                        ign::feature::Feature feature = fAu;
                        feature.setGeometry( ringParts[i] );
                        shapeLogger->writeFeature( "not_boundaries_trim", feature );
                    }

                    ign::geometry::Point previousRingEndPoint = ringParts.rbegin()->endPoint();

                    ign::feature::Feature fPot = fAu;
                    fPot.setGeometry( previousRingEndPoint );
                    shapeLogger->writeFeature( "point", fPot );

                    ign::geometry::LineString newRing;
                    bool bErrorConstructingRing = false;
                    for ( int i = 0 ; i < ringParts.size() ; ++i )
                    {
                        std::pair< bool, ign::geometry::LineString > pathFound = boundaryTool->getPath( 
                            previousRingEndPoint,
                            ringParts[i].startPoint(),
                            searchDistance,
                            snapDistOnVertex
                        );
                        if ( !pathFound.first ) 
                        {
                            logger->log(epg::log::ERROR, "Path not found for object [id] " + fAu.getId());
                            bErrorConstructingRing = true;
                            break;
                        }

                        ign::feature::Feature fPAth = fAu;
                        fPAth.setGeometry( pathFound.second );
                        shapeLogger->writeFeature( "path", fPAth );

                        for( ign::geometry::LineString::const_iterator lsit = pathFound.second.begin() ; lsit != pathFound.second.end(); ++lsit ) {
                            newRing.addPoint(*lsit);
                        }

                        for( ign::geometry::LineString::const_iterator lsit = ringParts[i].begin()+1 ; lsit != ringParts[i].end()-1 ; ++lsit) {
                            newRing.addPoint(*lsit);
                        }

                        previousRingEndPoint = ringParts[i].endPoint();

                        if ( i == ringParts.size()-1 )
                        {
                            newRing.addPoint(newRing.startPoint());
                        }
                    }
                    if (bErrorConstructingRing)
                    {
                        logger->log(epg::log::ERROR, "Error constructing ring [id] " + fAu.getId());
                        continue;
                    }

                    vRings.push_back(newRing);
 
                    for ( int i = 0 ; i < ringParts.size() ; ++i )
                    {
                        ign::feature::Feature feature = fAu;
                        feature.setGeometry( ringParts[i] );
                        shapeLogger->writeFeature( "not_boundaries_merged", feature );
                    }    
                }
                newGeometry.addGeometry(ign::geometry::Polygon(vRings));
            }

            if (!bIsModified) {
                ++display;
                continue;
            }

            if ( !newGeometry.isValid() )
            {
                logger->log(epg::log::ERROR, "Polygon is not valid [id] " + fAu.getId());

                ign::feature::Feature feature = fAu;
                feature.setGeometry(newGeometry);
                shapeLogger->writeFeature( "resulting_polygons_not_valid", feature );

                ++display;
                continue;
            }
            if (!newGeometry.equals(mpAu))
            {
                ign::feature::Feature feature = fAu;
                feature.setAttribute("w_modification_type", ign::data::String("M"));
                feature.setAttribute("w_modification_step", ign::data::String("10"));
                feature.setGeometry(newGeometry);
                shapeLogger->writeFeature( "resulting_polygons", feature );
            }
            else {
                shapeLogger->writeFeature( "resulting_polygons_not_modified", fAu );
            }
            // break;
            ++display;
        }
        shapeLogger->closeShape( "not_boundaries" );
        shapeLogger->closeShape( "contact_points" );
        shapeLogger->closeShape( "not_boundaries_trim" );
        shapeLogger->closeShape( "not_boundaries_merged" );
        shapeLogger->closeShape( "coastline_path_not_found" );
        shapeLogger->closeShape( "coastline_path" );
        shapeLogger->closeShape( "resulting_polygons" );
        shapeLogger->closeShape( "resulting_polygons_not_modified" );
        shapeLogger->closeShape( "resulting_polygons_not_valid" );
        shapeLogger->closeShape( "path" );
        shapeLogger->closeShape( "point" );

        delete indexedBoundaries;
        delete indexedCoasts;
        delete boundaryTool;
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