#include "ClassLayoutEngine.h"
#include <QHash>
#include <QMap>
#include <qmath.h>
#include <algorithm> // 引入 std::sort
#include <QDebug>

RenderDocument ClassLayoutEngine::layout(const ClassDiagramAst &ast, const RenderTheme &theme)
{
    Q_UNUSED(theme);
    RenderDocument doc;
    
    // 布局常量参数 (专为 2D 象限及拓扑排版定制)
    const double packagePaddingLeft = 30.0;
    const double packagePaddingRight = 30.0;
    const double packagePaddingTop = 55.0; // 顶部留出绘制包标题的空间
    const double packagePaddingBottom = 30.0;
    
    const double classSpacingX = 60.0; // 同一行类卡片之间的横向间距
    const double classSpacingY = 40.0; // 各行之间的纵向间距
    const double memberLineHeight = 18.0;
    const double headerHeight = 35.0;
    
    const double packageSpacingX = 100.0;
    const double packageSpacingY = 100.0;
    const double startX = 60.0;
    const double startY = 60.0;
    
    QHash<QString, QRectF> classRects; // 缓存类卡片的全局 QRectF
    
    bool leftToRight = ast.leftToRight; // 提取是否为从左到右方向
    
    // 0. 首先统计出全局所有的关联节点，用来摆放孤立节点
    QHash<QString, bool> isClassConnected;
    for (const auto &c : ast.classes) {
        isClassConnected[c.id] = false;
    }
    for (const auto &rel : ast.relations) {
        isClassConnected[rel.from] = true;
        isClassConnected[rel.to] = true;
    }
    
    // 1. 归纳每个类所属的 package
    QMap<QString, QVector<ClassDecl>> packageClasses;
    for (const auto &c : ast.classes) {
        packageClasses[c.packageName].append(c);
    }
    
    // 2. 收集所有包，把没有归属包的类追加到一个默认的 Default 包里
    QVector<ClassDiagramAst::PackageDecl> allPackages = ast.packages;
    if (packageClasses.contains("") && !packageClasses[""].isEmpty()) {
        ClassDiagramAst::PackageDecl defaultPkg;
        defaultPkg.id = "";
        defaultPkg.displayName = "Default Layer";
        defaultPkg.color = "#f9fafb";
        allPackages.append(defaultPkg);
    }
    
    // 用于拓扑排序和重心排布的辅助节点结构
    struct LNode {
        ClassDecl decl;
        double width = 160.0;
        double height = 60.0;
        int level = 0;
        double barycenter = 0.0;
        int indexInLevel = 0;
        bool isIsolated = false;
    };
    
    struct PkgLayoutInfo {
        ClassDiagramAst::PackageDecl pkgDecl;
        QVector<LNode> nodes;
        double width = 0.0;
        double height = 0.0;
        int gridX = 0;
        int gridY = 0;
        
        // 用于从左到右
        int numCols = 0;
        QVector<double> colWidths;
        QVector<double> colXOffsets;
        double pkgHeight = 0.0;
        
        // 用于从上到下
        int numRows = 0;
        QVector<double> rowHeights;
        QVector<double> rowYOffsets;
        double pkgWidth = 0.0;
        
        QVector<QVector<int>> levelNodes;
    };
    
    QVector<PkgLayoutInfo> pkgLayouts;
    
    // ==================== 动态计算包之间的拓扑关系以决定网格坐标 ====================
    // 1. 建立类 ID 到包 ID 的映射
    QHash<QString, QString> classToPkg;
    for (const auto &c : ast.classes) {
        classToPkg[c.id] = c.packageName;
    }
    
    // 2. 收集所有含有类成员的包 ID 列表
    QVector<QString> activePkgIds;
    for (const auto &P : allPackages) {
        if (packageClasses.contains(P.id) && !packageClasses[P.id].isEmpty()) {
            activePkgIds.append(P.id);
        }
    }
    
    // 3. 构建包与包之间的有向关系图
    QHash<QString, QVector<QString>> pkgAdj;
    for (const auto &rel : ast.relations) {
        QString fromPkg = classToPkg.value(rel.from, "");
        QString toPkg = classToPkg.value(rel.to, "");
        
        // 当连线跨包时，添加包与包之间的依赖边 (fromPkg -> toPkg)
        if (fromPkg != toPkg && activePkgIds.contains(fromPkg) && activePkgIds.contains(toPkg)) {
            if (rel.direction == "left") {
                if (!pkgAdj[toPkg].contains(fromPkg)) {
                    pkgAdj[toPkg].append(fromPkg);
                }
            } else {
                if (!pkgAdj[fromPkg].contains(toPkg)) {
                    pkgAdj[fromPkg].append(toPkg);
                }
            }
        }
    }
    
    // 4. 计算每个包的入度和出度，用于三级软件架构水平分层
    QHash<QString, int> pkgInDegree;
    QHash<QString, int> pkgOutDegree;
    for (const auto &pkgId : activePkgIds) {
        pkgInDegree[pkgId] = 0;
        pkgOutDegree[pkgId] = 0;
    }
    for (auto it = pkgAdj.begin(); it != pkgAdj.end(); ++it) {
        QString from = it.key();
        for (const QString &to : it.value()) {
            pkgInDegree[to]++;
            pkgOutDegree[from]++;
        }
    }

    // 5. 对包执行 Kahn 拓扑排序计算 Level，作为同层内左右位置的排序依据
    QHash<QString, int> pkgLevels;
    for (const auto &pkgId : activePkgIds) {
        pkgLevels[pkgId] = 0;
    }
    QVector<QString> pkgQueue;
    QHash<QString, int> tempInDegree = pkgInDegree;
    for (const auto &pkgId : activePkgIds) {
        if (tempInDegree[pkgId] == 0) {
            pkgQueue.append(pkgId);
            pkgLevels[pkgId] = 0;
        }
    }
    int pkgHead = 0;
    while (pkgHead < pkgQueue.size()) {
        QString u = pkgQueue[pkgHead++];
        for (const QString &v : pkgAdj[u]) {
            pkgLevels[v] = qMax(pkgLevels[v], pkgLevels[u] + 1);
            tempInDegree[v]--;
            if (tempInDegree[v] == 0) {
                pkgQueue.append(v);
            }
        }
    }
    int maxAssignedLevel = 0;
    for (const auto &pkgId : activePkgIds) {
        maxAssignedLevel = qMax(maxAssignedLevel, pkgLevels[pkgId]);
    }
    for (const auto &pkgId : activePkgIds) {
        if (pkgLevels[pkgId] == 0 && tempInDegree[pkgId] > 0) {
            pkgLevels[pkgId] = maxAssignedLevel + 1;
        }
    }

    // 6. 应用金字塔架构分层规则：
    // - Row 0：入度为 0 且出度大于 0 的包（入口源，如 UI）以及孤立包。
    // - Row 2：出度为 0 且入度大于 0 的包（纯底层实现，如 Infrastructure）（在 leftToRight 下归入 Row 1 以防折返）。
    // - Row 1：中介包。
    QMap<int, QVector<QString>> rowToPkgs;
    for (const auto &pkgId : activePkgIds) {
        int inD = pkgInDegree.value(pkgId, 0);
        int outD = pkgOutDegree.value(pkgId, 0);
        
        // 使用 Kahn 拓扑排序计算出的 pkgLevels 自动且精确地分配包的行号，自适应支持任意层级，无硬编码 Row 0/1/2 限制
        int row = pkgLevels.value(pkgId, 0);
        rowToPkgs[row].append(pkgId);
    }

    // 7. 在各行内根据拓扑 Level 排序分配 Grid 象限坐标，使其在 X 轴上按拓扑流向放置
    QHash<QString, QPoint> pkgGridCoords;
    for (auto it = rowToPkgs.begin(); it != rowToPkgs.end(); ++it) {
        int row = it.key();
        auto pkgs = it.value();
        
        // 按照 Kahn 算出的 Level 升序排列同行的包，保证横向引力对齐
        std::sort(pkgs.begin(), pkgs.end(), [&](const QString &a, const QString &b) {
            return pkgLevels[a] < pkgLevels[b];
        });
        
        for (int i = 0; i < pkgs.size(); ++i) {
            pkgGridCoords[pkgs[i]] = QPoint(i, row); // gridX = 行内顺序, gridY = 行号
        }
    }
    
    // 调试打印拓扑映射结果
    for (auto it = pkgGridCoords.begin(); it != pkgGridCoords.end(); ++it) {
        qDebug() << "[Layout] 拓扑映射:" << it.key() << "->" << it.value();
    }
    
    // 3. 第一阶段排版：计算每个包的内部局部相对定位与包的实际大小 (根据全局流向自适应)
    for (const auto &P : allPackages) {
        if (!packageClasses.contains(P.id) || packageClasses[P.id].isEmpty()) {
            continue;
        }
        
        PkgLayoutInfo info;
        info.pkgDecl = P;
        
        // 动态分配 2D 网格平面坐标，杜绝硬编码
        QPoint coords = pkgGridCoords.value(P.id, QPoint(0, 0));
        info.gridX = coords.x();
        info.gridY = coords.y();
        
        qDebug() << "[Layout] 包:" << P.id << "实际分配 gridX/Y:" << info.gridX << "," << info.gridY;
        
        const auto &classesInPkg = packageClasses[P.id];
        int numInPkg = classesInPkg.size();
        
        // 测量卡片自适应尺寸并归纳是否为孤立节点
        for (int i = 0; i < numInPkg; ++i) {
            const auto &c = classesInPkg[i];
            LNode n;
            n.decl = c;
            
            // 宽度自适应测量
            int maxLen = c.id.length();
            if (c.metaType == "interface") maxLen = qMax(maxLen, 13);
            else if (c.metaType == "enum") maxLen = qMax(maxLen, 15);
            for (const auto &m : c.members) {
                maxLen = qMax(maxLen, m.length());
            }
            n.width = qMax(160.0, maxLen * 7.5 + 30.0);
            
            // 高度自适应测量
            n.height = headerHeight + 8.0;
            if (!c.members.isEmpty()) {
                n.height = headerHeight + 10.0 + c.members.size() * memberLineHeight;
            }
            
            n.isIsolated = !isClassConnected.value(c.id, false);
            info.nodes.append(n);
        }
        
        // 提取参与关系的活动节点列表，用于纯拓扑排版
        QVector<int> activeNodeIndices;
        QHash<QString, int> idToActiveIdx;
        for (int i = 0; i < info.nodes.size(); ++i) {
            if (!info.nodes[i].isIsolated) {
                idToActiveIdx[info.nodes[i].decl.id] = activeNodeIndices.size();
                activeNodeIndices.append(i);
            }
        }
        
        // 仅对活动节点进行有向拓扑排序
        int numActive = activeNodeIndices.size();
        QVector<QVector<int>> adj(numActive);
        QVector<int> inDegree(numActive, 0);
        for (const auto &rel : ast.relations) {
            if (idToActiveIdx.contains(rel.from) && idToActiveIdx.contains(rel.to)) {
                int fromIdx = idToActiveIdx[rel.from];
                int toIdx = idToActiveIdx[rel.to];
                if (rel.direction == "left") {
                    adj[toIdx].append(fromIdx);
                    inDegree[fromIdx]++;
                } else {
                    adj[fromIdx].append(toIdx);
                    inDegree[toIdx]++;
                }
            }
        }
        
        QVector<int> queue;
        for (int i = 0; i < numActive; ++i) {
            if (inDegree[i] == 0) {
                queue.append(i);
                int realIdx = activeNodeIndices[i];
                info.nodes[realIdx].level = 0;
            }
        }
        
        int head = 0;
        while (head < queue.size()) {
            int u = queue[head++];
            for (int v : adj[u]) {
                int realV = activeNodeIndices[v];
                int realU = activeNodeIndices[u];
                info.nodes[realV].level = qMax(info.nodes[realV].level, info.nodes[realU].level + 1);
                inDegree[v]--;
                if (inDegree[v] == 0) {
                    queue.append(v);
                }
            }
        }
        
        int maxActiveLevel = 0;
        for (int idx : activeNodeIndices) {
            maxActiveLevel = qMax(maxActiveLevel, info.nodes[idx].level);
        }
        int activeCount = (numActive > 0) ? (maxActiveLevel + 1) : 0;
        
        // ==================== 根据全局流向进行包内相对排版 ====================
        if (leftToRight) {
            // 1. 从左到右布局 (X 轴分列层级)
            int activeCols = activeCount;
            info.levelNodes.resize(activeCols);
            for (int idx : activeNodeIndices) {
                info.levelNodes[info.nodes[idx].level].append(idx);
            }
            
            // 同列重心启发式排序 (基于连接的左侧节点Y位置排序)
            for (int col = 1; col < activeCols; ++col) {
                for (int nodeIdx : info.levelNodes[col]) {
                    double sumY = 0.0;
                    int count = 0;
                    for (int prevCol = 0; prevCol < col; ++prevCol) {
                        for (int prevNodeIdx : info.levelNodes[prevCol]) {
                            if (idToActiveIdx.contains(info.nodes[prevNodeIdx].decl.id) && 
                                idToActiveIdx.contains(info.nodes[nodeIdx].decl.id)) {
                                int actPrev = idToActiveIdx[info.nodes[prevNodeIdx].decl.id];
                                int actCurr = idToActiveIdx[info.nodes[nodeIdx].decl.id];
                                if (adj[actPrev].contains(actCurr)) {
                                    sumY += info.nodes[prevNodeIdx].indexInLevel;
                                    count++;
                                }
                            }
                        }
                    }
                    info.nodes[nodeIdx].barycenter = (count > 0) ? (sumY / count) : 0.0;
                }
                
                std::sort(info.levelNodes[col].begin(), info.levelNodes[col].end(), [&](int a, int b) {
                    return info.nodes[a].barycenter < info.nodes[b].barycenter;
                });
                
                for (int i = 0; i < info.levelNodes[col].size(); ++i) {
                    info.nodes[info.levelNodes[col][i]].indexInLevel = i;
                }
            }
            
            // 计算列宽与列局部 X 偏移
            info.colWidths.assign(activeCols, 160.0);
            for (int col = 0; col < activeCols; ++col) {
                for (int nodeIdx : info.levelNodes[col]) {
                    info.colWidths[col] = qMax(info.colWidths[col], info.nodes[nodeIdx].width);
                }
            }
            
            info.colXOffsets.assign(activeCols, 0.0);
            double currentX = packagePaddingLeft;
            for (int col = 0; col < activeCols; ++col) {
                info.colXOffsets[col] = currentX;
                currentX += info.colWidths[col] + classSpacingX;
            }
            double activeWidth = currentX - classSpacingX + packagePaddingRight;
            
            // 1) 放置活动节点：基于前驱重心 + 消冲突(Push-apart)一维布局算法
            for (int idx : activeNodeIndices) {
                info.nodes[idx].barycenter = 0.0;
            }
            
            for (int col = 0; col < activeCols; ++col) {
                int nInCol = info.levelNodes[col].size();
                if (nInCol == 0) continue;
                
                QVector<double> idealYs(nInCol, 0.0);
                for (int i = 0; i < nInCol; ++i) {
                    int nodeIdx = info.levelNodes[col][i];
                    QString nodeId = info.nodes[nodeIdx].decl.id;
                    
                    double sumY = 0.0;
                    int count = 0;
                    
                    // 基于所有指向它的左侧前驱节点已计算的 Y 坐标算平均重心
                    for (int prevCol = 0; prevCol < col; ++prevCol) {
                        for (int prevNodeIdx : info.levelNodes[prevCol]) {
                            QString prevId = info.nodes[prevNodeIdx].decl.id;
                            if (idToActiveIdx.contains(prevId) && idToActiveIdx.contains(nodeId)) {
                                int actPrev = idToActiveIdx[prevId];
                                int actCurr = idToActiveIdx[nodeId];
                                if (adj[actPrev].contains(actCurr)) {
                                    sumY += info.nodes[prevNodeIdx].barycenter;
                                    count++;
                                }
                            }
                        }
                    }
                    
                    if (count > 0) {
                        idealYs[i] = sumY / count;
                    } else {
                        // 若无前驱(如第一列)，则顺延放置，以初始排开作为理想位置
                        if (i == 0) {
                            idealYs[i] = packagePaddingTop;
                        } else {
                            int prevNodeIdx = info.levelNodes[col][i - 1];
                            idealYs[i] = info.nodes[prevNodeIdx].barycenter + info.nodes[prevNodeIdx].height + classSpacingY;
                        }
                    }
                }
                
                // 排序，保证在消冲突时依然维持相对重心顺序
                QVector<int> sortedIndices(nInCol);
                for (int i = 0; i < nInCol; ++i) sortedIndices[i] = i;
                std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
                    return idealYs[a] < idealYs[b];
                });
                
                // 一维消冲突 Push-apart
                QVector<double> finalYs(nInCol, 0.0);
                double currentMinY = packagePaddingTop;
                for (int i = 0; i < nInCol; ++i) {
                    int sortedIdx = sortedIndices[i];
                    int nodeIdx = info.levelNodes[col][sortedIdx];
                    double idealY = idealYs[sortedIdx];
                    double y = qMax(currentMinY, idealY);
                    finalYs[sortedIdx] = y;
                    currentMinY = y + info.nodes[nodeIdx].height + classSpacingY;
                }
                
                // 回填 Y 坐标
                for (int i = 0; i < nInCol; ++i) {
                    int nodeIdx = info.levelNodes[col][i];
                    info.nodes[nodeIdx].level = col;
                    info.nodes[nodeIdx].barycenter = finalYs[i];
                }
            }
            
            // 动态求得真实的包活动高度
            double activeHeight = 0.0;
            for (int col = 0; col < activeCols; ++col) {
                for (int nodeIdx : info.levelNodes[col]) {
                    double nodeBottom = info.nodes[nodeIdx].barycenter + info.nodes[nodeIdx].height - packagePaddingTop;
                    activeHeight = qMax(activeHeight, nodeBottom);
                }
            }
            
            // 孤立节点放置在底部 (每行最多 2 个紧凑网格收纳)
            QVector<int> isolatedNodeIndices;
            for (int i = 0; i < info.nodes.size(); ++i) {
                if (info.nodes[i].isIsolated) {
                    isolatedNodeIndices.append(i);
                }
            }
            
            int numIso = isolatedNodeIndices.size();
            int isoCols = 2;
            int isoRows = (numIso + isoCols - 1) / isoCols;
            
            // 动态计算各孤立列的最大宽度和各孤立行的最大高度
            QVector<double> maxIsoColWidths(isoCols, 0.0);
            QVector<double> maxIsoRowHeights(isoRows, 0.0);
            for (int i = 0; i < numIso; ++i) {
                int nodeIdx = isolatedNodeIndices[i];
                int r = i / isoCols;
                int c = i % isoCols;
                maxIsoColWidths[c] = qMax(maxIsoColWidths[c], info.nodes[nodeIdx].width);
                maxIsoRowHeights[r] = qMax(maxIsoRowHeights[r], info.nodes[nodeIdx].height);
            }
            
            double totalIsoWidth = 0.0;
            for (double w : maxIsoColWidths) {
                totalIsoWidth += w;
            }
            if (isoCols > 1) {
                totalIsoWidth += (isoCols - 1) * classSpacingX;
            }
            
            double isoAreaHeight = 0.0;
            for (double h : maxIsoRowHeights) {
                isoAreaHeight += h;
            }
            if (isoRows > 1) {
                isoAreaHeight += (isoRows - 1) * classSpacingY;
            }
            if (numIso > 0) {
                isoAreaHeight += 30.0; // 上部留白与活动类的距离
            }
            
            double totalPkgHeight = packagePaddingTop + activeHeight + packagePaddingBottom + isoAreaHeight;
            double totalPkgWidth = qMax(activeWidth, packagePaddingLeft + totalIsoWidth + packagePaddingRight);
            
            info.width = totalPkgWidth;
            info.height = totalPkgHeight;
            info.pkgHeight = totalPkgHeight;
            info.numRows = activeCols; // 这里临时保存活动列数，便于第三阶段遍历
            
            // 2) 放置孤立节点 (自适应网格，杜绝硬编码 170x80)
            double isoStartX = (totalPkgWidth - totalIsoWidth) / 2.0;
            double isoStartY = packagePaddingTop + activeHeight + 30.0;
            
            QVector<double> isoColXOffsets(isoCols, 0.0);
            double curIsoX = isoStartX;
            for (int c = 0; c < isoCols; ++c) {
                isoColXOffsets[c] = curIsoX;
                curIsoX += maxIsoColWidths[c] + classSpacingX;
            }
            
            QVector<double> isoRowYOffsets(isoRows, 0.0);
            double curIsoY = isoStartY;
            for (int r = 0; r < isoRows; ++r) {
                isoRowYOffsets[r] = curIsoY;
                curIsoY += maxIsoRowHeights[r] + classSpacingY;
            }
            
            for (int i = 0; i < numIso; ++i) {
                int nodeIdx = isolatedNodeIndices[i];
                int r = i / isoCols;
                int c = i % isoCols;
                
                info.nodes[nodeIdx].level = -1; // 孤立标识
                info.nodes[nodeIdx].barycenter = isoRowYOffsets[r]; // Y
                info.nodes[nodeIdx].indexInLevel = isoColXOffsets[c]; // X
            }
            
        } else {
            // 2. 从上到下布局 (Y 轴分行层级，符合传统树形流向)
            // 对完全孤立无线的节点，收拢在最底部，并以每行最多 2 个的紧凑网格收纳
            int startIsoRow = activeCount;
            int isoInRowMax = 2;
            QVector<int> isolatedNodeIndices;
            for (int i = 0; i < info.nodes.size(); ++i) {
                if (info.nodes[i].isIsolated) {
                    int isoIdx = isolatedNodeIndices.size();
                    int isoRow = startIsoRow + isoIdx / isoInRowMax;
                    info.nodes[i].level = isoRow;
                    isolatedNodeIndices.append(i);
                }
            }
            
            info.numRows = startIsoRow + (isolatedNodeIndices.size() + isoInRowMax - 1) / isoInRowMax;
            info.levelNodes.resize(info.numRows);
            for (int i = 0; i < info.nodes.size(); ++i) {
                info.levelNodes[info.nodes[i].level].append(i);
            }
            
            // 初始化 X 坐标
            for (int i = 0; i < info.nodes.size(); ++i) {
                info.nodes[i].indexInLevel = 0.0;
            }
            
            // 逐行计算 X 坐标
            for (int row = 0; row < info.numRows; ++row) {
                int nInRow = info.levelNodes[row].size();
                if (nInRow == 0) continue;
                
                QVector<double> idealXs(nInRow, 0.0);
                for (int i = 0; i < nInRow; ++i) {
                    int nodeIdx = info.levelNodes[row][i];
                    QString nodeId = info.nodes[nodeIdx].decl.id;
                    
                    double sumX = 0.0;
                    int count = 0;
                    
                    // 基于所有指向它的上方前驱节点已计算的 X 坐标算平均重心
                    for (int prevRow = 0; prevRow < row; ++prevRow) {
                        for (int prevNodeIdx : info.levelNodes[prevRow]) {
                            QString prevId = info.nodes[prevNodeIdx].decl.id;
                            if (idToActiveIdx.contains(prevId) && idToActiveIdx.contains(nodeId)) {
                                int actPrev = idToActiveIdx[prevId];
                                int actCurr = idToActiveIdx[nodeId];
                                if (adj[actPrev].contains(actCurr)) {
                                    sumX += info.nodes[prevNodeIdx].indexInLevel;
                                    count++;
                                }
                            }
                        }
                    }
                    
                    if (count > 0) {
                        idealXs[i] = sumX / count;
                    } else {
                        // 若无前驱，则顺延排开以作为理想位置
                        if (i == 0) {
                            idealXs[i] = packagePaddingLeft;
                        } else {
                            int prevNodeIdx = info.levelNodes[row][i - 1];
                            idealXs[i] = info.nodes[prevNodeIdx].indexInLevel + info.nodes[prevNodeIdx].width + classSpacingX;
                        }
                    }
                }
                
                // 按理想重心排序
                QVector<int> sortedIndices(nInRow);
                for (int i = 0; i < nInRow; ++i) sortedIndices[i] = i;
                std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
                    return idealXs[a] < idealXs[b];
                });
                
                // 一维消冲突 Push-apart
                QVector<double> finalXs(nInRow, 0.0);
                double currentMinX = packagePaddingLeft;
                for (int i = 0; i < nInRow; ++i) {
                    int sortedIdx = sortedIndices[i];
                    int nodeIdx = info.levelNodes[row][sortedIdx];
                    double idealX = idealXs[sortedIdx];
                    double x = qMax(currentMinX, idealX);
                    finalXs[sortedIdx] = x;
                    currentMinX = x + info.nodes[nodeIdx].width + classSpacingX;
                }
                
                // 回填局部 X 坐标到 indexInLevel
                for (int i = 0; i < nInRow; ++i) {
                    int nodeIdx = info.levelNodes[row][i];
                    info.nodes[nodeIdx].indexInLevel = finalXs[i];
                }
            }
            
            // 计算行高与行局部 Y 偏移
            info.rowHeights.assign(info.numRows, 60.0);
            for (int row = 0; row < info.numRows; ++row) {
                for (int nodeIdx : info.levelNodes[row]) {
                    info.rowHeights[row] = qMax(info.rowHeights[row], info.nodes[nodeIdx].height);
                }
            }
            
            info.rowYOffsets.assign(info.numRows, 0.0);
            double currentY = packagePaddingTop;
            for (int row = 0; row < info.numRows; ++row) {
                info.rowYOffsets[row] = currentY;
                currentY += info.rowHeights[row] + classSpacingY;
            }
            info.height = currentY - classSpacingY + packagePaddingBottom;
            info.pkgHeight = info.height;
            
            // 计算自适应包宽度 (基于计算出的 X 坐标的真实右侧最大值)
            double maxRowWidth = packagePaddingLeft + packagePaddingRight;
            for (int row = 0; row < info.numRows; ++row) {
                for (int nodeIdx : info.levelNodes[row]) {
                    double nodeRight = info.nodes[nodeIdx].indexInLevel + info.nodes[nodeIdx].width + packagePaddingRight;
                    maxRowWidth = qMax(maxRowWidth, nodeRight);
                }
            }
            info.pkgWidth = maxRowWidth;
            info.width = maxRowWidth;
        }
        
        pkgLayouts.append(info);
    }

    // 提前统计每一行的最大包高度，供重心迭代中的全局 Y 轴偏移估算及结算阶段使用
    QMap<int, double> rowMaxHeights;
    for (const auto &info : pkgLayouts) {
        rowMaxHeights[info.gridY] = qMax(rowMaxHeights[info.gridY], info.height);
    }

    // ==================== 跨包引力全球重心迭代与消冲突算子 (迭代3次以传递引力) ====================
    for (int iter = 0; iter < 3; ++iter) {
        // 1. 动态根据每行包的实际最大高度累加估算各行的全局 Y 坐标偏移，消除 550.0 / 1300.0 的硬编码
        QMap<int, double> rowEstimateYMap;
        double currentEstY = 0.0;
        QVector<int> iterRows;
        for (auto it = rowMaxHeights.begin(); it != rowMaxHeights.end(); ++it) {
            iterRows.append(it.key());
        }
        std::sort(iterRows.begin(), iterRows.end());
        for (int r : iterRows) {
            rowEstimateYMap[r] = currentEstY;
            currentEstY += rowMaxHeights[r] + packageSpacingY;
        }

        QHash<QString, int> classToPkgIdx;
        QHash<QString, int> classToNodeIdx;
        QHash<QString, double> classAbsY; // 缓存每个类的估计全局 Y 坐标
        
        for (int pIdx = 0; pIdx < pkgLayouts.size(); ++pIdx) {
            const auto &info = pkgLayouts[pIdx];
            double estY = rowEstimateYMap.value(info.gridY, 0.0);
            for (int nIdx = 0; nIdx < info.nodes.size(); ++nIdx) {
                QString cId = info.nodes[nIdx].decl.id;
                classToPkgIdx[cId] = pIdx;
                classToNodeIdx[cId] = nIdx;
                classAbsY[cId] = estY + info.nodes[nIdx].barycenter;
            }
        }
        
        // 2. 收集连线，对每个类计算关联节点的 Y 轴吸引重心
        QHash<QString, double> classIdealLocalY;
        for (int pIdx = 0; pIdx < pkgLayouts.size(); ++pIdx) {
            const auto &info = pkgLayouts[pIdx];
            double estY = rowEstimateYMap.value(info.gridY, 0.0);
            QString fromPkg = info.pkgDecl.id;
            
            for (int nIdx = 0; nIdx < info.nodes.size(); ++nIdx) {
                QString cId = info.nodes[nIdx].decl.id;
                
                double sumAbsY = 0.0;
                double sumWeights = 0.0;
                
                // 遍历 AST 中所有关系线进行引力收集 (包内与跨包关联均计算引力，使同一包内具有依赖的类在Y轴靠齐)
                for (const auto &rel : ast.relations) {
                    if (rel.from == cId && classAbsY.contains(rel.to)) {
                        QString toPkg = classToPkg.value(rel.to, "");
                        // 包内关联引力全额计算 (1.0)，跨包关联引力大幅衰减 (0.15)，防止长依赖过度拉伸
                        double weight = (fromPkg == toPkg) ? 1.0 : 0.15;
                        double targetY = classAbsY[rel.to];
                        if (rel.direction == "up") {
                            targetY += 120.0;
                        } else if (rel.direction == "down") {
                            targetY -= 120.0;
                        }
                        sumAbsY += targetY * weight;
                        sumWeights += weight;
                    }
                    else if (rel.to == cId && classAbsY.contains(rel.from)) {
                        QString toPkg = classToPkg.value(rel.from, "");
                        double weight = (fromPkg == toPkg) ? 1.0 : 0.15;
                        double targetY = classAbsY[rel.from];
                        if (rel.direction == "up") {
                            targetY -= 120.0;
                        } else if (rel.direction == "down") {
                            targetY += 120.0;
                        }
                        sumAbsY += targetY * weight;
                        sumWeights += weight;
                    }
                }
                
                if (sumWeights > 0.0) {
                    double idealAbsY = sumAbsY / sumWeights;
                    double idealLocalY = idealAbsY - estY;
                    // 动态物理高墙限幅：包的高度由其固有内容大小（info.height）决定，
                    // 引力拉扯卡片的最大局部 Y 绝对不能超过包的物理底部 padding 边缘，防止将包撑大或发生垂直撕裂
                    double maxLocalY = info.height - packagePaddingBottom - info.nodes[nIdx].height;
                    classIdealLocalY[cId] = qBound(packagePaddingTop, idealLocalY, qMax(packagePaddingTop, maxLocalY));
                } else {
                    classIdealLocalY[cId] = info.nodes[nIdx].barycenter;
                }
            }
        }
        
        // 3. 应用引力拉扯，在包内重新运行 Push-apart 一维消冲突算法，确保不发生重叠并更新 barycenter
        for (int pIdx = 0; pIdx < pkgLayouts.size(); ++pIdx) {
            auto &info = pkgLayouts[pIdx];
            
            // 我们只对有列分配的活动节点重新进行排序消冲突
            if (leftToRight) {
                int activeCols = info.levelNodes.size();
                for (int col = 0; col < activeCols; ++col) {
                    int nInCol = info.levelNodes[col].size();
                    if (nInCol == 0) continue;
                    
                    // 提取理想 Y
                    QVector<double> idealYs(nInCol, 0.0);
                    for (int i = 0; i < nInCol; ++i) {
                        int nodeIdx = info.levelNodes[col][i];
                        idealYs[i] = classIdealLocalY.value(info.nodes[nodeIdx].decl.id, info.nodes[nodeIdx].barycenter);
                    }
                    
                    // 根据理想 Y 对这一列的所有节点重新排序，防止错位
                    QVector<int> sortedIndices(nInCol);
                    for (int i = 0; i < nInCol; ++i) sortedIndices[i] = i;
                    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
                        return idealYs[a] < idealYs[b];
                    });
                    
                    // 重新运行一维消冲突 Push-apart
                    QVector<double> finalYs(nInCol, 0.0);
                    double currentMinY = packagePaddingTop;
                    for (int i = 0; i < nInCol; ++i) {
                        int sortedIdx = sortedIndices[i];
                        int nodeIdx = info.levelNodes[col][sortedIdx];
                        double idealY = idealYs[sortedIdx];
                        double y = qMax(currentMinY, idealY);
                        finalYs[sortedIdx] = y;
                        currentMinY = y + info.nodes[nodeIdx].height + classSpacingY;
                    }
                    
                    // 回填 Y 坐标
                    for (int i = 0; i < nInCol; ++i) {
                        int nodeIdx = info.levelNodes[col][i];
                        info.nodes[nodeIdx].barycenter = finalYs[i];
                        
                        // 仅在最后一次迭代输出 UI 调试日志
                        if (iter == 2 && info.pkgDecl.id == "UI Layer (用户界面层)") {
                            qDebug() << "[Debug UI Gravity] 类:" << info.nodes[nodeIdx].decl.id 
                                     << "列:" << col 
                                     << "原始Y:" << classIdealLocalY.value(info.nodes[nodeIdx].decl.id, -999)
                                     << "重新Push-apart后Y:" << finalYs[i];
                        }
                    }
                }
            }
            
            // 重新计算包的实际活动高度
            double activeHeight = 0.0;
            if (leftToRight) {
                int activeCols = info.levelNodes.size();
                for (int col = 0; col < activeCols; ++col) {
                    for (int nodeIdx : info.levelNodes[col]) {
                        double nodeBottom = info.nodes[nodeIdx].barycenter + info.nodes[nodeIdx].height - packagePaddingTop;
                        activeHeight = qMax(activeHeight, nodeBottom);
                    }
                }
                // 重新计算包高度
                double totalPkgHeight = packagePaddingTop + activeHeight + packagePaddingBottom;
                
                // 加上孤立节点的高度并动态更新其Y坐标，防止重叠 (同样采用自适应网格，杜绝 170x80)
                QVector<int> isolatedNodeIndices;
                for (int i = 0; i < info.nodes.size(); ++i) {
                    if (info.nodes[i].isIsolated) {
                        isolatedNodeIndices.append(i);
                    }
                }
                int numIso = isolatedNodeIndices.size();
                int isoCols = 2;
                int isoRows = (numIso + isoCols - 1) / isoCols;
                
                QVector<double> maxIsoColWidths(isoCols, 0.0);
                QVector<double> maxIsoRowHeights(isoRows, 0.0);
                for (int i = 0; i < numIso; ++i) {
                    int nodeIdx = isolatedNodeIndices[i];
                    int r = i / isoCols;
                    int c = i % isoCols;
                    maxIsoColWidths[c] = qMax(maxIsoColWidths[c], info.nodes[nodeIdx].width);
                    maxIsoRowHeights[r] = qMax(maxIsoRowHeights[r], info.nodes[nodeIdx].height);
                }
                
                double totalIsoWidth = 0.0;
                for (double w : maxIsoColWidths) {
                    totalIsoWidth += w;
                }
                if (isoCols > 1) {
                    totalIsoWidth += (isoCols - 1) * classSpacingX;
                }
                
                double isoAreaHeight = 0.0;
                for (double h : maxIsoRowHeights) {
                    isoAreaHeight += h;
                }
                if (isoRows > 1) {
                    isoAreaHeight += (isoRows - 1) * classSpacingY;
                }
                if (numIso > 0) {
                    isoAreaHeight += 30.0;
                }
                
                if (numIso > 0) {
                    double isoStartX = (info.width - totalIsoWidth) / 2.0;
                    double isoStartY = packagePaddingTop + activeHeight + 30.0;
                    
                    QVector<double> isoColXOffsets(isoCols, 0.0);
                    double curIsoX = isoStartX;
                    for (int c = 0; c < isoCols; ++c) {
                        isoColXOffsets[c] = curIsoX;
                        curIsoX += maxIsoColWidths[c] + classSpacingX;
                    }
                    
                    QVector<double> isoRowYOffsets(isoRows, 0.0);
                    double curIsoY = isoStartY;
                    for (int r = 0; r < isoRows; ++r) {
                        isoRowYOffsets[r] = curIsoY;
                        curIsoY += maxIsoRowHeights[r] + classSpacingY;
                    }
                    
                    for (int i = 0; i < numIso; ++i) {
                        int nodeIdx = isolatedNodeIndices[i];
                        int r = i / isoCols;
                        int c = i % isoCols;
                        
                        info.nodes[nodeIdx].barycenter = isoRowYOffsets[r]; // 动态推离活动节点！
                        info.nodes[nodeIdx].indexInLevel = isoColXOffsets[c]; // 保持孤立类的横向网格对齐
                    }
                }
                
                info.height = totalPkgHeight + isoAreaHeight;
                info.pkgHeight = info.height;
            }
        }
    }
    
    // ==================== 自适应局部行宽紧凑平铺结算阶段 ====================
    rowMaxHeights.clear();
    for (const auto &info : pkgLayouts) {
        rowMaxHeights[info.gridY] = qMax(rowMaxHeights[info.gridY], info.height);
    }
    
    QVector<int> sortedRows;
    for (auto it = rowMaxHeights.begin(); it != rowMaxHeights.end(); ++it) {
        sortedRows.append(it.key());
    }
    std::sort(sortedRows.begin(), sortedRows.end());
    
    struct PlacedPkg {
        PkgLayoutInfo info;
        double x = 0.0;
        double y = 0.0;
    };
    QVector<PlacedPkg> placedPkgs;
    
    QMap<int, double> rowWidths; // 缓存每行的总包平铺宽度
    
    double curY = startY;
    for (int row : sortedRows) {
        QVector<PkgLayoutInfo> rowPkgs;
        for (const auto &info : pkgLayouts) {
            if (info.gridY == row) {
                rowPkgs.append(info);
            }
        }
        
        // 按照同行内的 gridX (拓扑顺序) 进行升序排列
        std::sort(rowPkgs.begin(), rowPkgs.end(), [](const PkgLayoutInfo &a, const PkgLayoutInfo &b) {
            return a.gridX < b.gridX;
        });
        
        // 局部横向紧凑平摊
        double curX = startX;
        for (const auto &info : rowPkgs) {
            PlacedPkg p;
            p.info = info;
            p.x = curX;
            p.y = curY;
            placedPkgs.append(p);
            
            curX += info.width + packageSpacingX;
        }
        
        // 记录该行纯包平铺总宽度 (除去最左侧 startX 与最右侧多加的 packageSpacingX)
        double rowW = 0.0;
        if (!rowPkgs.isEmpty()) {
            rowW = curX - packageSpacingX - startX;
        }
        rowWidths[row] = rowW;
        
        // 换行，并加间距
        curY += rowMaxHeights[row] + packageSpacingY;
    }
    
    // 找出所有行中的最大包平铺宽度
    double maxRowWidth = 0.0;
    for (auto it = rowWidths.begin(); it != rowWidths.end(); ++it) {
        maxRowWidth = qMax(maxRowWidth, it.value());
    }
    
    // 建立包 ID 到已放置包的快速指针映射，以便高层单包或少包可以定位其所依赖的下游包
    QHash<QString, PlacedPkg*> idToPlaced;
    for (int i = 0; i < placedPkgs.size(); ++i) {
        idToPlaced[placedPkgs[i].info.pkgDecl.id] = &placedPkgs[i];
    }

    // 应用行级整体水平对齐与依赖重心微调 (消除硬编码居中，实现金字塔层级包完美垂直对齐)
    for (auto &p : placedPkgs) {
        int r = p.info.gridY;
        double rWidth = rowWidths.value(r, 0.0);
        
        // 统计当前行的包数量
        int rowPkgCount = 0;
        for (const auto &info : pkgLayouts) {
            if (info.gridY == r) {
                rowPkgCount++;
            }
        }
        
        // 如果当前行只有一个包（例如顶层 UI Layer 只有一个包），我们优先让其水平中心对齐到它所依赖的下游包重心
        if (rowPkgCount == 1) {
            QString pkgId = p.info.pkgDecl.id;
            double sumDownstreamCenterX = 0.0;
            int downstreamCount = 0;
            
            if (pkgAdj.contains(pkgId)) {
                for (const QString &toPkg : pkgAdj[pkgId]) {
                    if (idToPlaced.contains(toPkg)) {
                        PlacedPkg *down = idToPlaced[toPkg];
                        sumDownstreamCenterX += down->x + down->info.width / 2.0;
                        downstreamCount++;
                    }
                }
            }
            
            if (downstreamCount > 0) {
                double idealCenterX = sumDownstreamCenterX / downstreamCount;
                p.x = idealCenterX - p.info.width / 2.0;
                // 确保不越过最左侧的安全边距
                p.x = qMax(startX, p.x);
                continue;
            }
        }
        
        // 常规多包行应用水平居中偏移量
        double offset = (maxRowWidth - rWidth) / 2.0;
        if (offset > 0.0) {
            p.x += offset;
        }
    }
    
    double maxWidth = 0.0;
    double maxHeight = 0.0;
    for (const auto &p : placedPkgs) {
        maxWidth = qMax(maxWidth, p.x + p.info.width + startX);
        maxHeight = qMax(maxHeight, p.y + p.info.height + startY);
    }
    
    // 5. 第三阶段排版：将结算好的各包全局坐标应用 to 渲染模型与子节点中
    for (const auto &p : placedPkgs) {
        double pkgX = p.x;
        double pkgY = p.y;
        const auto &info = p.info;
        
        if (leftToRight) {
            // ==================== 从左到右布局：节点位置应用 ====================
            for (int i = 0; i < info.nodes.size(); ++i) {
                double relX = 0.0;
                double relY = info.nodes[i].barycenter;
                
                if (info.nodes[i].level == -1) {
                    relX = info.nodes[i].indexInLevel;
                } else {
                    int col = info.nodes[i].level;
                    relX = info.colXOffsets[col];
                }
                
                double finalX = pkgX + relX;
                double finalY = pkgY + relY;
                
                if (info.pkgDecl.id == "UI Layer (用户界面层)") {
                    qDebug() << "[Debug UI Final Node] ID:" << info.nodes[i].decl.id 
                             << "pkgY:" << pkgY 
                             << "relY:" << relY 
                             << "finalY:" << finalY;
                }
                
                RenderNode node;
                node.id = info.nodes[i].decl.id;
                node.displayName = info.nodes[i].decl.id;
                node.rect = QRectF(finalX, finalY, info.nodes[i].width, info.nodes[i].height);
                node.members = info.nodes[i].decl.members;
                node.metaType = info.nodes[i].decl.metaType;
                node.kind = RenderNodeKind::ClassBox;
                node.location = info.nodes[i].decl.location;
                
                doc.nodes.append(node);
                classRects[node.id] = node.rect;
            }
        } else {
            // ==================== 从上到下布局：节点位置应用 ====================
            for (int i = 0; i < info.nodes.size(); ++i) {
                double relX = info.nodes[i].indexInLevel;
                double relY = info.rowYOffsets[info.nodes[i].level];
                
                double finalX = pkgX + relX;
                double finalY = pkgY + relY;
                
                RenderNode node;
                node.id = info.nodes[i].decl.id;
                node.displayName = info.nodes[i].decl.id;
                node.rect = QRectF(finalX, finalY, info.nodes[i].width, info.nodes[i].height);
                node.members = info.nodes[i].decl.members;
                node.metaType = info.nodes[i].decl.metaType;
                node.kind = RenderNodeKind::ClassBox;
                node.location = info.nodes[i].decl.location;
                
                doc.nodes.append(node);
                classRects[node.id] = node.rect;
            }
        }
        
        // 生成包背景板
        RenderPackage pkgNode;
        pkgNode.id = info.pkgDecl.id;
        pkgNode.displayName = info.pkgDecl.displayName;
        pkgNode.rect = QRectF(pkgX, pkgY, info.width, info.height);
        pkgNode.color = info.pkgDecl.color;
        doc.packages.append(pkgNode);
    }
    
    // 6. 精确关系连线端口计算 (防穿透与折线多维感)
    for (const auto &rel : ast.relations) {
        if (!classRects.contains(rel.from) || !classRects.contains(rel.to)) {
            continue;
        }
        
        QRectF rectFrom = classRects.value(rel.from);
        QRectF rectTo = classRects.value(rel.to);
        
        QPointF startPt;
        QPointF endPt;
        
        double fromCenterX = rectFrom.x() + rectFrom.width() / 2.0;
        double fromCenterY = rectFrom.y() + rectFrom.height() / 2.0;
        double toCenterX = rectTo.x() + rectTo.width() / 2.0;
        double toCenterY = rectTo.y() + rectTo.height() / 2.0;
        
        double dx = qAbs(fromCenterX - toCenterX);
        double dy = qAbs(fromCenterY - toCenterY);
        
        if (rel.direction == "left") {
            startPt = QPointF(rectFrom.left(), fromCenterY);
            endPt = QPointF(rectTo.right(), toCenterY);
        } else if (rel.direction == "right") {
            startPt = QPointF(rectFrom.right(), fromCenterY);
            endPt = QPointF(rectTo.left(), toCenterY);
        } else if (rel.direction == "up") {
            startPt = QPointF(fromCenterX, rectFrom.top());
            endPt = QPointF(toCenterX, rectTo.bottom());
        } else if (rel.direction == "down") {
            startPt = QPointF(fromCenterX, rectFrom.bottom());
            endPt = QPointF(toCenterX, rectTo.top());
        } else {
            if (dx > dy) {
                // 水平流向占主导
                if (fromCenterX < toCenterX) {
                    startPt = QPointF(rectFrom.right(), fromCenterY);
                    endPt = QPointF(rectTo.left(), toCenterY);
                } else {
                    startPt = QPointF(rectFrom.left(), fromCenterY);
                    endPt = QPointF(rectTo.right(), toCenterY);
                }
            } else {
                // 垂直流向占主导
                if (fromCenterY < toCenterY) {
                    startPt = QPointF(fromCenterX, rectFrom.bottom());
                    endPt = QPointF(toCenterX, rectTo.top());
                } else {
                    startPt = QPointF(fromCenterX, rectFrom.top());
                    endPt = QPointF(toCenterX, rectTo.bottom());
                }
            }
        }
        
        RenderEdge edge;
        edge.fromNodeId = rel.from;
        edge.toNodeId = rel.to;
        edge.startPoint = startPt;
        edge.endPoint = endPt;
        edge.label = rel.text;
        
        if (rel.kind == RelationKind::Inheritance) {
            edge.kind = RenderEdgeKind::Inheritance;
        } else if (rel.kind == RelationKind::Composition) {
            edge.kind = RenderEdgeKind::Composition;
        } else if (rel.kind == RelationKind::Aggregation) {
            edge.kind = RenderEdgeKind::Aggregation;
        } else if (rel.kind == RelationKind::Realization) {
            edge.kind = RenderEdgeKind::Realization;
        } else if (rel.kind == RelationKind::Dependency) {
            edge.kind = RenderEdgeKind::Dependency;
        } else {
            edge.kind = RenderEdgeKind::Association;
        }
        edge.location = rel.location;
        
        doc.edges.append(edge);
    }
    
    doc.width = qMax(600.0, maxWidth);
    doc.height = qMax(400.0, maxHeight);
    
    return doc;
}
