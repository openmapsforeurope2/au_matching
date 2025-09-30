#ifndef _APP_STEP_INITLANDMASKCOAST_H_
#define _APP_STEP_INITLANDMASKCOAST_H_

#include <epg/step/StepBase.h>
#include <app/params/ThemeParameters.h>

namespace app{
namespace step{

	class InitLandmaskCoast : public epg::step::StepBase< app::params::ThemeParametersS > {

	public:

		/// \brief
		int getCode() { return 610; };

		/// \brief
		std::string getName() { return "InitLandmaskCoast"; };

		/// \brief
		void onCompute( bool );

		/// \brief
		void init();

	};

}
}

#endif