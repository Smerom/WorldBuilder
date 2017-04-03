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
        
        std::tuple<Vec3, wb_float, bool> edgeIntersection(Vec3 p, Vec3 q, Vec3 a, Vec3 v) {
            Vec3 n = a-p;
            auto normScaleX = normalize3VectorWithScale(q - p);
            wb_float xNormFactor = normScaleX.second;
            Vec3 x = normScaleX.first;
            
            wb_float mMag = n.dot(x);
            Vec3 m = x*mMag;
            
            Vec3 l = n - m;
            Vec3 y = normalize3Vector(l);
            
            Vec3 vPrime = v - (x.cross(y) * v.dot(x.cross(y)));
            
            wb_float a2[2], vPrime2[2];
            
            a2[0] = a.dot(x);
            a2[1] = a.dot(y);
            vPrime2[0] = vPrime.dot(x);
            vPrime2[1] = vPrime.dot(y);
            
            // find x intercept in 3 space
            wb_float xIntercept = xNormFactor * (a2[0] - a2[1]*vPrime2[0]/vPrime2[1]);
            Vec3 xInterceptPoint = p + (q - p) * xIntercept;
            bool isBetween = (xIntercept > 0 && xIntercept < 1);
            wb_float vCount = -1 * a2[1] / vPrime2[1]; // if v'[1] is negative, it points towards the edge, return positive v lenghts
            
            return std::make_tuple(xInterceptPoint, vCount, isBetween);
        }
        
        std::tuple<Vec3, wb_float, bool, uint8_t> triangleIntersection(Vec3 p, Vec3 q, Vec3 r, Vec3 a, Vec3 v, bool checkPQ) {
            // find Z
            Vec3 z = normalize3Vector((q-p).cross(r-p));
            
            // move a into plane such that it stays in the triangle
            a = a * (p.dot(z) / a.dot(z));
            
            // subtract z from v to get v' in the plane
            Vec3 vPrime = v - z*(v.dot(z));
            
            // get our second axis
            Vec3 y = normalize3Vector(vPrime);
            
            // get our third axis
            Vec3 x = y.cross(z);
            
            // find the x and y coords for each vector of interest, with the origin moved to a
            wb_float pPrime[2], qPrime[2], rPrime[2], aPrime[2];
            aPrime[0] = a.dot(x);
            aPrime[1] = a.dot(y);
            
            pPrime[0] = p.dot(x) - aPrime[0];
            pPrime[1] = p.dot(y) - aPrime[1];
            
            qPrime[0] = q.dot(x) - aPrime[0];
            qPrime[1] = q.dot(y) - aPrime[1];
            
            rPrime[0] = r.dot(x) - aPrime[0];
            rPrime[1] = r.dot(y) - aPrime[1];
            
            wb_float intercept = 0;
            wb_float vPrimeLengths = 0;
            bool validIntersection = false;
            uint8_t between = 0;
            Vec3 intersectionPoint;

            // since a is assumed to be inside the triangle, only one pair can both span the y axis and have at least one point above it
            
            // p - r
            if (((pPrime[0] < 0 && rPrime[0] > 0) || (pPrime[0] > 0 && rPrime[0] < 0)) && (pPrime[1] > 0 || rPrime[1] > 0)) {
                intercept = rPrime[1] - rPrime[0]*(rPrime[1] - pPrime[1])/(rPrime[0] - pPrime[0]);
                vPrimeLengths = intercept / vPrime.length();
                between = 1 << 1;
                validIntersection = true;
                intersectionPoint = p + (r - p) * (pPrime[0]/(pPrime[0] - rPrime[0]));
            } else if (((qPrime[0] < 0 && rPrime[0] > 0) || (qPrime[0] > 0 && rPrime[0] < 0)) && (qPrime[1] > 0 || rPrime[1] > 0)) { // q - r
                intercept = rPrime[1] - rPrime[0]*(rPrime[1] - qPrime[1])/(rPrime[0] - qPrime[0]);
                vPrimeLengths = intercept / vPrime.length();
                between = 1 << 2;
                validIntersection = true;
                intersectionPoint = q + (r - q) * (qPrime[0]/(qPrime[0] - rPrime[0]));
            } else if (checkPQ) { // p - q
                if (((pPrime[0] < 0 && qPrime[0] > 0) || (pPrime[0] > 0 && qPrime[0] < 0)) && (pPrime[1] > 0 || qPrime[1] > 0)) {
                    intercept = qPrime[1] - qPrime[0]*(qPrime[1] - pPrime[1])/(qPrime[0] - pPrime[0]);
                    vPrimeLengths = intercept / vPrime.length();
                    between = 1 << 0;
                    validIntersection = true;
                    intersectionPoint = p + (q - p) * (pPrime[0]/(pPrime[0] - qPrime[0]));
                }
            }
            return std::make_tuple(intersectionPoint, vPrimeLengths, validIntersection, between);
        }
    }
}