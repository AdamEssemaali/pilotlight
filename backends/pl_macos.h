/*
   pl_macos.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations
// [SECTION] includes
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MACOS_H
#define PL_MACOS_H

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external
typedef struct _plIOApiI plIOApiI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void pl_init_macos               (plIOApiI* ptIoApi);
void pl_cleanup_macos            (void);
void pl_new_frame_macos          (NSView* view);
void pl_update_mouse_cursor_macos(void);
bool pl_macos_procedure          (NSEvent* event, NSView* view);

#endif //PL_MACOS_H