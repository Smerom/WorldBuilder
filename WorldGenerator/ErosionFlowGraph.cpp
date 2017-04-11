// --
//  ErosionFlowGraph.cpp
//  WorldGenerator
//


#include "ErosionFlowGraph.hpp"

#include <iostream>

namespace WorldBuilder {
    void MaterialFlowNode::upTreeFlow(){
        if (this->touched == false) {
            // touch self
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
            
            if (totalMaterialMoved - suspendedMaterial > float_epsilon) {
                throw "High percentage of extra material moved!";
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
            throw "eeerorrr";
        }
        return true;
    }
    
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
    }// MaterialFlowGraph::flowAll()
}