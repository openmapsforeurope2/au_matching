//APP
#include <app/calcul/matching/AuMatchingOp.h>
#include <app/params/ThemeParameters.h>
#include <app/detail/Angle.h>

//BOOST
#include <boost/progress.hpp>

//SOCLE
#include <ign/geometry/algorithm/LineMergerOpGeos.h>
#include <ign/math/Line2T.h>

//EPG
#include <epg/Context.h>
#include <epg/params/EpgParameters.h>
#include <epg/sql/tools/numFeatures.h>
#include <epg/sql/DataBaseManager.h>
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

        double const maxDist1 = themeParameters->getValue( AU_BOUNDARY_LOOP_MAX_DIST ).toDouble();
        double const searchDistance1 = themeParameters->getValue( AU_BOUNDARY_LOOP_SEARCH_DIST ).toDouble();
        double const snapDistOnVertex1 = themeParameters->getValue( AU_BOUNDARY_LOOP_SNAP_DIST ).toDouble();
        double const searchDistance2 = themeParameters->getValue( AU_BOUNDARY_SEARCH_DIST ).toDouble();
        double const snapDistOnVertex2 = themeParameters->getValue( AU_BOUNDARY_SNAP_DIST ).toDouble();
        double const maxDist3 = themeParameters->getValue( AU_COAST_MAX_DIST ).toDouble();
        double const searchDistance3 = themeParameters->getValue( AU_COAST_SEARCH_DIST ).toDouble();
        double const snapDistOnVertex3 = themeParameters->getValue( AU_COAST_SNAP_DIST ).toDouble();
        double const segmentMinLength = themeParameters->getValue( AU_SEGMENT_MIN_LENGTH ).toDouble();

        /*
        *********************************************************************************************
        ** On 
        *********************************************************************************************
        */
        ign::feature::FeatureIteratorPtr itCoast = _fsBoundary->getFeatures(ign::feature::FeatureFilter(countryCodeName+" = '"+_countryCode+"' AND "+boundaryTypeName+" = '"+typeCostlineValue+"'"));

        // calculer tous les chemins sur le Landmask en prenant pour source et target les extrémités des costlines
        _logger->log(epg::log::INFO, "[START] coast path computation: "+epg::tools::TimeTools::getTime());
        ign::geometry::MultiLineString mlsLandmaskCoastPath;
        while (itCoast->hasNext())
        {
            ign::feature::Feature const& fCoast = itCoast->next();
            ign::geometry::LineString const& lsCoast = fCoast.getGeometry().asLineString();

            std::pair< bool, ign::geometry::LineString > pathFound = _mlsToolLandmask->getPathAlong(
                lsCoast.startPoint(),
                lsCoast.endPoint(),
                lsCoast,
                maxDist3,
                searchDistance3,
                snapDistOnVertex3
            );

            if ( !pathFound.first )
            {
                _shapeLogger->writeFeature( "coastline_path_not_found", fCoast );
            }
            else
            {
                mlsLandmaskCoastPath.addGeometry(pathFound.second);
                
                ign::feature::Feature f = fCoast;
                f.setGeometry(pathFound.second);
                _shapeLogger->writeFeature( "coastline_path", f );
            }
        }
        _logger->log(epg::log::INFO, "[END] coast path computation: "+epg::tools::TimeTools::getTime());

        /*
        *********************************************************************************************
        ** On extrait les parties du landmask qui ne sont pas des cotes
        *********************************************************************************************
        */
        ign::geometry::MultiPolygon mpLandmask;
        ign::feature::FeatureIteratorPtr itLandmask= _fsLandmask->getFeatures(ign::feature::FeatureFilter(countryCodeName+" = '"+_countryCode+"'"));
        while (itLandmask->hasNext())
        {
            ign::feature::Feature const& fLandmask = itLandmask->next();
            ign::geometry::MultiPolygon const& mp = fLandmask.getGeometry().asMultiPolygon();
            for ( int i = 0 ; i < mp.numGeometries() ; ++i ) {
                mpLandmask.addGeometry(mp.polygonN(i));
            }
        }

        _logger->log(epg::log::INFO, "[START] no coast computation: "+epg::tools::TimeTools::getTime());
        const epg::tools::geometry::SegmentIndexedGeometryInterface* indexedLandmaskCoasts = new epg::tools::geometry::SegmentIndexedGeometry( &mlsLandmaskCoastPath );

        std::vector<std::vector<std::vector<std::pair<int,int>>>> vLandmaskNoCoasts;
        _extractNotTouchingParts( indexedLandmaskCoasts, mpLandmask, vLandmaskNoCoasts );

        std::vector<ign::geometry::LineString> vLsLandmaskNoCoasts;
        epg::tools::geometry::SegmentIndexedGeometryCollection* indexedLandmaskNoCoasts = new epg::tools::geometry::SegmentIndexedGeometryCollection();
        for ( int np = 0 ; np < vLandmaskNoCoasts.size() ; ++np ) {
            for ( int nr = 0 ; nr < vLandmaskNoCoasts[np].size() ; ++nr ) {
                for ( int i = 0 ; i < vLandmaskNoCoasts[np][nr].size() ; ++i ) {
                    vLsLandmaskNoCoasts.push_back( _getSubString(vLandmaskNoCoasts[np][nr][i], mpLandmask.polygonN(np).ringN(nr)) );
                }
            }
        }
        // a faire dans un deuxième temps car les pointeurs sur les éléments de vecteur peuvent être modifiés en cas de réallocation
        for (size_t i = 0 ; i < vLsLandmaskNoCoasts.size() ; ++i) {
            indexedLandmaskNoCoasts->addGeometry(&vLsLandmaskNoCoasts[i]);
        }

        // epg::tools::geometry::SegmentIndexedGeometryCollection* indexedLandmaskNoCoasts = new epg::tools::geometry::SegmentIndexedGeometryCollection();
        // indexedLandmaskNoCoasts->addGeometry(&mpLandmask);

        _logger->log(epg::log::INFO, "[END] no coast computation: "+epg::tools::TimeTools::getTime());

        for ( int i = 0 ; i < vLsLandmaskNoCoasts.size() ; ++i )
        {
            ign::feature::Feature feature;
            feature.setGeometry( vLsLandmaskNoCoasts[i] );
            _shapeLogger->writeFeature( "boundary_not_touching_coast", feature );
        }

        /*
        *********************************************************************************************
        ** 
        *********************************************************************************************
        */

        // Go through objects intersecting the boundary
        ign::feature::sql::FeatureStorePostgis* fsAu = context->getDataBaseManager().getFeatureStore(workingTable, idName, geomName);
        ign::feature::FeatureIteratorPtr itAu = fsAu->getFeatures(ign::feature::FeatureFilter(countryCodeName+" = '"+_countryCode+"'"));
        // ign::feature::FeatureIteratorPtr itAu = fsAu->getFeatures(ign::feature::FeatureFilter("inspireid in ('0d40d383-ebd2-4c61-a75f-9ac37ec23b95')"));

        //patience
        int numFeatures = epg::sql::tools::numFeatures( *fsAu, ign::feature::FeatureFilter(countryCodeName+" = '"+_countryCode+"'"));
        boost::progress_display display( numFeatures , std::cout, "[ au_matching  % complete ]\n") ;        

        while (itAu->hasNext())
        {
            ign::feature::Feature fAu = itAu->next();
            ign::geometry::MultiPolygon const& mpAu = fAu.getGeometry().asMultiPolygon();
            ign::geometry::MultiPolygon newGeometry;

            if (_verbose) _logger->log(epg::log::DEBUG,fAu.getId());

            bool bIsModified = false;
            for ( int i = 0 ; i < mpAu.numGeometries() ; ++i )
            {
                ign::geometry::Polygon const& pAu = mpAu.polygonN(i);

                // if (pAu.intersects(ign::geometry::Point(3969637.78,3160444.92))){
                //     bool pouet = true;
                // }else {
                //     continue;
                // }
                
                std::vector<ign::geometry::LineString> vRings;
                for ( int j = 0 ; j < pAu.numRings() ; ++j )                                                                                                                                                                                                                                                                              
                {
                    ign::geometry::LineString const& ring = pAu.ringN(j);

                    // on extrait les partis de l'AU qui ne sont pas des frontieres
                    std::vector<std::pair<int,int>> vpNotTouchingParts;
                    std::vector<int> vTouchingPoints;
                    _extractNotTouchingParts( indexedLandmaskNoCoasts, ring, vpNotTouchingParts, &vTouchingPoints);

                    // on gere les boucles
                    if (vpNotTouchingParts.empty()) {
                        std::pair< bool, ign::geometry::LineString > foundPath = _mlsToolBoundary->getPathAlong( ring.startPoint(), ring.endPoint(), ring, maxDist1, searchDistance1, snapDistOnVertex1);
                        if (foundPath.first) {
                            bIsModified = true;
                            vRings.push_back(foundPath.second);

                            ign::feature::Feature feature = fAu;
                            feature.setGeometry( foundPath.second );
                            _shapeLogger->writeFeature( "boucle", feature );
                        } else {
                            vRings.push_back(ring);
                            // on projete les eventuels points de contact avec la frontiere
                            _projectTouchingPoints(_mlsToolBoundary, vRings.back(), vTouchingPoints, searchDistance2, snapDistOnVertex2);
                            if (vTouchingPoints.size() > 0) bIsModified = true;
                        }
                        continue;
                    }
                    bIsModified = true;

                    // on projete les eventuels points de contact avec la frontiere
                    ign::geometry::LineString ringWithContactPoints = ring;
                    _projectTouchingPoints(_mlsToolBoundary, ringWithContactPoints, vTouchingPoints, searchDistance2, snapDistOnVertex2);

                    std::vector<ign::geometry::LineString> vLsNotTouchingParts;
                    for ( int i = 0 ; i < vpNotTouchingParts.size() ; ++i ) {
                        vLsNotTouchingParts.push_back(_getSubString(vpNotTouchingParts[i], ringWithContactPoints));
                    }

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
                    _findAngles(_mlsToolBoundary, vLsNotTouchingParts, vGeomFeatures, searchDistance2);

                    // on reconstitue le nouveau contour en concatenant les parties ne touchant pas 
                    // la frontiere et les parties touchant la frontieres projetees sur la frontiere cible
                    ign::geometry::Point previousRingEndPoint = vLsNotTouchingParts.rbegin()->endPoint();

                    ign::geometry::LineString newRing;
                    bool bErrorConstructingRing = false;
                    for ( int i = 0 ; i < vLsNotTouchingParts.size() ; ++i )
                    {
                        std::pair< bool, ign::geometry::LineString > pathFound = _mlsToolBoundary->getPath( 
                            previousRingEndPoint,
                            vLsNotTouchingParts[i].startPoint(),
                            searchDistance2,
                            snapDistOnVertex2
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

                    vRings.push_back(newRing);

                    for ( int i = 0 ; i < vLsNotTouchingParts.size() ; ++i )
                    {
                        ign::feature::Feature feature = fAu;
                        feature.setGeometry( vLsNotTouchingParts[i] );
                        _shapeLogger->writeFeature( "not_boundaries_merged", feature );
                    }    
                }
                ign::geometry::Polygon poly(vRings);

                if ( !poly.isEmpty() )
                {
                    if ( !poly.isValid() )
                    {
                        _logger->log(epg::log::ERROR, "Polygon is not valid [id] " + fAu.getId());
                    }
                    newGeometry.addGeometry(poly);
                } else {
                    _logger->log(epg::log::ERROR, "Polygon is empty [id] " + fAu.getId());
                }
            }

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
        // delete indexedLandmaskCoasts;
        
    };

    ///
	///
	///
    void AuMatchingOp::_getAngles( 
        epg::tools::geometry::SegmentIndexedGeometryCollection* indexedGeom, 
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
        epg::tools::geometry::SegmentIndexedGeometryCollection* indexedGeom, 
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
            if (abs(angle-M_PI)<0.052359878 /*3 degres*/) return 0;
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
        const epg::tools::geometry::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::LineString & ls, 
        std::vector<std::pair<int,int>> & vNotTouchingParts,
        std::vector<int>* vTouchingPoints) const
    {
        bool isRing = ls.isClosed();
        int nbSegments = ls.numSegments();
        int nbPoints = ls.numPoints();

        std::vector < bool > vIsTouchingPoints(nbPoints, false);
        for ( int k = 0 ; k < nbPoints ; ++k ) {
            if ( refGeom->distance( ls.pointN(k), 0.1 ) < 0 ) continue;
            vIsTouchingPoints[k] = true;
        }

        int notTouchingFirstSegment = -1;
        bool previousSegmentIsTouching = !isRing ? false : vIsTouchingPoints[nbSegments-1] && vIsTouchingPoints[nbSegments] ;
        std::vector < bool > vTouchingSegments(nbSegments, false);
        for ( int currentSegment = 0 ; currentSegment < nbSegments ; ++currentSegment )
        {
            bool currentSegmentIsTouching = vIsTouchingPoints[currentSegment] && vIsTouchingPoints[currentSegment+1];

            if (currentSegmentIsTouching)
            {
                vTouchingSegments[currentSegment] = true;
            } else if ( !previousSegmentIsTouching ) {
                if ( vIsTouchingPoints[currentSegment] ) {
                    if (vTouchingPoints != 0) vTouchingPoints->push_back(currentSegment);
                }
            } else if (notTouchingFirstSegment < 0) {
                notTouchingFirstSegment = currentSegment;
            }
            previousSegmentIsTouching = vTouchingSegments[currentSegment];
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
    void AuMatchingOp::_extractNotTouchingParts(
        const epg::tools::geometry::SegmentIndexedGeometryInterface* refGeom,
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
        const epg::tools::geometry::SegmentIndexedGeometryInterface* refGeom,
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
}
}
}