#ifndef GAMEELEMENT_H
#define GAMEELEMENT_H

#include <QPoint>
#include <QHash>


enum class SpecialType {
    None = 0,
    LineHorizontal, // 4连：横向激光
    LineVertical,   // 4连：纵向激光
    Bomb,           // 5连(L/T型)：3x3爆炸
    MagicBird       // 5连(直线)：同色全消
};

struct Tile {
    int color = 0; // 0为空，1-5为正常颜色
    SpecialType special = SpecialType::None;
    bool isEmpty() const { return color == 0; }

    // 如果你以后想加动画状态，也可以加在这里
    bool isMatched = false;
};


// 游戏大小标准
namespace GameConfig {
const int TILE_SIZE = 80;   // 单个方块像素大小
const int BOARD_ROWS = 8;
const int BOARD_COLS = 8;
}

// 为 QPoint 实现哈希函数
// 这个函数会将 x 和 y 坐标通过位运算组合成一个唯一的 ID
inline uint qHash(const QPoint &key, uint seed = 0) {
    // 经典的哈希组合算法：将 x 和 y 异或并移位
    // 这样即便坐标交换（如 1,2 和 2,1），生成的哈希值通常也会不同
    return qHash(key.x(), seed) ^ qHash(key.y(), seed);
}


#endif
