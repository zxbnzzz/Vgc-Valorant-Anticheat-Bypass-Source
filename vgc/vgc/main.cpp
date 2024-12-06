
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <processthreadsapi.h>
#include <algorithm>
#include <signal.h>
#include <Windows.h>
#include "auth.hpp"
#include <string>
#include <chrono>
#include <tlhelp32.h>
#include "skStr.h"
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <processthreadsapi.h>
#include <algorithm>
#include <signal.h>
#include <thread>
#include <chrono>
#include <conio.h>






std::vector<HANDLE> suspendedThreadsGlobal;
std::vector<DWORD> initialThreadIds;
std::vector<DWORD> resumedThreadIds;
std::vector<DWORD> this_thread;


DWORD GetProcessIdByName(const std::wstring& processName) {
    DWORD processId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (processName.compare(pe32.szExeFile) == 0) {
                    processId = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    return processId;
}

void freetype();

BOOL SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege) {
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid)) {
        std::cerr << "LookupPrivilegeValue error: " << GetLastError() << std::endl;
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = bEnablePrivilege ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        std::cerr << "AdjustTokenPrivileges error: " << GetLastError() << std::endl;
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        std::cerr << "The token does not have the specified privilege." << std::endl;
        return FALSE;
    }

    return TRUE;
}

void PrintIdealProcessor(HANDLE hThread) {
    PROCESSOR_NUMBER idealProcessor;
    if (GetThreadIdealProcessorEx(hThread, &idealProcessor)) {
        std::wcout << L"vgc.exe" << std::endl;
    }
    else {
        std::wcerr << L"vgc.exe" << std::endl;
    }
}

std::vector<HANDLE> suspendThreads(const std::wstring& processName, size_t numberOfThreadsToSuspend) {
    std::vector<HANDLE> suspendedThreads;
    suspendedThreadsGlobal.clear();
    initialThreadIds.clear();

    DWORD processId = GetProcessIdByName(processName);
    if (processId != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
        if (hProcess == NULL) {
            std::cerr << "Failed to open process with error: " << GetLastError() << std::endl;
            return suspendedThreads;
        }

        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te32;
            te32.dwSize = sizeof(THREADENTRY32);

            struct ThreadData {
                HANDLE hThread;
                ULONG64 initialCycleData;
                ULONG64 finalCycleData;
                DWORD idealProcessor;
            };

            std::vector<ThreadData> threads;

            if (Thread32First(hThreadSnap, &te32)) {
                do {
                    if (te32.th32OwnerProcessID == processId) {
                        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                        if (hThread) {
                            ULONG64 cycleData = 0;
                            if (QueryThreadCycleTime(hThread, &cycleData)) {
                                PROCESSOR_NUMBER idealProcessor;
                                if (GetThreadIdealProcessorEx(hThread, &idealProcessor)) {
                                    threads.push_back({ hThread, cycleData, 0, idealProcessor.Number });
                                    initialThreadIds.push_back(te32.th32ThreadID);
                                }
                                else {
                                    std::wcerr << L"Failed to get ideal processor." << std::endl;
                                    CloseHandle(hThread);
                                }
                            }
                            else {
                                std::wcerr << L"Failed to query cycle time." << std::endl;
                                CloseHandle(hThread);
                            }
                        }
                        else {
                            std::wcerr << L"Failed to open thread." << std::endl;
                        }
                    }
                } while (Thread32Next(hThreadSnap, &te32));
            }

            Sleep(1000);

            for (auto& thread : threads) {
                if (QueryThreadCycleTime(thread.hThread, &thread.finalCycleData) == 0) {
                    std::wcerr << L"Failed to query cycle time for thread." << std::endl;
                    CloseHandle(thread.hThread);
                    thread.hThread = nullptr;
                }
            }

            std::sort(threads.begin(), threads.end(), [](const ThreadData& a, const ThreadData& b) {
                return (a.finalCycleData - a.initialCycleData) > (b.finalCycleData - b.initialCycleData);
                });

            size_t suspendedCount = 0;
            size_t successCount = 0;
            for (const auto& thread : threads) {
                if (thread.hThread) {
                    if (SuspendThread(thread.hThread) != -1) {
                        suspendedThreads.push_back(thread.hThread);
                        suspendedThreadsGlobal.push_back(thread.hThread);
                        suspendedCount++;
                        if (successCount < 1) {
                            std::wcout << L"Emulating...." << std::endl;
                            successCount++;
                        }
                    }
                    else {
                        std::wcerr << L"Failed to suspend thread." << std::endl;
                        CloseHandle(thread.hThread);
                    }
                    if (suspendedCount >= numberOfThreadsToSuspend) {
                        break;
                    }
                }
            }

            for (const auto& thread : threads) {
                if (thread.hThread && std::find(suspendedThreads.begin(), suspendedThreads.end(), thread.hThread) == suspendedThreads.end()) {
                    CloseHandle(thread.hThread);
                }
            }

            CloseHandle(hThreadSnap);
        }
        else {
            std::wcerr << L"Failed to create thread snapshot." << std::endl;
        }

        CloseHandle(hProcess);
    }
    else {
        std::wcerr << L"Could not find process: " << processName << std::endl;
    }
    return suspendedThreads;
}


void resumeAndResuspendThreads() {

    for (HANDLE hThread : suspendedThreadsGlobal) {
        if (ResumeThread(hThread) != -1) {

        }

    }
    
}


void SuspendThreadsWithNoActiveCycleDelta() {
    DWORD processId = GetProcessIdByName(L"vgc.exe");
    if (processId != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
        if (hProcess == NULL) {
            std::cerr << "Failed to open process with error: " << GetLastError() << std::endl;
            return;
        }

        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te32;
            te32.dwSize = sizeof(THREADENTRY32);

            if (Thread32First(hThreadSnap, &te32)) {
                std::vector<HANDLE> threads;
                std::vector<ULONG64> initialCycleData;
                std::vector<DWORD> threadIds;
                
                do {
                    if (te32.th32OwnerProcessID == processId) {
                        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                        if (hThread) {
                            ULONG64 cycleData = 0;
                            if (QueryThreadCycleTime(hThread, &cycleData)) {
                                threads.push_back(hThread);
                                initialCycleData.push_back(cycleData);
                            }
                            else {
                                std::wcerr << L"Failed to query cycle time for thread." << std::endl;
                            }
                        }
                        else {
                            std::wcerr << L"Failed to open thread." << std::endl;
                        }
                    }
                } while (Thread32Next(hThreadSnap, &te32));

               

                bool messageDisplayed = false;

                for (size_t i = 0; i < threads.size(); ++i) {
                    ULONG64 newCycleData = 0;
                    if (QueryThreadCycleTime(threads[i], &newCycleData)) {
                        if (newCycleData == initialCycleData[i] || (newCycleData - initialCycleData[i] < 10)) {
                            if (SuspendThread(threads[i]) != -1) {
                                if (!messageDisplayed) {
                                    std::wcout << L"Emulating....." << std::endl;
                                    messageDisplayed = true;
                                }
                                suspendedThreadsGlobal.push_back(threads[i]);
                            }
                            else {
                                std::wcerr << L"Failed to suspend thread." << std::endl;
                                CloseHandle(threads[i]);
                            }
                        }
                        else {
                            CloseHandle(threads[i]);
                        }
                    }
                    else {
                        std::wcerr << L"Failed to query cycle time for thread." << std::endl;
                        CloseHandle(threads[i]);
                    }
                }

                CloseHandle(hThreadSnap);
            }
            else {
                std::wcerr << L"Failed to create thread snapshot." << std::endl;
            }
            CloseHandle(hProcess);
        }
        else {
            std::wcerr << L"Failed to create thread snapshot." << std::endl;
        }
    }
    else {
        std::wcerr << L"Could not find process: " << L"vgc.exe" << std::endl;
    }
}

void SuspendThreadsAboveCycleCount(ULONG64 cycleThreshold) {
    DWORD processId = GetProcessIdByName(L"vgc.exe");
    if (processId != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
        if (hProcess == NULL) {
            std::cerr << "Failed to open process with error: " << GetLastError() << std::endl;
            return;
        }

        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te32;
            te32.dwSize = sizeof(THREADENTRY32);

            if (Thread32First(hThreadSnap, &te32)) {
                std::vector<HANDLE> threads;
                std::vector<ULONG64> cycleData;

                do {
                    if (te32.th32OwnerProcessID == processId) {
                        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                        if (hThread) {
                            ULONG64 data = 150000;
                            if (QueryThreadCycleTime(hThread, &data)) {
                                // Print cycle count for debugging
                                

                                if (data > cycleThreshold) {
                                    threads.push_back(hThread);
                                    cycleData.push_back(data);
                                }
                                else {
                                    CloseHandle(hThread); // Close handles for threads below threshold
                                }
                            }
                            else {
                                std::wcerr << L"Failed to query cycle time for thread." << std::endl;
                                CloseHandle(hThread);
                            }
                        }
                        else {
                            std::wcerr << L"Failed to open thread." << std::endl;
                        }
                    }
                } while (Thread32Next(hThreadSnap, &te32));

                bool messageDisplayed = false;

                for (size_t i = 0; i < threads.size(); ++i) {
                    if (SuspendThread(threads[i]) != -1) {
                        if (!messageDisplayed) {
                            
                            messageDisplayed = true;
                        }
                        suspendedThreadsGlobal.push_back(threads[i]);
                    }
                    else {
                        std::wcerr << L"Failed to suspend thread." << std::endl;
                        CloseHandle(threads[i]);
                    }
                }

                CloseHandle(hThreadSnap);
            }
            else {
                std::wcerr << L"Failed to create thread snapshot." << std::endl;
            }
            CloseHandle(hProcess);
        }
        else {
            std::wcerr << L"Failed to create thread snapshot." << std::endl;
        }
    }
    else {
        std::wcerr << L"Could not find process: " << L"vgc.exe" << std::endl;
    }
}

void EnableDebugPrivilege() {
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tkp;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            tkp.PrivilegeCount = 1;
            tkp.Privileges[0].Luid = luid;
            tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL);
        }
        CloseHandle(hToken);
    }
}


void suspendThreadsWithNoActiveCycleDelta() {

    DWORD processId = GetProcessIdByName(L"vgc.exe");
    if (processId != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
        if (hProcess == NULL) {
            std::cerr << "Failed to open process with error: " << GetLastError() << std::endl;
            return;
        }

        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te32;
            te32.dwSize = sizeof(THREADENTRY32);

            if (Thread32First(hThreadSnap, &te32)) {
                std::vector<HANDLE> threads;
                std::vector<ULONG64> initialCycleData;
                std::vector<DWORD> threadIds;

                do {
                    if (te32.th32OwnerProcessID == processId) {
                        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                        if (hThread) {
                            ULONG64 cycleData = 0;
                            if (QueryThreadCycleTime(hThread, &cycleData)) {
                                threads.push_back(hThread);
                                initialCycleData.push_back(cycleData);
                                threadIds.push_back(te32.th32ThreadID);
                            }
                            else {
                                std::cerr << "\033[1;31m[-] Failed Error 1\033[0m" << std::endl;
                            }
                        }
                        else {
                            std::cerr << "\033[1;31m[-] Failed Error 2\033[0m" << std::endl;
                        }
                    }
                } while (Thread32Next(hThreadSnap, &te32));

                Sleep(1000);

                bool messageDisplayed = false;

                for (size_t i = 0; i < threads.size(); ++i) {
                    ULONG64 newCycleData = 0;
                    if (QueryThreadCycleTime(threads[i], &newCycleData)) {
                        if (newCycleData == initialCycleData[i] || (newCycleData - initialCycleData[i] < 10)) {
                            if (SuspendThread(threads[i]) != -1) {
                                if (!messageDisplayed) {

                                    messageDisplayed = true;
                                }
                                CloseHandle(threads[i]);
                                suspendedThreadsGlobal.push_back(threads[i]);
                            }
                            else {
                                CloseHandle(threads[i]); // Close here if suspend fails
                            }
                        }
                        else {
                            CloseHandle(threads[i]); // Close immediately if no action needed
                        }
                    }
                }
            }
        }
        CloseHandle(hThreadSnap); // Close snapshot handle
    }

  

}
        
    


std::vector<HANDLE> suspendLowestCycleDeltaThreads(const std::wstring& processName) {
    std::vector<HANDLE> suspendedThreads;

    DWORD processId = GetProcessIdByName(processName);
    if (processId != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
        if (hProcess == NULL) {
            std::cerr << "Failed" << GetLastError() << std::endl;
            return suspendedThreads;
        }

        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te32;
            te32.dwSize = sizeof(THREADENTRY32);

            struct ThreadData {
                HANDLE hThread;
                ULONG64 initialCycleData;
                ULONG64 finalCycleData;
            };

            std::vector<ThreadData> threads;

            if (Thread32First(hThreadSnap, &te32)) {
                do {
                    if (te32.th32OwnerProcessID == processId) {
                        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                        if (hThread) {
                            ULONG64 cycleData = 0;
                            if (QueryThreadCycleTime(hThread, &cycleData)) {
                                threads.push_back({ hThread, cycleData, 0 });
                            }
                            else {
                                std::wcerr << L"Failed" << std::endl;
                                CloseHandle(hThread);
                            }
                        }
                        else {
                            std::wcerr << L"Failed" << std::endl;
                        }
                    }
                } while (Thread32Next(hThreadSnap, &te32));
            }

            Sleep(1000);

            for (auto& thread : threads) {
                if (QueryThreadCycleTime(thread.hThread, &thread.finalCycleData) == 0) {
                    std::wcerr << L"Failed" << std::endl;
                    CloseHandle(thread.hThread);
                    thread.hThread = nullptr;
                }
            }

            std::sort(threads.begin(), threads.end(), [](const ThreadData& a, const ThreadData& b) {
                return (a.finalCycleData - a.initialCycleData) < (b.finalCycleData - a.initialCycleData);
                });

            size_t suspendedCount = 0;
            for (const auto& thread : threads) {
                if (thread.hThread) {
                    if (SuspendThread(thread.hThread) != -1) {
                        suspendedThreads.push_back(thread.hThread);
                        suspendedThreadsGlobal.push_back(thread.hThread);
                        suspendedCount++;
                    }
                    else {
                        std::wcerr << L"Failed to suspend thread." << std::endl;
                        CloseHandle(thread.hThread);
                    }
                    if (suspendedCount >= 2) {
                        break;
                    }
                }
            }

            for (const auto& thread : threads) {
                if (thread.hThread && std::find(suspendedThreads.begin(), suspendedThreads.end(), thread.hThread) == suspendedThreads.end()) {
                    CloseHandle(thread.hThread);
                }
            }

            CloseHandle(hThreadSnap);
        }
        else {
            std::wcerr << L"Failed to create thread snapshot." << std::endl;
        }

        CloseHandle(hProcess);
    }
    else {
        std::wcerr << L"Could not find process: " << processName << std::endl;
    }
    return suspendedThreads;
}


void SuspendSpecificThreads(DWORD processId) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te;
    te.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == processId) {
                // Add your criteria for suspending threads here
                // For example, suspend threads based on specific thread IDs or states

                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    SuspendThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te));
    }

    CloseHandle(hSnapshot);
}

void showLoadingBar(int duration) {
    const int barWidth = 50;
    std::cout << "\033[1;34m[+] Working\033[0m\n"; // Start of loading with blue text
    std::cout << "[";

    for (int i = 0; i < barWidth; ++i) {
        std::cout << " ";
    }
    std::cout << "]\r["; // Return to beginning of line for loading effect

    for (int i = 0; i < barWidth; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(duration / barWidth));
        std::cout << "+" << std::flush;
    }

    std::cout << "]\n"; // End of the loading bar
}


BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
    case CTRL_CLOSE_EVENT:
        std::system("taskkill /f /im vgc.exe");
        return TRUE;
    default:
        return FALSE;
    }
}

void SetConsoleColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}


void setConsoleColor(WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

bool isWindowOpen(const std::wstring& windowName) {
    HWND hwnd = FindWindowW(NULL, windowName.c_str());
    return hwnd != NULL;
}

void closePopup() {
    // Find the window with the title "VAN: RESTRICTION"
    HWND hwnd = FindWindow(NULL, L"VAN: RESTRICTION");

    if (hwnd) {
        // Hide the window
        ShowWindow(hwnd, SW_HIDE);

       
        
    }
    
}

int main() {
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        std::cerr << "Failed to install console control handler." << std::endl;
        return 1;
    }

    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
        std::cerr << "OpenProcessToken failed: " << GetLastError() << std::endl;
        return 1;
    }

    if (!SetPrivilege(hToken, SE_DEBUG_NAME, TRUE)) {
        std::cerr << "Failed to enable SeDebugPrivilege." << std::endl;
        return 1;
    }

    freetype(); // init font
   
    

            bool messageDisplayed = false; // Flag to ensure message is only displayed once


    
    
        while (true) {
            system(skCrypt("cls"));
            SetConsoleTitleA(skCrypt("[+] Prospect... Online Device"));

            std::cout << "\033[38;5;214m"  // Sets the color to orange
                << R"(
                             _____                               _   
                            |  __ \                             | |  
                            | |__) | __ ___  ___ _ __   ___  ___| |_ 
                            |  ___/ '__/ _ \/ __| '_ \ / _ \/ __| __|
                            | |   | | | (_) \__ \ |_) |  __/ (__| |_ 
                            |_|   |_|  \___/|___/ .__/ \___|\___|\__|
                            | |                 | |
                            |_|                 |_|
)" << "\033[0m\n";
            std::cerr << "\033=========================================================================================================================\033[0m" << std::endl;



            std::cout << "\n [1] Emulate (normal)\n [2] Start game (with bypass)\n [3] 2pc\n > ";
            int option;
            std::cin >> option;

            switch (option) {
            case 0:
                break;

            default:
                std::wcout << L"Invalid option. Try again!\n" << std::endl;
                break;

            case 1:
                system(skCrypt("taskkill /f /im vgtray.exe >nul 2>&1"));
                
                system(skCrypt("taskkill /f /im vgc.exe >nul 2>&1"));
                system(skCrypt("sc start vgc >nul 2>&1"));
                showLoadingBar(2000);
                std::cerr << "\033[1;32m[+] Successfully Emulated\033[0m" << std::endl;
                EnableDebugPrivilege();

               


                system(skCrypt("cls"));

                showLoadingBar(2000);
                system(skCrypt("cls"));

                std::cout << "\033[38;5;214m"  // Sets the color to orange
                    << R"(
                             _____                               _   
                            |  __ \                             | |  
                            | |__) | __ ___  ___ _ __   ___  ___| |_ 
                            |  ___/ '__/ _ \/ __| '_ \ / _ \/ __| __|
                            | |   | | | (_) \__ \ |_) |  __/ (__| |_ 
                            |_|   |_|  \___/|___/ .__/ \___|\___|\__|
                            | |                 | |
                            |_|                 |_|
)" << "\033[0m\n";
                std::cerr << "\033=========================================================================================================================\033[0m" << std::endl;



                std::cerr << "\033[1;32m[+] Successfully Working\033[0m" << std::endl;
                std::cerr << "\033[1;32m[-] Keep window open\033[0m" << std::endl;
                std::cerr << "\033[1;32m[+] Start Cloud Flair Warp if you want\033[0m" << std::endl;

                SuspendThreadsAboveCycleCount(700000);
                Sleep(10000);
                resumeAndResuspendThreads();
                while (true) {
                    suspendThreadsWithNoActiveCycleDelta();
                    closePopup();
                    if (!messageDisplayed) {
                        std::cout << "Popup Bypassed!! Enjoy :)))" << std::endl;
                        std::cout << "Close window to stop program." << std::endl;
                        messageDisplayed = true;
                    }
                    if (GetAsyncKeyState('3') & 0x8000) {
                        std::cout << "Stopping" << std::endl;
                        break;
                    }
                }
                    
                  
                
                break;

            case 2:
                system(skCrypt("taskkill /f /im VALORANT-Win64-Shipping.exe >nul 2>&1"));
                system(skCrypt("taskkill /f /im VALORANT.exe >nul 2>&1"));
                system(skCrypt("taskkill /f /im RiotClientServices.exe >nul 2>&1"));
                system(skCrypt("taskkill /f /im vgtray.exe >nul 2>&1"));
                system(skCrypt("taskkill /f /im vgc.exe >nul 2>&1"));
                system(skCrypt("sc start vgc >nul 2>&1"));
                Sleep(2000);
                    while (true) {
                        suspendThreadsWithNoActiveCycleDelta();
                        
                    }
                break;

            case 3:

                system(skCrypt("taskkill /f /im VALORANT-Win64-Shipping.exe >nul 2>&1"));
                system(skCrypt("taskkill /f /im VALORANT.exe >nul 2>&1"));
                system(skCrypt("taskkill /f /im RiotClientServices.exe >nul 2>&1"));
                system(skCrypt("taskkill /f /im vgtray.exe >nul 2>&1"));
                system(skCrypt("taskkill /f /im vgc.exe >nul 2>&1"));
                system(skCrypt("sc start vgc >nul 2>&1"));
                //SuspendThreadsAboveCycleCount(150000);
                Sleep(3000);
                    suspendThreads(L"vgc.exe", 3);
                    
                    suspendThreadsWithNoActiveCycleDelta();
                
                
            }
        }
    }





