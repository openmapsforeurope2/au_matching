#ifndef _APP_DETAIL_GETSUBSTRING_H_
#define _APP_DETAIL_GETSUBSTRING_H_

// SOCLE
#include <ign/geometry/LineString.h>

namespace app{
namespace detail{
    //--
    ign::geometry::LineString getSubString(
        std::pair<int, int> pStartEnd, 
        const ign::geometry::LineString & lsRef
    );
}
}

#endif