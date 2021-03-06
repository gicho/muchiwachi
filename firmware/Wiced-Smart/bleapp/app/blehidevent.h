/*******************************************************************************
* ------------------------------------------------------------------------------
*
* Copyright (c), 2005 BROADCOM Corp.
*
*          ALL RIGHTS RESERVED
*
********************************************************************************
*
* File Name: hidevent.h
*******************************************************************************/

/** \addtogroup HidEvents HID Events
 *  \ingroup hidapp
 *  The 2042 HID application assumes that there is an event queue that
 *  holds user events until they can be placed in reports and sent over to the
 *  host. For the convenience of applications, the HID application pre-defines
 *  certain common events and a structure for each event. Application are free
 *  to define their own events.
 */
 
/// @{
#ifndef __BLE_HID_EVENT_H_
#define __BLE_HID_EVENT_H_

#include "types.h"
#include "keyscan.h"

/////////////////////////////////////////////////////////////////////////////////
// Types and Defines
/////////////////////////////////////////////////////////////////////////////////

/// Predefined event types 
enum
{
    /// Reserved event type indicating that no event type is acceptable in places where an event type is required
    HID_EVENT_NONE=0,
    
    /// Motion along axis 0
    HID_EVENT_MOTION_AXIS_0,
    
    /// Motion along axis 1
    HID_EVENT_MOTION_AXIS_1,
    
    /// Motion along axis 2
    HID_EVENT_MOTION_AXIS_2,
    
    /// Motion along axis 3
    HID_EVENT_MOTION_AXIS_3,

    /// Motion along 2 axis. Useful for any input with 2 axis. 
    HID_EVENT_MOTION_AXIS_X_Y,

    /// Motion along 2 axis. Useful for a second input with 2 axis, e.g. trackball on a mouse
    HID_EVENT_MOTION_AXIS_A_B,

    /// Change in button state
    HID_EVENT_NEW_BUTTON_STATE,

    /// Change in key state
    HID_EVENT_KEY_STATE_CHANGE,

    /// Event fifo overflow event
    HID_EVENT_EVENT_FIFO_OVERFLOW = 0xfe,

    /// Reserved event type indicating that any event type is acceptable in places where an event type is required
    HID_EVENT_ANY=0xff
};

/// Pack all events to limit the amount of memory used
#pragma pack(1)

/// Basic HID event
typedef PACKED struct
{
    /// Type of event
    BYTE    eventType;

    /// Poll sequence number. Used to identify events from seperate polls
    BYTE    pollSeqn;
}HidEvent;

/// Single axis event structure
typedef PACKED struct
{
    /// Base event info
    HidEvent eventInfo;

    /// Motion along the axis
    INT16   motion;
}HidEventMotionSingleAxis;

/// XY motion event structure
typedef PACKED struct
{
    /// Base event info
    HidEvent eventInfo;

    /// Motion along axis X
    INT16    motionX;
    
    /// Motion along axis Y
    INT16   motionY;
}HidEventMotionXY;

/// AB motion event structure
typedef PACKED struct
{
    /// Base event info
    HidEvent eventInfo;

    /// Motion along axis A
    INT16   motionA;
    
    /// Motion along axis B
    INT16   motionB;
}HidEventMotionAB;

/// Button state change event structure
typedef PACKED struct
{
    /// Base event info
    HidEvent eventInfo;

    /// New state of buttons
    WORD    buttonState;
}HidEventButtonStateChange;

/// Key state change event structure
typedef PACKED struct
{
    /// Base event info
    HidEvent eventInfo;

    /// Key event.  
    KeyEvent keyEvent;
}HidEventKey;

/// Any generic event
typedef PACKED struct
{
  /// Base event info
  HidEvent eventInfo;

  /// The event
  UINT32 anyEvent;
}HidEventAny;

#pragma pack()

/// @}

#endif



