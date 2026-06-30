#pragma once

#include <QLineEdit>

// ============================================================================
// Types.h — 项目共享类型定义
// 被 MotionController 和 Services 层共同引用，避免循环依赖
// ============================================================================

enum Axis
{
    X_AXIS = 0,
    Y_MASTER = 1,
    Y_SLAVE = 2,
    Z_FRONT_AXIS = 3,
    Z_BACK_AXIS = 4
};

struct AxisConfig
{
    int axisNo;
    QLineEdit* stateLineEdit;
    QLineEdit* locateLineEdit;
    QLineEdit* speedLineEdit;
};

enum ScanState
{
    SCAN_IDLE,           // 空闲
    SCAN_MOVE_TO_START,  // 移动到起点（栅格扫查）
    SCAN_FORWARD,        // X正向扫查中（栅格扫查）
    SCAN_REVERSE,        // X反向扫查中（栅格扫查）
    SCAN_STEP,           // Y步进中（栅格扫查）
    SCAN_CONTINUOUS,     // 连续扫查中（X/Y联动斜线）
    SCAN_DONE            // 扫描完成
};
