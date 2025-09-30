#ifndef _APP_CALCUL_INITLANDMASKCOASTOP_H_
#define _APP_CALCUL_INITLANDMASKCOASTOP_H_

//EPG
#include <epg/log/EpgLogger.h>
#include <epg/log/ShapeLogger.h>
#include <epg/tools/MultiLineStringTool.h>
#include <ome2/feature/sql/NotDestroyedTools.h>

//APP
#include <app/tools/SegmentIndexedGeometry.h>

namespace app{
namespace calcul{

	/// @brief 
	class InitLandmaskCoastOp {

	public:

		/// @brief 
		/// @param countryCode Code pays simple
		/// @param verbose Mode verbeux
		static void Compute(
			std::string countryCode, 
			bool verbose
		);

	private:
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsCoast;
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsBoundary;
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsLandmask;
		//--
		epg::tools::MultiLineStringTool*                   _mlsToolLandmask;
		//--
		epg::log::EpgLogger*                               _logger;
		//--
		epg::log::ShapeLogger*                             _shapeLogger;
		//--
		std::string                                        _countryCode;
		//--
		bool                                               _verbose;

	private:

		//--
		InitLandmaskCoastOp( std::string countryCode, bool verbose );

		//--
		~InitLandmaskCoastOp();

		//--
		void _init();

		//--
		void _compute() const;
    };
}
}

#endif