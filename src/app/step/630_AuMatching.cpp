#include <app/step/630_AuMatching.h>

// EPG
#include <ome2/utils/CopyTableUtils.h>

// APP
#include <app/calcul/AuMatchingOp.h>

namespace app {
	namespace step {

		///
		///
		///
		void AuMatching::init()
		{
			addWorkingEntity(AREA_TABLE_INIT);
		}

		///
		///
		///
		void AuMatching::onCompute(bool verbose = false)
		{
			// copie 
			_epgParams.setParameter(AREA_TABLE, ign::data::String(getCurrentWorkingTableName(AREA_TABLE_INIT)));
			ome2::utils::CopyTableUtils::copyAreaTable(getLastWorkingTableName(AREA_TABLE_INIT), "", false, true);

			// traitement
			app::params::ThemeParameters* themeParameters = app::params::ThemeParametersS::getInstance();
			std::string countryCodeW = themeParameters->getParameter(COUNTRY_CODE_W).getValue().toString();

			app::calcul::AuMatchingOp::Compute(countryCodeW, verbose);

		}

	}
}