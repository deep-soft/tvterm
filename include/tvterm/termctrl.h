#ifndef TVTERM_TERMCTRL_H
#define TVTERM_TERMCTRL_H

#define Uses_TPoint
#include <tvision/tv.h>

#include <tvterm/termemu.h>
#include <tvterm/pty.h>
#include <atomic>
#include <memory>
#include <type_traits>

namespace tvterm
{

class TerminalController
{
public:

    // Returns a new-allocated TerminalController.
    // On error, invokes the 'onError' callback and returns null.
    static TerminalController *create( TPoint size,
                                     TerminalEmulatorFactory &terminalEmulatorFactory,
                                     void (&onError)(const char *reason),
                                     const ClientOptions &options = {} ) noexcept;
    // Takes ownership over 'this'.
    void shutDown() noexcept;

    void sendEvent(const TerminalEvent &event) noexcept;

    bool stateHasBeenUpdated() noexcept;
    bool clientIsDisconnected() noexcept;
    // Returns false if the client is disconnected or if not supported.
    bool getClientProcessInfo(ClientProcessInfo &info) noexcept;

    template <class Func>
    // This method locks a mutex, so reentrance will lead to a deadlock.
    // * 'func' takes a 'TerminalState &' by parameter.
    auto lockState(Func &&func);

    template <class Func>
    // Runs 'func' with exclusive access to the TerminalEmulator, which is
    // otherwise used by the worker thread. For short operations only (e.g.
    // reading text). Must not be invoked from within 'lockState()', since
    // the worker thread locks both in the opposite order.
    // * 'func' takes a 'TerminalEmulator &' by parameter.
    void withEmulator(Func &&func);

private:

    struct TerminalEventLoop;

    PtyMaster ptyMaster;
    Mutex<TerminalState> terminalState;
    TerminalEventLoop &eventLoop;
    TerminalEmulator &terminalEmulator;

    std::atomic<bool> updated {false};
    std::atomic<bool> disconnected {false};

    std::shared_ptr<TerminalController> selfOwningPtr;

    void withEmulatorImpl(void (&)(TerminalEmulator &, void *), void *) noexcept;

    TerminalController(TPoint, TerminalEmulatorFactory &, PtyDescriptor) noexcept;
    ~TerminalController();
};

inline bool TerminalController::stateHasBeenUpdated() noexcept
{
    return updated.exchange(false) == true;
}

inline bool TerminalController::clientIsDisconnected() noexcept
{
    return disconnected;
}

inline bool TerminalController::getClientProcessInfo(ClientProcessInfo &info) noexcept
{
    // The pty is only closed after 'shutDown()', so it is still valid here.
    return !disconnected && ptyMaster.getProcessInfo(info);
}

template <class Func>
inline auto TerminalController::lockState(Func &&func)
{
    return terminalState.lock([&] (auto &state) {
        return func(state);
    });
}

template <class Func>
inline void TerminalController::withEmulator(Func &&func)
{
    struct Thunk
    {
        static void call(TerminalEmulator &emu, void *f)
        {
            (*(typename std::remove_reference<Func>::type *) f)(emu);
        }
    };
    withEmulatorImpl(Thunk::call, (void *) &func);
}

} // namespace tvterm

#endif // TVTERM_TERMCTRL_H
