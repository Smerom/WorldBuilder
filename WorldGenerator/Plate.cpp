// --
//  Plate.cpp
//  WorldGenerator
//

#include "Plate.hpp"

#include <math.h>

namespace WorldBuilder {
    
    size_t Plate::surfaceSize() const{
        size_t size = 0;
        
        for (std::vector<PlateCell>::const_iterator cell = this->cells.begin();
             cell != this->cells.end(); cell++)
        {
            if (!cell->isSubducted() && !cell->rock.isEmpty()) {
                size++;
            }
        }
        return size;
    }
    
    void Plate::homeostasis(const WorldAttributes worldAttributes, float timestep){
        for (std::vector<PlateCell>::iterator cell = this->cells.begin();
             cell != this->cells.end(); cell++) {
            cell->homeostasis(worldAttributes, timestep);
        }
    }
    
    void Plate::move(float timestep){
        Matrix3x3 timestepRotation = math::rotationMatrixAboutAxis(this->pole, this->angularSpeed*timestep);
        this->rotationMatrix = math::matrixMul(this->rotationMatrix, timestepRotation);
    }
    
    void Plate::updateCellRadii(){
        for (std::vector<PlateCell>::iterator cell = this->cells.begin();
             cell != this->cells.end();
             cell++)
        {
            cell->poleRadius = math::distanceFromPole(cell->get_vertex()->get_vector(), this->pole);
        }
    }
    
    Plate::Plate(Grid *worldGrid) : rotationMatrix(math::identityMatrix()), cells(worldGrid->verts_size()) {
        size_t index = 0;
        for (std::vector<PlateCell>::iterator cell = this->cells.begin();
             cell != this->cells.end(); cell++, index++)
        {
            cell->vertex = &(worldGrid->get_vertices())[index];
        }
        this->angularSpeed = 0;
    }
    
}