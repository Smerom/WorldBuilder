// --
//  math.hpp
//  WorldGenerator
//
//  Light weight vector math and algorithms specific to the Grid data structure


#ifndef math_hpp
#define math_hpp

#include <cstddef>
#include <utility>
#include <cmath>
#include <tuple>
#include "Defines.h"

namespace WorldBuilder {
    struct Vec3{
        wb_float coords[3];
        
        Vec3(){
            coords[0] = 0;
            coords[1] = 0;
            coords[2] = 0;
        }
        
        wb_float operator[](std::size_t idx) const { return coords[idx];}
        Vec3 operator*(wb_float scaler) const {
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
        
        Vec3 cross(Vec3 otherVector) const {
            Vec3 result;
            result.coords[0] = this->coords[1]*otherVector.coords[2] - this->coords[2]*otherVector.coords[1];
            result.coords[1] = this->coords[2]*otherVector.coords[0] - this->coords[0]*otherVector.coords[2];
            result.coords[2] = this->coords[0]*otherVector.coords[1] - this->coords[1]*otherVector.coords[2];
            return result;
        }
        
        wb_float length() const {
            return std::sqrt(this->coords[0]*this->coords[0] + this->coords[1]*this->coords[1] + this->coords[2]*this->coords[2]);
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
        
        wb_float determinant(){
            return rows[0][0]*rows[1][1]*rows[2][2] + rows[0][1]*rows[1][2]*rows[2][0] + rows[0][2]*rows[1][0]*rows[2][1] - rows[0][2]*rows[1][1]*rows[2][0] - rows[0][1]*rows[1][0]*rows[2][2] - rows[0][0]*rows[1][2]*rows[2][1];
        };
    };
    
    namespace math {
        const wb_float piOverTwo = 1.5707963267948966192313216916397514420985847;

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
        
        wb_float circleIntersectionArea(wb_float distance, wb_float radius);
        
        // returns the intercept point, numver of |v| between a and intercept, and whether or not the intercept lies between p and q (otherwise its on on side or the other)
        std::tuple<Vec3, wb_float, bool> edgeIntersection(Vec3 p, Vec3 q, Vec3 a, Vec3 v);
        
        std::tuple<Vec3, wb_float, bool, uint8_t> triangleIntersection(Vec3 p, Vec3 q, Vec3 r, Vec3 a, Vec3 v, bool checkPQ);
    }
}

#endif /* math_hpp */
