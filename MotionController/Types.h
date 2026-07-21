#pragma once

#include <QLineEdit>

// ============================================================================
// Types.h - shared project type definitions.
// Used by MotionController and service layers to avoid circular dependencies.
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
    SCAN_IDLE,           // Idle.
    SCAN_MOVE_TO_START,  // Moving to the saved scan start point.
    SCAN_FORWARD,        // Scanning in positive X direction.
    SCAN_REVERSE,        // Scanning in negative X direction.
    SCAN_STEP,           // Stepping Y to the next raster line.
    SCAN_CONTINUOUS,     // Continuous scan with linked X/Y movement.
    SCAN_DONE            // Scan finished.
};
