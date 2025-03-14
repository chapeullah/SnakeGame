#include <iostream>
#include <windows.h>

int main() {
    if (!SetDllDirectoryA("libs")) {
        std::cerr << "Failed to set DLL path." << std::endl;
        return 1;
    }

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcess("core.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        std::cerr << "Failed to start game: " << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "Game started!" << std::endl;
    return 0;
}
