//APP
#include <app/calcul/matching/AuMatchingOp.h>
#include <app/params/ThemeParameters.h>
#include <app/detail/Angle.h>

//BOOST
#include <boost/progress.hpp>

//SOCLE
#include <ign/geometry/algorithm/LineMergerOpGeos.h>
#include <ign/geometry/algorithm/PolygonBuilder.h>
#include <ign/geometry/algorithm/OptimizedHausdorffDistanceOp.h>
#include <ign/math/Line2T.h>

//EPG
#include <epg/Context.h>
#include <epg/params/EpgParameters.h>
#include <epg/sql/tools/numFeatures.h>
#include <epg/sql/DataBaseManager.h>
#include <epg/tools/StringTools.h>
#include <epg/tools/TimeTools.h>
#include <epg/tools/geometry/project.h>
#include <epg/tools/geometry/angle.h>
#include <epg/tools/geometry/LineStringSplitter.h>


using namespace app::detail;

namespace app{
namespace calcul{
namespace matching{

	///
	///
	///
    void AuMatchingOp::compute(
        std::string workingTable, 
        std::string countryCode, 
        bool verbose) 
    {
        AuMatchingOp auMatchingOp(countryCode, verbose);
        auMatchingOp._compute(workingTable);
    }

    ///
	///
	///
    AuMatchingOp::AuMatchingOp( std::string countryCode, bool verbose ):
        _countryCode( countryCode ),
        _verbose( verbose )
    {
        _init();
    }

    ///
	///
	///
    AuMatchingOp::~AuMatchingOp()
    {
        delete _mlsToolBoundary;
        delete _mlsToolLandmask;
        
        _shapeLogger->closeShape( "not_boundaries" );
        _shapeLogger->closeShape( "contact_points" );
        _shapeLogger->closeShape( "not_boundaries_trim" );
        _shapeLogger->closeShape( "not_boundaries_merged" );
        _shapeLogger->closeShape( "coastline_path_not_found" );
        _shapeLogger->closeShape( "coastline_path" );
        _shapeLogger->closeShape( "boundary_not_touching_coast" );
        _shapeLogger->closeShape( "resulting_polygons" );
        _shapeLogger->closeShape( "resulting_polygons_not_modified" );
        _shapeLogger->closeShape( "resulting_polygons_not_valid" );
        _shapeLogger->closeShape( "path" );
        _shapeLogger->closeShape( "point" );
        _shapeLogger->closeShape( "boucle" );
        _shapeLogger->closeShape( "projected_point_on_sharp" );
        _shapeLogger->closeShape( "deleted_segments" );
        _shapeLogger->closeShape( "debug_segments" );

        epg::log::ShapeLoggerS::kill();
    }

    ///
	///
	///
    void AuMatchingOp::_init() 
    {
        //--
        _logger= epg::log::EpgLoggerS::getInstance();
        _logger->log(epg::log::INFO, "[START] initialization: "+epg::tools::TimeTools::getTime());

        //--
        epg::Context* context = epg::ContextS::getInstance();

        // epg parameters
        epg::params::EpgParameters const& epgParams = context->getEpgParameters();

        std::string const idName = epgParams.getValue( ID ).toString();
        std::string const geomName = epgParams.getValue( GEOM ).toString();
        std::string const countryCodeName = epgParams.getValue( COUNTRY_CODE ).toString();

        // app parameters
        params::ThemeParameters* themeParameters = params::ThemeParametersS::getInstance();
        std::string const landmaskTableName = themeParameters->getValue( LANDMASK_TABLE ).toString();
        
        //--
        _fsBoundary = context->getFeatureStore( epg::TARGET_BOUNDARY );
        //--
        _fsLandmask = context->getDataBaseManager().getFeatureStore(landmaskTableName, idName, geomName);
        //--
        _mlsToolBoundary = new epg::tools::MultiLineStringTool( ign::feature::FeatureFilter("country LIKE '%"+_countryCode+"%'"), *_fsBoundary );
        //--
        _mlsToolLandmask = new epg::tools::MultiLineStringTool( ign::feature::FeatureFilter(countryCodeName+" = '"+_countryCode+"'"), *_fsLandmask );
        
        //--
        _shapeLogger = epg::log::ShapeLoggerS::getInstance();
        _shapeLogger->setDataDirectory(context->getLogDirectory());
        _shapeLogger->addShape( "not_boundaries", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "contact_points", epg::log::ShapeLogger::POINT );
        _shapeLogger->addShape( "not_boundaries_trim", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "not_boundaries_merged", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "coastline_path_not_found", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "coastline_path", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "boundary_not_touching_coast", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "resulting_polygons", epg::log::ShapeLogger::POLYGON );
        _shapeLogger->addShape( "resulting_polygons_not_valid", epg::log::ShapeLogger::POLYGON );
        _shapeLogger->addShape( "resulting_polygons_not_modified", epg::log::ShapeLogger::POLYGON );
        _shapeLogger->addShape( "path", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "point", epg::log::ShapeLogger::POINT );
        _shapeLogger->addShape( "boucle", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "projected_point_on_sharp", epg::log::ShapeLogger::POINT );
        _shapeLogger->addShape( "deleted_segments", epg::log::ShapeLogger::POINT );
        _shapeLogger->addShape( "debug_segments", epg::log::ShapeLogger::LINESTRING );

        //--
        _logger->log(epg::log::INFO, "[END] initialization: "+epg::tools::TimeTools::getTime());
    };

    ///
	///
	///
    void AuMatchingOp::_compute(std::string workingTable) 
    {

        epg::Context* context = epg::ContextS::getInstance();

        //epg params
        epg::params::EpgParameters const& epgParams = context->getEpgParameters();

        std::string const idName = epgParams.getValue( ID ).toString();
        std::string const geomName = epgParams.getValue( GEOM ).toString();
        std::string const countryCodeName = epgParams.getValue( COUNTRY_CODE ).toString();
        std::string const boundaryTypeName = epgParams.getValue( BOUNDARY_TYPE ).toString();
        std::string const typeCostlineValue = epgParams.getValue( TYPE_COASTLINE ).toString();

        //app params
        params::ThemeParameters* themeParameters = params::ThemeParametersS::getInstance();

        std::string const typeInlandValue = themeParameters->getValue( BOUNDARY_TYPE_INLAND_WATER_BOUNDARY ).toString();
        std::string const landCoverTypeName = themeParameters->getValue( LAND_COVER_TYPE ).toString();
		std::string const landAreaValue = themeParameters->getValue( TYPE_LAND_AREA ).toString();
        double const boundMaxDist = themeParameters->getValue( AU_BOUNDARY_MAX_DIST ).toDouble();
        double const boundSearchDist = themeParameters->getValue( AU_BOUNDARY_SEARCH_DIST ).toDouble();
        double const boundSnapDist = themeParameters->getValue( AU_BOUNDARY_SNAP_DIST ).toDouble();
        double const coastMaxDist = themeParameters->getValue( AU_COAST_MAX_DIST ).toDouble();
        double const coastSearcDist = themeParameters->getValue( AU_COAST_SEARCH_DIST ).toDouble();
        double const coastSnapDist = themeParameters->getValue( AU_COAST_SNAP_DIST ).toDouble();
        double const segmentMinLength = themeParameters->getValue( AU_SEGMENT_MIN_LENGTH ).toDouble();

        /*
        *********************************************************************************************
        ** On 
        *********************************************************************************************
        */
        ign::geometry::MultiLineString mlsLandmaskCoastPath;
        // NO DEBUG
        // ign::feature::FeatureIteratorPtr itCoast = _fsBoundary->getFeatures(ign::feature::FeatureFilter(countryCodeName+" LIKE '%"+_countryCode+"%' AND ("+boundaryTypeName+"::text LIKE '%"+typeCostlineValue+"%' OR "+boundaryTypeName+"::text LIKE '%"+typeInlandValue+"%')"));
        ign::feature::FeatureIteratorPtr itCoast = _fsBoundary->getFeatures(ign::feature::FeatureFilter(countryCodeName+" LIKE '%"+_countryCode+"%' AND "+boundaryTypeName+"::text LIKE '%"+typeCostlineValue+"%'"));
        ign::geometry::algorithm::LineMergerOpGeos merger;
        while (itCoast->hasNext())
        {
            ign::feature::Feature const& fCoast = itCoast->next();
            ign::geometry::LineString const& lsCoast = fCoast.getGeometry().asLineString();
            std::string icc = fCoast.getAttribute( countryCodeName ).toString();
            std::string boundType = fCoast.getAttribute( boundaryTypeName ).toString();

            if ( boundType.find("#") != std::string::npos ) {
                std::vector<std::string> vBoundType;
                epg::tools::StringTools::Split(boundType, "#", vBoundType);

                std::vector<std::string> vCountry;
                epg::tools::StringTools::Split(icc, "#", vCountry);

                for ( size_t i = 0 ; i < vCountry.size() ; ++i ) {
                    if ( i < vCountry.size() && vCountry[i] == _countryCode) {
                        boundType = vBoundType[i];
                        break;
                    }
                }
                if(boundType != typeCostlineValue) continue;
            }
            merger.add(lsCoast);
        }

        std::vector< ign::geometry::LineString > vCoastLs = merger.getMergedLineStrings();

        // calculer tous les chemins sur le Landmask en prenant pour source et target les extrémités des costlines
        _logger->log(epg::log::INFO, "[START] coast path computation: "+epg::tools::TimeTools::getTime());
        for (size_t i = 0 ; i < vCoastLs.size() ; ++i)
        {
            std::pair< bool, ign::geometry::LineString > pathFound = _mlsToolLandmask->getPathAlong(
                vCoastLs[i].startPoint(),
                vCoastLs[i].endPoint(),
                vCoastLs[i],
                coastMaxDist,
                coastSearcDist,
                coastSnapDist
            );

            if ( !pathFound.first )
            {
                ign::feature::Feature feat;
                feat.setGeometry(vCoastLs[i]);
                _shapeLogger->writeFeature( "coastline_path_not_found", feat );
            }
            else
            {
                mlsLandmaskCoastPath.addGeometry(pathFound.second);
                
                ign::feature::Feature feat;
                feat.setGeometry(pathFound.second);
                _shapeLogger->writeFeature( "coastline_path", feat );
            }
        }
        // NO DEBUG

        // DEBUG
        // ign::feature::FeatureIteratorPtr itDebug = _fsLandmask->getFeatures(ign::feature::FeatureFilter(countryCodeName+" LIKE '%"+_countryCode+"%'"));
        // while (itDebug->hasNext())
        // {
        //     ign::feature::Feature const& featDebug = itDebug->next();
        //     ign::geometry::MultiPolygon const& mpDebug = featDebug.getGeometry().asMultiPolygon();

        //     for ( size_t i = 0 ; i < mpDebug.numGeometries() ; ++i) {
        //         for (size_t j = 0 ; j < mpDebug.polygonN(i).numRings() ; ++j) {
        //             mlsLandmaskCoastPath.addGeometry(mpDebug.polygonN(i).ringN(j));
        //         }
        //     }
        // }
        // DEBUG

        _logger->log(epg::log::INFO, "[END] coast path computation: "+epg::tools::TimeTools::getTime());

        /*
        *********************************************************************************************
        ** On extrait les parties du landmask qui ne sont pas des cotes
        *********************************************************************************************
        */
        ign::geometry::MultiPolygon mpLandmask;
        ign::feature::FeatureIteratorPtr itLandmask= _fsLandmask->getFeatures(ign::feature::FeatureFilter(landCoverTypeName+" = '"+landAreaValue+"' AND "+countryCodeName+" = '"+_countryCode+"'"));
        while (itLandmask->hasNext())
        {
            ign::feature::Feature const& fLandmask = itLandmask->next();
            ign::geometry::MultiPolygon const& mp = fLandmask.getGeometry().asMultiPolygon();
            for ( int i = 0 ; i < mp.numGeometries() ; ++i ) {
                mpLandmask.addGeometry(mp.polygonN(i));
            }
        }

        _logger->log(epg::log::INFO, "[START] no coast computation: "+epg::tools::TimeTools::getTime());
        // NO DEBUG
        const tools::SegmentIndexedGeometryInterface* indexedLandmaskCoasts = new tools::SegmentIndexedGeometry( &mlsLandmaskCoastPath );

        std::vector<std::vector<std::vector<std::pair<int,int>>>> vLandmaskNoCoasts;
        _extractNotTouchingParts( indexedLandmaskCoasts, mpLandmask, vLandmaskNoCoasts );

        std::vector<ign::geometry::LineString> vLsLandmaskNoCoasts;
        tools::SegmentIndexedGeometryCollection* indexedLandmaskNoCoasts = new tools::SegmentIndexedGeometryCollection();
        for ( int np = 0 ; np < vLandmaskNoCoasts.size() ; ++np ) {
            for ( int nr = 0 ; nr < vLandmaskNoCoasts[np].size() ; ++nr ) {
                for ( int i = 0 ; i < vLandmaskNoCoasts[np][nr].size() ; ++i ) {
                    vLsLandmaskNoCoasts.push_back( _getSubString(vLandmaskNoCoasts[np][nr][i], mpLandmask.polygonN(np).ringN(nr)) );
                }
            }
        }
        // a faire dans un deuxième temps car les pointeurs sur les éléments de vecteur peuvent être modifiés en cas de réallocation
        int numGroup = 0;
        for (size_t i = 0 ; i < vLsLandmaskNoCoasts.size() ; ++i) {
            int group = vLsLandmaskNoCoasts[i].isClosed() ? numGroup++ : -1;
            indexedLandmaskNoCoasts->addGeometry(&vLsLandmaskNoCoasts[i], group);
        }

        for ( int i = 0 ; i < vLsLandmaskNoCoasts.size() ; ++i )
        {
            ign::feature::Feature feature;
            feature.setGeometry( vLsLandmaskNoCoasts[i] );
            _shapeLogger->writeFeature( "boundary_not_touching_coast", feature );
        }
        // NO DEBUG

        // DEBUG
        // tools::SegmentIndexedGeometryCollection* indexedLandmaskNoCoasts = new tools::SegmentIndexedGeometryCollection();
        // indexedLandmaskNoCoasts->addGeometry(&mpLandmask);
        // DEBUG

        _logger->log(epg::log::INFO, "[END] no coast computation: "+epg::tools::TimeTools::getTime());

        /*
        *********************************************************************************************
        ** 
        *********************************************************************************************
        */
        // on indexe les contours frontière fermés 
        ign::feature::FeatureIteratorPtr itBoundary = _fsBoundary->getFeatures(ign::feature::FeatureFilter(countryCodeName+" LIKE '%"+_countryCode+"%'"));
        ign::geometry::algorithm::LineMergerOpGeos merger2;
        while (itBoundary->hasNext()) {
            ign::feature::Feature const& fBoundary = itBoundary->next();
            ign::geometry::LineString const& lsBoundary = fBoundary.getGeometry().asLineString();
            merger2.add(lsBoundary);
        }
        std::vector< ign::geometry::LineString > vMergedBoundaryLs = merger2.getMergedLineStrings();

        std::vector< tools::SegmentIndexedGeometryInterface* > vMergedBoundaryIndexedLs(vMergedBoundaryLs.size(), 0);
        ign::geometry::index::QuadTree< size_t > qTreeClosedBoundary;
        for ( size_t i = 0 ; i < vMergedBoundaryLs.size() ; ++i ) {
            if ( vMergedBoundaryLs[i].isClosed() ) {
                vMergedBoundaryIndexedLs[i] = new tools::SegmentIndexedGeometry( &vMergedBoundaryLs[i] );
                qTreeClosedBoundary.insert( i, vMergedBoundaryLs[i].getEnvelope() );
            }
        }

        // Go through objects intersecting the boundary
        ign::feature::sql::FeatureStorePostgis* fsAu = context->getDataBaseManager().getFeatureStore(workingTable, idName, geomName);
        ign::feature::FeatureIteratorPtr itAu = fsAu->getFeatures(ign::feature::FeatureFilter(countryCodeName+" = '"+_countryCode+"'"));
        // ign::feature::FeatureIteratorPtr itAu = fsAu->getFeatures(ign::feature::FeatureFilter("inspireid in ('40655405-8e6f-40dd-8673-07c2379b06a8')"));

        //patience
        int numFeatures = epg::sql::tools::numFeatures( *fsAu, ign::feature::FeatureFilter(countryCodeName+" = '"+_countryCode+"'"));
        boost::progress_display display( numFeatures , std::cout, "[ au_matching  % complete ]\n") ;        

        while (itAu->hasNext())
        {
            ign::feature::Feature fAu = itAu->next();
            ign::geometry::MultiPolygon const& mpAu = fAu.getGeometry().asMultiPolygon();
            ign::geometry::algorithm::PolygonBuilderV1 polyBuilder;
            std::set< size_t > sAddedClosedBoundary;
            // ign::geometry::MultiPolygon newGeometry;

            if (_verbose) _logger->log(epg::log::DEBUG,fAu.getId());

            bool bIsModified = false;
            for ( int i = 0 ; i < mpAu.numGeometries() ; ++i )
            {
                ign::geometry::Polygon const& pAu = mpAu.polygonN(i);

                // if (pAu.intersects(ign::geometry::Point(3969556.5,3159378.5))){
                //     bool pouet = true;
                // }else {
                //     continue;
                // }
                
                // std::vector<ign::geometry::LineString> vRings;
                for ( int j = 0 ; j < pAu.numRings() ; ++j )                                                                                                                                                                                                                                                                              
                {
                    ign::geometry::LineString const& ring = pAu.ringN(j);

                    // on extrait les partis de l'AU qui ne sont pas des frontieres
                    std::vector<std::pair<int,int>> vpNotTouchingParts;
                    std::vector<int> vTouchingPoints;
                    _extractNotTouchingParts( indexedLandmaskNoCoasts, ring, vpNotTouchingParts, &vTouchingPoints);

                    // on gere les boucles
                    if (vpNotTouchingParts.empty()) {
                        bIsModified = true;

                        std::set< size_t > sClosedBoundary;
                        qTreeClosedBoundary.query( ring.getEnvelope(), sClosedBoundary );

                        bool foundBoundary = false;

                        std::set< size_t >::const_iterator sit;
                        for( sit = sClosedBoundary.begin() ; sit != sClosedBoundary.end() ; ++sit )
                        {
                            // if ( sAddedClosedBoundary.find(*sit) != sAddedClosedBoundary.end() ) continue;

                            if ( vMergedBoundaryIndexedLs[*sit]->distance(ring, boundMaxDist).first < 0 ) continue;
                            ign::geometry::algorithm::OptimizedHausdorffDistanceOp hausdorfOp(ring, vMergedBoundaryLs[*sit], -1, boundMaxDist);
                            double distance = hausdorfOp.getDemiHausdorff(ign::geometry::algorithm::OptimizedHausdorffDistanceOp::DhdFromAtoB);
                            if (distance < 0) {
                                distance = hausdorfOp.getDemiHausdorff(ign::geometry::algorithm::OptimizedHausdorffDistanceOp::DhdFromBtoA);
                            }
                            if (distance < 0) continue;

                            foundBoundary = true;

                            if ( sAddedClosedBoundary.find(*sit) != sAddedClosedBoundary.end() ) continue;

                            polyBuilder.addLineString(vMergedBoundaryLs[*sit]);
                            sAddedClosedBoundary.insert(*sit);
                            
                            ign::feature::Feature feature = fAu;
                            feature.setGeometry( vMergedBoundaryLs[*sit] );
                            _shapeLogger->writeFeature( "boucle", feature );
                        }
                        if (!foundBoundary) {
                            _logger->log(epg::log::ERROR, "Not found closed boundary line [id] " + fAu.getId());
                        }
                        continue;
                    }
                    
                    // on projete les eventuels points de contact avec la frontiere
                    ign::geometry::LineString ringWithContactPoints = ring;
                    _projectTouchingPoints(_mlsToolBoundary, ringWithContactPoints, vTouchingPoints, boundSearchDist, boundSnapDist);

                    std::vector<ign::geometry::LineString> vLsNotTouchingParts;
                    for ( int i = 0 ; i < vpNotTouchingParts.size() ; ++i ) {
                        vLsNotTouchingParts.push_back(_getSubString(vpNotTouchingParts[i], ringWithContactPoints));
                    }

                    // c est un contour fermé qui ne touche pas les frontières
                    if ( vLsNotTouchingParts.size() == 1 && vLsNotTouchingParts.front().isClosed() ) {
                        polyBuilder.addLineString(vLsNotTouchingParts.front());
                        if (vTouchingPoints.size() > 0) bIsModified = true;
                        continue;
                    }

                    bIsModified = true;

                    // recupérer les angles de la frontière au niveau des extremites des vpNotTouchingParts
                    std::vector<std::pair<double, double>> vGeomFeatures;
                    _getAngles(indexedLandmaskNoCoasts, vLsNotTouchingParts, vGeomFeatures);
                    
                    for ( int i = 0 ; i < vLsNotTouchingParts.size() ; ++i ) {
                        ign::feature::Feature feature = fAu;
                        feature.setGeometry( vLsNotTouchingParts[i] );
                        _shapeLogger->writeFeature( "not_boundaries", feature );
                    }
                    for ( int i = 0 ; i < vTouchingPoints.size() ; ++i ) {
                        ign::feature::Feature feature = fAu;
                        feature.setGeometry( ring.pointN(vTouchingPoints[i]) );
                        _shapeLogger->writeFeature( "contact_points", feature );
                    }

                    // on supprime les overshots
                    std::vector<std::pair<ign::geometry::Point,ign::geometry::Point>> vpStartEnd;
                    for ( int i = 0 ; i < vLsNotTouchingParts.size() ; ++i )
                    {
                        epg::tools::geometry::LineStringSplitter lsSplitter( vLsNotTouchingParts[i], 1e-5 );

                        ign::geometry::MultiLineString mls;
                        _mlsToolBoundary->getLocal(vLsNotTouchingParts[i].getEnvelope(), mls);
                        lsSplitter.addCuttingGeometry(mls);

                        vpStartEnd.push_back(std::make_pair(vLsNotTouchingParts[i].startPoint(), vLsNotTouchingParts[i].endPoint()));
                        vLsNotTouchingParts[i] = lsSplitter.truncAtEnds();
                    }

                    // on supprime les petits segments aux extremites qui ont etes tronquees
                    for ( int i = 0 ; i < vLsNotTouchingParts.size() ; ++i )
                    {
                        if ( !vLsNotTouchingParts[i].startPoint().equals(vpStartEnd[i].first) && vLsNotTouchingParts[i].numPoints() > 2 ) {
                            if (vLsNotTouchingParts[i].startPoint().distance(vLsNotTouchingParts[i].pointN(1)) < segmentMinLength) {
                                ign::feature::Feature feature = fAu;
                                feature.setGeometry( vLsNotTouchingParts[i].pointN(1) );
                                _shapeLogger->writeFeature( "deleted_segments", feature );

                                vLsNotTouchingParts[i].removePointN(1);

                                
                            }
                        }
                        if ( !vLsNotTouchingParts[i].endPoint().equals(vpStartEnd[i].second) && vLsNotTouchingParts[i].numPoints() > 2 ) {
                            if (vLsNotTouchingParts[i].endPoint().distance(vLsNotTouchingParts[i].pointN(vLsNotTouchingParts[i].numPoints()-2)) < segmentMinLength) {
                                ign::feature::Feature feature = fAu;
                                feature.setGeometry( vLsNotTouchingParts[i].pointN(vLsNotTouchingParts[i].numPoints()-2) );
                                _shapeLogger->writeFeature( "deleted_segments", feature );

                                vLsNotTouchingParts[i].removePointN(vLsNotTouchingParts[i].numPoints()-2);
                            }
                        }
                    }

                    for ( int i = 0 ; i < vLsNotTouchingParts.size() ; ++i )
                    {
                        ign::feature::Feature feature = fAu;
                        feature.setGeometry( vLsNotTouchingParts[i] );
                        _shapeLogger->writeFeature( "not_boundaries_trim", feature );
                    }

                    // on identifie les similarité geometrique landmask/boundary et on remplace les 
                    // extremites des vLsNotTouchingParts si un candidat est trouve
                    _findAngles(_mlsToolBoundary, vLsNotTouchingParts, vGeomFeatures, boundSearchDist);

                    // on reconstitue le nouveau contour en concatenant les parties ne touchant pas 
                    // la frontiere et les parties touchant la frontieres projetees sur la frontiere cible
                    ign::geometry::Point previousRingEndPoint = vLsNotTouchingParts.rbegin()->endPoint();

                    // on recupere les parties longeant les frontieres pour guider les chemins le long des trous
                    std::vector<std::pair<int,int>> vpTouchingParts = _getTouchingParts(vpNotTouchingParts, ring.numPoints(), true);
                    if ( vpTouchingParts.empty() || (vpTouchingParts.front().second != vpNotTouchingParts.front().first) ) {
                        _logger->log(epg::log::ERROR, "Touching/not touching parts are not matching " + fAu.getId());
                    }

                    std::vector<ign::geometry::LineString> vLsTouchingParts;
                    for ( int i = 0 ; i < vpTouchingParts.size() ; ++i ) {
                        vLsTouchingParts.push_back(_getSubString(vpTouchingParts[i], ringWithContactPoints));
                    }

                    ign::geometry::LineString newRing;
                    bool bErrorConstructingRing = false;
                    for ( int i = 0 ; i < vLsNotTouchingParts.size() ; ++i )
                    {
                        std::pair< bool, ign::geometry::LineString > pathFound = _mlsToolBoundary->getPathAlong( 
                            previousRingEndPoint,
                            vLsNotTouchingParts[i].startPoint(),
                            vLsTouchingParts[i],
                            boundSearchDist, // maxDist
                            boundSearchDist,
                            boundSnapDist
                        );
                        if ( !pathFound.first ) 
                        {
                            _logger->log(epg::log::ERROR, "Path not found for object [id] " + fAu.getId());
                            bErrorConstructingRing = true;
                            break;
                        }

                        ign::feature::Feature fPAth = fAu;
                        fPAth.setGeometry( pathFound.second );
                        _shapeLogger->writeFeature( "path", fPAth );

                        for( ign::geometry::LineString::const_iterator lsit = pathFound.second.begin() ; lsit != pathFound.second.end(); ++lsit ) {
                            newRing.addPoint(*lsit);
                        }

                        for( ign::geometry::LineString::const_iterator lsit = vLsNotTouchingParts[i].begin()+1 ; lsit != vLsNotTouchingParts[i].end()-1 ; ++lsit) {
                            newRing.addPoint(*lsit);
                        }

                        previousRingEndPoint = vLsNotTouchingParts[i].endPoint();

                        if ( i == vLsNotTouchingParts.size()-1 )
                        {
                            newRing.addPoint(newRing.startPoint());
                        }
                    }
                    if (bErrorConstructingRing)
                    {
                        _logger->log(epg::log::ERROR, "Error constructing ring [id] " + fAu.getId());
                        continue;
                    }

                    // vRings.push_back(newRing);
                    polyBuilder.addLineString(newRing);

                    for ( int i = 0 ; i < vLsNotTouchingParts.size() ; ++i )
                    {
                        ign::feature::Feature feature = fAu;
                        feature.setGeometry( vLsNotTouchingParts[i] );
                        _shapeLogger->writeFeature( "not_boundaries_merged", feature );
                    }    
                }
                // ign::geometry::Polygon poly(vRings);
                // ign::geometry::MultiPolygon poly = polyBuilder.getMultiPolygon();

                // if ( !poly.isEmpty() )
                // {
                //     if ( !poly.isValid() )
                //     {
                //         _logger->log(epg::log::ERROR, "Polygon is not valid [id] " + fAu.getId());
                //     }
                //     for (size_t i = 0 ; i < poly.numGeometries() ; ++i) {
                //         newGeometry.addGeometry(poly.polygonN(i));
                //     }
                // } else {
                //     _logger->log(epg::log::ERROR, "Polygon is empty [id] " + fAu.getId());
                // }
            }

            ign::geometry::MultiPolygon newGeometry = polyBuilder.getMultiPolygon();

            if (!bIsModified || newGeometry.equals(mpAu)) {
                _shapeLogger->writeFeature( "resulting_polygons_not_modified", fAu );
            } else {
                fAu.setGeometry(newGeometry);
                if ( !newGeometry.isEmpty() )
                {
                    fsAu->modifyFeature(fAu);
                } else {
                    _logger->log(epg::log::ERROR, "MultiPolygon is empty [id] " + fAu.getId());
                }
                
                if ( !newGeometry.isValid() ) {
                    _logger->log(epg::log::ERROR, "MultiPolygon is not valid [id] " + fAu.getId());
                    _shapeLogger->writeFeature( "resulting_polygons_not_valid", fAu );
                }
                _shapeLogger->writeFeature( "resulting_polygons", fAu );
            }
            ++display;
        }

        delete indexedLandmaskNoCoasts;
        delete indexedLandmaskCoasts;
        for (size_t i = 0 ; i < vMergedBoundaryIndexedLs.size() ; ++i) {
            delete vMergedBoundaryIndexedLs[i];
        }
    };

    ///
	///
	///
    void AuMatchingOp::_getAngles( 
        tools::SegmentIndexedGeometryCollection* indexedGeom, 
        const std::vector<ign::geometry::LineString> & vLs,
        std::vector<std::pair<double, double>> & vGeomFeatures) const
    {
        for ( size_t i = 0 ; i < vLs.size() ; ++i ) {
            vGeomFeatures.push_back(std::make_pair(_getAngle(indexedGeom, vLs[i].startPoint()), _getAngle(indexedGeom, vLs[i].endPoint())));
        }
    };

    ///
	///
	///
    double AuMatchingOp::_getAngle(
        tools::SegmentIndexedGeometryCollection* indexedGeom, 
        const ign::geometry::Point & pt ) const 
    {
        std::vector<ign::geometry::LineString> vSegments;
        ign::geometry::Envelope bbox = pt.getEnvelope().expandBy( 1e-5 );
        ign::geometry::Polygon bboxGeom = bbox.toPolygon();
        indexedGeom->getSegments( bbox, vSegments );
        if (vSegments.size()==2) {
            
            ign::geometry::Point const& endPoint1 = bboxGeom.intersects(vSegments.front().startPoint())? vSegments.front().endPoint() : vSegments.front().startPoint();
            ign::geometry::Point const& endPoint2 = bboxGeom.intersects(vSegments.back().startPoint())? vSegments.back().endPoint() : vSegments.back().startPoint();

            double angle = epg::tools::geometry::angle(endPoint1.toVec2d()-pt.toVec2d(), endPoint2.toVec2d()-pt.toVec2d());
            //DEBUG
            double angleTest = abs(angle-M_PI);
            if (abs(angle-M_PI)<0.174532925 /*10 degres*/) return 0;
            return angle;
        }
        return 0;
    };

    ///
	///
	///
    int AuMatchingOp::_findIndex( 
        const ign::geometry::LineString & ls, 
        size_t startIndex,
        const ign::geometry::Point & refPoint,
        double angle,
        double searchDistance) const 
    {
        std::pair<int, double>  newIndex1 = _findIndex(ls, startIndex, refPoint, angle, searchDistance, true);
        std::pair<int, double>  newIndex2 = _findIndex(ls, --startIndex, refPoint, angle, searchDistance, false);
        if ( newIndex1.first < 0 && newIndex2.first < 0 ) return -1;
        return newIndex1.second > newIndex2.second ? newIndex1.first : newIndex2.first;
    };

    ///
	///
	///
    double AuMatchingOp::_getScore(
        double refAngle, 
        double angle, 
        double distance) const 
    {
        double deltaAngle = abs(refAngle-angle);
        double angleScore = 0;
        if ( deltaAngle < 0.174532925 ) // 10
            angleScore = 10;
        else if ( deltaAngle < 0.261799388 ) // 15
            angleScore = 8;
        else if ( deltaAngle < 0.523598776 ) // 30
            angleScore = 5;
        else if ( deltaAngle < 0.785398163 ) // 45
            angleScore = 2;
        else if ( deltaAngle < 1.047197551 ) // 60
            angleScore = 1;
        else 
            angleScore = -1;

        double distScore = 1/(1+distance);
        double test2 = 1/exp(distance);

        return angleScore*distScore;
    };

    ///
	///
	///
    std::pair<int, double> AuMatchingOp::_findIndex( 
        const ign::geometry::LineString & ls, 
        size_t startIndex,
        const ign::geometry::Point & refPoint,
        double refAngle,
        double searchDistance, 
        bool positiveDirection ) const 
    {
        double distance = ls.pointN(startIndex).distance(refPoint);
        size_t index = startIndex;
        size_t bestIndex = -1;
        double bestScore = -1;
        while ( distance <= searchDistance && index > 0 && index < ls.numPoints()-1) {
            double angle = epg::tools::geometry::angle(
                ls.pointN(index-1).toVec2d()-ls.pointN(index).toVec2d(),
                ls.pointN(index+1).toVec2d()-ls.pointN(index).toVec2d()
            );
            double score = _getScore(refAngle, angle, distance);
            if ( score > bestScore ) {
                bestIndex = index;
                bestScore = score;
            }

            index = index + (positiveDirection ? 1 : -1);
            distance =ls.pointN(index).distance(refPoint);
        }
        return std::make_pair(bestIndex, bestScore);
    };

    ///
	///
	///
    std::pair<bool, ign::geometry::Point> AuMatchingOp::_findCandidate( 
        epg::tools::MultiLineStringTool* mlsTool, 
        const ign::geometry::Point & pt,
        double angle,
        double searchDistance
        ) const
    {					
        std::pair< bool, ign::geometry::Point > foundProjection = mlsTool->project( pt, searchDistance);
        if (!foundProjection.first) return std::make_pair(false, ign::geometry::Point());

        ign::geometry::MultiLineString mls;
        mlsTool->getLocal( pt.getEnvelope().expandBy(searchDistance), mls);

        std::vector<ign::geometry::LineString> vMergedLs = ign::geometry::algorithm::LineMergerOpGeos::MergeLineStrings(mls);
        for ( size_t i = 0 ; i < vMergedLs.size() ; ++i ) {
            ign::geometry::Polygon bbox = foundProjection.second.getEnvelope().expandBy(1e-5).toPolygon();
            if ( bbox.intersects(vMergedLs[i]) ) {
                int indexPoint = -1;
                for ( size_t j = 0 ; j < vMergedLs[i].numSegments() ; ++j ) {
                    if ( bbox.intersects(ign::geometry::LineString(vMergedLs[i].pointN(j), vMergedLs[i].pointN(j+1))) ) {
                        ign::math::Line2d line2( vMergedLs[i].pointN(j).toVec2d(), vMergedLs[i].pointN(j+1).toVec2d() );
                        double abscisse = line2.project(foundProjection.second.toVec2d(), true /*clamp*/);
                        indexPoint =  abscisse > 0.5 ? j+1 : j;
                        break;
                    }
                }
                if ( indexPoint < 0 ) break;

                int newIndex = _findIndex(vMergedLs[i], indexPoint, foundProjection.second, angle, searchDistance);

                if ( newIndex < 0 ) break;

                ign::feature::Feature feature;
                feature.setGeometry( vMergedLs[i].pointN(newIndex) );
                _shapeLogger->writeFeature( "projected_point_on_sharp", feature );

                return std::make_pair(true, vMergedLs[i].pointN(newIndex));
            }
        }
        return std::make_pair(false, ign::geometry::Point());
    };

    ///
	///
	///
    void AuMatchingOp::_findAngles( 
        epg::tools::MultiLineStringTool* mlsTool, 
        std::vector<ign::geometry::LineString> & vLs,
        const std::vector<std::pair<double, double>> & vGeomFeatures,
        double searchDistance
        ) const
    {
        double snapDistance = 2.;
        for ( size_t i = 0 ; i < vLs.size() ; ++i ) {
            // on projete les points sur la NotTouchingPart et on coupe
            epg::tools::geometry::LineStringSplitter lsSplitter( vLs[i], 1e-5 );

            std::pair<bool, ign::geometry::Point> foundPointStart = std::make_pair(false, ign::geometry::Point());
            std::pair<bool, ign::geometry::Point> foundPointEnd = std::make_pair(false, ign::geometry::Point());

            if ( vGeomFeatures[i].first != 0 ) {
                foundPointStart = _findCandidate(mlsTool, vLs[i].startPoint(), vGeomFeatures[i].first, searchDistance);
                if (foundPointStart.first) {
                    ign::geometry::Point projectedPoint;
                    bool found = epg::tools::geometry::project( vLs[i], foundPointStart.second, projectedPoint, snapDistance);
                    if ( !found ) projectedPoint = foundPointStart.second;

                    lsSplitter.addCuttingGeometry(projectedPoint);
                }
            }
            if ( vGeomFeatures[i].second != 0 ) {
                foundPointEnd = _findCandidate(mlsTool, vLs[i].endPoint(), vGeomFeatures[i].second, searchDistance);
                if (foundPointEnd.first) {
                    ign::geometry::Point projectedPoint;
                    bool found = epg::tools::geometry::project( vLs[i], foundPointEnd.second, projectedPoint, snapDistance);
                    if ( !found ) projectedPoint = foundPointEnd.second;

                    lsSplitter.addCuttingGeometry(projectedPoint);;
                }
            }
            vLs[i] = lsSplitter.truncAtEnds();
            if (foundPointStart.first) vLs[i].startPoint() = foundPointStart.second;
            if (foundPointEnd.first) vLs[i].endPoint() = foundPointEnd.second;
        }
    };

    ///
	///
	///
    void AuMatchingOp::_projectTouchingPoints(
        epg::tools::MultiLineStringTool* mlsTool, 
        ign::geometry::LineString & ls, 
        const std::vector<int> & vTouchingPoints,
        double searchDistance,
        double snapDistOnVertex) const
    {
        if (vTouchingPoints.empty() ) return;

        for ( int i = 0 ; i < vTouchingPoints.size() ; ++i )
        {
            std::pair< bool, ign::geometry::Point > foundProjectedPoint = mlsTool->project(ls.pointN(vTouchingPoints[i]), searchDistance, snapDistOnVertex);
            if (foundProjectedPoint.first) {
                ls.setPointN(foundProjectedPoint.second, vTouchingPoints[i]);
            } else {
                _logger->log(epg::log::ERROR, "Touching point not projected : " + ls.pointN(vTouchingPoints[i]).toString());
            }
        }

        if (ls.isClosed() && vTouchingPoints.front() == 0) {
            ls.setPointN(ls.startPoint(), ls.numPoints()-1);
        }
    };

    ///
	///
	///
    ign::geometry::LineString AuMatchingOp::_getSubString(
        std::pair<int, int> pStartEnd, 
        const ign::geometry::LineString & lsRef) const
    {
        if (pStartEnd.first == 0 && pStartEnd.second == lsRef.numPoints()-1) return lsRef;

        ign::geometry::LineString subLs;

        bool isRing = lsRef.isClosed();
        int nbPoints = isRing ? lsRef.numPoints()-1 : lsRef.numPoints();

        int current = pStartEnd.first;
        int end = (pStartEnd.second == nbPoints-1) ? 0 : pStartEnd.second+1;
        do
        {
            int next = (current == nbPoints-1) ? 0 : current+1;

            subLs.addPoint(lsRef.pointN(current));

            current = next;
        } while ( current != end );

        return subLs;
    }

    ///
	///
	///
    void AuMatchingOp::_extractNotTouchingParts(
        const tools::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::LineString & ls, 
        std::vector<std::pair<int,int>> & vNotTouchingParts,
        std::vector<int>* vTouchingPoints) const
    {
        bool isRing = ls.isClosed();
        int nbSegments = ls.numSegments();
        int nbPoints = ls.numPoints();

        std::vector < bool > vIsTouchingPoints(nbPoints, false);
        std::vector < std::set<int> > vGroup(nbPoints, std::set<int>());
        for ( int k = 0 ; k < nbPoints ; ++k ) {
            std::pair<double, std::set<int>> distGroup = refGeom->distance( ls.pointN(k), 0.1 );
            if ( distGroup.first < 0 ) {
                continue;
            }
            vIsTouchingPoints[k] = true;
            vGroup[k] = distGroup.second;
        }

        int notTouchingFirstSegment = -1;
        bool bNothingIsTouching = true;
        bool previousSegmentIsTouching = !isRing ? false : vIsTouchingPoints[nbSegments-1] && vIsTouchingPoints[nbSegments] && _commonGroupExists(vGroup[nbSegments-1], vGroup[nbSegments]) ;
        std::vector < bool > vTouchingSegments(nbSegments, false);
        for ( int currentSegment = 0 ; currentSegment < nbSegments ; ++currentSegment )
        {
            bool currentSegmentIsTouching = vIsTouchingPoints[currentSegment] && vIsTouchingPoints[currentSegment+1] && _commonGroupExists(vGroup[currentSegment], vGroup[currentSegment+1]);

            if (currentSegmentIsTouching)
            {
                vTouchingSegments[currentSegment] = true;
                bNothingIsTouching = false;
            } else if ( !previousSegmentIsTouching ) {
                if ( vIsTouchingPoints[currentSegment] ) {
                    if (vTouchingPoints != 0) vTouchingPoints->push_back(currentSegment);
                }
            } else if (notTouchingFirstSegment < 0) {
                notTouchingFirstSegment = currentSegment;
            }
            previousSegmentIsTouching = vTouchingSegments[currentSegment];
        }

        if (bNothingIsTouching) {
            vNotTouchingParts.push_back(std::make_pair(0, nbSegments));
            return;
        }

        if (notTouchingFirstSegment >= 0) {
            int currentSegment = !isRing ? 0 : notTouchingFirstSegment;
            notTouchingFirstSegment = -1;
            int end = currentSegment;
            do
            {
                int next = (currentSegment == nbSegments-1) ? 0 : currentSegment+1;

                if ( vTouchingSegments[currentSegment] || (!isRing && next == 0) ) {
                    if ( notTouchingFirstSegment >= 0 )
                    {
                        vNotTouchingParts.push_back(std::make_pair(notTouchingFirstSegment, currentSegment));
                        notTouchingFirstSegment = -1;
                    }
                } else {
                    if ( notTouchingFirstSegment < 0 )
                    {
                        notTouchingFirstSegment = currentSegment;
                    }
                }
                currentSegment = next;
            } while ( currentSegment != end );
        }
    };

    ///
    ///
    ///
    bool AuMatchingOp::_commonGroupExists( std::set<int> const& sGroup1, std::set<int> const& sGroup2 ) const 
    {
        if (sGroup1.empty() && sGroup2.empty()) return true;

        for ( std::set<int>::const_iterator sit1 = sGroup1.begin() ; sit1 != sGroup1.end() ; ++sit1 ) {
            if( sGroup2.find(*sit1) != sGroup2.end() ) return true;
        }
        return false;
    }

    ///
	///
	///
    void AuMatchingOp::_extractNotTouchingParts(
        const tools::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::Polygon & p, 
        std::vector<std::vector<std::pair<int,int>>> & vNotTouchingParts,
        std::vector<std::vector<int>>* vTouchingPoints) const
    {
        for (int i = 0 ; i < p.numRings() ; ++i)
        {
            vNotTouchingParts.push_back(std::vector<std::pair<int,int>>());
            std::vector<int>* vTouchingPoints_ = 0;
            if (vTouchingPoints)
            {
                vTouchingPoints->push_back(std::vector<int>());
                vTouchingPoints_ =  &vTouchingPoints->back();
            }
            _extractNotTouchingParts(refGeom, p.ringN(i), vNotTouchingParts.back(), vTouchingPoints_);
        }
    };

    ///
	///
	///
    void AuMatchingOp::_extractNotTouchingParts(
        const tools::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::MultiPolygon & mp, 
        std::vector<std::vector<std::vector<std::pair<int,int>>>> & vNotTouchingParts,
        std::vector<std::vector<std::vector<int>>>* vTouchingPoints) const
    {
        for (int i = 0 ; i < mp.numGeometries() ; ++i)
        {
            vNotTouchingParts.push_back(std::vector<std::vector<std::pair<int,int>>>());
            std::vector<std::vector<int>>* vTouchingPoints_ = 0;
            if (vTouchingPoints)
            {
                vTouchingPoints->push_back(std::vector<std::vector<int>>());
                vTouchingPoints_ =  &vTouchingPoints->back();
            }
            _extractNotTouchingParts(refGeom, mp.polygonN(i), vNotTouchingParts.back(), vTouchingPoints_);
        }
    };



    std::vector<std::pair<int,int>> AuMatchingOp::_getTouchingParts(
        std::vector<std::pair<int,int>> const& vpNotTouchingParts, 
        size_t nbPoints, 
        bool isClosed) const 
    {
        std::vector<std::pair<int,int>> vpTouchingParts;

        if ( vpNotTouchingParts.empty() ) {
            vpTouchingParts.push_back(std::make_pair(0, nbPoints-1));
        }

        int previousTouchingPoint = isClosed ? vpNotTouchingParts.back().second : vpNotTouchingParts.front().first == 0 ? vpNotTouchingParts.front().second : 0 ;

        for ( size_t i = 0 ; i < vpNotTouchingParts.size() ; ++i ) {
            if (i == 0)  {
                if (!isClosed && vpNotTouchingParts[i].first == 0 ) continue;
            }

            vpTouchingParts.push_back(std::make_pair(previousTouchingPoint, vpNotTouchingParts[i].first));
            previousTouchingPoint = vpNotTouchingParts[i].second;

            if (i == vpNotTouchingParts.size()-1)  {
                if (!isClosed && vpNotTouchingParts[i].second != nbPoints-1 ) {
                    vpTouchingParts.push_back(std::make_pair(previousTouchingPoint, nbPoints-1));
                }
            }
        }

        return vpTouchingParts;
    }
}
}
}