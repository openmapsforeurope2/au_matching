#ifndef _APP_PARAMS_THEMEPARAMETERS_H_
#define _APP_PARAMS_THEMEPARAMETERS_H_

//STL
#include <string>

//EPG
#include <epg/params/ParametersT.h>
#include <epg/SingletonT.h>



	enum AU_PARAMETERS{
		
		BOUNDARY_TYPE_INLAND_WATER_BOUNDARY,
		LANDMASK_TABLE,
		LAND_COVER_TYPE,
		TYPE_LAND_AREA,
		AU_BOUNDARY_MAX_DIST,
		AU_BOUNDARY_SEARCH_DIST,
		AU_BOUNDARY_SNAP_DIST,
		AU_COAST_MAX_DIST,
		AU_COAST_SEARCH_DIST,
		AU_COAST_SNAP_DIST,
		AU_SEGMENT_MIN_LENGTH
		
	};

namespace app{
namespace params{

	class ThemeParameters : public epg::params::ParametersT< AU_PARAMETERS >
	{
		typedef  epg::params::ParametersT< AU_PARAMETERS > Base;

		public:

			/// \brief
			ThemeParameters();

			/// \brief
			~ThemeParameters();

			/// \brief
			virtual std::string getClassName()const;

	};

	typedef epg::Singleton< ThemeParameters >   ThemeParametersS;

}
}

#endif