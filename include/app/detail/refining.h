#ifndef _APP_DETAIL_REFINING_H_
#define _APP_DETAIL_REFINING_H_

// APP
#include <app/tools/SegmentIndexedGeometry.h>


namespace app{
namespace detail{

	//--
    void refineAreaWithLsEndings(
        ign::geometry::MultiLineString const& mls,
        ign::geometry::MultiPolygon & mp,
        double precision = 0.1
    );

    //--
    void refine(
        ign::geometry::LineString & ls,
        ign::geometry::Point const& point,
        double precision
    );
}
}

#endif