// --
//  MomentumTracker.cpp
//  WorldGenerator
//


#include "MomentumTracker.hpp"

#include <iostream>
#include <math.h>
#include <stdio.h>

namespace WorldBuilder {
/**************** Commit Transfer ****************/
    static wb_float frictionCoeff = 0.01;
    
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
        std::unordered_map<uint32_t, Vec3> newMomentumPoles;
        newMomentumPoles.reserve(this->plates.size());
        
        for (auto&& plateIt : this->plates) {
            uint32_t index = plateIt.first;
            std::shared_ptr<Plate>& plate = plateIt.second;
            newMomentumPoles[index] = plate->pole * this->startingMomentum[index];
            if (badTransfer(newMomentumPoles[index])) {
                //std::printf("Bad new pole at start\n");
            }
        }
        for (auto&& sourcePlateIt : this->plates) {
            uint32_t sourcePlateIndex = sourcePlateIt.first;
            for (auto&& destinationPlateIt : this->plates) {
                uint32_t destinationPlateIndex = destinationPlateIt.first;
                if (sourcePlateIndex != destinationPlateIndex) {
                    Vec3 transferPole = this->plates[sourcePlateIndex]->pole * this->momentumTransfers[((uint64_t)sourcePlateIndex << 32) | destinationPlateIndex];
                    // check if bad transfer!!!!!!!
                    if (badTransfer(transferPole)) {
                        //std::cout << "Bad mass transfer" << std::endl;
                    }
                    
                    // add to destination
                    newMomentumPoles[destinationPlateIndex] = newMomentumPoles[destinationPlateIndex] + transferPole;
                    
                    // subtract from source
                    newMomentumPoles[sourcePlateIndex] = newMomentumPoles[sourcePlateIndex] - transferPole;
                    
                }
            }
        } // end mass transfer
        
        // tranfer momentum from friction
        for (auto firstPlateIt = this->plates.begin(), end = this->plates.end(); firstPlateIt != end; firstPlateIt++) {
            uint32_t firstPlateIndex = firstPlateIt->first;
            std::shared_ptr<Plate>& firstPlate = firstPlateIt->second;
            auto secondPlateIt = firstPlateIt;
            secondPlateIt++;
            for (; secondPlateIt != end; secondPlateIt++) {
                uint32_t secondPlateIndex = secondPlateIt->first;
                std::shared_ptr<Plate>& secondPlate = secondPlateIt->second;
                
                size_t combinedCount = this->collidedCellCount[((uint64_t)firstPlateIndex << 32) | secondPlateIndex] + this->collidedCellCount[((uint64_t) secondPlateIndex) | firstPlateIndex];
                if (combinedCount != 0 && this->startingCellCount[firstPlateIndex] != 0 && this->startingCellCount[secondPlateIndex] != 0) {
                    wb_float collisionFractionI = (wb_float)combinedCount / (wb_float)this->startingCellCount[firstPlateIndex] / 2;
                    wb_float collisionFractionJ = (wb_float)combinedCount / (wb_float)this->startingCellCount[secondPlateIndex] / 2;
                    
                    // to i we are subtracting a portion of i and adding a portion of j
                    Vec3 transferPole = secondPlate->pole * (frictionCoeff*this->startingMomentum[firstPlateIndex]*collisionFractionI) -
                                        firstPlate->pole * (frictionCoeff*this->startingMomentum[secondPlateIndex]*collisionFractionJ);
                    
                    if (badTransfer(transferPole)) {
                        //std::cout << "Bad friction transfer" << std::endl;
                    }
                    
                    // add to plate i
                    newMomentumPoles[firstPlateIndex] = newMomentumPoles[firstPlateIndex] + transferPole;
                    
                    // subtract from plate j
                    newMomentumPoles[secondPlateIndex] = newMomentumPoles[secondPlateIndex] - transferPole;
                }
            }
        }
        
        // calculate new poles
        for (auto&& plateIt : this->plates){
            uint32_t index = plateIt.first;
            std::shared_ptr<Plate>& plate = plateIt.second;
            wb_float newMagnitude;
            Vec3 newPole;
            std::tie(newPole, newMagnitude) = math::normalize3VectorWithScale(newMomentumPoles[index]);
            if (newMagnitude == 0 || std::isnan(newMagnitude)) {
                //std::cout << "Bad angular momentum magnitude on commit" << std::endl;
                std::printf("Starting Mag: %f\n", this->startingMomentum[index]);
                std::printf("New Momentum Pole: (%f, %f, %f)\n", newMomentumPoles[index][0], newMomentumPoles[index][1], newMomentumPoles[index][2]);
                std::printf("Plate Index: %u\n", index);
                newPole.coords[0] = 0;
                newPole.coords[1] = 0;
                newPole.coords[2] = 1;
                newMagnitude = 0.00001; // something really small
            }
            plate->pole = newPole;
            
            // update cell radii with new pole
            plate->updateCellRadii();
            
            wb_float poleChangeMomentumMagnitude = 0;
            for (auto&& cellIt : plate->cells)
            {
                std::shared_ptr<PlateCell>& cell = cellIt.second;
                // TODO may be able to get rid of angular speed multiplication here if we don't scale by it just below
                poleChangeMomentumMagnitude += cell->poleRadius * cell->rock.mass() * plate->angularSpeed;
            }
            
            plate->angularSpeed = plate->angularSpeed * newMagnitude / poleChangeMomentumMagnitude;
            
            if (plate->angularSpeed <= 0 || isnan(plate->angularSpeed)) {
                std::cout << "Plate has invalid angular speed after momentum change, setting to near 0" << std::endl;
                plate->angularSpeed = 0.000000001;
            }
        }
    }
    
    
/**************** Modifiers ****************/
    void AngularMomentumTracker::transferMomentumOfCell(std::shared_ptr<Plate> source, std::shared_ptr<Plate> destination, std::shared_ptr<PlateCell> cell){
        // transfers the entire momentum from the cell
        this->momentumTransfers[((uint64_t)source->id << 32) | destination->id] += cell->rock.mass() * cell->poleRadius * source->angularSpeed;
    }
    

    
    void AngularMomentumTracker::addCollision(std::shared_ptr<Plate> source, std::shared_ptr<Plate> destination) {
        if (source->id == destination->id) {
            // something's fishy
            return;
        }
        this->collidedCellCount[((uint64_t)source->id) | destination->id] += 1;
    }
    
/**************** Constructors ****************/
    AngularMomentumTracker::AngularMomentumTracker(std::unordered_map<uint32_t, std::shared_ptr<Plate>> iPlates){
        this->plates = iPlates;
        
        this->startingMomentum.reserve(iPlates.size());
        this->startingCellCount.reserve(iPlates.size());
        this->momentumTransfers.reserve(iPlates.size() * iPlates.size());
        this->collidedCellCount.reserve(iPlates.size() * iPlates.size());
        
        // initialize with zero
        for (auto&& plateIt : this->plates) {
            for (auto&& secondPlateIt : this->plates) {
                this->momentumTransfers[((uint64_t)plateIt.second->id << 32) | secondPlateIt.second->id] = 0;
                this->collidedCellCount[((uint64_t)plateIt.second->id << 32) | secondPlateIt.second->id] = 0;
            }
        }
        
        // calculate starting angular momentum
        // calculate starting surface cell count
        for (auto&& plateIt : this->plates) {
            std::shared_ptr<Plate>& plate = plateIt.second;
            this->startingCellCount[plate->id] = plate->cells.size();
            // update cell radii for momentum calculations
            plate->updateCellRadii();
            wb_float momentumMagnitude = 0;
            for (auto&& cellIt : plate->cells)
            {
                std::shared_ptr<PlateCell>& cell = cellIt.second;
                wb_float mag = cell->poleRadius * cell->rock.mass() * plate->angularSpeed;
                if (std::isfinite(mag)) {
                    momentumMagnitude += cell->poleRadius * cell->rock.mass() * plate->angularSpeed;
                }
            }
            this->startingMomentum[plate->id] = momentumMagnitude;
        }
    }
}