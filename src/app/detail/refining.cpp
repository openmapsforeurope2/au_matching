// APP
#include <app/detail/refining.h>

//SOCLE
#include <ign/math/Line2T.h>

namespace app{
namespace detail{

    ///
	///
	///
    void refineAreaWithLsEndings(
        ign::geometry::MultiLineString const& mls,
        ign::geometry::MultiPolygon & mp,
        double precision
    ) {
        for ( size_t i = 0 ; i < mls.numGeometries() ; ++i ) {
            ign::geometry::LineString const& ls = mls.lineStringN(i);

            // TODO : confirmer qu'on peut ne pas traiter les boucles
            if( ls.isClosed() )
                continue;

            std::vector<ign::geometry::Point> vEndings;
            vEndings.push_back(ls.startPoint());
            vEndings.push_back(ls.endPoint());
            
            for ( size_t ne = 0 ; ne < vEndings.size() ; ++ne ) {
                bool foundTouchingRing = false;
                for( size_t np = 0 ; np < mp.numGeometries() ; ++np ) {
                    for( size_t nr = 0 ; nr < mp.polygonN(np).numRings() ; ++nr ) {
                        if( mp.polygonN(np).ringN(nr).distance(vEndings[ne]) < precision) {
                            refine(mp.polygonN(np).ringN(nr), vEndings[ne], precision);
                            foundTouchingRing = true;
                            break;
                        }
                    }
                    if( foundTouchingRing )
                        break;
                }
            }
        }
    };

    ///
	///
	///
    void refine(
        ign::geometry::LineString & ls,
        ign::geometry::Point const& point,
        double precision
    ) {
        ign::math::Vec2d refPoint = point.toVec2d();
        double precision2 = precision*precision;
        for ( size_t ns = 0 ; ns < ls.numSegments() ; ++ns ) {
            ign::math::Line2d currentLine(ls.pointN(ns).toVec2d(), ls.pointN(ns+1).toVec2d());
            if( currentLine.distance2(refPoint, true) < precision2 ) {
                if( !refPoint.distance2(ls.pointN(ns).toVec2d()) < precision2 && !refPoint.distance2(ls.pointN(ns+1).toVec2d()) < precision2 ) {
                    ls.addPoint(point, ns+1);
                }
                return;
            }
        }
    };

}
}