// --
//  ErosionFlowGraph.cpp
//  WorldGenerator
//


#include "ErosionFlowGraph.hpp"

#include <iostream>
#include <unordered_set>

namespace WorldBuilder {
    
    
/****************************** Material Flow Node ******************************/

    bool compareElevations(const MaterialFlowNode& left, const MaterialFlowNode& right){
        return left.elevation() < right.elevation();
    }
    
    void MaterialFlowNode::upTreeFlow(){
        if (this->touched == false) {
            // touch self (TODO: race condition!!!!!! comp swap it!)
            this->touched = true;
            
            // flow up
            wb_float suspendedMaterial = 0;
            for (auto&& flowEdge : this->inflowTargets)
            {
                flowEdge->source->upTreeFlow();
                // collect material
                suspendedMaterial += flowEdge->materialHeight;
                flowEdge->materialHeight = 0;
            }
            
            // TODO, move to specialized flow node
            // TODO, dynamic sealevel
            if (this->elevation() < 9620 - 300) {
                //
                wb_float fillAmount = 9620 - 300 - this->elevation();
                if (fillAmount > suspendedMaterial) {
                    fillAmount = suspendedMaterial;
                }
                suspendedMaterial -= 0.95*fillAmount;
                this->materialHeight += 0.95*fillAmount;
            }
            
            // add material from self
            suspendedMaterial += this->suspendedMaterialHeight;
            this->suspendedMaterialHeight = 0;
            
            // move material
            wb_float totalMaterialMoved = 0;
            for (auto&& flowEdge : this->outflowTargets)
            {
                flowEdge->materialHeight = flowEdge->weight * suspendedMaterial*0.99;
                totalMaterialMoved += flowEdge->weight * suspendedMaterial*0.99;
            }
            
            
            // remove material, suspended from this cell plus any discrepancy between suspended and total moved, if possible
            wb_float desiredMaterialHeight = this->materialHeight + (suspendedMaterial - totalMaterialMoved);
            if (desiredMaterialHeight < 0) {
                desiredMaterialHeight = 0;
            }
            this->set_materialHeight(desiredMaterialHeight);
        } // end if touched
    }// MaterialFlowNode::upTreeFlow()
    
    bool MaterialFlowNode::checkWeight() const {
        if (this->outflowTargets.size() == 0) {
            return true;
        }
        wb_float total = 0;
        for (auto&& downhillEdge : this->outflowTargets) {
            if (downhillEdge->weight < 0) {
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
                    throw std::logic_error("bad");
                }
            } else {
                nextNode->set_basin(this);
                this->addNode(nextNode);
            }
        }
        
        // check overflow
        if (result.result == Continue) {
            // add all our uphill neighbors
            for (auto&& edge : nextNode->inflowTargets) {
                this->addUpslope(edge->source);
            }
            // test all downhill neighbors
            wb_float outElevation = nextNode->elevation();
            bool overflowFound = false;
            MaterialFlowNode* overflowTarget = nullptr;
            for (auto&& edge : nextNode->outflowTargets) {
                if (this->nodes.find(edge->destination) == this->nodes.end()) {
                    if (edge->destination->elevation() < outElevation) {
                        outElevation = edge->destination->elevation();
                        overflowFound = true;
                        overflowTarget = edge->destination;
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
            this->addNode(node);
            node->set_basin(this);
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
    
    
    
    
/****************************** Material Flow Graph ******************************/

    bool MaterialFlowGraph::checkWeights() const {
        for (size_t index = 0; index < this->nodeCount; index++) {
            if (!this->nodes[index].checkWeight()) {
                return false;
            }
        }
        return true;
    }
    
    wb_float MaterialFlowGraph::totalMaterial() const {
        wb_float total = 0;
        for (size_t index = 0; index < this->nodeCount; index++) {
            total += this->nodes[index].get_materialHeight() + this->nodes[index].get_suspendedMaterialHeight();
        }
        return total;
    }
    
    wb_float MaterialFlowGraph::inTransitOutMaterial() const {
        wb_float total = 0;
        for (size_t index = 0; index < this->nodeCount; index++) {
            for(auto&& outflowNode : this->nodes[index].outflowTargets){
                total += outflowNode->get_materialHeight();
            }
        }
        return total;
    }
    wb_float MaterialFlowGraph::inTransitInMaterial() const {
        wb_float total = 0;
        for (size_t index = 0; index < this->nodeCount; index++) {
            for(auto&& inflowNode : this->nodes[index].inflowTargets){
                total += inflowNode->get_materialHeight();
            }
        }
        return total;
    }
    
    
    void MaterialFlowGraph::flowAll(){
        // clear touched status
        for (size_t nodeIndex = 0; nodeIndex < this->nodeCount; nodeIndex++) {
            this->nodes[nodeIndex].touched = false;
        }
        
        // flow entire graph, could be done from roots???
        for (size_t nodeIndex = 0; nodeIndex < this->nodeCount; nodeIndex++) {
            if (this->nodes[nodeIndex].touched == false) {
                this->nodes[nodeIndex].upTreeFlow();
            }
        }
    } // MaterialFlowGraph::flowAll()
    
    void MaterialFlowGraph::fillBasins(){
        
        // create basins
        for (auto&& node : this->nodes) {
            if (node.outflowTargets.size() == 0) {
                // create a basin with this node
                
                MaterialFlowBasin* basin = new MaterialFlowBasin(node.elevation() - node.get_materialHeight(), node.get_materialHeight(), &node);
                this->activeBasins.push(basin);
                
                // all material is moved to the basin
                node.set_materialHeight(0);
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
                    node.set_materialHeight(node.get_materialHeight() + node.get_basin()->elevation() - node.elevation());
                } else {
                    //throw std::logic_error("node above basin height");
                }
            }
        }
        
    } // MaterialFlowGraph::fillBasins()
}
