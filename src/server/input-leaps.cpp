/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "inputleap/ServerApp.h"
#include "arch/Arch.h"
#include "base/Log.h"
#include "base/EventQueue.h"

#if WINAPI_MSWINDOWS
#include "MSWindowsServerTaskBarReceiver.h"
#endif

#if SYSAPI_UNIX
#include <chrono>
#include <cstdlib>
#include <signal.h>
#include <thread>
#include <unistd.h>
#endif

namespace inputleap {

#if WINAPI_XWINDOWS || WINAPI_LIBEI || WINAPI_CARBON
CreateTaskBarReceiverFunc createTaskBarReceiver = nullptr;
#endif

#if SYSAPI_UNIX
void startGuiParentMonitor()
{
    const char* parentPidValue = std::getenv("INPUTLEAP_GUI_PARENT_PID");
    if (parentPidValue == nullptr || parentPidValue[0] == '\0') {
        return;
    }

    char* end = nullptr;
    const long parentPid = std::strtol(parentPidValue, &end, 10);
    if (end == parentPidValue || *end != '\0' || parentPid <= 1) {
        return;
    }

    std::thread([parentPid]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (::kill(static_cast<pid_t>(parentPid), 0) != 0) {
                ::kill(::getpid(), SIGTERM);
                return;
            }
        }
    }).detach();
}
#endif

int server_main(int argc, char** argv)
{
#if SYSAPI_WIN32
    // record window instance for tray icon, etc
    ArchMiscWindows::setInstanceWin32(GetModuleHandle(nullptr));
#endif

#ifdef __APPLE__
    /* Silence "is calling TIS/TSM in non-main thread environment" as it is a red
    herring that causes a lot of issues to be filed for the MacOS client/server.
    */
    setenv("OS_ACTIVITY_DT_MODE", "NO", true);
#endif

#if SYSAPI_UNIX
    startGuiParentMonitor();
#endif

    Arch arch;
    arch.init();

    Log log;
    EventQueue events;

    ServerApp app(&events, createTaskBarReceiver);
    int result = app.run(argc, argv);
#if SYSAPI_WIN32
    if (IsDebuggerPresent()) {
        printf("\n\nHit a key to close...\n");
        getchar();
    }
#endif
    return result;
}

} // namespace inputleap

int main(int argc, char** argv)
{
    return inputleap::server_main(argc, argv);
}
