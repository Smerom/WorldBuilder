// --
//  main.cpp
//  WorldGenerator
//


#include <iostream>

#include <random>
#include <thread>

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

#include "api/Basic.pb.h"
#include "api/Basic.grpc.pb.h"
#include "SimulationRunner.hpp"
#include "BasicGenerator.hpp"
#include "BombardmentGenerator.hpp"
#include "World.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

class WorldBuilderImpl final : public api::WorldBuilder::Service {
public:
    explicit WorldBuilderImpl(std::string theTag){
        this->tag = theTag;
    }
    
    Status GenerateWorld(::grpc::ServerContext* context, ::grpc::ServerReaderWriter< ::api::SimulationInfo, ::api::Grid>* stream) override {
        
        std::cout << "Attempting to build world" << std::endl;
        
        // read the stream until end
        uint32_t gridVertexCount;
        uint32_t currentGridCount = 0;
        api::Grid readGrid;
        
        WorldBuilder::Grid *grid;
        stream->Read(&readGrid);
        if (readGrid.totalverts() == 0) {
            return Status::CANCELLED;
        } else {
            gridVertexCount = readGrid.totalverts();
            grid = new WorldBuilder::Grid(readGrid.totalverts());
            grid->addGrpcGridPart(&readGrid);
            currentGridCount += readGrid.vertices_size();
        }
        // read remaining grid
        while (currentGridCount < gridVertexCount) {
            stream->Read(&readGrid);
            grid->addGrpcGridPart(&readGrid);
            currentGridCount += readGrid.vertices_size();
        }
        
        // finish grid creation
        grid->buildCenters();
        
        std::random_device rd;
        //uint seed = 2202871464; // current test seed
        uint seed = rd(); // get a random seed
        std::printf("Random Seed: %u\n", seed);
        // create the generator
        std::shared_ptr<WorldBuilder::Random> randomSource(new WorldBuilder::Random(seed));
        WorldBuilder::SimulationRunner runner(new WorldBuilder::BombardmentGenerator(randomSource), new WorldBuilder::World(grid, randomSource));
        runner.set_runTimesteps(5);
        runner.set_runMinTimestep(0.05);
        runner.shouldLogRunTiming = false;
        runner.shouldLogRockDelta = true;
        
        bool open = true;
        while (open) {
            
// commented out so the debugger stops on all exceptions
            try {
                runner.Run();
            } catch (const std::exception& e) {
                std::printf("Broken simulation on seed: %u\n", seed);
                std::cout << "Exception: " << e.what() << std::endl;
                break;
            }
        
            api::SimulationInfo info;
            
            std::chrono::time_point<std::chrono::high_resolution_clock> renderStart;
            std::chrono::time_point<std::chrono::high_resolution_clock> renderEnd;
            // split rendering among cores
            renderStart = std::chrono::high_resolution_clock::now();
            auto renderPart = [grid, runner](size_t startIndex, size_t end_index, std::shared_ptr<std::vector<WorldBuilder::LocationInfo>> storage) {
                for (size_t index = startIndex; index < end_index; index++) {
                    WorldBuilder::LocationInfo info = runner.get_world()->get_locationInfo(grid->get_vertices()[index].get_vector());
                    storage->push_back(info);
                }
            };
            
            unsigned int concurentThreadMax = std::thread::hardware_concurrency();
            //concurentThreadMax = 1; // force single threaded
            
            uint32_t vertsPerThread = grid->get_vertices().size() / concurentThreadMax;
            uint32_t leftOverVerts = grid->get_vertices().size() % concurentThreadMax;
            
            
            std::vector<std::shared_ptr<std::vector<WorldBuilder::LocationInfo>>> results;
            std::vector<std::thread> threads;
            
            size_t currentStart = 0;
            for (unsigned int threadIndex = 0; threadIndex < concurentThreadMax; threadIndex++) {
                size_t vertsToDo = vertsPerThread;
                if (threadIndex < leftOverVerts) {
                    vertsToDo++;
                }
                std::shared_ptr<std::vector<WorldBuilder::LocationInfo>> threadResults = std::make_shared<std::vector<WorldBuilder::LocationInfo>>();
                threadResults->reserve(vertsToDo);
                results.push_back(threadResults);
                
                threads.push_back(std::thread(renderPart, currentStart, currentStart + vertsToDo, threadResults));
                
                currentStart += vertsToDo;
            }
            
            // join our threads and get the results to the grpc object
            auto resultsIt = results.begin();
            for (auto threadIt = threads.begin(); threadIt != threads.end(); threadIt++, resultsIt++) {
                threadIt->join();
                // results are now done for this thread, add them
                for (auto&& infoIt = (*resultsIt)->begin(); infoIt != (*resultsIt)->end(); infoIt++) {
                    info.add_elevations(infoIt->elevation);
                    info.add_sediment(infoIt->sediment);
                    info.add_plates(infoIt->plateId);
                    info.add_tempurature(infoIt->tempurature);
                    info.add_precipitation(infoIt->precipitation);
                }
            }
            
//            for (auto vertexIt = grid->get_vertices().begin(); vertexIt != grid->get_vertices().end(); vertexIt++)
//            {
//                float elevation = runner.get_world()->get_elevation(vertexIt->get_vector());
//                //std::cout << elevation << std::endl;
//                if (elevation > 18000) {
//                    //std::cout << elevation << std::endl;
//                }
//                                
//                
//                //info.add_sediment(sedimentHeight);
//                //info.add_plates(plateIndex);
//            }
            renderEnd = std::chrono::high_resolution_clock::now();
            
            std::chrono::duration<double> renderDuration = renderEnd - renderStart;
            
            //std::cout << "Rendering took " << renderDuration.count() << " seconds." << std::endl;
            
            info.set_age(runner.get_world()->get_age());
            info.set_sealevel(runner.get_world()->get_attributes().sealevel);
            open = stream->Write(info);
            
            //open = false; // only one to capture starting state
        }
        
        return Status::OK;
    }
    
private:
    std::string tag;
    
};



int main(int argc, const char * argv[]) {
    std::string server_address("0.0.0.0:18082");
    WorldBuilderImpl service("chicken");
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
    
    return 0;
}
