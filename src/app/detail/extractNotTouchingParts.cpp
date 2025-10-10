// APP
#include <app/detail/extractNotTouchingParts.h>


namespace app{
namespace detail{

    ///
	///
	///
    void extractNotTouchingParts(
        const tools::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::LineString & ls, 
        std::vector<std::pair<int,int>> & vNotTouchingParts,
        std::vector<int>* vTouchingPoints
    ) {
        bool isRing = ls.isClosed();
        int nbSegments = ls.numSegments();
        int nbPoints = ls.numPoints();

        std::vector < bool > vIsTouchingPoints(nbPoints, false);
        std::vector < std::set<int> > vGroup(nbPoints, std::set<int>());
        for ( int k = 0 ; k < nbPoints ; ++k ) {
            //DEBUG
            // ign::geometry::Point const& pDebug = ls.pointN(k);
            // if( pDebug.distance(ign::geometry::Point(3368958.44,2323138.63)) < 0.2 ) {
            //     bool test =true;
            // };
            // if( pDebug.distance(ign::geometry::Point(3368963.45,2323132.65)) < 0.2 ) {
            //     bool test =true;
            // };
            // if( pDebug.distance(ign::geometry::Point(3368758.91,2323094.37)) < 0.2 ) {
            //     bool test =true;
            // };
            // if( pDebug.distance(ign::geometry::Point(3368739.94,2323105.48)) < 0.2 ) {
            //     bool test =true;
            // };
            
            std::pair<double, std::set<int>> distGroup = refGeom->distance( ls.pointN(k), 0.1 );
            if ( distGroup.first < 0 ) {
                continue;
            }
            vIsTouchingPoints[k] = true;
            vGroup[k] = distGroup.second;
        }

        int notTouchingFirstSegment = -1;
        bool bNothingIsTouching = true;
        bool previousSegmentIsTouching = !isRing ? false : vIsTouchingPoints[nbSegments-1] && vIsTouchingPoints[nbSegments] && commonGroupExists(vGroup[nbSegments-1], vGroup[nbSegments]) ;
        std::vector < bool > vTouchingSegments(nbSegments, false);
        for ( int currentSegment = 0 ; currentSegment < nbSegments ; ++currentSegment )
        {
            bool currentSegmentIsTouching = vIsTouchingPoints[currentSegment] && vIsTouchingPoints[currentSegment+1] && commonGroupExists(vGroup[currentSegment], vGroup[currentSegment+1]);

            if (currentSegmentIsTouching)
            {
                vTouchingSegments[currentSegment] = true;
                bNothingIsTouching = false;
            } else if ( !previousSegmentIsTouching ) {
                if ( vIsTouchingPoints[currentSegment] ) {
                    if (vTouchingPoints != 0) vTouchingPoints->push_back(currentSegment);
                }
            } else if (notTouchingFirstSegment < 0) {
                notTouchingFirstSegment = currentSegment;
            }
            previousSegmentIsTouching = vTouchingSegments[currentSegment];
        }

        if (bNothingIsTouching) {
            vNotTouchingParts.push_back(std::make_pair(0, nbSegments));
            return;
        }

        if (notTouchingFirstSegment >= 0) {
            int currentSegment = !isRing ? 0 : notTouchingFirstSegment;
            notTouchingFirstSegment = -1;
            int end = currentSegment;
            do
            {
                int next = (currentSegment == nbSegments-1) ? 0 : currentSegment+1;

                if ( vTouchingSegments[currentSegment] || (!isRing && next == 0) ) {
                    if ( notTouchingFirstSegment >= 0 )
                    {
                        vNotTouchingParts.push_back(std::make_pair(notTouchingFirstSegment, currentSegment));
                        notTouchingFirstSegment = -1;
                    }
                } else {
                    if ( notTouchingFirstSegment < 0 )
                    {
                        notTouchingFirstSegment = currentSegment;
                    }
                }
                currentSegment = next;
            } while ( currentSegment != end );
        }
    };

    ///
	///
	///
    void extractNotTouchingParts(
        const tools::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::Polygon & p, 
        std::vector<std::vector<std::pair<int,int>>> & vNotTouchingParts,
        std::vector<std::vector<int>>* vTouchingPoints
    ) {
        for (int i = 0 ; i < p.numRings() ; ++i)
        {
            vNotTouchingParts.push_back(std::vector<std::pair<int,int>>());
            std::vector<int>* vTouchingPoints_ = 0;
            if (vTouchingPoints)
            {
                vTouchingPoints->push_back(std::vector<int>());
                vTouchingPoints_ =  &vTouchingPoints->back();
            }
            extractNotTouchingParts(refGeom, p.ringN(i), vNotTouchingParts.back(), vTouchingPoints_);
        }
    };

    ///
	///
	///
    void extractNotTouchingParts(
        const tools::SegmentIndexedGeometryInterface* refGeom,
        const ign::geometry::MultiPolygon & mp, 
        std::vector<std::vector<std::vector<std::pair<int,int>>>> & vNotTouchingParts,
        std::vector<std::vector<std::vector<int>>>* vTouchingPoints
    ) {
        for (int i = 0 ; i < mp.numGeometries() ; ++i)
        {
            vNotTouchingParts.push_back(std::vector<std::vector<std::pair<int,int>>>());
            std::vector<std::vector<int>>* vTouchingPoints_ = 0;
            if (vTouchingPoints)
            {
                vTouchingPoints->push_back(std::vector<std::vector<int>>());
                vTouchingPoints_ =  &vTouchingPoints->back();
            }
            extractNotTouchingParts(refGeom, mp.polygonN(i), vNotTouchingParts.back(), vTouchingPoints_);
        }
    };

    ///
    ///
    ///
    bool commonGroupExists( std::set<int> const& sGroup1, std::set<int> const& sGroup2 ) 
    {
        if (sGroup1.empty() && sGroup2.empty()) return true;

        for ( std::set<int>::const_iterator sit1 = sGroup1.begin() ; sit1 != sGroup1.end() ; ++sit1 ) {
            if( sGroup2.find(*sit1) != sGroup2.end() ) return true;
        }
        return false;
    }

}
}