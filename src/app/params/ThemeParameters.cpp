
//APP
#include <app/params/ThemeParameters.h>

//SOCLE
#include <ign/Exception.h>


namespace app{
namespace params{


	///
	///
	///
	ThemeParameters::ThemeParameters()
	{
		_initParameter( BOUNDARY_TYPE_INLAND_WATER_BOUNDARY, "BOUNDARY_TYPE_INLAND_WATER_BOUNDARY" );
		_initParameter( LANDMASK_TABLE, "LANDMASK_TABLE" );
		_initParameter( AU_BOUNDARY_LOOP_MAX_DIST, "AU_BOUNDARY_LOOP_MAX_DIST" );
		_initParameter( AU_BOUNDARY_LOOP_SEARCH_DIST, "AU_BOUNDARY_LOOP_SEARCH_DIST" );
		_initParameter( AU_BOUNDARY_LOOP_SNAP_DIST, "AU_BOUNDARY_LOOP_SNAP_DIST" );
		_initParameter( AU_BOUNDARY_SEARCH_DIST, "AU_BOUNDARY_SEARCH_DIST" );
		_initParameter( AU_BOUNDARY_SNAP_DIST, "AU_BOUNDARY_SNAP_DIST" );
		_initParameter( AU_COAST_MAX_DIST, "AU_COAST_MAX_DIST" );
		_initParameter( AU_COAST_SEARCH_DIST, "AU_COAST_SEARCH_DIST" );
		_initParameter( AU_COAST_SNAP_DIST, "AU_COAST_SNAP_DIST" );
		_initParameter( AU_SEGMENT_MIN_LENGTH, "AU_SEGMENT_MIN_LENGTH" );
	}

	///
	///
	///
	ThemeParameters::~ThemeParameters()
	{
	}

	///
	///
	///
	std::string ThemeParameters::getClassName()const
	{
		return "app::params::ThemeParameters";
	}



}
}