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
    cmNewLogView,
    cmShowAlerts, // From the Event Viewer (infoPtr): list the active alerts.
    cmViewFile,
    cmSwitchDesktop,  // infoInt: the desktop (1-9).
    cmDesktopMenu,    // Choose the desktop to show.
    cmMoveWindowMenu, // Choose the desktop to move the current window to.
    cmEditorSaveAs,   // Save As of the focused editor.
    cmLoadProfileInEditor, // A profile opened in a tvterm editor.
    // Items of the desktop menus: the desktop is the difference (1-9).
    cmDesktopPick = 2200,
};

enum : ushort
{
    hcMenu = 1000,
    hcInputGrabbed,
    hcSelecting,
    hcEditor,
    hcHistory,
    hcEventViewer,
    hcLogView,
};

#endif // TVTERM_CMDS_H
