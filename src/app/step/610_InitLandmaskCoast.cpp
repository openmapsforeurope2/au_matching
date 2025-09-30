#include <app/step/610_InitLandmaskCoast.h>

// EPG
#include <ome2/utils/CopyTableUtils.h>

// APP
#include <app/calcul/InitLandmaskCoastOp.h>
#include <app/utils/createCoastNoCoastTables.h>

namespace app {
	namespace step {

		///
		///
		///
		void InitLandmaskCoast::init()
		{
		}

		///
		///
		///
		void InitLandmaskCoast::onCompute(bool verbose = false)
		{
			// traitement
			app::params::ThemeParameters* themeParameters = app::params::ThemeParametersS::getInstance();
			std::string countryCodeW = themeParameters->getParameter(COUNTRY_CODE_W).getValue().toString();

			//init table
			epg::params::EpgParameters const& epgParams = epg::ContextS::getInstance()->getEpgParameters();
			std::string areaTableName = epgParams.getParameter(AREA_TABLE).getValue().toString();
			utils::createCoastTable(areaTableName);

			app::calcul::InitLandmaskCoastOp::Compute(countryCodeW, verbose);

		}

	}
}