#ifndef TVTERM_CRASHLOG_H
#define TVTERM_CRASHLOG_H

#include <string>

// Flight recorder (E95, item 59): on SIGSEGV, SIGILL, SIGBUS, SIGFPE or
// SIGABRT, tvterm writes ~/tvterm-crash-YYYY-MM-DD-hhmmss.txt (in /tmp if
// the home directory cannot be written) with the startup information, the
// window and dialog that had the focus, the last Event Viewer messages and
// the last keys, clicks and commands; then the signal goes on as before
// (default action: core file, exit status 128 + signal).
//
// Everything the report needs is copied in advance into fixed buffers;
// the signal handler only formats numbers and calls open() / write(), so
// it is safe in a crashing process. Text typed into a terminal is not
// recorded (passwords): only "a character".
namespace crashlog
{

// Installs the handlers. Called first thing in main(), before the
// application: tvision installs its own handlers later (they restore the
// terminal) and then calls these.
void install();

// A startup line (version, build, executable, command line, profile at
// start; up to 7) or, with 'loadedProfile', the line of the last Load
// Profile (replaced each time).
void setInfo(const std::string &text, bool loadedProfile = false);

// The focused window and the dialog (or other modal view) open over it,
// empty if none; updated at each event.
void setContext(const std::string &window, const std::string &dialog);

// An Event Viewer message (the last 50 are kept).
void addMessage(const std::string &text);

// A key, click or command (the last 30 are kept).
void addInput(const std::string &text);

} // namespace crashlog

#endif // TVTERM_CRASHLOG_H
