#ifndef _APP_DETAIL_EXTRACTNOTTOUCHINGPARTS_H_
#define _APP_DETAIL_EXTRACTNOTTOUCHINGPARTS_H_

// APP
#include <app/tools/SegmentIndexedGeometry.h>


namespace app{
namespace detail{

	//--
    void extractNotTouchingParts(
        const tools::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::LineString & ls, 
        std::vector<std::pair<int,int>> & vNotTouchingParts,
        std::vector<int>* vTouchingPoints = 0
    );

    //--
    void extractNotTouchingParts(
        const tools::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::Polygon & p, 
        std::vector<std::vector<std::pair<int,int>>> & vNotTouchingParts,
        std::vector<std::vector<int>>* vTouchingPoints = 0
    );

    //--
    void extractNotTouchingParts(
        const tools::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::MultiPolygon & mp, 
        std::vector<std::vector<std::vector<std::pair<int,int>>>> & vNotTouchingParts,
        std::vector<std::vector<std::vector<int>>>* vTouchingPoints = 0
    );

    //--
    bool commonGroupExists( 
        std::set<int> const& sGroup1,
        std::set<int> const& sGroup2
    );
}
}

#endif