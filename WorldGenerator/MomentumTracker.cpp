// --
//  MomentumTracker.cpp
//  WorldGenerator
//


#include "MomentumTracker.hpp"

#include <math.h>
#include <iostream>
#include <stdio.h>

namespace WorldBuilder {
/**************** Commit Transfer ****************/
    static float frictionCoeff = 0.01;
    
    bool badTransfer(Vec3 transfer){
        return (isnan(transfer[0]) ||
                isnan(transfer[1]) ||
                isnan(transfer[2]) ||
                isinf(transfer[0]) ||
                isinf(transfer[1]) ||
                isinf(transfer[2])
                );
    }
    
    void AngularMomentumTracker::commitTransfer(){
        Vec3* newMomentumPoles = (Vec3*)calloc(this->plateCount, sizeof(Vec3));
        
        for (size_t i = 0; i < this->plateCount; i++) {
            newMomentumPoles[i] = this->plates[i]->pole * this->startingMomentumMagnitude[i];
            if (badTransfer(newMomentumPoles[i])) {
                std::printf("Bad new pole at start\n");
            }
        }
        for (size_t source = 0; source < this->plateCount; source++) {
            for (size_t destination = 0; destination < this->plateCount; destination++) {
                if (source != destination) {
                    Vec3 transferPole = this->plates[source]->pole * this->momentumDeltaMagnitude[source + destination*this->plateCount];
                    // check if bad transfer!!!!!!!
                    if (badTransfer(transferPole)) {
                        std::cout << "Bad mass transfer" << std::endl;
                    }
                    
                    // add to destination
                    newMomentumPoles[destination] = newMomentumPoles[destination] + transferPole;
                    
                    // subtract from source
                    newMomentumPoles[source] = newMomentumPoles[source] - transferPole;
                    
                }
            }
        } // end mass transfer
        
        // tranfer momentum from friction
        for (size_t i = 0; i < this->plateCount; i++) {
            for (size_t j = i+1; j < this->plateCount; j++) {
                // check we didn't collide too many times
                if (this->colidedSurfaceCellCount[i+j*this->plateCount] > this->startingSurfaceCellCount[i] ||
                    this->colidedSurfaceCellCount[i+j*this->plateCount] > this->startingSurfaceCellCount[j]) {
                    std::cout << "More collided cells than started with!" << std::endl;
                }
                float collisionFractionI = this->colidedSurfaceCellCount[i+j*this->plateCount] / this->startingSurfaceCellCount[i];
                float collisionFractionJ = this->colidedSurfaceCellCount[i+j*this->plateCount] / this->startingSurfaceCellCount[j];
                
                // to i we are subtracting a portion of i and adding a portion of j
                Vec3 transferPole = this->plates[j]->pole*(frictionCoeff*this->startingMomentumMagnitude[i]*collisionFractionI) -
                                    this->plates[i]->pole*(frictionCoeff*this->startingMomentumMagnitude[j]*collisionFractionJ);
                
                if (badTransfer(transferPole)) {
                    std::cout << "Bad friction transfer" << std::endl;
                }
                
                if (this->startingSurfaceCellCount[i] == 0 || this->startingSurfaceCellCount[j] ==0) {
                    std::cout << "Bad start count\n";
                }
                
                // add to plate i
                newMomentumPoles[i] = newMomentumPoles[i] + transferPole;
                
                // subtract from plate j
                newMomentumPoles[j] = newMomentumPoles[j] - transferPole;
            }
        }
        
        // calculate new poles
        size_t index = 0;
        for (std::vector<std::shared_ptr<Plate>>::iterator plate = this->plates.begin();
             plate != this->plates.end();
             plate++, index ++)
        {
            float newMagnitude;
            Vec3 newPole;
            std::tie(newPole, newMagnitude) = math::normalize3VectorWithScale(newMomentumPoles[index]);
            if (newMagnitude == 0 || isnan(newMagnitude)) {
                std::cout << "Bad angular momentum magnitude on commit" << std::endl;
                std::printf("Starting Mag: %f\n", this->startingMomentumMagnitude[index]);
                std::printf("New Momentum Pole: (%f, %f, %f)\n", newMomentumPoles[index][0], newMomentumPoles[index][1], newMomentumPoles[index][2]);
                std::printf("Plate Index: %lu\n", index);
            }
            (*plate)->pole = newPole;
            
            // update cell radii with new pole
            (*plate)->updateCellRadii();
            
            float poleChangeMomentumMagnitude = 0;
            for (std::vector<PlateCell>::iterator cell = (*plate)->cells.begin();
                 cell != (*plate)->cells.end();
                 cell++)
            {
                // TODO may be able to get rid of angular speed multiplication here if we don't scale by it just below
                poleChangeMomentumMagnitude += cell->poleRadius * cell->rock.mass() * (*plate)->angularSpeed;
            }
            
            (*plate)->angularSpeed = (*plate)->angularSpeed * newMagnitude / poleChangeMomentumMagnitude;
            
            if ((*plate)->angularSpeed <= 0 || isnan((*plate)->angularSpeed)) {
                std::cout << "Plate has invalid angular speed after momentum change" << std::endl;
            }
        }
        // clean
        free(newMomentumPoles);
    }
    
    
/**************** Modifiers ****************/
    void AngularMomentumTracker::transferMomentumMagnitude(size_t source, size_t destination, float magnitude){
        this->momentumDeltaMagnitude[source+this->plateCount*destination] += magnitude;
    }
    
    void AngularMomentumTracker::removeMomentumFromPlate(size_t plateIndex, float magnitude){
        this->startingMomentumMagnitude[plateIndex] -= magnitude;
    }
    
    void AngularMomentumTracker::addCollision(size_t i, size_t j) {
        if (i == j) {
            // something's fishy
            return;
        }
        if (i > j) {
            size_t temp = i;
            i = j;
            j = temp;
        }
        this->colidedSurfaceCellCount[i + j*this->plateCount] += 1;
    }
    
/**************** Constructors ****************/
    AngularMomentumTracker::AngularMomentumTracker(std::vector<std::shared_ptr<Plate>> iPlates){
        this->plates = iPlates;
        plateCount = plates.size();
        // allocate a bunch of arrays
        this->startingMomentumMagnitude = (float*)calloc(plateCount, sizeof(float));
        this->momentumDeltaMagnitude = (float*)calloc(plateCount*plateCount, sizeof(float));
        this->startingSurfaceCellCount = (size_t*)calloc(plateCount, sizeof(size_t));
        this->colidedSurfaceCellCount = (size_t*)calloc(plateCount*plateCount, sizeof(size_t));
        
        // calculate starting angular momentum
        // calculate starting surface cell count
        size_t index = 0;
        for (std::vector<std::shared_ptr<Plate>>::iterator plate = plates.begin();
             plate != plates.end();
             plate++, index++)
        {
            // update cell radii for momentum calculations
            (*plate)->updateCellRadii();
            float momentumMagnitude = 0;
            size_t surfaceCount = 0;
            for (std::vector<PlateCell>::iterator cell = (*plate)->cells.begin();
                 cell != (*plate)->cells.end();
                 cell++)
            {
                // only surface cells???
                if (cell->rock.isEmpty() == false) {
                    float mag = cell->poleRadius * cell->rock.mass() * (*plate)->angularSpeed;
                    if (isnan(mag) || isinf(mag)) {
                        //std::printf("Bad cell!");
                        throw "Bad cell!";
                    }
                    
                    momentumMagnitude += cell->poleRadius * cell->rock.mass() * (*plate)->angularSpeed;
                    // check surface cell
                    if (cell->isSubducted() == false) {
                        surfaceCount++;
                    }
                }
            }
            this->startingMomentumMagnitude[index] = momentumMagnitude;
            this->startingSurfaceCellCount[index] = surfaceCount;
        }
    }
    AngularMomentumTracker::~AngularMomentumTracker(){
        free(this->startingMomentumMagnitude);
        free(this->momentumDeltaMagnitude);
        free(this->startingSurfaceCellCount);
        free(this->colidedSurfaceCellCount);
    }
}