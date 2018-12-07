// --
//  Plate.cpp
//  WorldGenerator
//

#include "Plate.hpp"

#include <math.h>

namespace WorldBuilder {
    
    size_t Plate::surfaceSize() const{
        size_t size = 0;
        
        for (auto cellIt = this->cells.begin(); cellIt != this->cells.end(); cellIt++)
        {
            if (!cellIt->second->isSubducted() && !cellIt->second->rock.isEmpty()) {
                size++;
            }
        }
        return size;
    }
    
    void Plate::homeostasis(const WorldAttributes worldAttributes, wb_float timestep){
        for (auto cellIt = this->cells.begin(); cellIt != this->cells.end(); cellIt++)
        {
            cellIt->second->homeostasis(worldAttributes, timestep);
        }
    }
    
    void Plate::move(wb_float timestep){
        Matrix3x3 timestepRotation = math::rotationMatrixAboutAxis(this->pole, this->angularSpeed*timestep);
        this->rotationMatrix = math::matrixMul(this->rotationMatrix, timestepRotation);
    }
    
    void Plate::updateCellRadii(){
        for (auto cellIt = this->cells.begin(); cellIt != this->cells.end(); cellIt++)
        {
            cellIt->second->poleRadius = math::distanceFromPole(cellIt->second->get_vertex()->get_vector(), this->pole);
        }
    }
    
    Plate::Plate(uint32_t initialCellCount, uint32_t ourId) : rotationMatrix(math::identityMatrix()), centerVertex(nullptr), maxEdgeAngle(0), id(ourId) {
        this->cells.reserve(initialCellCount);
        this->angularSpeed = 0;
    }
    
}