#include "builder_template.h"

#include <string.h>
#include <queue>
namespace database
{
    GridLayout::GridLayout(int _width, int _height) {
        width = _width;
        height = _height;
            // std::cout << "DDD" << std::endl;
        layout.resize(width);
        priority.resize(width);

        for (int i = 0; i < width; i++) {
            layout[i].resize(height);
            priority[i].resize(height);
            for (int j = 0; j < height; j++) {
                layout[i][j] = GridContent("NULL", nullptr, 0, 0);
                priority[i][j] = 0;
            }
        }

        totalPins = 0;
    } 

    void GridLayout::setPriority(int prio, int startX, int endX, int startY, int endY) {
        for (int i = startX; i <= endX; i++) {
            for (int j = startY; j <= endY; j++) {
                if (prio > priority[i][j])
                    priority[i][j] = prio;
            }
        }
    }

    int GridLayout::getPriority(int startX, int endX, int startY, int endY) {
        int prio = NO_MODULE;
        for (int i = startX; i <= endX; i++)
            for (int j = startY; j <= endY; j++)
                prio = std::max(prio, priority[i][j]);
        return prio;
    }

    void GridLayout::addModuleTemplate(std::string moduleName, pugi::xml_node moduleInfo, pugi::xml_node archInfo) {
        moduleLibrary[moduleName] = buildModule(moduleName, moduleInfo, archInfo);
        
        int nowPinId = 0;
        std::queue<std::shared_ptr<Module>> q;
        q.push(moduleLibrary[moduleName]);

        while (!q.empty()) {
            auto module = q.front();
            q.pop();
            for (auto it : module->allPorts()) {
                auto port = it.second;
                int width = port->getWidth();
                for (int i = 0; i < width; i++) {
                    port->getPinByIdx(i)->setPinId(nowPinId++);
                }
            }
            for (auto it : module->allSubmodules()) {
                q.push(it.second);
            }
        }
        modulePinNum[moduleName] = nowPinId;

    }

    void buildCoreLayout(std::shared_ptr<GridLayout> layout, int base_x, int base_y, pugi::xml_node coreInfo, pugi::xml_node archInfo) {
        for (auto gridInfo : coreInfo.children("grid")) {
            std::string type = gridInfo.attribute("type").value();
            if (type == "TILE") {
                int width = gridInfo.attribute("width").as_int();
                int height = gridInfo.attribute("height").as_int();

                pugi::xml_node tilesInfo = archInfo.child("tile_blocks");
                for (auto tileInfo : tilesInfo.children("tile")) {
                    layout->addModuleTemplate(tileInfo.attribute("type").value(), tileInfo, archInfo); 
                }
                pugi::xml_node defaultInfo = gridInfo.child("default");
                int dPri = defaultInfo.attribute("pri").as_int();
                layout->setPriority(dPri, base_x,  base_x + width - 1, base_y, base_y + height - 1);
                for (pugi::xml_node regionInfo : gridInfo.children("region")) {
                    layout->setPriority(regionInfo.attribute("pri").as_int(),
                                                    base_x + regionInfo.attribute("start_x").as_int(),
                                                    base_x + regionInfo.attribute("end_x").as_int(),
                                                    base_y + regionInfo.attribute("start_y").as_int(),
                                                    base_y + regionInfo.attribute("end_y").as_int());
                } 
                for (pugi::xml_node instInfo : gridInfo.children("inst")) {
                    layout->setPriority(instInfo.attribute("pri").as_int(),
                                                    base_x + instInfo.attribute("start_x").as_int(),
                                                    base_x + instInfo.attribute("start_x").as_int(),
                                                    base_x + instInfo.attribute("end_x").as_int(),
                                                    base_x + instInfo.attribute("end_x").as_int());
                }
                

                pugi::xml_node dTileInfo = getTile(defaultInfo.attribute("type").value(), archInfo);
                int dWidth = dTileInfo.attribute("width").as_int();
                int dHeight = dTileInfo.attribute("height").as_int();
                for (int i = base_x; i < base_x + width; i += dWidth)
                    for (int j = base_y; j < base_y + height; j += dHeight) {
                        if(layout->getPriority(i, i + dWidth - 1, j, j + dHeight - 1) == dPri) {
                            layout->addContent("DEFAULT[" + std::to_string(i) + "][" + std::to_string(j) + "]", dTileInfo.attribute("type").value(), dWidth, dHeight, i, j);
                        }
                    }

                for (pugi::xml_node regionInfo : gridInfo.children("region")) {
                    //std::cout << "dd" << std::endl;
                    int pri = regionInfo.attribute("pri").as_int();
                    int start_x = base_x + regionInfo.attribute("start_x").as_int();
                    int end_x = base_x + regionInfo.attribute("end_x").as_int();
                    int start_y = base_y + regionInfo.attribute("start_y").as_int();
                    int end_y = base_y + regionInfo.attribute("end_y").as_int();

                    std::string tileType = regionInfo.attribute("type").value();
                    std::string tileName = regionInfo.attribute("name").value();
                    // std::cout << tileName << ' ' << tileType << ' ' << pri << ' ' << start_x << ' ' << end_x << ' ' << start_y <<' ' << end_y << std::endl;
                    pugi::xml_node tileInfo = getTile(tileType, archInfo);
                    int tileWidth = tileInfo.attribute("width").as_int();
                    int tileHeight = tileInfo.attribute("height").as_int();

                    for (int i = start_x; i <= end_x; i += tileWidth)
                        for (int j = start_y; j <= end_y; j += tileHeight) {
                            if (layout->getPriority(i, i + tileWidth - 1, j, j + tileHeight - 1) == pri) {
                                std::string name = tileInfo.attribute("type").value();
                                layout->addContent(name + "[" + std::to_string(i) + "][" + std::to_string(j) + "]", tileInfo.attribute("type").value(), tileWidth, tileHeight, i, j);
                            }
                        }
                }

                for (pugi::xml_node instInfo : gridInfo.children("inst")) {
                    int pri = instInfo.attribute("pri").as_int();
                    int start_x = base_x + instInfo.attribute("start_x").as_int();
                    int end_x = base_x + instInfo.attribute("end_x").as_int();

                    std::string tileType = instInfo.attribute("type").value();
                    pugi::xml_node tileInfo = getTile(tileType, archInfo);
                    int tileWidth = tileInfo.attribute("width").as_int();
                    int tileHeight = tileInfo.attribute("height").as_int();

                    if (layout->getPriority(start_x, start_x + tileWidth - 1, end_x, end_x + tileHeight - 1) == pri) {
                        layout->addContent("INST[" + std::to_string(start_x) + "][" + std::to_string(end_x) + "]", tileType, tileWidth, tileHeight, start_x, end_x);
                    }
                }

            }
        }
    }

    std::shared_ptr<GridLayout> buildGridLayout(pugi::xml_node archInfo) {
        
        pugi::xml_node chipGrid = archInfo.child("chip").child("grid");
        int gridCols = chipGrid.attribute("cols").as_int();
        int gridRows = chipGrid.attribute("rows").as_int();

        std::vector<int> baseX(gridCols + 1, 0), baseY(gridRows + 1, 0);

        for (auto core : chipGrid.children("core")) {
            int x = core.attribute("x").as_int();
            int y = core.attribute("y").as_int();
            int w = core.attribute("width").as_int();
            int h = core.attribute("height").as_int();

            baseX[x+1] = std::max(baseX[x+1], w);
            baseY[y+1] = std::max(baseY[y+1], h);
        }

        for (int i=0; i<gridCols; i++) {
            baseX[i + 1] += baseX[i];
        }
        for (int i=0; i<gridRows; i++) {
            baseY[i + 1] += baseY[i];
        }

        std::shared_ptr<GridLayout> layout = std::shared_ptr<GridLayout>(new GridLayout(baseX[gridCols], baseY[gridRows]));
        pugi::xml_node cores = archInfo.child("cores");

        for (auto core : chipGrid.children("core")) {

            int x = core.attribute("x").as_int();
            int y = core.attribute("y").as_int();
            auto type = core.attribute("type").value();

            pugi::xml_node coreInfo;

            for (auto core_ : cores.children("core")) {
                if (strcmp(core_.attribute("name").value(), type) == 0) {
                    coreInfo = core_;
                    break;
                }
            }

            buildCoreLayout(layout, baseX[x], baseY[y], coreInfo, archInfo);
        }

        return layout;
    }
} // namespace database

