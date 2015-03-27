/*
 * Copyright 2014, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

/** @file
*
* Define functions to access Keyscan driver
*/
#ifndef __KEYSCAN_DRIVER_H__
#define __KEYSCAN_DRIVER_H__

#include "types.h"
#include "gpiodriver.h"

/**  \addtogroup keyscanQueue
 *  \ingroup keyscan
 */

/*! @{ */
/**
* Defines a keyscan driver.
*
* The keyscan interface is practically defined as a queue from the consumer's
* perspective. Key up/down events are seen as a stream coming from the driver.
* In addition the interface provides the user with the ability to reset the
* HW as well as turn keyscanning on/off.
*/

/// Up/down flag
enum
{
    KEY_DOWN = 0x0,
    KEY_UP   = 0x1
};

/// Special event codes used internally by the driver
enum
{
    /// Rollover event generated by the keyscan driver in case of an error (ghost or overflow)
    ROLLOVER = 0xff,

    /// Event returned to indicate the end of a scan cycle.
    END_OF_SCAN_CYCLE = 0xfe
};

/// Keyscan  driver constants
enum
{
    ///Mia keyevent HW FIFO size.
    MIA_KEY_EVENT_FIFO_SIZE              = 20,

    ///keyscan FW FIFO size. This FIFO  is implemented with KeyscanQueue.
    KEYSCAN_FW_FIFO_SIZE                 = (2*MIA_KEY_EVENT_FIFO_SIZE + 6),
};

/// Keyscan circular event queue state.
typedef struct
{
    /// Location where the queue starts. Provided during initialization
    BYTE *bufStart;

    /// Maximum size of elements. Provided during initialization
    BYTE elementSize;

    /// Maximum number of elements that can be placed in the queue. Provided during initialization
    BYTE maxNumElements;

    /// Number of elements currently in the queue
    BYTE curNumElements;

    /// Read index into the queue
    BYTE readIndex;

    /// Write index into the queue
    BYTE writeIndex;

    /// Saved write index for rollback.
    BYTE savedWriteIndexForRollBack;

    /// Saved number of elements for rollback.
    BYTE savedNumElements;
} KeyscanQueueState;

#pragma pack(1)
/// Definition of a key event.
typedef PACKED struct
{
    /// Key code. This is the location in the keyscan matrix that is pressed/released.
    /// May be implemented as ((row * numCols) + col) or ((col * numRows) + row.
    BYTE keyCode;

    /// Reserved bits
    BYTE reserved : 6;

    /// Up/down flag
    BYTE upDownFlag : 1;

    /// Should be toggled for every scan cycle in which a new event is queued.
    /// Use of this flag is optional. If used, it allows the consumer to determine whether
    /// an event is detected in the same scan cycle as the previous event or a different one.
    /// Note that this flag does not indicate any separation in time.
    BYTE scanCycleFlag : 1;
}KeyEvent;
#pragma pack()

/// Initialize the keyscan queue.
/// \param buffer Pointer to a permanently allocated buffer that will be used as
/// the storage area for the circular queue. Use cfa_mm_Sbrk() to allocate in app create.
/// \param elementSize The max size of each element in the allocated keyscan queue.
/// \param maxElements The maximum number of elements that can fit in the allocated queue.
void ksq_init(void* buffer, BYTE elementSize, BYTE maxElements);

/// Flush all elements from the keyscan queue.
void ksq_flush(void);

/// Get the number of elements currently in the queue.
/// \return The number of elements in the queue.
BYTE ksq_getCurNumElements(void);

/// Add the given element (by copying into) to the queue, leaving one slot
/// for an overflow indicator.
/// \param elm Pointer to the buffer that contains the data to be added to the queue.
/// \param len Length of the element being added (can be < max size).
/// \return TRUE if successful; FALSE if there is space for only 1 element in the queue (ele will not be added).
BOOL32 ksq_putExcludeOverflowSlot(void *elm, BYTE len);

/// Add the given element (by copying into) to the queue.
/// \param elm Pointer to the buffer that contains the data to be added to the queue.
/// \param len Length of the element being added (can be < max size).
/// \return TRUE if successful; FALSE there is no space left.
BOOL32 ksq_putIncludeOverflowSlot(void *elm, BYTE len);

/// Get element at the front of the queue.
/// \return Pointer to the element at the front of the queue.
void *ksq_getCurElmPtr(void);

/// Remove the element at the front of the queue.
void ksq_removeCurElement(void);

/// Puts an element into the queue. Does not perform any bound checking.
/// \param elm pointer to the element.
/// \param len number of bytes in element. This number of bytes is copied into the
///    internal storage of the queue. This must be <= the maximum element size
///    specified when the queue was initialized, otherwise the results are undefined.
BOOL32 ksq_put(void *elm, BYTE len);

/// Mark current events to save them when an atomic add is required. This is useful for rolling
/// back what we added in case we encounter errors.
void ksq_markCurrentEventForRollBack (void);

/// Rollback to marked item on error when using atomic adds.
void ksq_rollbackUptoMarkedEvents(void);

/// Put a key event into the keyscan data queue.
/// \param event Pointer to the event that is to be added to the end of the queue.
void ksq_putEvent(KeyEvent *event);

/// Copy over the keyscan event at the front of the queue and delete from the queue.
/// \param event The buffer into which to copy the event at the head.
void ksq_getEvent(KeyEvent *event);

/// Check if the KeyscanQueue is empty.
/// \return 0 if there are elements in the queue; else !0.
INLINE BOOLEAN ksq_isEmpty (void)
{
    return (ksq_getCurNumElements() == 0);
}


/* @} */

/**  \addtogroup keyscan
 *  \ingroup HardwareDrivers
*/
/*! @{ */
/**
* Defines the BCM standard keyscan driver. The   BCM standard keyscan driver
* provides the status and control for the keyscan driver. It provides the
* keyEvents to the interface, maintains the queue behind it. It also supports
* keyscanning turning on or off.
*
*/

/// No event.
enum
{
    /// Keycode value if no key is pressed.
    EVENT_NONE                   = 0xfd
};

/// Internal HW control and status bits.
enum
{
    HW_CTRL_SCAN_CTRL_MASK               = 0x00001,
    HW_CTRL_GHOST_DETECT_MASK            = 0x00004,
    HW_CTRL_INTERRUPT_CTRL_MASK          = 0x00008,
    HW_CTRL_RESET_MASK                   = 0x00010,
    HW_CTRL_RC_EXT_MASK                  = 0x000C0,
    HW_CTRL_ROW_MASK                     = 0x00700,
    HW_CTRL_COL_MASK                     = 0x0F800,
    HW_CTRL_COL_DRIVE_CTRL_MASK          = 0x10000,
    HW_CTRL_ROW_DRIVE_CTRL_MASK          = 0x20000,

    HW_CTRL_SCAN_ENABLE                  = 0x00001,
    HW_CTRL_GHOST_ENABLE                 = 0x00004,
    HW_CTRL_KS_INTERRUPT_ENABLE          = 0x00008,
    HW_CTRL_RESET_ENABLE                 = 0x00010,
    HW_CTRL_CLK_ALWAYS_ON                = 0x40000,
    HW_CTRL_RC_DEFAULT                   = 0x000C0,
    HW_CTRL_ROW_SHIFT                    = 8,
    HW_CTRL_COL_SHIFT                    = 11,
    HW_CTRL_COL_ACTIVE_DRIVE             = 0x10000,
    HW_CTRL_ROW_ACTIVE_DRIVE             = 0x20000,

    HW_CTRL_SCAN_DISABLE                 = 0x00000,
    HW_CTRL_GHOST_DISABLE                = 0x00000,
    HW_CTRL_INTERRUPT_DISABLE            = 0x00000,
    HW_CTRL_RESET_DISABLE                = 0x00000,
    HW_CTRL_CLK_AUTO_CTRL                = 0x00000,
    HW_CTRL_COLUMN_PASSIVE_DRIVE         = 0x00000,
    HW_CTRL_ROW_PASSIVE_DRIVE            = 0x00000
};

/// Keyscan interrupt mask. Internal.
enum
{
  HW_MIA_STATUS_KEYSCAN_INT_SET_MASK   = 0x00040,
 };

/// Keyscan HW status bits. Internal.
enum
{
    HW_LHL_CTRL_CLR_KEYS                 = 0x0002,

    HW_LHL_STATUS_GHOST                  = 0x0008,
    HW_LHL_STATUS_KEY_FIFO_OVERFLOW      = 0x0004,
    HW_LHL_STATUS_KEYCODE                = 0x0002
};

/// Keyscan configuration and status bits.
enum
{
    HW_KEYCODE_MASK                      = 0x000000ff,
    HW_SCAN_CYCLE_MASK                   = 0x40000000,
    HW_KEY_UP_DOWN_MASK                  = (int)0x80000000,

    HW_KEYCODE_SHIFT_COUNT               = 0,
    HW_SCAN_CYCLE_SHIFT_COUNT            = 30,
    HW_KEY_UP_DOWN_SHIFT_COUNT           = 31,


    HW_KEYCODE_GHOST                     = 0xf5,
    HW_KEYCODE_INIT                      = 0xff,

    HW_KEY_UP                            = (int)0x80000000,
    HW_KEY_DOWN                          = 0x00000000
};

enum
{
    CH_SEL_KEYSCAN_ROW_INPUT_50_57      = 0x0000,
    CH_SEL_KEYSCAN_ROW_INPUT_16_23      = 0x0001
};

/// Keyscan event notification registration structure. Internal.
typedef struct KeyscanRegistration
{
    void (*userfn)(void*);
    void *userdata;
    struct KeyscanRegistration *next;
} KeyscanRegistration;

/// Keyscan driver state.
typedef struct
{
    /// Registration chain of the appplication level keyscan interrupt/event handlers
    KeyscanRegistration* keyscanFirstReg;

    /// Reserved space for keyscan data.
    KeyEvent keyscan_events[KEYSCAN_FW_FIFO_SIZE];

    /// Whether HW polling is done from Keyscan 
    BOOL8 keyscan_pollingKeyscanHw;

    // number of key down events that are not yet matched by key up events,
    // which gives the number of keys currently being pressed
    UINT8 keysPressedCount;


} KeyscanState;

/// Keyscan driver initialization function. This allocates
/// space for the FW FIFO for key events and initializes
/// the keyscan HW. This function should be invoked only once
/// before using any keyscan services. Subsequent invocations
/// will invoke undefined behavior.
/// Keyscan driver must be initialized AFTER mia driver is initialized.
void keyscan_init(void);

/// Reset the keyscan HW. Any existing events will be thrown away.
void keyscan_reset(void);

/// Check if there are any pending key events.
/// \return TRUE if there are pending key events, FALSE otherwise
BOOL32 keyscan_eventsPending(void);

/// Get next key event
/// \param event pointer to where the event should be stored
/// \return TRUE if an event is returned, FALSE otherwise
BOOL32 keyscan_getNextEvent(KeyEvent *event);

/// Disables keyscanning and and any associated wakeup
void keyscan_turnOff(void);

/// Enabled keyscanning and and any associated wakeup functionality. Should only be used
/// if keyscanning was disabled via turnOff()
void keyscan_turnOn(void);

/// Register for notification of changes.
/// Once registered, you CAN NOT UNREGISTER; registration is meant to
/// be a startup activity
/// \param userfn points to the function to call when the interrupt
/// comes and matches one of the masks (if you pass a method, it must
/// be static). The function does not need to clear the interrupt
/// status; this will be done by KeyscanDriver::lhlInterrupt(). The
/// function will be called ONCE per interrupt.
/// \param userdata will be passed back to userfn as-is; it can be used to 
/// carry the "this", for example
void keyscan_registerForEventNotification(void (*userfn)(void*), void* userdata);

/// Configures the interrupt source from Keyscan HW and enables event detection. A callback
/// function using keyscan_registerForEventNotification() must have been registered prior to
/// enabling event detection.
void keyscan_enableEventDetection(void);

/// Clears the keys in the HW queue.
void keyscan_flushHwEvents(void);

/// MIA clock freeze notification. Retrieves any pending events from the
/// HW fifo. Internal.
void keyscan_miaFreezeCallBack(void);

/// MIA clock unfreeze notification. Internal.
void keyscan_miaUnfreezeCallBack(void);

/// Enables the interrupts from Keyscan HW. Note that Keyscan interrupt must
/// be configured before being used enabled, else, interrupts will not fire.
void keyscan_enableInterrupt(void);

/// Disables interrupts from the keyscan HW block.
void keyscan_disableInterrupt(void);

/// Thread context interrupt handler. Internal.
void keyscan_ksInterrupt(void);

/// Reenables keyscan after a soft power up. Internal.
void keyscan_restoreActivity(void);

/// Clear the count of unmatched key down events. Internal.
void keyscan_clearKeysPressedCount(void);

/// Get count of unmatched key down events.
UINT8 keyscan_getKeysPressedCount(void);

/// Enable/disable keyscan ghost detection.
void keyscan_enableGhostDetection(BOOL32 enable);

/// Soft Reset the keyscan HW once.
void keyscan_hwResetOnce(void);

/// Initialize the keyscan HE. Internal.
void keyscan_initHW(void);

/// Configure the GPIOs used by the keyscan HW based on keyscan config.
void keyscan_configGpios(void);

/// Retrieve events from the HW fifo and places them in the FW fifo.
/// End of scan cycle events are inserted to separate keys detected
/// in separate scans. This function assumes that MIA clock is frozen.
void keyscan_getEventsFromHWFifo(void);

/* @} */

#endif
