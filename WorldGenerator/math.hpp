// --
//  math.hpp
//  WorldGenerator
//


#ifndef math_hpp
#define math_hpp

#include <cstddef>
#include <utility>
#include "Defines.h"

namespace WorldBuilder {
    struct Vec3{
        wb_float coords[3];
        
        wb_float operator[](std::size_t idx) const { return coords[idx];}
        Vec3 operator*(float scaler) const {
            Vec3 result;
            result.coords[0] = this->coords[0]*scaler;
            result.coords[1] = this->coords[1]*scaler;
            result.coords[2] = this->coords[2]*scaler;
            return result;
        }
        Vec3 operator+(Vec3 addition) const {
            Vec3 result;
            result.coords[0] = this->coords[0] + addition.coords[0];
            result.coords[1] = this->coords[1] + addition.coords[1];
            result.coords[2] = this->coords[2] + addition.coords[2];
            return result;
        }
        Vec3 operator-(Vec3 subtract) const {
            Vec3 result;
            result.coords[0] = this->coords[0] - subtract.coords[0];
            result.coords[1] = this->coords[1] - subtract.coords[1];
            result.coords[2] = this->coords[2] - subtract.coords[2];
            return result;
        }
        wb_float dot(Vec3 otherVector) const {
            return (this->coords[0]*otherVector[0] + this->coords[1]*otherVector[1] + this->coords[2]*otherVector[2]);
        }
        
    };
    
    struct Matrix3x3{
        Vec3 rows[3];
        Vec3 operator[](size_t idx) const {
            return rows[idx];
        };
        Vec3 column(size_t idx) const {
            Vec3 retVal;
            retVal.coords[0] = rows[0][idx];
            retVal.coords[1] = rows[1][idx];
            retVal.coords[2] = rows[2][idx];
            return retVal;
        };
    };
    
    namespace math {
        wb_float distanceFromPole(const Vec3 position, const Vec3 pole); // assumes normalized vectors
        wb_float distanceBetween3Points(const Vec3 point1, const Vec3 point2);
        wb_float squareDistanceBetween3Points(const Vec3 point1, const Vec3 point2);
        Vec3 normalize3Vector(const Vec3 vector);
        std::pair<Vec3, wb_float> normalize3VectorWithScale(const Vec3 vector);
        
        wb_float angleBetweenUnitVectors(const Vec3 vector1, const Vec3 vector2);
        
        Matrix3x3 rotationMatrixAboutAxis(const Vec3 axis, const wb_float angleRadians);
        Matrix3x3 identityMatrix();
        Matrix3x3 matrixMul(Matrix3x3 a, Matrix3x3 b);
        Matrix3x3 transpose(Matrix3x3 a);
        
        Vec3 affineRotaionMulVec(const Matrix3x3 rotationTransform, const Vec3 vector);
        
        wb_float circleIntersectionArea(wb_float distance, wb_float radius);    }
}

#endif /* math_hpp */
