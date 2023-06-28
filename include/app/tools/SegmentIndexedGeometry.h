#ifndef _APP_TOOLS_SEGMENTINDEXEDGEOMETRYCOLLECTION_H_
#define _APP_TOOLS_SEGMENTINDEXEDGEOMETRYCOLLECTION_H_


//SOCLE
#include <ign/geometry/Geometry.h>
#include <ign/geometry/index/QuadTree.h>
#include <ign/geometry.h>



namespace app{
namespace tools{
	class SegmentIndexedGeometryInterface{
	public:
		virtual std::pair<double, std::set<int>> distance( ign::geometry::Geometry const& geom, double threshold )const =0;
		virtual void getSegments( ign::geometry::Envelope const& bbox, std::vector<ign::geometry::LineString> & vLs )const =0;
	};

	

	class SegmentIndexedGeometry : public SegmentIndexedGeometryInterface
	{
	public:

		/// \brief
		SegmentIndexedGeometry( const ign::geometry::Geometry* geometry ):
			_geom( geometry )
		{ 
			switch( _geom->getGeometryType() )
			{
			case ign::geometry::Geometry::GeometryTypeLineString :
				{
					_loadQuadTree( _geom->asLineString() );
					break;
				}
			case ign::geometry::Geometry::GeometryTypeMultiLineString :
				{
					_loadQuadTree( _geom->asMultiLineString() );
					break;
				}
			case ign::geometry::Geometry::GeometryTypePolygon :
				{
					_loadQuadTree( _geom->asPolygon() );
					break;
				}
			case ign::geometry::Geometry::GeometryTypeMultiPolygon :
				{
					_loadQuadTree( _geom->asMultiPolygon() );
					break;
				}
			default :
				IGN_THROW_EXCEPTION( "[ app::tools::IndexedGeometry ] Geometry type '"+ign::geometry::Geometry::GeometryTypeName(_geom->getGeometryType())+"' not allowed." );
			};
		}

		/// \brief
		~SegmentIndexedGeometry(){
		}

		/// \brief
		virtual std::pair<double, std::set<int>> distance( ign::geometry::Geometry const& geom, double threshold )const 
		{
			std::set< std::pair< const ign::geometry::Point*, const ign::geometry::Point* > > sSegments;
			_qTree.query( geom.getEnvelope().expandBy( threshold ), sSegments );

			double minDist = threshold;
			bool found = false;
			std::set< std::pair< const ign::geometry::Point*, const ign::geometry::Point* > >::const_iterator sit;
			for( sit = sSegments.begin() ; sit != sSegments.end() ; ++sit )
			{
				double distance = ign::geometry::LineString( *sit->first, *sit->second ).distance( geom );
				if( distance <= minDist )
				{
					minDist = distance;
					found = true;
				}
			}
			if( !found ) return std::make_pair(-1, std::set<int>());
			return std::make_pair(minDist, std::set<int>());
		}

		/// \brief
		virtual void getSegments( ign::geometry::Envelope const& bbox, std::vector<ign::geometry::LineString> & vLs )const 
		{
			std::set< std::pair< const ign::geometry::Point*, const ign::geometry::Point* > > sSegments;
			_qTree.query( bbox, sSegments );
			std::set< std::pair< const ign::geometry::Point*, const ign::geometry::Point* > >::const_iterator sit;
			for( sit = sSegments.begin() ; sit != sSegments.end() ; ++sit )
			{
				vLs.push_back(ign::geometry::LineString( *sit->first, *sit->second ));
			}
		}

	private :


		ign::geometry::index::QuadTree< std::pair< const ign::geometry::Point*, const ign::geometry::Point* > >  _qTree ;
		const ign::geometry::Geometry*                                                                           _geom ;


	private:

		//--
		void _loadQuadTree( ign::geometry::LineString const& ls )
		{
			for( size_t i = 0 ; i < ls.numSegments() ; ++i )
				_qTree.insert( std::make_pair( &ls.pointN(i), &ls.pointN(i+1) ), ign::geometry::Envelope( ls.pointN(i), ls.pointN(i+1) ) );
		}
		//--
		void _loadQuadTree( ign::geometry::MultiLineString const& mls )
		{
			for( size_t i = 0 ; i < mls.numGeometries() ; ++i )
				_loadQuadTree( mls.lineStringN(i) );
		}
		//--
		void _loadQuadTree( ign::geometry::Polygon const& poly )
		{
			for( size_t i = 0 ; i < poly.numRings() ; ++i )
				_loadQuadTree( poly.ringN(i) );
		}
		//--
		void _loadQuadTree( ign::geometry::MultiPolygon const& mp )
		{
			for( size_t i = 0 ; i < mp.numGeometries() ; ++i )
				_loadQuadTree( mp.polygonN(i) );
		}


	} ;

	class SegmentIndexedGeometryCollection : public SegmentIndexedGeometryInterface
	{
		public:
			/// \brief
			SegmentIndexedGeometryCollection(){}

			/// \brief
			~SegmentIndexedGeometryCollection()
			{
				for( size_t i = 0 ; i < _vIndexedGeoms.size() ; ++i )
					delete _vIndexedGeoms[i];
			}

			/// \brief
			void addGeometry( const ign::geometry::Geometry* geometry, int group = -1 )
			{
				_vIndexedGeoms.push_back( new SegmentIndexedGeometry( geometry ) );
				_vGroups.push_back( group );
			}

			/// \brief
			virtual std::pair<double, std::set<int>> distance( ign::geometry::Geometry const& geom, double threshold )const
			{
				double minDistance = std::numeric_limits<double>::infinity();
				std::set<int> minGroup;
				bool found = false;
				for( size_t i = 0 ; i < _vIndexedGeoms.size() ; ++i )
				{
					double distance = _vIndexedGeoms[i]->distance( geom, threshold ).first;
					if( distance < 0 ) continue;
					if( distance == minDistance ){
						minGroup.insert(_vGroups[i]);
					}
					if( distance < minDistance )
					{
						minDistance = distance;
						minGroup = std::set<int>({_vGroups[i]});
						found = true;
					}
				}
				if( !found ) return std::make_pair(-1, std::set<int>());
				return std::make_pair(minDistance, minGroup);
			}

			/// \brief
			virtual void getSegments( ign::geometry::Envelope const& bbox, std::vector<ign::geometry::LineString> & vLs )const 
			{
				for( size_t i = 0 ; i < _vIndexedGeoms.size() ; ++i ) {
					_vIndexedGeoms[i]->getSegments(bbox, vLs);
				}
			}

		private:

			std::vector< SegmentIndexedGeometry* >   _vIndexedGeoms;
			std::vector< int >                       _vGroups;
	};

}
}


#endif
