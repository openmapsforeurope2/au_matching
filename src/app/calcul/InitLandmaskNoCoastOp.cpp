//APP
#include <app/calcul/InitLandmaskNoCoastOp.h>
#include <app/params/ThemeParameters.h>
#include <app/detail/extractNotTouchingParts.h>
#include <app/detail/getSubString.h>

//BOOST
#include <boost/progress.hpp>

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
    void InitLandmaskNoCoastOp::Compute(
        std::string countryCode, 
        bool verbose) 
    {
        InitLandmaskNoCoastOp InitLandmaskNoCoastOp(countryCode, verbose);
        InitLandmaskNoCoastOp._compute();
    }

    ///
	///
	///
    InitLandmaskNoCoastOp::InitLandmaskNoCoastOp( std::string countryCode, bool verbose ):
        _countryCode( countryCode ),
        _verbose( verbose )
    {
        _init();
    }

    ///
	///
	///
    InitLandmaskNoCoastOp::~InitLandmaskNoCoastOp()
    {        
    }

    ///
	///
	///
    void InitLandmaskNoCoastOp::_init() 
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
        std::string const nocoastTableName = themeParameters->getValue( NOCOAST_TABLE ).toString();
        
        //--
        _fsLandmask = context->getDataBaseManager().getFeatureStore(landmaskTableName, idName, geomName);
        //--
        _fsNoCoast = context->getDataBaseManager().getFeatureStore(nocoastTableName, idName, geomName);
        
        //--
        _logger->log(epg::log::INFO, "[END] initialization: "+epg::tools::TimeTools::getTime());
    };

    ///
	///
	///
    void InitLandmaskNoCoastOp::_compute() 
    {

        epg::Context* context = epg::ContextS::getInstance();

        //epg params
        epg::params::EpgParameters const& epgParams = context->getEpgParameters();

        std::string const idName = epgParams.getValue( ID ).toString();
        std::string const geomName = epgParams.getValue( GEOM ).toString();
        std::string const countryCodeName = epgParams.getValue( COUNTRY_CODE ).toString();

        //app params
        params::ThemeParameters* themeParameters = params::ThemeParametersS::getInstance();

        std::string const landCoverTypeName = themeParameters->getValue( LAND_COVER_TYPE ).toString();
		std::string const landAreaValue = themeParameters->getValue( TYPE_LAND_AREA ).toString();

        //--
        ign::geometry::MultiPolygon mpLandmask;
		ign::feature::FeatureIteratorPtr itLandmask = ome2::feature::sql::NotDestroyedTools::GetFeatures(*_fsLandmask, ign::feature::FeatureFilter(landCoverTypeName + " = '" + landAreaValue + "' AND " + countryCodeName + " = '" + _countryCode + "'"));
		while (itLandmask->hasNext())
        {
            ign::feature::Feature const& fLandmask = itLandmask->next();
            ign::geometry::MultiPolygon const& mp = fLandmask.getGeometry().asMultiPolygon();
            for ( int i = 0 ; i < mp.numGeometries() ; ++i ) {
                mpLandmask.addGeometry(mp.polygonN(i));
            }
        }

        //--
        ign::geometry::MultiLineString mlsLandmaskCoastPath;
        ign::feature::FeatureIteratorPtr itCoast = ome2::feature::sql::NotDestroyedTools::GetFeatures(*_fsCoast, ign::feature::FeatureFilter(countryCodeName+" LIKE '%"+_countryCode+"%'"));
        while (itCoast->hasNext()) {
             ign::feature::Feature const& fCoast = itCoast->next();
             ign::geometry::LineString const& lsCoast = fCoast.getGeometry().asLineString();
             mlsLandmaskCoastPath.addGeometry(lsCoast);
        }

        //--
        const tools::SegmentIndexedGeometryInterface* indexedLandmaskCoasts = new tools::SegmentIndexedGeometry( &mlsLandmaskCoastPath );

        std::vector<std::vector<std::vector<std::pair<int,int>>>> vLandmaskNoCoasts;
        detail::extractNotTouchingParts( indexedLandmaskCoasts, mpLandmask, vLandmaskNoCoasts );

        for ( int np = 0 ; np < vLandmaskNoCoasts.size() ; ++np ) {
            for ( int nr = 0 ; nr < vLandmaskNoCoasts[np].size() ; ++nr ) {
                for ( int i = 0 ; i < vLandmaskNoCoasts[np][nr].size() ; ++i ) {
                    ign::feature::Feature fNoCoast = _fsNoCoast->newFeature();
                    fNoCoast.setGeometry(detail::getSubString(vLandmaskNoCoasts[np][nr][i], mpLandmask.polygonN(np).ringN(nr)));
                    _fsNoCoast->createFeature(fNoCoast);
                }
            }
        }
    };
}
}

