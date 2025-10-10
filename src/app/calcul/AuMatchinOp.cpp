//APP
#include <app/calcul/AuMatchingOp.h>
#include <app/params/ThemeParameters.h>
#include <app/detail/Angle.h>
#include <app/detail/extractNotTouchingParts.h>
#include <app/detail/getSubString.h>
#include <app/detail/refining.h>

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
#include <ome2/feature/sql/NotDestroyedTools.h>


using namespace app::detail;

namespace app{
namespace calcul{

	///
	///
	///
    void AuMatchingOp::Compute(
        std::string countryCode, 
        bool verbose) 
    {
        AuMatchingOp auMatchingOp(countryCode, verbose);
        auMatchingOp._compute();
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
        
        _shapeLogger->closeShape( "not_boundaries" );
        _shapeLogger->closeShape( "contact_points" );
        _shapeLogger->closeShape( "not_boundaries_trim" );
        _shapeLogger->closeShape( "not_boundaries_merged" );
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
        std::string const areaTableName = epgParams.getValue( AREA_TABLE ).toString();
        std::string const boundaryTypeName = epgParams.getValue( BOUNDARY_TYPE ).toString();
        std::string const typeCostlineValue = epgParams.getValue( TYPE_COASTLINE ).toString();

        // app parameters
        params::ThemeParameters* themeParameters = params::ThemeParametersS::getInstance();
        std::string const landmaskTableName = themeParameters->getValue( LANDMASK_TABLE ).toString();
        std::string const noCoastTableName = themeParameters->getValue( NOCOAST_TABLE ).toString();
        
        //--
        _fsBoundary = context->getFeatureStore( epg::TARGET_BOUNDARY );
        //--
        _fsLandmask = context->getDataBaseManager().getFeatureStore(landmaskTableName, idName, geomName);
        //--
        _fsNoCoast = context->getDataBaseManager().getFeatureStore(noCoastTableName, idName, geomName);
        //--
        _fsArea = context->getDataBaseManager().getFeatureStore(areaTableName, idName, geomName);
        //--
        _mlsToolBoundary = new epg::tools::MultiLineStringTool( ome2::feature::sql::NotDestroyedTools::GetFeatureFilter("country LIKE '%"+_countryCode+"%' AND "+boundaryTypeName+"::text NOT LIKE '%"+typeCostlineValue+"%'", _fsBoundary), *_fsBoundary );
        
        //--
        _shapeLogger = epg::log::ShapeLoggerS::getInstance();
        _shapeLogger->addShape( "not_boundaries", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "contact_points", epg::log::ShapeLogger::POINT );
        _shapeLogger->addShape( "not_boundaries_trim", epg::log::ShapeLogger::LINESTRING );
        _shapeLogger->addShape( "not_boundaries_merged", epg::log::ShapeLogger::LINESTRING );
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

        //--
        _logger->log(epg::log::INFO, "[END] initialization: "+epg::tools::TimeTools::getTime());
    };

    ///
	///
	///
    void AuMatchingOp::_compute() 
    {

        epg::Context* context = epg::ContextS::getInstance();

        //epg params
        epg::params::EpgParameters const& epgParams = context->getEpgParameters();

        std::string const idName = epgParams.getValue( ID ).toString();
        std::string const geomName = epgParams.getValue( GEOM ).toString();
        std::string const countryCodeName = epgParams.getValue( COUNTRY_CODE ).toString();

        //app params
        params::ThemeParameters* themeParameters = params::ThemeParametersS::getInstance();

        double const boundMaxDist = themeParameters->getValue( AU_BOUNDARY_MAX_DIST ).toDouble();
        double const boundSearchDist = themeParameters->getValue( AU_BOUNDARY_SEARCH_DIST ).toDouble();
        double const boundSnapDist = themeParameters->getValue( AU_BOUNDARY_SNAP_DIST ).toDouble();
        double const segmentMinLength = themeParameters->getValue( AU_SEGMENT_MIN_LENGTH ).toDouble();

        //--
        ign::geometry::MultiLineString mLsLandmaskNoCoasts;
        tools::SegmentIndexedGeometryCollection* indexedLandmaskNoCoasts = new tools::SegmentIndexedGeometryCollection();

        ign::feature::FeatureIteratorPtr itNoCoast = ome2::feature::sql::NotDestroyedTools::GetFeatures(*_fsNoCoast, ign::feature::FeatureFilter(countryCodeName+" LIKE '%"+_countryCode+"%'"));
        while (itNoCoast->hasNext()) {
             ign::feature::Feature const& fNoCoast = itNoCoast->next();
             ign::geometry::LineString const& lsNoCoast = fNoCoast.getGeometry().asLineString();
             mLsLandmaskNoCoasts.addGeometry(lsNoCoast);
        }
        // a faire dans un deuxième temps car les pointeurs sur les éléments de vecteur peuvent être modifiés en cas de réallocation
        int numGroup = 0;
        for (size_t i = 0 ; i < mLsLandmaskNoCoasts.numGeometries() ; ++i) {
            int group = mLsLandmaskNoCoasts.lineStringN(i).isClosed() ? numGroup++ : -1;
            indexedLandmaskNoCoasts->addGeometry(&mLsLandmaskNoCoasts.lineStringN(i), group);
        }

        // on indexe les contours frontière fermés 
        ign::feature::FeatureIteratorPtr itBoundary = ome2::feature::sql::NotDestroyedTools::GetFeatures(*_fsBoundary, ign::feature::FeatureFilter(countryCodeName+" LIKE '%"+_countryCode+"%'"));
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
        ign::feature::FeatureIteratorPtr itArea = ome2::feature::sql::NotDestroyedTools::GetFeatures( *_fsArea, ign::feature::FeatureFilter(countryCodeName+" = '"+_countryCode+"'"));

        //patience
        int numFeatures = ome2::feature::sql::NotDestroyedTools::NumFeatures( *_fsArea, ign::feature::FeatureFilter(countryCodeName+" = '"+_countryCode + "'"));
        boost::progress_display display( numFeatures , std::cout, "[ au_matching % complete ]\n") ;

        while (itArea->hasNext())
        {
            ++display;

            ign::feature::Feature fAu = itArea->next();
            ign::geometry::MultiPolygon mpAu = fAu.getGeometry().asMultiPolygon();
            ign::geometry::algorithm::PolygonBuilderV1 polyBuilder;
            std::set< size_t > sAddedClosedBoundary;

            

            //DEBUG
            // 3376289.285,2318766.474 
            // 3457799,2246605
            // bool test = false;
            // if (fAu.getId() == "48043b89-a1d6-404b-823d-b03bd65378ba" ) {
            //     test = true;
            // }
            // if (fAu.getId() == "48043b89-a1d6-404b-823d-b03bd65378ba" ) {
            //     test = true;
            // }
            // if (fAu.getId() == "48043b89-a1d6-404b-823d-b03bd65378ba" ) {
            //     test = true;
            // }
            // if (fAu.getId() == "5300ab5a-df6e-4c09-8d05-ae5b7e20290f" ) {
            //     test = true;
            // }
            // if ( !test )
            //     continue;

            if (_verbose) _logger->log(epg::log::DEBUG,fAu.getId());

            detail::refineAreaWithLsEndings(mLsLandmaskNoCoasts, mpAu);

            bool bIsModified = false;
            for ( int i = 0 ; i < mpAu.numGeometries() ; ++i )
            {
                ign::geometry::Polygon & pAu = mpAu.polygonN(i);

                //DEBUG
                // if( pAu.intersects(ign::geometry::Point(3758303.1,2173207.6))) {
                //     bool t = true;
                // }

                for ( int j = 0 ; j < pAu.numRings() ; ++j )                                                                                                                                                                                                                                                                              
                {
                    ign::geometry::LineString & ring = pAu.ringN(j);

                    // on extrait les partis de l'AU qui ne sont pas des frontieres
                    std::vector<std::pair<int,int>> vpNotTouchingParts;
                    std::vector<int> vTouchingPoints;
                    detail::extractNotTouchingParts( indexedLandmaskNoCoasts, ring, vpNotTouchingParts, &vTouchingPoints);

                    //DEBUG
                    // if (!vpNotTouchingParts.empty()) {
                    //     ign::geometry::Point p1 = ring[vpNotTouchingParts.front().first];
                    //     ign::geometry::Point p2 = ring[vpNotTouchingParts.front().second];
                    // }

                    // on gere les boucles
                    if (vpNotTouchingParts.empty()) {
                        bIsModified = true;

                        std::set< size_t > sClosedBoundary;
                        qTreeClosedBoundary.query( ring.getEnvelope(), sClosedBoundary );

                        bool foundBoundary = false;

                        std::set< size_t >::const_iterator sit;
                        for( sit = sClosedBoundary.begin() ; sit != sClosedBoundary.end() ; ++sit )
                        {
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
                    
                    // on projette les eventuels points de contact avec la frontiere
                    ign::geometry::LineString ringWithContactPoints = ring;
                    _projectTouchingPoints(_mlsToolBoundary, ringWithContactPoints, vTouchingPoints, boundSearchDist, boundSnapDist);

                    std::vector<ign::geometry::LineString> vLsNotTouchingParts;
                    for ( int i = 0 ; i < vpNotTouchingParts.size() ; ++i ) {
                        vLsNotTouchingParts.push_back(detail::getSubString(vpNotTouchingParts[i], ringWithContactPoints));
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

                        //DEBUG
                        // int nb1 = vLsNotTouchingParts[i].numPoints();
                        // ign::geometry::Point pt1_s = vLsNotTouchingParts[i].startPoint();
                        // ign::geometry::Point pt1_e = vLsNotTouchingParts[i].endPoint();

                        vLsNotTouchingParts[i] = lsSplitter.truncAtEnds();

                        //DEBUG
                        // int nb2 = vLsNotTouchingParts[i].numPoints();
                        // ign::geometry::Point pt2_s = vLsNotTouchingParts[i].startPoint();
                        // ign::geometry::Point pt2_e = vLsNotTouchingParts[i].endPoint();
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
                    _findAngles(_mlsToolBoundary, vLsNotTouchingParts, vGeomFeatures, boundSearchDist, boundSnapDist);

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
                        vLsTouchingParts.push_back(detail::getSubString(vpTouchingParts[i], ringWithContactPoints));
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
            }

            ign::geometry::MultiPolygon newGeometry = polyBuilder.getMultiPolygon();

            if (!bIsModified || newGeometry.equals(mpAu)) {
                _shapeLogger->writeFeature( "resulting_polygons_not_modified", fAu );
            } else {
                fAu.setGeometry(newGeometry);
                if ( !newGeometry.isEmpty() )
                {
                    _fsArea->modifyFeature(fAu);
                } else {
                    _logger->log(epg::log::ERROR, "MultiPolygon is empty [id] " + fAu.getId());
                }
                
                if ( !newGeometry.isValid() ) {
                    _logger->log(epg::log::ERROR, "MultiPolygon is not valid [id] " + fAu.getId());
                    _shapeLogger->writeFeature( "resulting_polygons_not_valid", fAu );
                }
                _shapeLogger->writeFeature( "resulting_polygons", fAu );
            }
        }

        delete indexedLandmaskNoCoasts;
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
        const ign::geometry::Point & pt
    ) const {
        std::vector<ign::geometry::LineString> vSegments;
        ign::geometry::Envelope bbox = pt.getEnvelope().expandBy( 1e-5 );
        ign::geometry::Polygon bboxGeom = bbox.toPolygon();
        indexedGeom->getSegments( bbox, vSegments );
        if (vSegments.size()==2) {
            
            ign::geometry::Point const& endPoint1 = bboxGeom.intersects(vSegments.front().startPoint())? vSegments.front().endPoint() : vSegments.front().startPoint();
            ign::geometry::Point const& endPoint2 = bboxGeom.intersects(vSegments.back().startPoint())? vSegments.back().endPoint() : vSegments.back().startPoint();

            double angle = epg::tools::geometry::angle(endPoint1.toVec2d()-pt.toVec2d(), endPoint2.toVec2d()-pt.toVec2d());

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
        double searchDistance
    ) const {
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
        double distance
    ) const {
        double deltaAngle = abs(refAngle-angle);
        double angleScore = -1;
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

        double distScore = 1/(1+distance);
        // double test2 = 1/exp(distance);

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
        bool positiveDirection 
    ) const {
        double distance = ls.pointN(startIndex).distance(refPoint);
        size_t index = startIndex;
        size_t bestIndex = -1;
        double bestScore = 0;
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
        double boundSearchDistance,
        double vertexSearchDistance
        ) const
    {					
        std::pair< bool, ign::geometry::Point > foundProjection = mlsTool->project( pt, boundSearchDistance);
        if (!foundProjection.first) return std::make_pair(false, ign::geometry::Point());

        ign::geometry::MultiLineString mls;
        mlsTool->getLocal( pt.getEnvelope().expandBy(boundSearchDistance), mls);

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

                int newIndex = _findIndex(vMergedLs[i], indexPoint, foundProjection.second, angle, vertexSearchDistance);

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
        double searchDistance,
        double vertexSnapDist
    ) const {
        for ( size_t i = 0 ; i < vLs.size() ; ++i ) {
            // on projete les points sur la NotTouchingPart et on coupe
            epg::tools::geometry::LineStringSplitter lsSplitter( vLs[i], 1e-5 );

            std::pair<bool, ign::geometry::Point> foundPointStart = std::make_pair(false, ign::geometry::Point());
            std::pair<bool, ign::geometry::Point> foundPointEnd = std::make_pair(false, ign::geometry::Point());

            if ( vGeomFeatures[i].first != 0 ) {
                foundPointStart = _findCandidate(mlsTool, vLs[i].startPoint(), vGeomFeatures[i].first, searchDistance, vertexSnapDist);
                if (foundPointStart.first) {
                    ign::geometry::Point projectedPoint;
                    bool found = epg::tools::geometry::project( vLs[i], foundPointStart.second, projectedPoint, vertexSnapDist);
                    if ( !found ) projectedPoint = foundPointStart.second;

                    lsSplitter.addCuttingGeometry(projectedPoint);
                }
            }
            if ( vGeomFeatures[i].second != 0 ) {
                foundPointEnd = _findCandidate(mlsTool, vLs[i].endPoint(), vGeomFeatures[i].second, searchDistance, vertexSnapDist);
                if (foundPointEnd.first) {
                    ign::geometry::Point projectedPoint;
                    bool found = epg::tools::geometry::project( vLs[i], foundPointEnd.second, projectedPoint, vertexSnapDist);
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
        double snapDistOnVertex
    ) const {
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