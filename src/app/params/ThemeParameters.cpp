
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
		_initParameter( DB_CONF_FILE, "DB_CONF_FILE" );
		_initParameter( BOUNDARY_TYPE_INLAND_WATER_BOUNDARY, "BOUNDARY_TYPE_INLAND_WATER_BOUNDARY" );
		_initParameter( AREA_TABLE_INIT, "AREA_TABLE_INIT" );
		_initParameter( LANDMASK_TABLE, "LANDMASK_TABLE" );
		_initParameter( LAND_COVER_TYPE, "LAND_COVER_TYPE" );
		_initParameter( TYPE_LAND_AREA, "TYPE_LAND_AREA" );
		_initParameter( COUNTRY_CODE_W, "COUNTRY_CODE_W" );

		_initParameter( COAST_TABLE, "COAST_TABLE" );
		_initParameter( NOCOAST_TABLE, "NOCOAST_TABLE" );
		_initParameter( COAST_TABLE_SUFFIX, "COAST_TABLE_SUFFIX" );
		_initParameter( NOCOAST_TABLE_SUFFIX, "NOCOAST_TABLE_SUFFIX" );

		_initParameter( AU_BOUNDARY_MAX_DIST, "AU_BOUNDARY_MAX_DIST" );
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