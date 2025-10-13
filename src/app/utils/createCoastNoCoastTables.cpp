// APP
#include <app/utils/createCoastNoCoastTables.h>
#include <app/params/ThemeParameters.h>

// EPG
#include <epg/Context.h>


namespace app{
namespace utils{
    //--
    void createCoastTable() {
        epg::Context* context = epg::ContextS::getInstance();
        app::params::ThemeParameters* themeParameters = app::params::ThemeParametersS::getInstance();

        std::string const idName = context->getEpgParameters().getValue(ID).toString();
	    std::string const geomName = context->getEpgParameters().getValue(GEOM).toString();
        std::string const countryCodeName = context->getEpgParameters().getValue(COUNTRY_CODE).toString();

        std::string const coastTableName = themeParameters->getValue(COAST_TABLE).toString();
        {
            std::ostringstream ss;
            ss << "DROP TABLE IF EXISTS " << coastTableName << " ;";

            ss << "CREATE TABLE " << coastTableName
                << " (" 
                << idName << " uuid NOT NULL DEFAULT gen_random_uuid(), "
                << countryCodeName << " varchar(255), "
                << geomName << " geometry(LineString,3035)"
                << ");"
                << " CREATE INDEX IF NOT EXISTS " + coastTableName+"_"+countryCodeName+"_idx ON " + coastTableName
                << " USING btree ("+countryCodeName+");";

            context->getDataBaseManager().getConnection()->update(ss.str());
        }
    }

    //--
    void createNoCoastTable() {
        epg::Context* context = epg::ContextS::getInstance();
        app::params::ThemeParameters* themeParameters = app::params::ThemeParametersS::getInstance();

        std::string const idName = context->getEpgParameters().getValue(ID).toString();
	    std::string const geomName = context->getEpgParameters().getValue(GEOM).toString();
        std::string const countryCodeName = context->getEpgParameters().getValue(COUNTRY_CODE).toString();

        std::string const nocoastTableName = themeParameters->getValue(NOCOAST_TABLE).toString();
        {
            std::ostringstream ss;
            ss << "DROP TABLE IF EXISTS " << nocoastTableName << " ;";

            ss << "CREATE TABLE " << nocoastTableName
                << " (" 
                << idName << " uuid NOT NULL DEFAULT gen_random_uuid(), "
                << countryCodeName << " varchar(255), "
                << geomName << " geometry(LineString,3035)"
                << ");"
                << " CREATE INDEX IF NOT EXISTS " + nocoastTableName+"_"+countryCodeName+"_idx ON " + nocoastTableName
                << " USING btree ("+countryCodeName+");";

            context->getDataBaseManager().getConnection()->update(ss.str());
        }
    }
}
}