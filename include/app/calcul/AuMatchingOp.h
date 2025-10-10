#ifndef _APP_CALCUL_AUMATCHINGOP_H_
#define _APP_CALCUL_AUMATCHINGOP_H_

//EPG
#include <epg/log/EpgLogger.h>
#include <epg/log/ShapeLogger.h>
#include <epg/tools/MultiLineStringTool.h>
#include <ome2/feature/sql/NotDestroyedTools.h>

//APP
#include <app/tools/SegmentIndexedGeometry.h>

namespace app{
namespace calcul{

	/// @brief Classe consacrée à la mise en cohérence des surfaces
	/// administratives avec les frontières.
	class AuMatchingOp {

	public:

		/// @brief Lance la reconstruction des surfaces administratives frontalières.
		/// Ce travail de mise en cohérence doit être réalisé sur la table des unités 
		/// administratives de plus faible échelon (qui diffère selon le pays). Les échelons
		/// supérieurs seront dérivés par fusion des échelons inférieurs.
		/// @param countryCode Code pays simple
		/// @param verbose Mode verbeux
		static void Compute(
			std::string countryCode, 
			bool verbose
		);

	private:
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsNoCoast;
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsArea;
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsBoundary;
		//--
		ign::feature::sql::FeatureStorePostgis*            _fsLandmask;
		//--
		epg::tools::MultiLineStringTool*                   _mlsToolBoundary;
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
		void _compute();

		//--
		void _getAngles( 
			tools::SegmentIndexedGeometryCollection* indexedGeom, 
			const std::vector<ign::geometry::LineString> & vLs,
			std::vector<std::pair<double, double>> & vGeomFeatures
		) const;

		//--
		double _getAngle(
			tools::SegmentIndexedGeometryCollection* indexedGeom, 
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
			double boundSearchDistance,
        	double vertexSearchDistance
		) const;

		//--
		void _findAngles( 
			epg::tools::MultiLineStringTool* mlsTool, 
			std::vector<ign::geometry::LineString> & vLs,
			const std::vector<std::pair<double, double>> & vGeomFeatures,
			double searchDistance,
			double vertexSnapDist
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
		std::vector<std::pair<int,int>> _getTouchingParts(
			std::vector<std::pair<int,int>> const& vpNotTouchingParts, 
			size_t nbPoints, 
			bool isClosed
		) const;

	};
}
}

#endif
