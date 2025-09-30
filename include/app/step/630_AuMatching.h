#ifndef _APP_STEP_AUMATCHING_H_
#define _APP_STEP_AUMATCHING_H_

#include <epg/step/StepBase.h>
#include <app/params/ThemeParameters.h>

namespace app{
namespace step{

	class AuMatching : public epg::step::StepBase< app::params::ThemeParametersS > {

	public:

		/// \brief
		int getCode() { return 630; };

		/// \brief
		std::string getName() { return "AuMatching"; };

		/// \brief
		void onCompute( bool );

		/// \brief
		void init();

	};

}
}

#endif