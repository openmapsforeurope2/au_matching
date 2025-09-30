#ifndef _APP_STEP_TOOLS_INITSTEPS_H_
#define _APP_STEP_TOOLS_INITSTEPS_H_

//EPG
#include <epg/step/StepSuite.h>
#include <epg/step/factoryNew.h>

//APP
#include <app/step/610_InitLandmaskCoast.h>
#include <app/step/620_InitLandmaskNoCoast.h>
#include <app/step/630_AuMatching.h>


namespace app{
namespace step{
namespace tools{

	template<  typename StepSuiteType >
	void initSteps( StepSuiteType& stepSuite )
	{
		stepSuite.addStep( epg::step::factoryNew< InitLandmaskCoast >() );
		stepSuite.addStep( epg::step::factoryNew< InitLandmaskNoCoast >() );
		stepSuite.addStep( epg::step::factoryNew< AuMatching >() );
	}

}
}
}

#endif