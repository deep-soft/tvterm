#ifndef TVTERM_CMDS_H
#define TVTERM_CMDS_H

#include <tvision/tv.h>

enum : ushort
{
    cmGrabInput = 100,
    cmReleaseInput,
    cmTileCols,
    cmTileRows,
    cmStartSelection,
    cmHistoryMode,
    // Commands that cannot be deactivated.
    cmNewTerm = 2000,
    cmCheckTerminalUpdates,
    cmTerminalUpdated,
    cmGetOpenTerms,
    cmCopySelection,
    cmCancelSelection,
    cmNewEditor,
    cmOpenEditor,
    cmEventViewer,
    cmFindEventViewer, // Broadcast: answered by an open Event Viewer.
    cmLoadProfile,
    cmSaveProfile,
    cmWindowList,
    cmRenameTerm,
    cmEditSettings,
    cmReloadSettings,
    cmAbout,
    cmShowNotes, // From the Event Viewer (infoPtr): show the kept messages.
    cmAlertSettings,
    cmNextAlert,
};

enum : ushort
{
    hcMenu = 1000,
    hcInputGrabbed,
    hcSelecting,
    hcEditor,
    hcHistory,
    hcEventViewer,
};

#endif // TVTERM_CMDS_H
