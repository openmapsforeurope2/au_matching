
// APP
#include <app/detail/getSubString.h>


namespace app{
namespace detail{
    ///
	///
	///
    ign::geometry::LineString getSubString(
        std::pair<int, int> pStartEnd, 
        const ign::geometry::LineString & lsRef
    ) {
        if (pStartEnd.first == 0 && pStartEnd.second == lsRef.numPoints()-1) return lsRef;

        ign::geometry::LineString subLs;

        bool isRing = lsRef.isClosed();
        int nbPoints = isRing ? lsRef.numPoints()-1 : lsRef.numPoints();

        int current = pStartEnd.first;
        int end = (pStartEnd.second == nbPoints-1) ? 0 : pStartEnd.second+1;
        do
        {
            int next = (current == nbPoints-1) ? 0 : current+1;

            subLs.addPoint(lsRef.pointN(current));

            current = next;
        } while ( current != end );

        return subLs;
    };
}
}