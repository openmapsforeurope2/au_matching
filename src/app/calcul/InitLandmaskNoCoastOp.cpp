//APP
#include <app/calcul/InitLandmaskNoCoastOp.h>
#include <app/params/ThemeParameters.h>
#include <app/detail/extractNotTouchingParts.h>
#include <app/detail/getSubString.h>
#include <app/detail/refining.h>

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
        std::string const coastTableName = themeParameters->getValue( COAST_TABLE ).toString();
        std::string const nocoastTableName = themeParameters->getValue( NOCOAST_TABLE ).toString();
        
        //--
        _fsLandmask = context->getDataBaseManager().getFeatureStore(landmaskTableName, idName, geomName);
        //--
        _fsCoast = context->getDataBaseManager().getFeatureStore(coastTableName, idName, geomName);
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

        // verifier que les cotes ont leurs extremités qui correspondent à des points intermédiaires du landmask 
        // (le calcul du chemin réalisé à l'étape précédente peut démarré/finir à l'intérieur d'un segment)
        _logger->log(epg::log::INFO, "[START] refining landmask : "+epg::tools::TimeTools::getTime());
        detail::refineAreaWithLsEndings(mlsLandmaskCoastPath, mpLandmask);
        _logger->log(epg::log::INFO, "[END] refining landmask : "+epg::tools::TimeTools::getTime());

        //--
        _logger->log(epg::log::INFO, "[START] extracting nocoast landmask parts : "+epg::tools::TimeTools::getTime());
        const tools::SegmentIndexedGeometryInterface* indexedLandmaskCoasts = new tools::SegmentIndexedGeometry( &mlsLandmaskCoastPath );

        std::vector<std::vector<std::vector<std::pair<int,int>>>> vLandmaskNoCoasts;
        detail::extractNotTouchingParts( indexedLandmaskCoasts, mpLandmask, vLandmaskNoCoasts );
        _logger->log(epg::log::INFO, "[END] extracting nocoast landmask parts : "+epg::tools::TimeTools::getTime());

        //patience
        size_t numCoast = 0;
        for ( int np = 0 ; np < vLandmaskNoCoasts.size() ; ++np ) 
            for ( int nr = 0 ; nr < vLandmaskNoCoasts[np].size() ; ++nr )
                numCoast += vLandmaskNoCoasts[np][nr].size();
        boost::progress_display display( numCoast , std::cout, "[ compute landmask no coast parts % complete ]\n") ;

        //--
        for ( int np = 0 ; np < vLandmaskNoCoasts.size() ; ++np ) {
            for ( int nr = 0 ; nr < vLandmaskNoCoasts[np].size() ; ++nr ) {
                for ( int i = 0 ; i < vLandmaskNoCoasts[np][nr].size() ; ++i, ++display ) {
                    ign::feature::Feature fNoCoast = _fsNoCoast->newFeature();
                    fNoCoast.setGeometry(detail::getSubString(vLandmaskNoCoasts[np][nr][i], mpLandmask.polygonN(np).ringN(nr)));
                    fNoCoast.setAttribute(countryCodeName,ign::data::String(_countryCode));
                    _fsNoCoast->createFeature(fNoCoast);
                }
            }
        }
    };
}
}

