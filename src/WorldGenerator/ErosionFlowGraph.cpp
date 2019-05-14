// --
//  ErosionFlowGraph.cpp
//  WorldGenerator
//


#include "ErosionFlowGraph.hpp"

#include <iostream>
#include <unordered_set>

namespace WorldBuilder {
    
    // const wb_float basinThicknessThreshold = 1000;
    
/****************************** Material Flow Node ******************************/

    bool compareElevations(const MaterialFlowNode& left, const MaterialFlowNode& right){
        return left.elevation() < right.elevation();
    }
    
    void MaterialFlowNode::upTreeFlow(wb_float sealevel, wb_float timestep){
        if (this->touched == false) {
            // touch self (TODO: race condition!!!!!! comp swap it!)
            this->touched = true;
            
            // flow up
            wb_float suspendedMaterial = 0;
            wb_float waterVolume = 0;
            for (auto&& flowEdge : this->inflowTargets)
            {
                flowEdge->source->upTreeFlow(sealevel, timestep);
                // collect material
                suspendedMaterial += flowEdge->materialHeight;
                waterVolume += flowEdge->waterVolume;
                flowEdge->materialHeight = 0;
            }
            
            // if underwater, deposit on shelf
            wb_float elev = this->elevation();
            if (elev < sealevel - 300) {
                //
                wb_float fillAmount = sealevel - 300 - this->elevation();
                if (fillAmount > suspendedMaterial) {
                    fillAmount = suspendedMaterial;
                }
                suspendedMaterial -= 0.95*fillAmount;
                this->source->rock.sediment.set_thickness(0.95*fillAmount + this->source->rock.sediment.get_thickness());
            } else {
                // add self
                waterVolume += this->source->precipitation * timestep * 0.3; // 30% precipitation as runnoff, random guess
                wb_float sedThick = this->source->rock.sediment.get_thickness();
                wb_float sedSuspended = 0;
                if (elev - sedThick < sealevel - 300) {
                    sedSuspended = elev - sealevel + 300;
                    sedThick = sedThick - sedSuspended;
                } else {
                    sedSuspended = sedThick;
                    sedThick = 0;
                }
                suspendedMaterial += sedSuspended;
                this->source->rock.sediment.set_thickness(sedThick);

                // erode any bedrock
                // only if downhill
                if (this->downhillSlope > 0) {
                    wb_float slope = this->downhillSlope;
                    // max out water erosion slope
                    if (slope > 0.25) {
                        slope = 0.25;
                    }
                    wb_float waterDepth = waterVolume * timestep;
                    // calculated capacity of amazon basin is: .002 meters of sediment per meter of water
                    // but how close to carrying capacity is that?
                    // 0.0006666666667 slope (244 meters of 366km)
                    // 0.00000044444444444 squared slope?
                    // calls for a K of 4500 if at capacity (for mf = 1, nf = 2)
                    // estimate twice that at 10^4
                    const wb_float capFactor = 10000;
                    const wb_float rateFactor = 0.001 * capFactor;

                    // capacity is not timestep dependent (same function as rate)
                    wb_float capacity = capFactor * waterDepth * slope * slope;

                    // of remaining capacity:
                    
                    if (suspendedMaterial - capacity < 0 && capacity != 0) {
                        wb_float waterFrac = waterDepth * suspendedMaterial / capacity;
                        // rate = constantFactor * (waterVolume)^mf * (downhillSlope)^nf
                        // TODO: should be exponential in timestep?
                        wb_float rate = rateFactor * std::sqrt(waterFrac) * slope;
                        // cap the rate
                        if (rate > 1000) {
                            rate = 1000;
                        }
                        wb_float bedrockDepth = rate * timestep;

                        // erode?
                        auto rockSegmentEroded = this->source->erodeThickness(bedrockDepth);
                        suspendedMaterial += rockSegmentEroded.get_thickness();
                    } else {
                        // deposit
                        wb_float depositAmount = (suspendedMaterial - capacity);
                        this->source->rock.sediment.set_thickness(depositAmount);
                        suspendedMaterial -= depositAmount;
                    }
                } else {
                    // deposit all?
                    this->source->rock.sediment.set_thickness(suspendedMaterial);
                    suspendedMaterial = 0;
                }
            }
            
            // move material
            wb_float totalMaterialMoved = 0;
            for (auto&& flowEdge : this->outflowTargets)
            {
                flowEdge->materialHeight = flowEdge->weight * suspendedMaterial*0.99;
                totalMaterialMoved += flowEdge->weight * suspendedMaterial*0.99;
                // move water with 20% evaporation
                flowEdge->waterVolume = flowEdge->weight * waterVolume * 0.8;
            }
            
            
            // add back any missed material height
            wb_float desiredThickness = this->source->rock.sediment.get_thickness() + (suspendedMaterial - totalMaterialMoved);
            if (desiredThickness < 0) {
                desiredThickness = 0;
            }
            this->source->rock.sediment.set_thickness(desiredThickness);
        } // end if touched
    }// MaterialFlowNode::upTreeFlow()
    
    bool MaterialFlowNode::checkWeight() const {
        if (this->outflowTargets.size() == 0) {
            return true;
        }
        wb_float total = 0;
        for (auto&& downhillEdge : this->outflowTargets) {
            if (downhillEdge->weight < 0 && std::isfinite(downhillEdge->weight)) {
                std::cout << "Weight is: " << downhillEdge->weight << std::endl;
                throw std::logic_error("Negative edge weight!");
            }
            total += downhillEdge->weight;
        }
        if (std::abs(total - 1) > float_epsilon) {
            std::cout << "Total weight is: " << total << std::endl;
            throw std::logic_error("eeerorrr");
        }
        return true;
    }
    
    MaterialFlowBasin* MaterialFlowNode::downhillBasin() const {
        if (this->basin != nullptr) {
            return this->basin;
        }
        
        wb_float lowestElevation = std::numeric_limits<wb_float>::max();
        MaterialFlowNode* lowestTarget = nullptr;
        
        for (auto&& downhillTestTarget : this->outflowTargets) {
            if (downhillTestTarget->destination->elevation() < lowestElevation) {
                lowestElevation = downhillTestTarget->destination->elevation();
                lowestTarget = downhillTestTarget->destination;
            }
        }
        
        if (lowestTarget == nullptr) {
            throw std::logic_error("Basin not constructed");
        } else {
            return lowestTarget->downhillBasin();
        }
    }
    
    
    
    
    
/****************************** MaterialFlowBasin ******************************/
    
    BasinFillingResult MaterialFlowBasin::fillNext(){
        BasinFillingResult result;
        result.result = Continue;
        result.collisionBasin = nullptr;
        result.overflowStart = nullptr;;
        result.overflowThickness = 0;
        
        // return filled if no more thickness remaining
        if (this->remainingThickness <= 0 || this->upslopeCandidates.size() == 0) {
            result.result = Filled;
            return result;
        }
        
        // get lowest upslope
        MaterialFlowNode* nextNode = *this->upslopeCandidates.begin();
        wb_float volumeToSelf = (nextNode->elevation() - this->currentElevation) * this->nodes.size();
        
        // modify current thickness
        if (volumeToSelf > this->remainingThickness) {
            result.result = Filled;
            wb_float addedElevation = this->remainingThickness / this->nodes.size();
            this->remainingThickness = 0;
            this->currentElevation += addedElevation;
            
        } else {
            if (volumeToSelf < 0) {
                this->remainingThickness = this->remainingThickness - (this->currentElevation - nextNode->elevation());
                // elevation remains unchanged
            } else {
                this->remainingThickness = this->remainingThickness - volumeToSelf;
                this->currentElevation = nextNode->elevation();
            }
            
            if (nextNode->get_basin() != nullptr) {
                result.result = Collision;
                result.collisionBasin = nextNode->get_basin();
                if (result.collisionBasin == this) {

                    std::cout << std::endl;
                    std::cout << std::endl;
                    std::cout << "EXEPTION!!!" << std::endl;
                    std::cout << std::endl;
                    std::cout << "Basin Size: " << this->nodes.size() << " Uphill count: " << this->upslopeCandidates.size() << std::endl;
                    std::cout << "Logging nodes:" << std::endl;
                    for(auto node : this->nodes){
                        node->log();
                    }
                    std::cout << "Logging upslope nodes:" << std::endl;
                    for(auto node : this->upslopeCandidates){
                        node->log();
                    }

                    throw std::logic_error("Next basin returned self");
                }
            } 
        }
        
        // check overflow
        if (result.result == Continue) {
            std::unordered_set<MaterialFlowNode*> uphillNodes;
            std::unordered_set<MaterialFlowNode*> downhillNodes;
            this->addNode(uphillNodes, downhillNodes, nextNode);
            // add all our uphill neighbors
            for (auto&& node : uphillNodes) {
                this->addUpslope(node);
            }
            // test all downhill neighbors
            wb_float outElevation = nextNode->elevation();
            bool overflowFound = false;
            MaterialFlowNode* overflowTarget = nullptr;
            for (auto&& node : downhillNodes) {
                if (this->nodes.find(node) == this->nodes.end()) {
                    if (node->elevation() < outElevation) {
                        outElevation = node->elevation();
                        overflowFound = true;
                        overflowTarget = node;
                    }
                }
            }
            if (overflowFound) {
                result.overflowThickness = this->remainingThickness;
                this->remainingThickness = 0;
                result.result = Overflow;
                result.overflowStart = overflowTarget;
            }
        }
        
        return result;
    }
    
    void MaterialFlowBasin::merge(WorldBuilder::MaterialFlowBasin* absorbedBasin){
        
        if (this == absorbedBasin) {
            throw std::logic_error("Can't merge with self");
        }
        
        // merge nodes into this
        for (auto&& node : absorbedBasin->nodes) {
            this->addSingleNode(node);
        }
        absorbedBasin->nodes.clear();
        
        // merge candidates into this, but only if not in upslope candidates
        for (auto&& node : absorbedBasin->upslopeCandidates) {
            this->addUpslope(node);
        }
        absorbedBasin->upslopeCandidates.clear();
        
        // merge thicknesses
        this->remainingThickness += absorbedBasin->remainingThickness;
        absorbedBasin->remainingThickness = 0;
    }


    void MaterialFlowBasin::addEqualNodes(std::unordered_set<MaterialFlowNode*> &equalNodes, MaterialFlowNode* testNode){
        equalNodes.insert(testNode);
        for (auto eqNode : testNode->equalNodes) {
            if (equalNodes.find(eqNode) == equalNodes.end()){
                this->addEqualNodes(equalNodes, eqNode);
            }
        }
    };

    void MaterialFlowBasin::addNode(std::unordered_set<MaterialFlowNode*> &uphillNodes, std::unordered_set<MaterialFlowNode*> &downhillNodes, MaterialFlowNode* node) {
        std::unordered_set<MaterialFlowNode*> equalNodes;

        this->addEqualNodes(equalNodes, node);

        for (auto nNode : equalNodes) {
            nNode->set_basin(this);
            // remove from upslope if needed
            this->upslopeCandidates.erase(nNode);
            // add to nodes
            this->nodes.insert(nNode);
        }

        for (auto nNode : equalNodes) {
            for (auto upslopeEdge : nNode->inflowTargets) {
                if (upslopeEdge->source->get_basin() != this) {
                    uphillNodes.insert(upslopeEdge->source);
                }
            }
            for (auto downslopeEdge : nNode->outflowTargets) {
                if (downslopeEdge->destination->get_basin() != this) {
                    downhillNodes.insert(downslopeEdge->destination);
                }
            }
        }
    }
    
    
    
    
/****************************** Material Flow Graph ******************************/

    bool MaterialFlowGraph::checkWeights() const {
        for (size_t index = 0; index < this->nodeCount; index++) {
            if (!this->nodes[index].checkWeight()) {
                return false;
            }
        }
        return true;
    }
    
    
    void MaterialFlowGraph::flowAll(wb_float sealevel, wb_float timestep){
        // clear touched status
        for (size_t nodeIndex = 0; nodeIndex < this->nodeCount; nodeIndex++) {
            this->nodes[nodeIndex].touched = false;
        }
        
        // flow entire graph, could be done from roots???
        for (size_t nodeIndex = 0; nodeIndex < this->nodeCount; nodeIndex++) {
            if (this->nodes[nodeIndex].touched == false) {
                this->nodes[nodeIndex].upTreeFlow(sealevel, timestep);
            }
        }
    } // MaterialFlowGraph::flowAll()
    
    void MaterialFlowGraph::fillBasins(){
        
        // create basins
        for (auto&& node : this->nodes) {
            if (node.outflowTargets.size() == 0) {
                // create a basin with this node
                
                MaterialFlowBasin* basin = new MaterialFlowBasin(node.elevation() - node.sedimentHeight(), node.sedimentHeight(), &node);
                this->activeBasins.push(basin);
                
                // all material is moved to the basin
                node.set_sedimentHeight(0);
            }
        }
        
        // while we have basins left to calculate
        while (this->activeBasins.size() > 0) {
            // get the top one
            MaterialFlowBasin* lowestBasin = this->activeBasins.top();
            this->activeBasins.pop();
            
            wb_float nextElevation;
            if (this->activeBasins.size() > 0) {
                nextElevation = this->activeBasins.top()->elevation();
            } else {
                nextElevation = std::numeric_limits<wb_float>::max();
            }
            
            bool done = false;
            bool completed = false;
            while (!done) {
                BasinFillingResult fillResult = lowestBasin->fillNext();
                switch (fillResult.result) {
                    case Continue:
                        if (lowestBasin->elevation() > nextElevation) {
                            done = true;
                        }
                        break;
                    case Filled:
                        done = true;
                        completed = true;
                        break;
                    case Overflow: {
                        // find the next basin, remove from inactive, add to active
                        MaterialFlowBasin* targetBasin = fillResult.overflowStart->downhillBasin();
                        
                        // move from inactive to active, and add the overflow thickness
                        targetBasin->addThickness(fillResult.overflowThickness);
                        this->activeBasins.push(targetBasin);
                        this->inactiveBasins.erase(targetBasin);
                        
                        done = true;
                        completed = true;
                        break;
                    }
                    case Collision:
                        // merge
                        lowestBasin->merge(fillResult.collisionBasin);
                        
                        // check elevation
                        if (lowestBasin->elevation() > nextElevation) {
                            done = true;
                        }
                        break;
                }
            }
            
            if (!completed) {
                this->activeBasins.push(lowestBasin);
            } else {
                this->inactiveBasins.insert(lowestBasin);
            }
        }
        
        // set material for all basin nodes
        for(auto&& node : this->nodes) {
            if (node.get_basin() != nullptr) {
                if (node.elevation() <= node.get_basin()->elevation()) {
                    node.set_sedimentHeight(node.sedimentHeight() + node.get_basin()->elevation() - node.elevation());
                } else {
                    //throw std::logic_error("node above basin height");
                }
            }
        }
        
    } // MaterialFlowGraph::fillBasins()
}
