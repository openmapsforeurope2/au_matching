#ifndef _APP_CALCUL_INITLANDMASKNOCOASTOP_H_
#define _APP_CALCUL_INITLANDMASKNOCOASTOP_H_

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
	class InitLandmaskNoCoastOp {

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
		ign::feature::sql::FeatureStorePostgis*            _fsNoCoast;
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsLandmask;
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
		InitLandmaskNoCoastOp( std::string countryCode, bool verbose );

		//--
		~InitLandmaskNoCoastOp();

		//--
		void _init();

		//--
		void _compute();
    };
}
}

#endif