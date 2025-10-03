//APP
#include <app/calcul/InitLandmaskCoastOp.h>
#include <app/params/ThemeParameters.h>

//BOOST
#include <boost/progress.hpp>

//SOCLE
#include <ign/geometry/algorithm/LineMergerOpGeos.h>

//EPG
#include <epg/Context.h>
#include <epg/params/EpgParameters.h>
#include <epg/sql/DataBaseManager.h>
#include <epg/tools/StringTools.h>
#include <epg/tools/TimeTools.h>
#include <ome2/feature/sql/NotDestroyedTools.h>


namespace app{
namespace calcul{

	///
	///
	///
    void InitLandmaskCoastOp::Compute(
        std::string countryCode, 
        bool verbose
    ) {
        InitLandmaskCoastOp InitLandmaskCoastOp(countryCode, verbose);
        InitLandmaskCoastOp._compute();
    }

    ///
	///
	///
    InitLandmaskCoastOp::InitLandmaskCoastOp( std::string countryCode, bool verbose ):
        _countryCode( countryCode ),
        _verbose( verbose )
    {
        _init();
    }

    ///
	///
	///
    InitLandmaskCoastOp::~InitLandmaskCoastOp()
    {
        delete _mlsToolLandmask;
        
        _shapeLogger->closeShape( "coastline_path_not_found" );

        epg::log::ShapeLoggerS::kill();
    }

    ///
	///
	///
    void InitLandmaskCoastOp::_init()
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
        std::string const coastTableName = themeParameters->getValue( COAST_TABLE ).toString();
        
        //--
        _fsBoundary = context->getFeatureStore( epg::TARGET_BOUNDARY );
        //--
        _fsLandmask = context->getDataBaseManager().getFeatureStore(landmaskTableName, idName, geomName);
        //--
        _fsCoast = context->getDataBaseManager().getFeatureStore(coastTableName, idName, geomName);
        
        _mlsToolLandmask = new epg::tools::MultiLineStringTool( ome2::feature::sql::NotDestroyedTools::GetFeatureFilter(countryCodeName+" = '"+_countryCode+"'"), *_fsLandmask );
        
        //--
        _shapeLogger = epg::log::ShapeLoggerS::getInstance();
        _shapeLogger->setDataDirectory(context->getLogDirectory());
        _shapeLogger->addShape( "coastline_path_not_found", epg::log::ShapeLogger::LINESTRING );

        //--
        _logger->log(epg::log::INFO, "[END] initialization: "+epg::tools::TimeTools::getTime());
    };

    ///
	///
	///
    void InitLandmaskCoastOp::_compute() const
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
        
        double const coastMaxDist = themeParameters->getValue( AU_COAST_MAX_DIST ).toDouble();
        double const coastSearcDist = themeParameters->getValue( AU_COAST_SEARCH_DIST ).toDouble();
        double const coastSnapDist = themeParameters->getValue( AU_COAST_SNAP_DIST ).toDouble();

        //--
		ign::feature::FeatureIteratorPtr itCoast = ome2::feature::sql::NotDestroyedTools::GetFeatures(*_fsBoundary, ign::feature::FeatureFilter(countryCodeName + " LIKE '%" + _countryCode + "%' AND " + boundaryTypeName + "::text LIKE '%" + typeCostlineValue + "%'"));

		ign::geometry::algorithm::LineMergerOpGeos merger;
        while (itCoast->hasNext())
        {
            ign::feature::Feature const& fCoast = itCoast->next();
            ign::geometry::LineString const& lsCoast = fCoast.getGeometry().asLineString();
            std::string icc = fCoast.getAttribute( countryCodeName ).toString();
            std::string boundType = fCoast.getAttribute( boundaryTypeName ).toString();

            //DEBUG
            _logger->log(epg::log::DEBUG, fCoast.getId());

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

        //DEBUG
        if(vCoastLs.size() == 0) {
            _logger->log(epg::log::DEBUG, "no coast!!");
        }

        // calculer tous les chemins sur le Landmask en prenant pour source et target les extrémités des costlines
        for (size_t i = 0 ; i < vCoastLs.size() ; ++i)
        {
            //DEBUG
            _logger->log(epg::log::DEBUG, vCoastLs[i].startPoint().toString());
            _logger->log(epg::log::DEBUG, vCoastLs[i].endPoint().toString());

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
                //DEBUG
                _logger->log(epg::log::DEBUG, "path not found");

                ign::feature::Feature feat;
                feat.setGeometry(vCoastLs[i]);
                _shapeLogger->writeFeature( "coastline_path_not_found", feat );
            }
            else
            {
                //DEBUG
                _logger->log(epg::log::DEBUG, "add coast");
                _logger->log(epg::log::DEBUG, pathFound.second.startPoint().toString());
                _logger->log(epg::log::DEBUG, pathFound.second.endPoint().toString());

                ign::feature::Feature feat = _fsCoast->newFeature();
                feat.setGeometry(pathFound.second);
                feat.setAttribute(countryCodeName,ign::data::String(_countryCode));
                _fsCoast->createFeature( feat );
            }
        }
    };
}
}