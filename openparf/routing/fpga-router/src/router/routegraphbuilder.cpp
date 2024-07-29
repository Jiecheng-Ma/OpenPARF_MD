#include "routegraphbuilder.h"
#include "database/port.h"

namespace router {

    bool isCrossDie(int x1, int y1, int x2, int y2) {
        return y1 / 120 != y2 / 120;  // TODO
        // return false;
    }

    int RouteGraphBuilder::globalGraphMaxWidth = 0;

    std::shared_ptr<RouteGraph> RouteGraphBuilder::run(pugi::xml_node archInfo) {
        int width = gridLayout->getwidth(), height = gridLayout->getHeight();
        gswVertexId.resize(width * height);
        std::shared_ptr<RouteGraph> graph(new RouteGraph(width, height, gridLayout->getTotalPins()));

        for (int i = 0; i < width; i++)
            for (int j = 0; j < height; j++) {
                // std::cout << "BUILDING Grid " << i << ' ' << j << std::endl;
                if (gridLayout->getContent(i, j).gridModule == nullptr) continue;
                // std::cout << gridLayout->getContent(i, j).gridModule->getName() << ' ' << gridLayout->getModulePinNum(gridLayout->getContent(i, j).gridModule->getName()) << std::endl;
                graph->vertexId[i * height + j].resize(gridLayout->getModulePinNum(gridLayout->getContent(i, j).gridModule->getName()));
                buildGraphRecursive(gridLayout->getContent(i, j).gridModule, graph, i, j, gridLayout->getContent(i, j).width, gridLayout->getContent(i, j).height);
                addConnectRecursive(gridLayout->getContent(i, j).gridModule, graph, i, j);
            }
        // exit(0);
        graph->globalGraph = std::make_shared<GlobalRouteGraph> (width, height);
        
        auto sllPort = gridLayout->getModuleLibrary()["LAGLAG"]->getPort("sll");
        int sllPortWidth = sllPort->getWidth();
        for (auto sll : archInfo.child("chip").child("sll_connections").children()) {
            int x1 = sll.attribute("x1").as_int();
            int y1 = sll.attribute("y1").as_int();
            int x2 = sll.attribute("x2").as_int();
            int y2 = sll.attribute("y2").as_int();
            
            for (int i = 0; i < sllPortWidth; i++) {
                auto pin = sllPort->getPinByIdx(i);
                int pinId1 = graph->getVertexId(x1, y1, pin);
                int pinId2 = graph->getVertexId(x2, y2, pin);
                graph->globalGraph->addEdge(x1, y1, x2, y2);
                graph->globalGraph->addEdge(x2, y2, x1, y1);
                graph->addEdge(pinId1, pinId2, 100 * baseCost);  // Delay 还没定义，先不加delay
                graph->addEdge(pinId2, pinId1, 100 * baseCost);
            } 
            // addsll(gridLayout->getContent(x1, y1).gridModule, gridLayout->getContent(x2, y2).gridModule, graph,x1, y1, x2, y2);
        }

        for (int i = 0; i < width; i++)
            for (int j = 0; j < height; j++) {
                // std::cout << "Adding GSW Interconnection " << i << ' ' << j << std::endl; 
                if (gridLayout->getContent(i, j).gridModule == nullptr) continue;
                int gWidth = gridLayout->getContent(i, j).width;
                int gHeight = gridLayout->getContent(i, j).height;
                auto module = gridLayout->getContent(i, j).gridModule;
                for (int k = 0; k < gWidth; k++)
                    for (int l = 0; l < gHeight; l++) {
                        int idx = k * gWidth + l;
                        if (idx >= module->getGSWSize()) continue;
                        auto gsw = module->getGSWbyIdx(idx);
                        for (auto it : gsw->allPorts()) {
                            auto port = it.second;
                            for (int pinId = 0; pinId < port->getWidth(); pinId++) {
                                // if (pinId >= 8) continue;
                                auto pin = port->getPinByIdx(pinId);
                                // std::cout << pin->getName() << ' ' << pin->getGSWConnectDirection() << ' ' << pin->getGSWConnectLength()  << ' ' << pin->getGSWConnectPin() << std::endl;
                                // getchar();
                                if (pin->getGSWConnectLength()) {
                                    int length = pin->getGSWConnectLength();
                                    int lengthCost;
                                    if (length == 1) lengthCost = 50;
                                    else if (length == 2) lengthCost = 60;
                                    else lengthCost = 120;
                                    if (pin->getGSWConnectDirection() == "north") {
                                        if (isCrossDie(i + k, j + l, i + k, j + l + length)) continue;
                                        if (j + l + length < height && gswVertexId[(i + k) * height + j + l + length].find(pin->getGSWConnectPin()) != gswVertexId[(i + k) * height + j + l + length].end()) {
                                            if (j + l + length < height && gswVertexId[(i + k) * height + j + l + length][pin->getGSWConnectPin()] == 0) {
                                                std::cout << "!!!!!!!" << pin->getName() << std::endl;
                                            }
                                            if (pinId < globalGraphMaxWidth)
                                                graph->globalGraph->addEdge(i + k, j + l, i + k, j + l + length);
                                            graph->addEdge(gswVertexId[(i + k) * height + j + l + length][pin->getGSWConnectPin()], graph->getVertexId(i, j, pin), length * baseCost, pin->getGSWConnectDelay());
                                        }
                                    }
                                    if (pin->getGSWConnectDirection() == "south") {
                                        if (isCrossDie(i + k, j + l, i + k, j + l - length)) continue;
                                        if (j + l - length >= 0 && gswVertexId[(i + k) * height + j + l - length].find(pin->getGSWConnectPin()) != gswVertexId[(i + k) * height + j + l - length].end()) {
                                            if (j + l - length >= 0 && gswVertexId[(i + k) * height + j + l - length][pin->getGSWConnectPin()] == 0) {
                                                std::cout << "!!!!!!!" << pin->getName() << std::endl;
                                            }
                                            if (pinId < globalGraphMaxWidth)
                                                graph->globalGraph->addEdge(i + k, j + l, i + k, j + l - length);
                                            graph->addEdge(gswVertexId[(i + k) * height + j + l - length][pin->getGSWConnectPin()], graph->getVertexId(i, j, pin), length * baseCost, pin->getGSWConnectDelay());
                                        }
                                    }
                                    if (pin->getGSWConnectDirection() == "east") {
                                        if (isCrossDie(i + k, j + l, i + k + length, j + l)) continue;
                                        if (i + k + length < width && gswVertexId[(i + k + length) * height + j + l].find(pin->getGSWConnectPin()) != gswVertexId[(i + k + length) * height + j + l].end()) {
                                            if (i + k + length < width && gswVertexId[(i + k + length) * height + j + l][pin->getGSWConnectPin()] == 0) {
                                                std::cout << "!!!!!!!" << pin->getName() << std::endl;
                                            }
                                            if (pinId < globalGraphMaxWidth)
                                                graph->globalGraph->addEdge(i + k, j + l, i + k + length, j + l);
                                            graph->addEdge(gswVertexId[(i + k + length) * height + j + l][pin->getGSWConnectPin()], graph->getVertexId(i, j, pin), length * baseCost, pin->getGSWConnectDelay());
                                        }
                                    }
                                    if (pin->getGSWConnectDirection() == "west") {
                                        if (isCrossDie(i + k, j + l, i + k - length, j + l)) continue;
                                        if (i + k - length >= 0 && gswVertexId[(i + k - length) * height + j + l].find(pin->getGSWConnectPin()) != gswVertexId[(i + k - length) * height + j + l].end()) {
                                            if (i + k - length >= 0 && gswVertexId[(i + k - length) * height + j + l][pin->getGSWConnectPin()] == 0) {
                                                std::cout << "!!!!!!!" << pin->getName() << std::endl;
                                            }
                                            if (pinId < globalGraphMaxWidth)
                                                graph->globalGraph->addEdge(i + k, j + l, i + k - length, j + l);
                                            graph->addEdge(gswVertexId[(i + k - length) * height + j + l][pin->getGSWConnectPin()], graph->getVertexId(i, j, pin), length * baseCost, pin->getGSWConnectDelay());
                                        }
                                    }
                                    if (pin->getGSWConnectDirection() == "CARRY") {
                                        if (j + l - length >= 0 && gswVertexId[(i + k) * height + j + l - length].find(pin->getGSWConnectPin()) != gswVertexId[(i + k) * height + j + l - length].end()) {
                                            if (j + l - length >= 0 && gswVertexId[(i + k) * height + j + l - length][pin->getGSWConnectPin()] == 0) {
                                                std::cout << "!!!!!!!" << pin->getName() << std::endl;
                                            }
                                // std::cout << pin->getName() << ' ' << pin->getGSWConnectDirection() << ' ' << pin->getGSWConnectLength()  << ' ' << pin->getGSWConnectPin() << std::endl;
                                // getchar();
                                            graph->addEdge(gswVertexId[(i + k) * height + j + l - length][pin->getGSWConnectPin()], graph->getVertexId(i, j, pin), length * baseCost);
                                        }
                                    }
                                }
                            }
                        }
                    }
            }
        std::cout << "Build Complete!\nGSW Vertex Num: " << graph->GSWVertexNum << std::endl;
//         graph->reportStatistics();

// #if 1
//         std::cout << "before recycle\n";
//         std::cout << "Mem Peak: " << get_memory_peak() << "M" << std::endl;
//         std::cout << "Mem Curr: " << get_memory_current() << "M" << std::endl;
//         decltype(graph->vertices)().swap(graph->vertices); 
//         decltype(graph->vertexPos)().swap(graph->vertexPos); 
//         decltype(graph->vertexCost)().swap(graph->vertexCost); 

//         std::cout << "after recycle0\n";
//         std::cout << "Mem Peak: " << get_memory_peak() << "M" << std::endl;
//         std::cout << "Mem Curr: " << get_memory_current() << "M" << std::endl;

//         decltype(graph->edges)().swap(graph->edges); 
//         // decltype(graph->edgeCosts)().swap(graph->edgeCosts); 

//         std::cout << "after recycle1\n";
//         std::cout << "Mem Peak: " << get_memory_peak() << "M" << std::endl;
//         std::cout << "Mem Curr: " << get_memory_current() << "M" << std::endl;

//         decltype(graph->vertexId)().swap(graph->vertexId); 
//         decltype(graph->gswVertexId)().swap(graph->gswVertexId); 

//         std::cout << "after recycle2\n";
//         std::cout << "Mem Peak: " << get_memory_peak() << "M" << std::endl;
//         std::cout << "Mem Curr: " << get_memory_current() << "M" << std::endl;
// #endif

        return graph;
    }

    void RouteGraphBuilder::buildGraphRecursive(std::shared_ptr<database::Module> currentModule, std::shared_ptr<RouteGraph> graph, int x, int y, int gWidth, int gHeight) {
        // std::cout << currentModule->getName() << std::endl;
        for (auto it : currentModule->allPorts()) {
            auto port = it.second;
            int width = port->getWidth();
            if (port->getPortType() == database::PortType::INPIN) {
                graph->addVertex(x, y, x + gWidth - 1, y + gHeight - 1, port);
            }
            else 
                for (int i = 0; i < width; i++) {
                    auto pin = port->getPinByIdx(i);
                    // std::cout << pin->getName() << ' ' << pin << std::endl;
                    int v = graph->addVertex(x, y, x + gWidth - 1, y + gHeight - 1, pin);
                    if (pin->getName().find("out[1]") != std::string::npos && pin->getName().find("LUT") != std::string::npos) {
                        graph->addVertexCost(v, 1);
                    }
                    if (pin->getName().find("O1") != std::string::npos && pin->getName().find("LUT") != std::string::npos) {
                        graph->addVertexCost(v, 0.5);
                    }
                    // if (pin->getName().find("o_gsw_switch") != std::string::npos || pin->getName().find("o_fu_byp") != std::string::npos || pin->getName().find("i_lsw_switch") != std::string::npos) {
                    //     graph->addVertexCap(v, 1);
                    // }
                }
        }
        for (auto it : currentModule->allSubmodules()) {
            auto subModule = it.second;
            buildGraphRecursive(subModule, graph, x, y, gWidth, gHeight);
        }
        for (int i = 0; i < gWidth; i++)
            for (int j = 0; j < gHeight; j++) {
                int idx = i * gWidth + j;
                if (idx >= currentModule->getGSWSize()) break;
                auto gsw = currentModule->getGSWbyIdx(idx);
                for (auto it : gsw->allPorts()) {
                    auto name = it.first;
                    auto port = it.second;
                    int width = port->getWidth();
                    for (int k = 0; k < width; k++) {
                        graph->GSWVertexNum++;
                        // graph->setPos(graph->getVertexId(x, y, port->getPinByIdx(k)), x + i, y + j);
                        // if (k <= 6 && k != 0)
                        // graph->addVertexCap(graph->getVertexId(x, y, port->getPinByIdx(k)), 1);
                        gswVertexId[(x + i) * gridLayout->getHeight() + y + j][name + "[" + std::to_string(k) + "]"] = graph->getVertexId(x, y, port->getPinByIdx(k));
                        graph->gswVertexId[(x + i) * gridLayout->getHeight() + y + j].push_back(graph->getVertexId(x, y, port->getPinByIdx(k)));
                    }
                }
            }
    }

    void RouteGraphBuilder::addConnectRecursive(std::shared_ptr<database::Module> currentModule, std::shared_ptr<RouteGraph> graph, int x, int y) {
        for (auto it : currentModule->allPorts()) {
            auto port = it.second;
            int width = port->getWidth();
            for (int i = 0; i < width; i++) {
                auto pin = port->getPinByIdx(i);
                int pinId = graph->getVertexId(x, y, pin);
                int connSize = pin->getConnectSize();
                for (int j = 0; j < connSize; j++) {
                    int connId = graph->getVertexId(x, y, pin->getConnectPinByIdx(j));
                        graph->addEdge(pinId, connId, 0.1 * baseCost, pin->getConnectDelayByIdx(j));
                    // graph->addVertexCost(connId, 1);
                }
            }
        }
        for (auto it : currentModule->allSubmodules()) {
            auto subModule = it.second;
            addConnectRecursive(subModule, graph, x, y);
        }

    }

    void RouteGraphBuilder::addsll(std::shared_ptr<RouteGraph> graph, std::shared_ptr<database::Module> mod1,std::shared_ptr<database::Module> mod2,int x1, int y1,int x2,int y2) {
        // std::cout<<currentModule->getName()<<"\n";
        for (auto it : mod1->allPorts()) {
            auto port1 = it.second;
            if(port1->getName().substr(0,10)!="LAGLAG.sll"){
                continue;
            }
            for(auto it2 : mod2->allPorts()){
                auto port2 = it2.second;
                if(port2->getName().substr(0,10)!="LAGLAG.sll"){
                    continue;
                }

                // std::cout<<port->getName()<<"\n";
                int width = port1->getWidth();

                for (int i = 0; i < width; i++) {
                    auto pin1 = port1->getPinByIdx(i);
                    int pinId1 = graph->getVertexId(x1, y1, pin1);
                    auto pin2 = port2->getPinByIdx(i);
                    int pinId2 = graph->getVertexId(x2, y2, pin2);

                    std::cout<< pinId1 << " " << pinId2 << std::endl;

                    graph->globalGraph->addEdge(x1, y1, x2, y2);
                    graph->globalGraph->addEdge(x2, y2, x1, y1);
                    graph->addEdge(pinId1, pinId2, 100 * baseCost);  // Delay 还没定义，先不加delay
                    graph->addEdge(pinId2, pinId1, 100 * baseCost);
                    // if(x1==10 && y1==119){
                    // std::cout<<"SLL:"<<"\n";
                    // std::cout<<pinId1<<"\n";}

                    // graph->addVertexCost(connId, 1);                
                }             
            }
        }
    }
}
