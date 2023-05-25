#ifndef _APP_CALCUL_MATCHING_AUMATCHINGOP_H_
#define _APP_CALCUL_MATCHING_AUMATCHINGOP_H_

//EPG
#include <epg/log/EpgLogger.h>
#include <epg/log/ShapeLogger.h>
#include <epg/tools/geometry/SegmentIndexedGeometry.h>
#include <epg/tools/MultiLineStringTool.h>


namespace app{
namespace calcul{
namespace matching{

	class AuMatchingOp {

	public:

		/// \brief
		static void compute(
			std::string workingTable, 
			std::string countryCode, 
			bool verbose
		);

	private:
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsBoundary;
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsLandmask;
		//--
		epg::tools::MultiLineStringTool*                   _mlsToolBoundary;
		//--
		epg::tools::MultiLineStringTool*                   _mlsToolLandmask;
		//--
		epg::log::EpgLogger*                               _logger;
		//--
		epg::log::ShapeLogger*                             _shapeLogger;
		//--
		std::string                                        _countryCode;
		//--
		bool                                               _verbose;

	private:

		//--
		AuMatchingOp( std::string countryCode, bool verbose );

		//--
		~AuMatchingOp();

		//--
		void _init();

		//--
		void _compute(std::string workingTable);

		//--
		void _getAngles( 
			epg::tools::geometry::SegmentIndexedGeometryCollection* indexedGeom, 
			const std::vector<ign::geometry::LineString> & vLs,
			std::vector<std::pair<double, double>> & vGeomFeatures
		) const;

		//--
		double _getAngle(
			epg::tools::geometry::SegmentIndexedGeometryCollection* indexedGeom, 
			const ign::geometry::Point & pt
		) const;

		//--
		int _findIndex( 
			const ign::geometry::LineString & ls, 
			size_t startIndex,
			const ign::geometry::Point & refPoint,
			double angle,
			double searchDistance
		) const ;

		//--
		double _getScore(
			double refAngle, 
			double angle, 
			double distance
		) const;

		//--
		std::pair<int, double> _findIndex( 
			const ign::geometry::LineString & ls, 
			size_t startIndex,
			const ign::geometry::Point & refPoint,
			double refAngle,
			double searchDistance, 
			bool positiveDirection
		) const;

		//--
		std::pair<bool, ign::geometry::Point> _findCandidate( 
			epg::tools::MultiLineStringTool* mlsTool, 
			const ign::geometry::Point & pt,
			double angle,
			double searchDistance
		) const;

		//--
		void _findAngles( 
			epg::tools::MultiLineStringTool* mlsTool, 
			std::vector<ign::geometry::LineString> & vLs,
			const std::vector<std::pair<double, double>> & vGeomFeatures,
			double searchDistance
		) const;

		//--
		void _projectTouchingPoints(
			epg::tools::MultiLineStringTool* mlsTool, 
			ign::geometry::LineString & ls, 
			const std::vector<int> & vTouchingPoints,
			double searchDistance,
			double snapDistOnVertex
		) const;

		//--
		ign::geometry::LineString _getSubString(
			std::pair<int, int> pStartEnd, 
			const ign::geometry::LineString & lsRef
		) const;

		//--
		void _extractNotTouchingParts(
			const epg::tools::geometry::SegmentIndexedGeometryInterface* refGeom,
			const ign::geometry::LineString & ls, 
			std::vector<std::pair<int,int>> & vNotTouchingParts,
			std::vector<int>* vTouchingPoints = 0
		) const;

		//--
		void _extractNotTouchingParts(
			const epg::tools::geometry::SegmentIndexedGeometryInterface* refGeom,
			const ign::geometry::Polygon & p, 
			std::vector<std::vector<std::pair<int,int>>> & vNotTouchingParts,
			std::vector<std::vector<int>>* vTouchingPoints = 0
		) const;

		//--
		void _extractNotTouchingParts(
			const epg::tools::geometry::SegmentIndexedGeometryInterface* refGeom,
			const ign::geometry::MultiPolygon & mp, 
			std::vector<std::vector<std::vector<std::pair<int,int>>>> & vNotTouchingParts,
			std::vector<std::vector<std::vector<int>>>* vTouchingPoints = 0
		) const;
	};
}
}
}

#endif
