// --
//  World.cpp
//  WorldGenerator
//

#include "World.hpp"

#include <iostream>
#include <limits>

#include <thread>

namespace WorldBuilder {
    
    const float erosionRate = 0.05; // per million years
    
    WorldUpdateTask World::progressByTimestep(float minTimestep) {
        WorldUpdateTask updateTask;
        float timestep = minTimestep;
        
        if (this->plates.size() == 0) {
            return updateTask;
        }
        
        // update plate indices?
        
        // find a reasonable timestep (currently broken)
        // should get fastest relative speeds
        float fastestPlateSpeed = 0; // fastest plate speeds
        for (std::vector<std::shared_ptr<Plate>>::iterator plate = this->plates.begin(); plate != this->plates.end(); plate++) {
            if (fastestPlateSpeed < std::abs((*plate)->angularSpeed)) {
                fastestPlateSpeed = std::abs((*plate)->angularSpeed);
            }
        }
        // since we are on the unit sphere (assumed)
        timestep = (this->cellSmallAngle / fastestPlateSpeed );
        if (timestep < minTimestep) {
            timestep = minTimestep;
        } else if (timestep > 10) {
            timestep = 10;
        }
        
        updateTask.timestepUsed = timestep;
        
        this->age = this->age + timestep;
        
        updateTask.move.start();
        this->movePlates(timestep);
        updateTask.move.end();
        
        updateTask.cast.start();
        this->castPlateCellsToWorld();
        updateTask.cast.end();
        
        updateTask.homeostasis.start();
        this->homeostasis(timestep);
        updateTask.homeostasis.end();
        
        
        // set up momentum transfer before collision phase
        delete this->momentumTransfer; // delete old transfer
        this->momentumTransfer = new AngularMomentumTracker(this->plates);
        
        // collision phase
        updateTask.collision.start();
        this->collision(timestep);
        updateTask.collision.end();
        
        // commit momentum tranfer after collision phase
        this->momentumTransfer->commitTransfer();
        
        // patch phase
        this->patch();
        
        // erosion phase
        updateTask.erode.start();
        this->erode(timestep);
        updateTask.erode.end();
        
        
        
        updateTask.superCycle.start();
        this->supercontinentCycle();
        updateTask.superCycle.end();
        
        // clean up phase
        std::vector<std::shared_ptr<Plate>> viablePlates;
        for (std::vector<std::shared_ptr<Plate>>::iterator plate = this->plates.begin();
             plate != this->plates.end();
             plate++)
        {
            // check plates have some cells
            bool surfaceCellFound = false;
            for (std::vector<PlateCell>::iterator cell = (*plate)->cells.begin();
                 cell != (*plate)->cells.end() && !surfaceCellFound;
                 cell ++)
            {
                if (!cell->isSubducted() && !cell->rock.isEmpty()) {
                    surfaceCellFound = true;
                }
            }
            if (surfaceCellFound) {
                viablePlates.push_back(*plate);
            }
        }
        this->plates = viablePlates;
        
        return updateTask;
    }
    
    // wants somewhere to go
    float World::randomPlateDensityOffset() {
        return this->randomSource->randomNormal(0.0, 1.0);
    }
    
/*************** Casting ***************/
    
    WorldCell* World::getNearestWorldCell(Vec3 absoluteCellPosition, WorldCell* startCellHint){
        if (startCellHint == nullptr) {
            startCellHint = &(this->cells)[0];
        }
        const GridVertex* currentNearest = startCellHint->get_vertex();
        const GridVertex* testSetNearest = currentNearest;
        float smallestSquareDistance = math::squareDistanceBetween3Points(absoluteCellPosition, startCellHint->get_vertex()->get_vector());
        // could loop for a very long time...
        do {
            float testDistance;
            currentNearest = testSetNearest;
            // loop neighbors
            // bool closestFound = false;
            for (std::vector<GridVertex*>::const_iterator vertex = currentNearest->get_neighbors().begin();
                 vertex != currentNearest->get_neighbors().end();
                 vertex++)
            {
                testDistance = math::squareDistanceBetween3Points(absoluteCellPosition, (*vertex)->get_vector());
                if (testDistance < smallestSquareDistance) {
                    smallestSquareDistance = testDistance;
                    testSetNearest = *vertex;
                }
            }
        } while (currentNearest != testSetNearest);
        /*
        if (smallestSquareDistance > this->cellSmallAngle) {
            std::printf("Smallest distance of: %f on cell distance: %f\n", smallestSquareDistance, this->cellSmallAngle);
        }*/
        
        currentNearest = testSetNearest;
        
        WorldCell* nearestWorldCell = &(this->cells)[currentNearest->get_index()];
        // something is fishy if nearest doesn't have the same index
        /*
        if (nearestWorldCell->get_vertex() != currentNearest) {
            std::cout << "WOLRD CELL ERROR! Doesn't match vertex index" << std::endl;
        }*/
        
        return nearestWorldCell;
    }
    
    void World::castPlateCellsToWorld(){
        
        for (std::vector<WorldCell>::iterator cell = this->cells.begin(); cell != this->cells.end(); cell++) {
            cell->clearPlateCells(this->plates.size());
        }
        
        // cast each plate
        size_t plateIndex = 0;
        std::vector<std::thread> plateThreads;
        for (std::vector<std::shared_ptr<Plate>>::iterator plate = this->plates.begin();
             plate != this->plates.end();
             plate++, plateIndex++)
        {
            // cast each
            auto threadedCast = [plate, this, plateIndex](){
                for (std::vector<PlateCell>::iterator plateCell = (*plate)->cells.begin();
                     plateCell != (*plate)->cells.end();
                     plateCell ++)
                {
                    WorldCell* nearestWorldCell = this->getNearestWorldCell((*plate)->localToWorld(plateCell->get_vertex()->get_vector()), plateCell->lastNearest);
                    if (plateCell->rock.isEmpty()) {
                        nearestWorldCell->addEmptyCell(&*plateCell, plateIndex);
                    } else if(plateCell->isSubducted()) {
                        nearestWorldCell->addSubductedCell(&*plateCell, plateIndex);
                    } else {
                        nearestWorldCell->addSurfaceCell(&*plateCell, plateIndex);
                    }
                    // update nearest cell
                    plateCell->lastNearest = nearestWorldCell;
                }
            };
            std::thread plateThread (threadedCast);
            
            plateThreads.push_back(std::move(plateThread));
        }
        
        // join all our thread
        for (std::vector<std::thread>::iterator thread = plateThreads.begin();
             thread != plateThreads.end();
             thread++)
        {
            thread->join();
        }
        
        // transfer any rock that was previously stranded
        // also sort by distance
        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
             cell != this->cells.end();
             cell++)
        {
            Vec3 cellVector = cell->get_vertex()->get_vector();
            auto lamdaSortByDistance = [cellVector](const PlateCell* a, const PlateCell* b){
                float aDistance = math::squareDistanceBetween3Points(cellVector, a->get_vertex()->get_vector());
                float bDistance = math::squareDistanceBetween3Points(cellVector, b->get_vertex()->get_vector());
                return aDistance < bDistance;
            };
            // surface
            for(std::vector<std::vector<PlateCell*>>::iterator surfacePlates = cell->surfaceCells.begin();
                surfacePlates != cell->surfaceCells.end();
                surfacePlates++)
            {
                std::sort(surfacePlates->begin(), surfacePlates->end(), lamdaSortByDistance);
            }
            // subducted
            for(std::vector<std::vector<PlateCell*>>::iterator subductedPlates = cell->subductedCells.begin();
                subductedPlates != cell->subductedCells.end();
                subductedPlates++)
            {
                std::sort(subductedPlates->begin(), subductedPlates->end(), lamdaSortByDistance);
            }
            // empty
            for(std::vector<std::vector<PlateCell*>>::iterator emptyPlates = cell->emptyCells.begin();
                emptyPlates != cell->emptyCells.end();
                emptyPlates++)
            {
                std::sort(emptyPlates->begin(), emptyPlates->end(), lamdaSortByDistance);
            }
            
            if (fabs(cell->strandedSediment.thickness) > 1) {
                int_fast32_t activePlateIndex = this->activePlateIndexForCell(&*cell);
                if (activePlateIndex >=0){
                    cell->surfaceCells[activePlateIndex][0]->rock.sediment = combineSegments(cell->surfaceCells[activePlateIndex][0]->rock.sediment, cell->strandedSediment);
                    cell->strandedSediment.thickness = 0;
                }
            }
        }
    }
    
/*************** Homeostasis ***************/
    void World::homeostasis(float timestep){
        for (std::vector<std::shared_ptr<Plate>>::iterator plate = this->plates.begin();
             plate != this->plates.end();
             plate++)
        {
            (*plate)->homeostasis(this->attributes, timestep);
        }
    }
    
/*************** Collision ***************/
    // TODO handle previous subducted cells where we have no surface cells
    // handles both convergent and divergent cells
    void World::collision(float timestep){
        size_t* plateHitCount = new size_t[this->plates.size()];
        // loop through all world cells
        std::vector<WorldCell*> unriftedCells;
        std::vector<WorldCell*> riftedCells;
        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
             cell != this->cells.end();
             cell++)
        {
            int_fast32_t activePlateIndex = this->activePlateIndexForCell(&*cell);
            if (activePlateIndex < 0) {
                // test for divergence
                // reset plateHit
                for (size_t index = 0; index < this->plates.size(); index++) {
                    plateHitCount[index] = 0;
                }
                // find active plate for neighbors
                size_t largestSurroundingCells = 0;
                size_t hightestCountNeighbor = 0;
                for (std::vector<GridVertex*>::const_iterator neighbor = cell->get_vertex()->get_neighbors().begin();
                     neighbor != cell->get_vertex()->get_neighbors().end();
                     neighbor++)
                {
                    WorldCell* neighborCell = &this->cells[(*neighbor)->get_index()];
                    int_fast32_t activeNeighbor = this->activePlateIndexForCell(neighborCell);
                    if (activeNeighbor >= 0) {
                        plateHitCount[activeNeighbor] += 1;
                        if (plateHitCount[activeNeighbor] > largestSurroundingCells) {
                            largestSurroundingCells = plateHitCount[activeNeighbor];
                            hightestCountNeighbor = activeNeighbor;
                        }
                    }
                }
                
                // check if we should rift given surrounding cells
                // rift if highest count has empty cell there
                if (largestSurroundingCells > 0) {
                    if (cell->emptyCells[hightestCountNeighbor].size() > 0) {
                        // TODO for now simply rift to the first empty
                        cell->emptyCells[hightestCountNeighbor][0]->rock = this->divergentOceanicColumn;
                        riftedCells.push_back(&*cell);
                    } else if (cell->subductedCells[hightestCountNeighbor].size() > 0) {
                        // force subducted to surface for now!
                        cell->subductedCells[hightestCountNeighbor][0]->rock = this->divergentOceanicColumn;
                        cell->subductedCells[hightestCountNeighbor][0]->bIsSubducted = false;
                        riftedCells.push_back(&*cell);
                    } else {
                        /* Seems to cause world eating :O
                        // try other non zero cells
                        // TODO may need ordering if we use a grid with more than 6 neighbors
                        bool riftableFound = false;
                        for (size_t index = 0; index < this->plates.size() && !riftableFound; index++) {
                            if (plateHitCount[index] != 0 && plateHitCount[index] != hightestCountNeighbor) {
                                if (cell->emptyCells[index].size() > 0) {
                                    riftableFound = true;
                                    cell->emptyCells[index][0]->rock = this->divergentOceanicColumn;
                                    riftedCells.push_back(&*cell);
                                }
                            }
                        }
                        if (!riftableFound) {
                            // if we couldn't find an empty cell, add it to unrifted, first round only
                            unriftedCells.push_back(&*cell);
                        }
                         */
                    }
                } else {
                    // failed to rift first round, add to second
                    unriftedCells.push_back(&*cell);
                }
            } else {
                // subduct all other plates under active
                PlateCell *activeCell = cell->surfaceCells[activePlateIndex][0]; // active should have at least one cell
                size_t index = 0;
                for (std::vector<std::vector<PlateCell*>>::iterator plateSection = cell->surfaceCells.begin();
                     plateSection != cell->surfaceCells.end();
                     plateSection++, index++)
                {
                    if (index != activePlateIndex) {
                        for (std::vector<PlateCell*>::iterator plateCell = plateSection->begin();
                             plateCell != plateSection->end();
                             plateCell++)
                        {
                            RockColumn transferRock;
                            // Decide what rock should be tranfered, currently all top stuff
                            transferRock.continental = (*plateCell)->rock.continental;
                            transferRock.sediment = (*plateCell)->rock.sediment;
                            //transferRock.oceanic = (*plateCell)->rock.oceanic;
                            transferRock.oceanic.thickness = 0;
                            transferRock.oceanic.density = 2900;
                            transferRock.root.thickness = 0;
                            transferRock.root.density = 3200;
                            
                            activeCell->rock = accreteColumns(activeCell->rock, transferRock);
                            
                            // modify the subduced rock
                            (*plateCell)->rock.continental.thickness = 0;
                            (*plateCell)->rock.sediment.thickness = 0;
                            //(*plateCell)->rock.oceanic.thickness = 0;
                            
                            // set subduction
                            (*plateCell)->bIsSubducted = true;
                            
                            // transfer momentum
                            this->momentumTransfer->transferMomentumMagnitude(index, activePlateIndex, (transferRock.mass()) * ((*plateCell)->poleRadius) * (this->plates[index]->angularSpeed));
                            this->momentumTransfer->addCollision(index, activePlateIndex);
                        }
                    }
                }
            }
        }
        
        // move rifted to surface, also call homeostasis so they start with reasonable elevation
        // while we have rifted cells...
        std::vector<WorldCell*> nextUnriftedCells;
        while (riftedCells.size() > 0) {
            // move rifted to surface
            for (std::vector<WorldCell*>::iterator riftedCell = riftedCells.begin();
                 riftedCell != riftedCells.end();
                 riftedCell++)
            {
                size_t plateIndex = 0;
                // could keep track of this above...
                for (std::vector<std::vector<PlateCell*>>::iterator riftedPlateSegment = (*riftedCell)->emptyCells.begin();
                     riftedPlateSegment != (*riftedCell)->emptyCells.end();
                     riftedPlateSegment++, plateIndex++)
                {
                    for (std::vector<PlateCell*>::iterator riftedPlateCell = riftedPlateSegment->begin();
                         riftedPlateCell != riftedPlateSegment->end();)
                    {
                        // if it is not empty, move it to surface
                        if (!(*riftedPlateCell)->rock.isEmpty()) {
                            (*riftedCell)->surfaceCells[plateIndex].push_back(*riftedPlateCell);
                            (*riftedPlateCell)->homeostasis(this->attributes, timestep); // sets elevation
                            riftedPlateCell = riftedPlateSegment->erase(riftedPlateCell);
                        } else {
                            riftedPlateCell++;
                        }
                    }
                }
            }
            
            
            
            // clear rifted and loop over the remaining unrifted
            riftedCells.clear(); /*
            for (std::vector<WorldCell*>::iterator cell = unriftedCells.begin();
                 cell != unriftedCells.end();
                 cell++)
            {
                // test for divergence
                // reset plateHit
                for (size_t index = 0; index < this->plates.size(); index++) {
                    plateHitCount[index] = 0;
                }
                // find active plate for neighbors
                size_t totalNeighbors = 0;
                size_t largestSurroundingCells = 0;
                size_t hightestCountNeighbor = 0;
                for (std::vector<GridVertex*>::const_iterator neighbor = (*cell)->get_vertex()->get_neighbors().begin();
                     neighbor != (*cell)->get_vertex()->get_neighbors().end();
                     neighbor++)
                {
                    WorldCell* neighborCell = &this->cells[(*neighbor)->get_index()];
                    int_fast32_t activeNeighbor = this->activePlateIndexForCell(neighborCell);
                    if (activeNeighbor >= 0) {
                        plateHitCount[activeNeighbor] += 1;
                        if (plateHitCount[activeNeighbor] > largestSurroundingCells) {
                            largestSurroundingCells = plateHitCount[activeNeighbor];
                            hightestCountNeighbor = activeNeighbor;
                        }
                    }
                    totalNeighbors += 1;
                }
                
                // check if we should rift given surrounding cells
                // rift if highest count has empty cell there
                if (largestSurroundingCells > 0) {
                    if ((*cell)->emptyCells[hightestCountNeighbor].size() > 0) {
                        // TODO for now simply rift to the first empty
                        (*cell)->emptyCells[hightestCountNeighbor][0]->rock = this->divergentOceanicColumn;
                        riftedCells.push_back(*cell);
                    } else {
                        // try other non zero cells
                        // TODO may need ordering if we use a grid with more than 6 neighbors
                        bool riftableFound = false;
                        for (size_t index = 0; index < this->plates.size() && !riftableFound; index++) {
                            if (plateHitCount[index] != 0 && plateHitCount[index] != hightestCountNeighbor) {
                                if ((*cell)->emptyCells[index].size() > 0) {
                                    riftableFound = true;
                                    (*cell)->emptyCells[index][0]->rock = this->divergentOceanicColumn;
                                    riftedCells.push_back(*cell);
                                }
                            }
                        }
                        if (!riftableFound) {
                            // if we couldn't find an empty cell, leave it
                            // could add since loop depends on new rifted cells
                        }
                    }
                } else {
                    // failed to rift first round, add to second
                    nextUnriftedCells.push_back(*cell);
                }
            } */
            // swap next unrifted and clear
            std::swap(nextUnriftedCells, unriftedCells);
            nextUnriftedCells.clear();
            
        } // while rifted remaining
        
        
        delete[] plateHitCount;
    }
    
    // TODO should cache this when found so we don't look for it every time during phase
    int_fast32_t World::activePlateIndexForCell(const WorldCell *cell) {
        int_fast32_t minDensityIndex = -1;
        float minDensity = std::numeric_limits<float>::max();
        int_fast32_t index = 0;
        for (std::vector<std::vector<PlateCell*>>::const_iterator surfacePlates = cell->surfaceCells.begin();
             surfacePlates != cell->surfaceCells.end(); surfacePlates++, index++)
        {
            if (surfacePlates->size() > 0 && this->plates[index]->densityOffset < minDensity) {
                minDensityIndex = index;
                minDensity = this->plates[index]->densityOffset;
            }
        }
        return minDensityIndex;
    }
    
/*************** Erosion ***************/
    void World::patch(){
        std::vector<WorldCell*> missedCells;
        // int* surrountingPlates; // used to display expected plates, not yet put here
        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
             cell != this->cells.end();
             cell++)
        {
            if (this->activePlateIndexForCell(&*cell) < 0) {
                float newDisplacement = 0;
                size_t validNeighborCount = 0;
                for (std::vector<GridVertex *>::const_iterator gridNeighbors = cell->get_vertex()->get_neighbors().begin();
                     gridNeighbors != cell->get_vertex()->get_neighbors().end();
                     gridNeighbors++)
                {
                    WorldCell* neighborCell = &(this->cells[(*gridNeighbors)->get_index()]);
                    int_fast32_t activeNeighborPlate = this->activePlateIndexForCell(neighborCell);
                    if (activeNeighborPlate >=0) {
                        PlateCell* neighborPlateCell = neighborCell->surfaceCells[activeNeighborPlate][0];
                        newDisplacement += neighborPlateCell->get_elevation();
                        validNeighborCount++;
                    }
                }
                if (validNeighborCount > 0) {
                    cell->strandedElevation = newDisplacement / validNeighborCount;
                } else {
                    // reset stranded in first pass since this cell was missed
                    cell->resetStrandedElevation();
                    missedCells.push_back(&*cell);
                }
            }
        }
        // patch mmissed cells
        std::vector<WorldCell*> nextMissedCells;
        while (missedCells.size() > 0) {
            // loop through missed
            for (std::vector<WorldCell*>::iterator missedCell = missedCells.begin();
                 missedCell != missedCells.end();
                 missedCell++)
            {
                float newDisplacement = 0;
                size_t validNeighborCount = 0;
                for (std::vector<GridVertex *>::const_iterator gridNeighbors = (*missedCell)->get_vertex()->get_neighbors().begin();
                     gridNeighbors != (*missedCell)->get_vertex()->get_neighbors().end();
                     gridNeighbors++)
                {
                    WorldCell* neighborCell = &(this->cells[(*gridNeighbors)->get_index()]);
                    int_fast32_t activeNeighborPlate = this->activePlateIndexForCell(neighborCell);
                    if (activeNeighborPlate >=0) {
                        PlateCell* neighborPlateCell = neighborCell->surfaceCells[activeNeighborPlate][0];
                        newDisplacement += neighborPlateCell->get_elevation();
                        validNeighborCount++;
                    } else if (neighborCell->strandedElevation != -1*std::numeric_limits<float>::max()) {
                        newDisplacement += neighborCell->strandedElevation;
                        validNeighborCount++;
                    }
                }
                if (validNeighborCount > 0) {
                    (*missedCell)->strandedElevation = newDisplacement / validNeighborCount;
                } else {
                    nextMissedCells.push_back(*missedCell);
                }
            }
            std::swap(missedCells, nextMissedCells);
            nextMissedCells.clear();
        }
    }
    
    // TODO need to add timestep, at least to suspension amounts
    std::unique_ptr<MaterialFlowGraph> World::createFlowGraph(){
        MaterialFlowGraph* graph = new MaterialFlowGraph(this->cells.size());
        
        size_t index = 0;
        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
             cell != this->cells.end();
             cell++, index++)
        {
            // set node material height and elevation offset
            int_fast32_t activePlateIndex = this->activePlateIndexForCell(&*cell);
            if (activePlateIndex >= 0) {
                graph->nodes[index].materialHeight = cell->surfaceCells[activePlateIndex][0]->rock.sediment.thickness;
                graph->nodes[index].offsetHeight = cell->surfaceCells[activePlateIndex][0]->get_elevation() - graph->nodes[index].materialHeight;
            } else {
                graph->nodes[index].materialHeight = cell->strandedSediment.thickness;
                graph->nodes[index].offsetHeight = cell->strandedElevation;
            }
        }
        
        // build from node heights
        std::vector<MaterialFlowNode*> outflowCandidates;
        std::vector<float> outflowCandidateSquareElevationDifferences;
        // find outflow only, inflow set from outflow nodes
        index = 0;
        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
             cell != this->cells.end();
             cell++, index++)
        {
            float elevation =  graph->nodes[index].materialHeight + graph->nodes[index].offsetHeight;
            // only nodes above -300 meters can have outflow nodes
            // TODO replace with different erosion model for oceans
            if (elevation > this->attributes.sealevel - 300) {
                float largestHeightDifference = 0; // determines suspended material
                float totalSquareElevationOut = 0;
                // get node index from grid for each neighbor
                for (std::vector<GridVertex*>::const_iterator vertex = cell->vertex->get_neighbors().begin();
                     vertex != cell->vertex->get_neighbors().end();
                     vertex++)
                {
                    MaterialFlowNode* candidate = &(graph->nodes[(*vertex)->get_index()]);
                    float neighborElevation = candidate->materialHeight + candidate->offsetHeight;
                    float heightDifference = elevation - neighborElevation;
                    if (heightDifference > 0) {
                        // downhill node found, take note
                        outflowCandidates.push_back(candidate);
                        outflowCandidateSquareElevationDifferences.push_back(heightDifference*heightDifference);
                        if (heightDifference > largestHeightDifference) {
                            largestHeightDifference = heightDifference;
                        }
                        totalSquareElevationOut += heightDifference*heightDifference;
                    }
                }
                
                // determine ouflow weights, and material suspended in this cell
                if (totalSquareElevationOut > 0) {
                    float x = largestHeightDifference / this->cellDistanceMeters;
                    if (x < 0){
                        x = 0;
                        std::printf("negative grade!");
                    }
                    float y = logf(x+1);
                    float suspended = 15237.4 * y + 170378.0 * y * y;
                    if (suspended >= graph->nodes[index].materialHeight) {
                        graph->nodes[index].suspendedMaterialHeight = graph->nodes[index].materialHeight;
                    } else {
                        graph->nodes[index].suspendedMaterialHeight = suspended;
                    }
                    // determine weights
                    size_t outflowIndex = 0;
                    for (std::vector<MaterialFlowNode*>::iterator outflowCandidate = outflowCandidates.begin();
                         outflowCandidate != outflowCandidates.end();
                         outflowCandidate++, outflowIndex++)
                    {
                        std::shared_ptr<FlowEdge> edge = std::make_shared<FlowEdge>(FlowEdge{});
                        edge->destination = *outflowCandidate;
                        edge->source = &(graph->nodes[index]);
                        edge->materialHeight = 0; // none moved yet
                        edge->weight = outflowCandidateSquareElevationDifferences[outflowIndex] / totalSquareElevationOut;
                        // add as outflow to this node
                        graph->nodes[index].outflowTargets.push_back(edge);
                        // add as inflow node to candidate
                        (*outflowCandidate)->inflowTargets.push_back(edge);
                    }
                }
                // TODO verify weighting sums to one
                
                // clear for next cell
                outflowCandidates.clear();
                outflowCandidateSquareElevationDifferences.clear();
            } else /* elevation < sealevel - 300 */{
                // weight linearly. Less directed water flow??
                float largestHeightDifference = 0; // determines suspended material
                float totalSquareElevationOut = 0;
                // get node index from grid for each neighbor
                for (std::vector<GridVertex*>::const_iterator vertex = cell->vertex->get_neighbors().begin();
                     vertex != cell->vertex->get_neighbors().end();
                     vertex++)
                {
                    MaterialFlowNode* candidate = &(graph->nodes[(*vertex)->get_index()]);
                    float neighborElevation = candidate->materialHeight + candidate->offsetHeight;
                    float heightDifference = elevation - neighborElevation;
                    if (heightDifference > 0) {
                        // downhill node found, take note
                        outflowCandidates.push_back(candidate);
                        outflowCandidateSquareElevationDifferences.push_back(heightDifference);
                        if (heightDifference > largestHeightDifference) {
                            largestHeightDifference = heightDifference;
                        }
                        totalSquareElevationOut += heightDifference;
                    }
                }
                
                // determine ouflow weights, and material suspended in this cell
                if (totalSquareElevationOut > 0) {
                    float x = largestHeightDifference / this->cellDistanceMeters;
                    if (x < 0){
                        x = 0;
                        std::printf("negative grade!");
                    }
                    float y = logf(x+1);
                    // TODO modify target slope for under water
                    float suspended = 15237.4 * y + 170378.0 * y * y;
                    // hack lower
                    suspended = suspended*0.25;
                    if (suspended >= graph->nodes[index].materialHeight) {
                        graph->nodes[index].suspendedMaterialHeight = graph->nodes[index].materialHeight;
                    } else {
                        graph->nodes[index].suspendedMaterialHeight = suspended;
                    }
                    // determine weights
                    size_t outflowIndex = 0;
                    for (std::vector<MaterialFlowNode*>::iterator outflowCandidate = outflowCandidates.begin();
                         outflowCandidate != outflowCandidates.end();
                         outflowCandidate++, outflowIndex++)
                    {
                        std::shared_ptr<FlowEdge> edge = std::make_shared<FlowEdge>(FlowEdge{});
                        edge->destination = *outflowCandidate;
                        edge->source = &(graph->nodes[index]);
                        edge->materialHeight = 0; // none moved yet
                        edge->weight = outflowCandidateSquareElevationDifferences[outflowIndex] / totalSquareElevationOut;
                        // add as outflow to this node
                        graph->nodes[index].outflowTargets.push_back(edge);
                        // add as inflow node to candidate
                        (*outflowCandidate)->inflowTargets.push_back(edge);
                    }
                }
                // TODO verify weighting sums to one
                
                // clear for next cell
                outflowCandidates.clear();
                outflowCandidateSquareElevationDifferences.clear();
                
            }
        }
        
        return std::unique_ptr<MaterialFlowGraph>(graph);
    }
    
    void World::erode(float timestep){
        // sediment flow, does this want to be first???
        std::unique_ptr<MaterialFlowGraph> flowGraph = this->createFlowGraph(); // TODO don't create each phase, reuse the memory!
        flowGraph->flowAll();
        
        // set sediment for each world cell
        size_t index = 0;
        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
             cell != this->cells.end();
             cell++, index++)
        {
            RockSegment sediment;
            sediment.thickness = flowGraph->nodes[index].materialHeight;
            sediment.density = 2700; // TODO calculate some actual density
            
            int_fast32_t activePlateIndex = this->activePlateIndexForCell(&*cell);
            if (activePlateIndex >= 0){
                cell->surfaceCells[activePlateIndex][0]->rock.sediment = sediment;
            } else {
                cell->strandedSediment = sediment;
            }
        }
        
        // bedrock phase
        for (std::vector<WorldCell>::iterator cell = this->cells.begin();
             cell != this->cells.end();
             cell++)
        {
            int_fast32_t activePlateIndex = this->activePlateIndexForCell(&*cell);
            if (activePlateIndex >= 0) {
                PlateCell* activePlateCell = cell->surfaceCells[activePlateIndex][0];
                float activeElevation = activePlateCell->get_elevation();
                if (activeElevation > this->attributes.sealevel - 300) {
                    // move some stuff to lower elevation cells
                    float erosionFactor = (activeElevation - 9260) / (4000);
                    float erosionHeight = 0;
                    if (erosionFactor > 0) {
                        // add erosion to self as sediment
                        erosionFactor = erosionFactor*erosionFactor*erosionFactor/400;
                        // limit to .1
                        if (erosionFactor > 0.5) {
                            erosionFactor = 0.5;
                        }
                        erosionHeight = erosionFactor * timestep * (activeElevation - 9260);
                        RockSegment erodedSegment = activePlateCell->erodeThickness(erosionHeight);
                        activePlateCell->rock.sediment = combineSegments(activePlateCell->rock.sediment, erodedSegment);
                    }
                    
                    float neighborCount = cell->get_vertex()->get_neighbors().size();
                    for (std::vector<GridVertex*>::const_iterator neighborVertex = cell->get_vertex()->get_neighbors().begin();
                         neighborVertex != cell->get_vertex()->get_neighbors().end();
                         neighborVertex++)
                    {
                        
                        int_fast32_t activeNeighborIndex = this->activePlateIndexForCell(&(this->cells[(*neighborVertex)->get_index()]));
                        float neighborElevation = this->cells[(*neighborVertex)->get_index()].get_elevation(activeNeighborIndex);
                        if (activeElevation > neighborElevation){
                            erosionHeight = (activeElevation - neighborElevation) * erosionRate * timestep / neighborCount;
                            
                            RockSegment erodedSegment = activePlateCell->erodeThickness(erosionHeight);
                            if (activeNeighborIndex >=0){
                                this->cells[(*neighborVertex)->get_index()].surfaceCells[activeNeighborIndex][0]->rock.sediment = combineSegments(erodedSegment, this->cells[(*neighborVertex)->get_index()].surfaceCells[activeNeighborIndex][0]->rock.sediment);
                            } else {
                                cell->strandedSediment = combineSegments(cell->strandedSediment, erodedSegment);
                            }//*/
                        }
                    }
                }
            }
        }
    }
    
/*************** Plate Movement ***************/
    void World::movePlates(float timestep){
        for (std::vector<std::shared_ptr<Plate>>::iterator plate = this->plates.begin();
             plate != this->plates.end();
             plate++)
        {
            (*plate)->move(timestep);
        }
    }
    float World::randomPlateSpeed(){
        return randomSource->randomNormal(0.0095, 0.0038); // mean, stdev
    }

/*************** Supercontinent Cycle ***************/
    void World::supercontinentCycle(){
        // for cycle if plates have eaten eachother (want more than two)
        if (this->age > this->supercontinentCycleDuration + this->supercontinentCycleStartAge || this->plates.size() <= 2) {
            this->supercontinentCycleStartAge = this->age;
            this->supercontinentCycleDuration = this->randomSource->randomNormal(300, 200);
            
            
            std::vector<size_t> plateSizes;
            while (this->plates.size() < this->desiredPlateCount) {
                size_t totalPlateSize = 0;
                // get total plate sizes, don't create random number yet, as total size may not
                // equal the size of world cells, but should be close
                for (std::vector<std::shared_ptr<Plate>>::iterator plate = this->plates.begin();
                     plate != this->plates.end();
                     plate++)
                {
                    size_t plateSize = (*plate)->surfaceSize();
                    totalPlateSize += plateSize;
                    plateSizes.push_back(plateSize);
                }
                // get random index of plate to split
                uint_fast32_t plateIndexToSplit = this->randomSource->randomUniform(0, totalPlateSize - 1);
                std::shared_ptr<Plate> plateToSplit;
                size_t currentTotalSize = 0;
                size_t index = 0;
                bool plateFound = false;
                for (std::vector<std::shared_ptr<Plate>>::iterator plate = this->plates.begin();
                     plate != this->plates.end() && !plateFound;
                     plate++, index++)
                {
                    currentTotalSize += plateSizes[index];
                    if (currentTotalSize > plateIndexToSplit) {
                        plateFound = true;
                        plateToSplit = *plate;
                    }
                }
                
                std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> newPlates = this->splitPlate(plateToSplit);
                this->deletePlate(plateToSplit); // TODO potential memory leak
                this->plates.push_back(newPlates.first);
                this->plates.push_back(newPlates.second);
                
                // clear plate sizes for next run
                plateSizes.clear();
            }
        }
    }
    
    std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> World::splitPlate(std::shared_ptr<Plate> plateToSplit){
        std::pair<std::shared_ptr<Plate>, std::shared_ptr<Plate>> newPlates;
        newPlates.first = std::make_shared<Plate>(this->worldGrid.get()); // large
        newPlates.second = std::make_shared<Plate>(this->worldGrid.get()); // small
        
        newPlates.first->rotationMatrix = plateToSplit->rotationMatrix;
        newPlates.second->rotationMatrix = plateToSplit->rotationMatrix;
        
        // create poles and speeds
        newPlates.first->pole = this->randomSource->getRandomPointUnitSphere();
        newPlates.second->pole = this->randomSource->getRandomPointUnitSphere();
        
        newPlates.first->angularSpeed = this->randomPlateSpeed();
        newPlates.second->angularSpeed = this->randomPlateSpeed();
        
        newPlates.first->densityOffset = this->randomPlateDensityOffset();
        newPlates.second->densityOffset = this->randomPlateDensityOffset();
        
        Vec3 smallCenter, largeCenter1, largeCenter2;
        std::tie(smallCenter, largeCenter1, largeCenter2) = this->getSplitPoints(plateToSplit);
        
        
        // check distances
        float distanceSmall, distanceLarge1, distanceLarge2;
        size_t index = 0;
        for (std::vector<PlateCell>::iterator cell = plateToSplit->cells.begin();
             cell != plateToSplit->cells.end();
             cell++, index++)
        {
            // copy last nearest to both
            newPlates.first->cells[index].lastNearest = cell->lastNearest;
            newPlates.second->cells[index].lastNearest = cell->lastNearest;
            
            distanceSmall = math::squareDistanceBetween3Points(smallCenter, cell->get_vertex()->get_vector());
            // could test distanceSmall in circle known to be closer
            distanceLarge1 = math::squareDistanceBetween3Points(largeCenter1, cell->get_vertex()->get_vector());
            if (distanceSmall > distanceLarge1) {
                // copy rock, subduction
                newPlates.first->cells[index].rock = cell->rock;
                newPlates.first->cells[index].bIsSubducted = cell->bIsSubducted;
            } else {
                distanceLarge2 = math::squareDistanceBetween3Points(largeCenter2, cell->get_vertex()->get_vector());
                if (distanceSmall > distanceLarge2) {
                    // copy rock, subduction
                    newPlates.first->cells[index].rock = cell->rock;
                    newPlates.first->cells[index].bIsSubducted = cell->bIsSubducted;
                } else {
                    // closer to small
                    // copy rock, subduction
                    newPlates.second->cells[index].rock = cell->rock;
                    newPlates.second->cells[index].bIsSubducted = cell->bIsSubducted;
                }
            }
        }
        
        
        return newPlates;
    }
    
    std::tuple<Vec3, Vec3, Vec3> World::getSplitPoints(std::shared_ptr<Plate> plateToSplit){
        const GridVertex* firstVertex = this->getRandomContinentalVertex(plateToSplit);
        
        // get a random neighbor
        const GridVertex* secondVertex = firstVertex->get_neighbors()[this->randomSource->randomUniform(0, firstVertex->get_neighbors().size() - 1)];
        
        // find shared neighbors
        std::vector<const GridVertex*> sharedNeighbors;
        // TODO, could be opitmized to break inner once match found
        for (std::vector<const GridVertex*>::const_iterator testOne = firstVertex->get_neighbors().begin();
             testOne != firstVertex->get_neighbors().end();
             testOne++)
        {
            for (std::vector<const GridVertex*>::const_iterator testTwo = secondVertex->get_neighbors().begin();
                 testTwo != secondVertex->get_neighbors().end();
                 testTwo++)
            {
                if (*testOne == *testTwo) {
                    sharedNeighbors.push_back(*testOne);
                }
            }
        }
        
#ifdef DEBUG
        if (sharedNeighbors.size() == 0) {
            throw "no shared neighbors!!!!!";
        }
#endif
        const GridVertex* thirdVertex = sharedNeighbors[this->randomSource->randomUniform(0, sharedNeighbors.size() - 1)];
        
        return std::make_tuple(firstVertex->get_vector(), secondVertex->get_vector(), thirdVertex->get_vector());
    }
    
    // for splitting only
    const GridVertex* World::getRandomContinentalVertex(std::shared_ptr<Plate> plateToSplit){
        std::vector<size_t> continentalIndicies;
        std::vector<size_t> oceanicIndicies;
        size_t index = 0;
        for (std::vector<PlateCell>::iterator plateCell = plateToSplit->cells.begin();
             plateCell != plateToSplit->cells.end();
             plateCell++, index++)
        {
            if (plateCell->isContinental()) {
                continentalIndicies.push_back(index);
            } else if (!plateCell->isSubducted() && !plateCell->rock.isEmpty()){
                oceanicIndicies.push_back(index);
            }
        }
        
        if (continentalIndicies.size() > 0) {
            return plateToSplit->cells[continentalIndicies[this->randomSource->uniformIndex(0, continentalIndicies.size() - 1)]].get_vertex();
        } else if (oceanicIndicies.size() > 0) {
            return plateToSplit->cells[oceanicIndicies[this->randomSource->uniformIndex(0, oceanicIndicies.size() - 1)]].get_vertex();
        } else {
            // plate has no cells?????
            return plateToSplit->cells[this->randomSource->uniformIndex(0, plateToSplit->cells.size() - 1)].get_vertex();
        }
    }
    
    void World::deletePlate(std::shared_ptr<Plate> plate) {
        std::vector<std::shared_ptr<Plate>>::iterator plateToRemove;
        bool found = false;
        for (std::vector<std::shared_ptr<Plate>>::iterator testPlate = this->plates.begin();
             testPlate != this->plates.end() && !found;
             testPlate++)
        {
            if (*testPlate == plate) {
                found = true;
                plateToRemove = testPlate;
            }
        }
        //this->deletedPlates.push_back(plate);
        this->plates.erase(plateToRemove);
    }
    
/*************** Constructors ***************/
    World::World(Grid *theWorldGrid, std::shared_ptr<Random> random) : worldGrid(theWorldGrid), plates(1), cells(theWorldGrid->verts_size()), randomSource(random){
        size_t count = theWorldGrid->verts_size();
        for (size_t index = 0; index < count;index++)
        {
            this->cells[index].vertex = &(theWorldGrid->get_vertices())[index];
        }
        // set default rock column
        this->divergentOceanicColumn.root.density = 3200.0;
        this->divergentOceanicColumn.root.thickness = 84000.0;
        this->divergentOceanicColumn.oceanic.density = 2890.0;
        this->divergentOceanicColumn.oceanic.thickness = 6000;
        
        // set attributes
        this->attributes.mantleDensity = 3400;
        this->attributes.radius = 6367;
        this->attributes.sealevel = 9620;
        this->attributes.waterDensity = 1026;
        
        // null transfer
        this->momentumTransfer = nullptr;
        
        // age = 0
        this->age = 0;
        
        // distance between cells in simulation
        // curently a multiple of cell small size
        this->cellDistanceMeters = math::distanceBetween3Points(this->worldGrid->get_vertices()[0].get_vector(), this->worldGrid->get_vertices()[0].get_neighbors()[0]->get_vector()) * this->attributes.radius * 1000;
        
        // supercontinent stuff
        this->supercontinentCycleDuration = 0;
        this->supercontinentCycleStartAge = 0;
        this->desiredPlateCount = 6;
        
        // set cell small angle
        // ~ distance on unit sphere
        this->cellSmallAngle = 0.45 * math::distanceBetween3Points(this->worldGrid->get_vertices()[0].get_vector(), this->worldGrid->get_vertices()[0].get_neighbors()[0]->get_vector());
        
        // create and add plates
        this->plates[0] = std::make_shared<Plate>(theWorldGrid);
        this->plates[0]->pole = this->randomSource->getRandomPointUnitSphere();
        this->plates[0]->angularSpeed = this->randomPlateSpeed();
        this->plates[0]->densityOffset = this->randomPlateDensityOffset();
        // set nearest WorldCells
        size_t index = 0;
        for (std::vector<WorldCell>::iterator worldCell = this->cells.begin();
             worldCell != this->cells.end();
             worldCell++, index++)
        {
            this->plates[0]->cells[index].lastNearest = &(*worldCell);
        }
        
    } // World(Grid, Random)
}