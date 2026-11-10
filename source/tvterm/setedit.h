#ifndef TVTERM_SETEDIT_H
#define TVTERM_SETEDIT_H

#include "editwnd.h"
#include "settings.h"

#include <string>

// The Settings editor: the editor opened by More... > Edit settings, on
// tvterm.ini. There is at most one; Edit settings brings it to the front (or
// to the current desktop) instead of opening another.
//
// Every 2 seconds (see tick):
// - Changes made outside (another tvterm, another program): an editor without
//   changes reloads the file; with unsaved changes, it asks Reload / Keep.
//   Saving over a file that changed on disk asks Overwrite / Cancel.
// - The editing marker tvterm.ini.editing (pid, host, time): written while
//   the editor has unsaved changes, refreshed every 2 seconds, removed when
//   they are saved, undone or dropped. A marker older than 10 seconds (or of
//   a process that is gone) is stale. While another tvterm has a fresh
//   marker, this editor has a cyan background and says so in its title.
class SettingsEditWindow : public TVTermEditWindow
{
public:

    // The open Settings editor, or null.
    static SettingsEditWindow *instance;

    SettingsEditWindow(const TRect &bounds, const std::string &path) noexcept;

    void handleEvent(TEvent &ev) override;
    Boolean valid(ushort command) override;
    void shutDown() override;
    const char *getTitle(short maxSize) override;
    // Reload (E65): the file on disk becomes the saved text again.
    bool reloadFile() override;

    // Called every 2 seconds (and once when the window opens).
    void tick();

    // The marker's path for the settings file 'path'.
    static std::string markerPath(const std::string &path);

private:

    std::string path;       // the settings file (as the editor has it)
    FileStamp diskStamp;    // the file on disk when it was loaded / saved
    FileStamp seenStamp;    // the last change on disk already handled
    std::string savedText;  // the text when it was loaded / saved
    int otherPid {0};       // another tvterm editing the file (0: none)
    bool ownMarker {false}; // the marker on disk is this window's
    bool busy {false};      // a question is open (tick is not reentrant)
    std::string title;

    // Whether the editor still edits the settings file (not after Save As).
    bool editsSettings() const;
    // Stops being the Settings editor (after Save As to another file).
    void detach();
    std::string text() const;
    bool hasChanges();
    void reloadFromDisk();
    // Save with the check for a newer file on disk.
    bool saveChecked();
    void afterSave();
    void updateMarker(bool changes);
    void removeOwnMarker();
    void setOtherPid(int pid);
};

#endif // TVTERM_SETEDIT_H
