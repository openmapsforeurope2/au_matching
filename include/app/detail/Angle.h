#ifndef _APP_DETAIL_ANGLE_H_
#define _APP_DETAIL_ANGLE_H_


namespace app{
namespace detail{

	class Angle{
	public:

        enum Type {
            SHARP,
            RIGTH,
            OBTUSE
        };

        /// \brief
        static bool equals(double angle1, double angle2, double precision = 0.087266463 /*5 degres*/)
        {
            return abs(normalize(angle1)-normalize(angle2)) < precision;

        };

        /// \brief
        static double normalize(double angle){
            return angle > M_PI ? M_PI*2 - angle : angle;
        }

        /// \brief
        static double getSharpAngleBetween2lines(double angle){
            return angle > M_PI_2 ? M_PI - angle : angle;
        }
        
    };
}
}

#endif