// --
//  WorldCell.cpp
//  WorldGenerator
//

#include "WorldCell.hpp"

#include "PlateCell.hpp"

namespace WorldBuilder {
    
    float WorldCell::get_elevation(int_fast32_t activePlateIndex) const{
        float elevation = 0.0;
        if (activePlateIndex >=0 && activePlateIndex < this->surfaceCells.size()) {
            // for now grap the first cell
            elevation = this->surfaceCells[activePlateIndex][0]->get_elevation();
        } else {
            elevation = this->strandedElevation + this->strandedSediment.thickness;
        }
        return elevation;
    }
    
    void WorldCell::clearPlateCells(size_t plateCount){
        // may only need to resize if plateCount is larger than size
        this->surfaceCells.resize(plateCount);
        this->subductedCells.resize(plateCount);
        this->emptyCells.resize(plateCount);
        
        // clear plate cells from surface
        for (std::vector<std::vector<PlateCell*>>::iterator plateVector = this->surfaceCells.begin(); plateVector != this->surfaceCells.end(); plateVector++) {
            plateVector->clear();
        }
        // clear plate cells from subducted
        for (std::vector<std::vector<PlateCell*>>::iterator plateVector = this->subductedCells.begin(); plateVector != this->subductedCells.end(); plateVector++) {
            plateVector->clear();
        }
        // clear plate cells from surface
        for (std::vector<std::vector<PlateCell*>>::iterator plateVector = this->emptyCells.begin(); plateVector != this->emptyCells.end(); plateVector++) {
            plateVector->clear();
        }
    }
    
}