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
};

enum : ushort
{
    hcMenu = 1000,
    hcInputGrabbed,
    hcSelecting,
    hcEditor,
    hcHistory,
};

#endif // TVTERM_CMDS_H
