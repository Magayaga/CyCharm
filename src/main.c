// CyCharm : Defines the entry point for the application.
// Copyright 2023-2025 Cyril John Magayaga

#include <windows.h>
#include <richedit.h>
#include <commdlg.h>
#include <CommCtrl.h>
#include <string.h>
#include <stdio.h>
#include "main.h"

// Global variables
HWND g_hEdit;
HWND g_hWnd; // Adding a global variable for the window handle
HWND g_hStatusBar;
HMENU hFileMenu; // Declare hFileMenu globally
HMENU hEditMenu; // Declare hEditMenu globally
HMENU hViewMenu;
HMENU hHelpMenu;

// Global variable for the custom font
HFONT g_hFont = NULL;

OPENFILENAME ofn; // Structure for the open and save common dialogs

// Global variable to track word wrap state
BOOL g_bWordWrap = FALSE;

// Global variable to track Auto Save state
BOOL g_bAutoSave = FALSE;

int g_zoomLevel = 100;
int g_currentLine = 1;
int g_currentColumn = 1;

// Function to update the status bar with current cursor position
void UpdateStatusBar() {
    // Get the current position of the cursor
    DWORD dwSelStart = 0, dwSelEnd = 0;
    SendMessage(g_hEdit, EM_GETSEL, (WPARAM)&dwSelStart, (LPARAM)&dwSelEnd);

    // Get the line number of the cursor
    g_currentLine = (int)SendMessage(g_hEdit, EM_LINEFROMCHAR, (WPARAM)dwSelStart, 0);
    
    // Get the column number of the cursor
    int lineStart = (int)SendMessage(g_hEdit, EM_LINEINDEX, g_currentLine, 0);
    g_currentColumn = dwSelStart - lineStart + 1;

    // Display the current line and column in the status bar
    char statusText[128];
    snprintf(statusText, sizeof(statusText), "Line: %d, Column: %d", g_currentLine + 1, g_currentColumn);
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)statusText);
}

void SetZoomLevel(int zoom);

// Function prototypes
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
LRESULT CALLBACK EditProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

// Original window procedure for the edit control
WNDPROC g_OldEditProc;

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, LPSTR lp_cmd_line, int n_cmd_show) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    // Load the RichEdit library
    HMODULE hRichEdit = LoadLibrary("RICHED32.DLL");
    if (!hRichEdit) {
        MessageBox(NULL, "RichEdit library not found!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Register the window class
    WNDCLASS window_class = { 0 };
    window_class.lpfnWndProc = WndProc;
    window_class.hInstance = h_instance;
    window_class.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
    window_class.lpszClassName = "CodeEditor";

    if (!RegisterClass(&window_class)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Calculate the center coordinates
    int windowWidth = 800;
    int windowHeight = 600;
    int x = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

    // Create the main window
    g_hWnd = CreateWindow("CodeEditor", "CyCharm", WS_OVERLAPPEDWINDOW,
        x, y, windowWidth, windowHeight, NULL, NULL, h_instance, NULL);

    if (g_hWnd == NULL) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Create the menu
    HMENU hMenu = CreateMenu();
    hFileMenu = CreateMenu(); // Initialize hFileMenu globally
    hEditMenu = CreateMenu(); // Initialize hEditMenu globally
    hViewMenu = CreateMenu(); // Initialize hViewMenu globally
    HMENU hZoomMenu = CreateMenu(); // Initialize hZoomMenu globally
    hHelpMenu = CreateMenu();

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "File");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, "Edit"); // Add Edit menu
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hViewMenu, "View");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, "Help");

    // Add File menu items
    AppendMenu(hFileMenu, MF_STRING, 16, "New File");
    
    // Add a horizontal line (separator)
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, 1, "Open");
    AppendMenu(hFileMenu, MF_STRING, 2, "Save");
    AppendMenu(hFileMenu, MF_STRING, 17, "Save As");
    
    // Add a horizontal line (separator)
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING | MF_UNCHECKED, 18, "Auto Save");
    
    // Add a horizontal line (separator)
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, 3, "Exit");

    // Add Edit menu items
    AppendMenu(hEditMenu, MF_STRING, 4, "Undo");
    AppendMenu(hEditMenu, MF_STRING, 5, "Redo");

    // Add a horizontal line (separator)
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, NULL);

    AppendMenu(hEditMenu, MF_STRING, 6, "Cut");
    AppendMenu(hEditMenu, MF_STRING, 7, "Copy");
    AppendMenu(hEditMenu, MF_STRING, 8, "Paste");

    // Add a horizontal line (separator)
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, NULL);

    AppendMenu(hEditMenu, MF_STRING, 14, "Find");
    AppendMenu(hEditMenu, MF_STRING, 15, "Replace");

    // Add View menu items
    AppendMenu(hViewMenu, MF_STRING, 9, "Word Wrap");

    // Word Wrap is initially disabled
    CheckMenuItem(hViewMenu, 9, MF_UNCHECKED);

    // Add Zoom dropdown
    AppendMenu(hViewMenu, MF_POPUP, (UINT_PTR)hZoomMenu, "Zoom");

    // Add a horizontal line (separator)
    AppendMenu(hViewMenu, MF_SEPARATOR, 0, NULL);

    // Add "Select All" option
    AppendMenu(hViewMenu, MF_STRING, 13, "Select All");

    // Add Zoom dropdown items
    AppendMenu(hZoomMenu, MF_STRING, 10, "Zoom In");
    AppendMenu(hZoomMenu, MF_STRING, 11, "Zoom Out");

    // Add Help menu items
    AppendMenu(hHelpMenu, MF_STRING, 12, "About");

    SetMenu(g_hWnd, hMenu); // Set the menu to the window

    // Create the status bar
    g_hStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE, "Line: 1, Column: 1", g_hWnd, IDC_STATUSBAR);

    // Create the edit control (using RichEdit instead of EDIT)
    g_hEdit = CreateWindowEx(0, RICHEDIT_CLASS, NULL, 
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 800, 600, g_hWnd, (HMENU)IDC_EDIT, h_instance, NULL);

    if (g_hEdit == NULL) {
        MessageBox(NULL, "RichEdit Control Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Subclass the edit control to handle messages
    g_OldEditProc = (WNDPROC)SetWindowLongPtr(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);

    // Show and update the window
    ShowWindow(g_hWnd, n_cmd_show);
    UpdateWindow(g_hWnd);

    // Set up auto-save timer (every 60 seconds)
    SetTimer(g_hWnd, AUTOSAVE_TIMER_ID, 60000, NULL);

    // Create the custom font
    g_hFont = CreateFont(
        20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FF_MODERN, "Cascadia Mono"
    );

    // Check if font creation was successful
    if (g_hFont == NULL) {
        MessageBox(NULL, "Font creation failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Set the custom font to the edit control
    SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // Initialize the status bar
    UpdateStatusBar();

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup: Delete the custom font when closing the application
    if (g_hFont != NULL) {
        DeleteObject(g_hFont);
    }

    // Free the RichEdit library
    FreeLibrary(hRichEdit);

    return msg.wParam;
}

LRESULT CALLBACK EditProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_CHAR:
        case WM_KEYUP:
        case WM_KEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
        case WM_VSCROLL:
        case WM_HSCROLL:
            // Update the status bar after handling the message
            LRESULT result = CallWindowProc(g_OldEditProc, hwnd, message, w_param, l_param);
            UpdateStatusBar();
            return result;
    }
    
    return CallWindowProc(g_OldEditProc, hwnd, message, w_param, l_param);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_SIZE:
        // Handle resizing of the window
        {
            int newWidth = LOWORD(l_param);
            int newHeight = HIWORD(l_param);

            // Resize status bar first
            SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
            
            // Get the height of the status bar
            RECT statusRect;
            GetWindowRect(g_hStatusBar, &statusRect);
            int statusHeight = statusRect.bottom - statusRect.top;
            
            // Resize the edit control
            MoveWindow(g_hEdit, 0, 0, newWidth, newHeight - statusHeight, TRUE);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(w_param)) {
        case 1: // Open
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = g_hWnd;
            ofn.lpstrFile = (LPSTR)malloc(MAX_PATH);
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD dwFileSize = GetFileSize(hFile, NULL);
                    char* buffer = (char*)malloc(dwFileSize + 1);

                    if (ReadFile(hFile, buffer, dwFileSize, NULL, NULL)) {
                        buffer[dwFileSize] = '\0';
                        SetWindowText(g_hEdit, buffer);
                        // Update cursor position after loading file
                        UpdateStatusBar();
                    }

                    free(buffer);
                    CloseHandle(hFile);
                }
            }

            free(ofn.lpstrFile);
            break;

        case 2: // Save
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = g_hWnd;
            ofn.lpstrFile = (LPSTR)malloc(MAX_PATH);
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

            if (GetSaveFileName(&ofn)) {
                HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD dwTextLength = GetWindowTextLength(g_hEdit);
                    char* buffer = (char*)malloc(dwTextLength + 1);

                    GetWindowText(g_hEdit, buffer, dwTextLength + 1);
                    WriteFile(hFile, buffer, dwTextLength, NULL, NULL);

                    free(buffer);
                    CloseHandle(hFile);
                }
            }

            free(ofn.lpstrFile);
            break;

        case 3: // Exit
            DestroyWindow(g_hWnd);
            break;

        case 4: // Undo
            SendMessage(g_hEdit, EM_UNDO, 0, 0);
            UpdateStatusBar(); // Update cursor position after undo
            break;

        case 5: // Redo
            SendMessage(g_hEdit, EM_REDO, 0, 0);
            UpdateStatusBar(); // Update cursor position after redo
            break;

        case 6: // Cut
            SendMessage(g_hEdit, WM_CUT, 0, 0);
            UpdateStatusBar(); // Update cursor position after cut
            break;

        case 7: // Copy
            SendMessage(g_hEdit, WM_COPY, 0, 0);
            break;

        case 8: // Paste
            SendMessage(g_hEdit, WM_PASTE, 0, 0);
            UpdateStatusBar(); // Update cursor position after paste
            break;

        case 9:
            // Toggle word wrap state
            g_bWordWrap = !g_bWordWrap;

            // Enable or disable word wrap using EM_SETTARGETDEVICE
            SendMessage(g_hEdit, EM_SETTARGETDEVICE, 0, g_bWordWrap ? 0 : 1);

            // Update the menu item state
            CheckMenuItem(hViewMenu, 9, g_bWordWrap ? MF_CHECKED : MF_UNCHECKED);
            break;

        case 10: // Zoom In
            g_zoomLevel += 10; // Increase zoom level by 10%
            if (g_zoomLevel > 500) g_zoomLevel = 500; // Limit maximum zoom level
            SetZoomLevel(g_zoomLevel);
            break;

        case 11: // Zoom Out
            g_zoomLevel -= 10; // Decrease zoom level by 10%
            if (g_zoomLevel < 10) g_zoomLevel = 10; // Limit minimum zoom level
            SetZoomLevel(g_zoomLevel);
            break;

        case 12: // About
            MessageBox(g_hWnd, "About CyCharm\nVersion 1.0-preview3\n\nDeveloped by Cyril John Magayaga", "About CyCharm", MB_OK | MB_ICONINFORMATION);
            break;

        case 13: // Select All
            SendMessage(g_hEdit, EM_SETSEL, 0, -1); // Select the entire content of the edit control
            break;
            
        case 14: // Find
        case 15: // Replace
            // These would typically open dialog boxes for find/replace functionality
            // For now, just show a placeholder message
            MessageBox(g_hWnd, "This feature is not yet implemented.", "Information", MB_OK | MB_ICONINFORMATION);
            break;
        
        case 16: // New File
            // Confirm if the user wants to discard unsaved changes
            if (GetWindowTextLength(g_hEdit) > 0) {
                int response = MessageBox(g_hWnd, "Do you want to save changes?", "Confirmation", MB_YESNOCANCEL | MB_ICONQUESTION);
                if (response == IDYES) {
                    // Trigger Save functionality
                    SendMessage(g_hWnd, WM_COMMAND, 2, 0);
                } else if (response == IDCANCEL) {
                    // User canceled the operation
                    break;
                }
            }
            // Clear the edit control
            SetWindowText(g_hEdit, "");
            UpdateStatusBar();
            break;
        
        case 17: // Save As
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = g_hWnd;
            ofn.lpstrFile = (LPSTR)malloc(MAX_PATH);
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        
            if (GetSaveFileName(&ofn)) {
                HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD dwTextLength = GetWindowTextLength(g_hEdit);
                    char* buffer = (char*)malloc(dwTextLength + 1);
        
                    GetWindowText(g_hEdit, buffer, dwTextLength + 1);
                    WriteFile(hFile, buffer, dwTextLength, NULL, NULL);
        
                    free(buffer);
                    CloseHandle(hFile);
                }
            }
        
            free(ofn.lpstrFile);
            break;
        
        case 18: // Auto Save
            // Toggle Auto Save state
            g_bAutoSave = !g_bAutoSave;
            
            // Update menu checkbox
            CheckMenuItem(hFileMenu, 18, g_bAutoSave ? MF_CHECKED : MF_UNCHECKED);
            break;
        }
        break;

    case WM_KEYDOWN:
        // Check for keyboard shortcuts
        if (GetKeyState(VK_LWIN) & 0x8000) { // Windows logo key is pressed
            if (w_param == VK_OEM_PLUS) { // Plus sign key
                // Increase zoom level
                g_zoomLevel += 10; // Increase zoom level by 10%

                if (g_zoomLevel > 500) g_zoomLevel = 500; // Limit maximum zoom level
                SetZoomLevel(g_zoomLevel);
            }

            else if (w_param == VK_OEM_MINUS) { // Minus sign key
                // Decrease zoom level
                g_zoomLevel -= 10; // Decrease zoom level by 10%

                if (g_zoomLevel < 10) g_zoomLevel = 10; // Limit minimum zoom level
                SetZoomLevel(g_zoomLevel);
            }
        }
        break;

    case WM_DESTROY:
        if (g_hFont != NULL) {
            DeleteObject(g_hFont);
        }
        PostQuitMessage(0);
        KillTimer(g_hWnd, AUTOSAVE_TIMER_ID);
        break;
    
    case WM_TIMER:
        if (w_param == AUTOSAVE_TIMER_ID && g_bAutoSave) {
            // Perform auto save if there's content and Auto Save is enabled
            if (GetWindowTextLength(g_hEdit) > 0) {
                // If we have a file path already, save to it
                if (ofn.lpstrFile != NULL && ofn.lpstrFile[0] != '\0') {
                    HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD dwTextLength = GetWindowTextLength(g_hEdit);
                        char* buffer = (char*)malloc(dwTextLength + 1);
    
                        GetWindowText(g_hEdit, buffer, dwTextLength + 1);
                        WriteFile(hFile, buffer, dwTextLength, NULL, NULL);
    
                        free(buffer);
                        CloseHandle(hFile);
                    }
                }
            }
        }
        break;

    default:
        return DefWindowProc(hwnd, message, w_param, l_param);
    }

    return 0;
}

void SetZoomLevel(int zoom) {
    // Calculate the zoom factor
    int numerator = zoom;
    int denominator = 100; // Base zoom level

    // Set the zoom level using EM_SETZOOM
    SendMessage(g_hEdit, EM_SETZOOM, numerator, denominator);
}
