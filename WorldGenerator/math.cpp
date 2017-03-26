// --
//  math.cpp
//  WorldGenerator
//


#include "math.hpp"
#include <cmath>
#include "Defines.h"
#include <iostream>

namespace WorldBuilder {
    namespace math {
        
        wb_float distanceFromPole(const Vec3 position, const Vec3 pole){
            wb_float x = position[0]*pole[0] + position[1]*pole[1] + position[2]*pole[2];
            wb_float xSquared = x*x;
            if (xSquared > 1 || isnan(xSquared)){
                //std::cout << "Distance from pole faliure" << std::endl;
                return 0;
            }
            // sin(arccos(x)) = sqrt(1-x^2)
            return sqrtf(1 - xSquared);
        }
        
        wb_float squareDistanceBetween3Points(const Vec3 point1, const Vec3 point2){
            wb_float x, y, z;
            x = point1[0] - point2[0];
            y = point1[1] - point2[1];
            z = point1[2] - point2[2];
            
            return x*x + y*y + z*z;
        };
        
        wb_float distanceBetween3Points(const Vec3 point1, const Vec3 point2){
            wb_float x, y, z;
            x = point1[0] - point2[0];
            y = point1[1] - point2[1];
            z = point1[2] - point2[2];
            
            return sqrtf(x*x + y*y + z*z);
        };
        
        Vec3 normalize3Vector(Vec3 vector){
            wb_float scale = sqrt(vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2]);
            if (scale * scale < float_epsilon){
                // TODO do something!
            }
            Vec3 result;
            result.coords[0] = vector[0] / scale;
            result.coords[1] = vector[1] / scale;
            result.coords[2] = vector[2] / scale;
            
            return result;
        }; // normalize3Vector
        std::pair<Vec3, wb_float> normalize3VectorWithScale(Vec3 vector){
            std::pair<Vec3, wb_float> result;
            wb_float scale = sqrt(vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2]);
            if (scale * scale < float_epsilon){
                result.second = 0;
                return result;
            }
            result.first.coords[0] = vector[0] / scale;
            result.first.coords[1] = vector[1] / scale;
            result.first.coords[2] = vector[2] / scale;
            
            result.second = scale;
            
            return result;
        }
        
        wb_float angleBetweenUnitVectors(const Vec3 vector1, const Vec3 vector2){
            return acos(vector1.dot(vector2));
        }
        
        
        Matrix3x3 identityMatrix(){
            Matrix3x3 matrix;
            matrix.rows[0].coords[0] = 1.0;
            matrix.rows[0].coords[1] = 0.0;
            matrix.rows[0].coords[2] = 0.0;
            matrix.rows[1].coords[0] = 0.0;
            matrix.rows[1].coords[1] = 1.0;
            matrix.rows[1].coords[2] = 0.0;
            matrix.rows[2].coords[0] = 0.0;
            matrix.rows[2].coords[1] = 0.0;
            matrix.rows[2].coords[2] = 1.0;
            return matrix;
        }
        
        Matrix3x3 rotationMatrixAboutAxis(Vec3 axis, wb_float angleRadians){
            Vec3 normalizedAxis = normalize3Vector(axis);
            
            // for clarity
            wb_float ux, uy, uz;
            ux = normalizedAxis.coords[0];
            uy = normalizedAxis.coords[1];
            uz = normalizedAxis.coords[2];
            
            // set up the matrix
            
            Matrix3x3 rotationMatrix;
            
            wb_float cosTheta, sinTheta;
            cosTheta = cos(angleRadians);
            sinTheta = sin(angleRadians);
            // set 0, 0
            rotationMatrix.rows[0].coords[0] = cosTheta + ux*ux*(1-cosTheta);
            // set 0, 1
            rotationMatrix.rows[0].coords[1] = ux*uy*(1-cosTheta) - uz*sinTheta;
            // set 0, 2
            rotationMatrix.rows[0].coords[2] = ux*uz*(1-cosTheta) + uy*sinTheta;
            // set 1, 0
            rotationMatrix.rows[1].coords[0] = uy*ux*(1-cosTheta) + uz*sinTheta;
            // set 1, 1
            rotationMatrix.rows[1].coords[1] = cosTheta + uy*uy*(1-cosTheta);
            // set 1, 2
            rotationMatrix.rows[1].coords[2] = uy*uz*(1-cosTheta) - ux*sinTheta;
            // set 2, 0
            rotationMatrix.rows[2].coords[0] = uz*ux*(1-cosTheta) - uy*sinTheta;
            // set 2, 1
            rotationMatrix.rows[2].coords[1] = uz*uy*(1-cosTheta) + ux*sinTheta;
            // set 2, 2
            rotationMatrix.rows[2].coords[2] = cosTheta + uz*uz*(1-cosTheta);
            
            return rotationMatrix;
        }; // rotationMatrixAboutAxis
        
        Vec3 affineRotaionMulVec(Matrix3x3 rotationTransform, Vec3 vector){
            Vec3 result;
            
            result.coords[0] = rotationTransform[0][0] * vector.coords[0] + rotationTransform[0][1] * vector.coords[1] + rotationTransform[0][2] * vector.coords[2];
            result.coords[1] = rotationTransform[1][0] * vector.coords[0] + rotationTransform[1][1] * vector.coords[1] + rotationTransform[1][2] * vector.coords[2];
            result.coords[2] = rotationTransform[2][0] * vector.coords[0] + rotationTransform[2][1] * vector.coords[1] + rotationTransform[2][2] * vector.coords[2];
            
            return result;
        }; // affineRotationMulVec
        
        float vectorMul(Vec3 a, Vec3 b){
            return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
        }
        
        Matrix3x3 transpose(Matrix3x3 a){
            Matrix3x3 result;
            result.rows[0] = a.column(0);
            result.rows[1] = a.column(1);
            result.rows[2] = a.column(2);
            return result;
        }
        
        Matrix3x3 matrixMul(Matrix3x3 a, Matrix3x3 b){
            Matrix3x3 result;
            result.rows[0].coords[0] = vectorMul(a[0], b.column(0));
            result.rows[0].coords[1] = vectorMul(a[0], b.column(1));
            result.rows[0].coords[2] = vectorMul(a[0], b.column(2));
            result.rows[1].coords[0] = vectorMul(a[1], b.column(0));
            result.rows[1].coords[1] = vectorMul(a[1], b.column(1));
            result.rows[1].coords[2] = vectorMul(a[1], b.column(2));
            result.rows[2].coords[0] = vectorMul(a[2], b.column(0));
            result.rows[2].coords[1] = vectorMul(a[2], b.column(1));
            result.rows[2].coords[2] = vectorMul(a[2], b.column(2));
            return result;
        }
        
        
        wb_float circleIntersectionArea(wb_float distance, wb_float radius){
            if (distance > 2*radius) {
                return 0;
            }
            return 2*radius*radius*acos(distance/(2*radius)) - 1/2 * distance * sqrt(4*radius*radius - distance*distance);
        }
    }
}