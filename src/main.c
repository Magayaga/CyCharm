// CyCharm : Defines the entry point for the application.
// Copyright 2023-2025 Cyril John Magayaga

#include <windows.h>
#include <richedit.h>
#include <commdlg.h>
#include <CommCtrl.h>
#include <string.h>
#include <stdio.h>
#include <shellapi.h>
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
int g_currentEncoding = ENCODING_UTF8; // Default to UTF-8

// Function to update the status bar with current cursor position and character count
void UpdateStatusBar() {
    // Get the current position of the cursor
    DWORD dwSelStart = 0, dwSelEnd = 0;
    SendMessage(g_hEdit, EM_GETSEL, (WPARAM)&dwSelStart, (LPARAM)&dwSelEnd);

    // Get the line number of the cursor
    g_currentLine = (int)SendMessage(g_hEdit, EM_LINEFROMCHAR, (WPARAM)dwSelStart, 0);
    
    // Get the column number of the cursor
    int lineStart = (int)SendMessage(g_hEdit, EM_LINEINDEX, g_currentLine, 0);
    g_currentColumn = dwSelStart - lineStart + 1;
    
    // Get total character count
    int charCount = GetWindowTextLength(g_hEdit);

    // Update each part of the status bar
    char positionText[64];
    char charCountText[64];
    char encodingText[32];
    char zoomText[32];
    
    // Format the text for each part
    snprintf(positionText, sizeof(positionText), "Line: %d, Column: %d", g_currentLine + 1, g_currentColumn);
    snprintf(charCountText, sizeof(charCountText), "Characters: %d", charCount);
    
    // Get the encoding text based on the current encoding
    const char* encodingStr = "Unknown";
    switch (g_currentEncoding) {
        case ENCODING_UTF8:
            encodingStr = "UTF-8";
            break;
        case ENCODING_UTF16LE:
            encodingStr = "UTF-16LE";
            break;
        case ENCODING_UTF16BE:
            encodingStr = "UTF-16BE";
            break;
        case ENCODING_ASCII:
            encodingStr = "ASCII";
            break;
        case ENCODING_ISO_8859_1:
            encodingStr = "ISO-8859-1";
            break;
        case ENCODING_ISO_8859_15:
            encodingStr = "ISO-8859-15";
            break;
        case ENCODING_WINDOWS_1252:
            encodingStr = "Windows-1252";
            break;
        case ENCODING_SHIFT_JIS:
            encodingStr = "Shift-JIS";
            break;
        case ENCODING_GB18030:
            encodingStr = "GB18030";
            break;
    }
    snprintf(encodingText, sizeof(encodingText), "Encoding: %s", encodingStr);
    snprintf(zoomText, sizeof(zoomText), "Zoom: %d%%", g_zoomLevel);
    
    // Set the text for each part
    SendMessage(g_hStatusBar, SB_SETTEXT, SB_PART_POSITION, (LPARAM)positionText);
    SendMessage(g_hStatusBar, SB_SETTEXT, SB_PART_CHARCOUNT, (LPARAM)charCountText);
    SendMessage(g_hStatusBar, SB_SETTEXT, SB_PART_ENCODING, (LPARAM)encodingText);
    SendMessage(g_hStatusBar, SB_SETTEXT, SB_PART_ZOOM, (LPARAM)zoomText);
}

void SetZoomLevel(int zoom);

// Function to detect text encoding from a buffer
int DetectEncoding(const unsigned char* buffer, size_t size) {
    // Check for UTF-16LE BOM (FF FE)
    if (size >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE) {
        return ENCODING_UTF16LE;
    }
    // Check for UTF-16BE BOM (FE FF)
    else if (size >= 2 && buffer[0] == 0xFE && buffer[1] == 0xFF) {
        return ENCODING_UTF16BE;
    }
    // Check for UTF-8 BOM (EF BB BF)
    else if (size >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        return ENCODING_UTF8;
    }
    // No BOM detected, perform heuristic check
    else {
        // Check for UTF-16LE (even bytes are often 0 in English/Latin text)
        int zeroBytes = 0;
        for (size_t i = 0; i < size && i < 100; i += 2) {
            if (i + 1 < size && buffer[i + 1] == 0) {
                zeroBytes++;
            }
        }
        if (zeroBytes > 10) { // If more than 10 zero bytes in alternating positions
            return ENCODING_UTF16LE;
        }
        
        // Check for UTF-16BE (odd bytes are often 0 in English/Latin text)
        zeroBytes = 0;
        for (size_t i = 1; i < size && i < 100; i += 2) {
            if (buffer[i] == 0) {
                zeroBytes++;
            }
        }
        if (zeroBytes > 10) { // If more than 10 zero bytes in alternating positions
            return ENCODING_UTF16BE;
        }
        
        // Check if it's ASCII (all bytes < 128)
        int isAscii = 1;
        for (size_t i = 0; i < size && i < 100; i++) {
            if (buffer[i] > 127) {
                isAscii = 0;
                break;
            }
        }
        if (isAscii) {
            return ENCODING_ASCII;
        }
        
        // Check for Shift-JIS (Japanese)
        // Common Shift-JIS patterns: bytes in ranges 0x81-0x9F and 0xE0-0xEF followed by 0x40-0xFC
        int shiftJisCount = 0;
        for (size_t i = 0; i < size - 1 && i < 200; i++) {
            if (((buffer[i] >= 0x81 && buffer[i] <= 0x9F) || (buffer[i] >= 0xE0 && buffer[i] <= 0xEF)) &&
                (buffer[i + 1] >= 0x40 && buffer[i + 1] <= 0xFC && buffer[i + 1] != 0x7F)) {
                shiftJisCount++;
            }
        }
        if (shiftJisCount > 5) { // If we detect several Shift-JIS character patterns
            return ENCODING_SHIFT_JIS;
        }
        
        // Check for GB18030 (Chinese)
        // Common GB18030 patterns: bytes in range 0x81-0xFE followed by 0x40-0xFE (excluding 0x7F)
        int gbCount = 0;
        for (size_t i = 0; i < size - 1 && i < 200; i++) {
            if ((buffer[i] >= 0x81 && buffer[i] <= 0xFE) &&
                (buffer[i + 1] >= 0x40 && buffer[i + 1] <= 0xFE && buffer[i + 1] != 0x7F)) {
                gbCount++;
            }
        }
        if (gbCount > 5 && gbCount > shiftJisCount) { // If we detect several GB18030 patterns and more than Shift-JIS
            return ENCODING_GB18030;
        }
        
        // Check for Windows-1252 or ISO-8859 encodings
        // These have specific byte patterns in the upper range (128-255)
        int windows1252Count = 0;
        int iso8859Count = 0;
        
        for (size_t i = 0; i < size && i < 200; i++) {
            // Windows-1252 specific characters (like €, ‚, etc.)
            if (buffer[i] >= 0x80 && buffer[i] <= 0x9F) {
                windows1252Count++;
            }
            // ISO-8859 range (shared by both ISO-8859-1 and ISO-8859-15)
            else if (buffer[i] >= 0xA0 && buffer[i] <= 0xFF) {
                iso8859Count++;
            }
        }
        
        if (windows1252Count > 3) {
            return ENCODING_WINDOWS_1252;
        }
        
        if (iso8859Count > 3) {
            // Check for Euro sign (€) which is specific to ISO-8859-15 at position 0xA4
            for (size_t i = 0; i < size && i < 200; i++) {
                if (buffer[i] == 0xA4) {
                    return ENCODING_ISO_8859_15;  // Likely ISO-8859-15 with Euro sign
                }
            }
            return ENCODING_ISO_8859_1;  // Default to ISO-8859-1 if no Euro sign found
        }
        
        // Default to UTF-8 if no other encoding detected
        return ENCODING_UTF8;
    }
}

// Function to convert text to the specified encoding
char* ConvertToEncoding(const char* text, size_t textLength, int targetEncoding, size_t* outSize) {
    // For simplicity, we'll just handle the most common case: converting between UTF-8 and ASCII
    // For real applications, you would use Windows API functions like MultiByteToWideChar and WideCharToMultiByte
    
    char* result = NULL;
    *outSize = 0;
    
    switch (targetEncoding) {
        case ENCODING_UTF8: {
            // Add UTF-8 BOM
            *outSize = textLength + 3; // 3 bytes for BOM
            result = (char*)malloc(*outSize);
            if (result) {
                // Add UTF-8 BOM (EF BB BF)
                result[0] = (char)0xEF;
                result[1] = (char)0xBB;
                result[2] = (char)0xBF;
                // Copy the text after the BOM
                memcpy(result + 3, text, textLength);
            }
            break;
        }
        
        case ENCODING_UTF16LE: {
            // Convert to UTF-16LE (simplified - just for ASCII range)
            *outSize = (textLength * 2) + 2; // 2 bytes per char + 2 bytes for BOM
            result = (char*)malloc(*outSize);
            if (result) {
                // Add UTF-16LE BOM (FF FE)
                result[0] = (char)0xFF;
                result[1] = (char)0xFE;
                
                // Convert each ASCII byte to UTF-16LE (char followed by zero byte)
                for (size_t i = 0; i < textLength; i++) {
                    result[2 + (i * 2)] = text[i];
                    result[2 + (i * 2) + 1] = 0;
                }
            }
            break;
        }
        
        case ENCODING_UTF16BE: {
            // Convert to UTF-16BE (simplified - just for ASCII range)
            *outSize = (textLength * 2) + 2; // 2 bytes per char + 2 bytes for BOM
            result = (char*)malloc(*outSize);
            if (result) {
                // Add UTF-16BE BOM (FE FF)
                result[0] = (char)0xFE;
                result[1] = (char)0xFF;
                
                // Convert each ASCII byte to UTF-16BE (zero byte followed by char)
                for (size_t i = 0; i < textLength; i++) {
                    result[2 + (i * 2)] = 0;
                    result[2 + (i * 2) + 1] = text[i];
                }
            }
            break;
        }
        
        case ENCODING_ISO_8859_1: {
            // ISO-8859-1 (Latin-1) - No BOM, just copy the text
            // For non-ASCII characters, would need proper conversion
            *outSize = textLength;
            result = (char*)malloc(*outSize);
            if (result) {
                memcpy(result, text, textLength);
            }
            break;
        }
        
        case ENCODING_ISO_8859_15: {
            // ISO-8859-15 (Latin-9) - No BOM, just copy the text
            // For non-ASCII characters, would need proper conversion
            *outSize = textLength;
            result = (char*)malloc(*outSize);
            if (result) {
                memcpy(result, text, textLength);
                
                // Simple conversion for Euro sign if present (ASCII 0x80 to ISO-8859-15 0xA4)
                // This is a simplified approach and not a complete conversion
                for (size_t i = 0; i < textLength; i++) {
                    if ((unsigned char)text[i] == 0x80) { // Euro sign in Windows-1252
                        result[i] = (char)0xA4;  // Euro sign position in ISO-8859-15
                    }
                }
            }
            break;
        }
        
        case ENCODING_WINDOWS_1252: {
            // Windows-1252 - No BOM, just copy the text
            *outSize = textLength;
            result = (char*)malloc(*outSize);
            if (result) {
                memcpy(result, text, textLength);
            }
            break;
        }
        
        case ENCODING_SHIFT_JIS: {
            // Shift-JIS - No BOM, simplified handling
            // For proper conversion, would need a full mapping table
            *outSize = textLength;
            result = (char*)malloc(*outSize);
            if (result) {
                memcpy(result, text, textLength);
            }
            break;
        }
        
        case ENCODING_GB18030: {
            // GB18030 - No BOM, simplified handling
            // For proper conversion, would need a full mapping table
            *outSize = textLength;
            result = (char*)malloc(*outSize);
            if (result) {
                memcpy(result, text, textLength);
            }
            break;
        }
        
        case ENCODING_ASCII:
        default: {
            // Just copy the text as-is for ASCII
            *outSize = textLength;
            result = (char*)malloc(*outSize);
            if (result) {
                memcpy(result, text, textLength);
            }
            break;
        }
    }
    
    return result;
}

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
    
    // Create Encoding submenu
    HMENU hEncodingMenu = CreateMenu();
    AppendMenu(hViewMenu, MF_POPUP, (UINT_PTR)hEncodingMenu, "Encoding");
    
    // Add encoding options
    AppendMenu(hEncodingMenu, MF_STRING, 21, "UTF-8");
    AppendMenu(hEncodingMenu, MF_STRING, 22, "UTF-16LE");
    AppendMenu(hEncodingMenu, MF_STRING, 23, "UTF-16BE");
    AppendMenu(hEncodingMenu, MF_STRING, 24, "ASCII");
    AppendMenu(hEncodingMenu, MF_STRING, 25, "ISO-8859-1");
    AppendMenu(hEncodingMenu, MF_STRING, 26, "ISO-8859-15");
    AppendMenu(hEncodingMenu, MF_STRING, 27, "Windows-1252");
    AppendMenu(hEncodingMenu, MF_STRING, 28, "Shift-JIS");
    AppendMenu(hEncodingMenu, MF_STRING, 29, "GB18030");
    
    // Check the default encoding (UTF-8)
    CheckMenuRadioItem(hEncodingMenu, 21, 29, 21, MF_BYCOMMAND);

    // Add Zoom dropdown
    AppendMenu(hViewMenu, MF_POPUP, (UINT_PTR)hZoomMenu, "Zoom");

    // Add a horizontal line (separator)
    AppendMenu(hViewMenu, MF_SEPARATOR, 0, NULL);

    // Add "Select All" option
    AppendMenu(hViewMenu, MF_STRING, 13, "Select All");

    // Add Zoom dropdown items
    AppendMenu(hZoomMenu, MF_STRING, 10, "Zoom In");
    AppendMenu(hZoomMenu, MF_STRING, 11, "Zoom Out");
    AppendMenu(hZoomMenu, MF_STRING, 20, "Restore Default Zoom");

    // Add Help menu items
    AppendMenu(hHelpMenu, MF_STRING, 19, "View License");
    // Add a horizontal line (separator)
    AppendMenu(hHelpMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hHelpMenu, MF_STRING, 12, "About");

    SetMenu(g_hWnd, hMenu); // Set the menu to the window

    // Create the status bar
    g_hStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE, "", g_hWnd, IDC_STATUSBAR);
    
    // Set up the status bar parts
    int statusWidths[4] = {200, 350, 500, -1}; // Width of each part, -1 means extend to the right edge
    SendMessage(g_hStatusBar, SB_SETPARTS, 4, (LPARAM)statusWidths);

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
    
    // Initialize the status bar
    UpdateStatusBar();

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

            // Calculate new status bar part widths based on window width
            int statusWidths[4];
            statusWidths[0] = newWidth / 5;        // Position info (20% of width)
            statusWidths[1] = statusWidths[0] * 2; // Character count (40% of width)
            statusWidths[2] = statusWidths[1] + newWidth / 5; // Encoding (20% of width)
            statusWidths[3] = -1;                 // Zoom level (extends to right edge)
            
            // Update the status bar parts
            SendMessage(g_hStatusBar, SB_SETPARTS, 4, (LPARAM)statusWidths);
            
            // Resize status bar first
            SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
            
            // Get the height of the status bar
            RECT statusRect;
            GetWindowRect(g_hStatusBar, &statusRect);
            int statusHeight = statusRect.bottom - statusRect.top;
            
            // Resize the edit control
            MoveWindow(g_hEdit, 0, 0, newWidth, newHeight - statusHeight, TRUE);
            
            // Update the status bar text
            UpdateStatusBar();
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
                    unsigned char* buffer = (unsigned char*)malloc(dwFileSize + 2); // +2 for null terminator and possible alignment

                    DWORD bytesRead = 0;
                    if (ReadFile(hFile, buffer, dwFileSize, &bytesRead, NULL)) {
                        // Detect the encoding of the file
                        g_currentEncoding = DetectEncoding(buffer, bytesRead);
                        
                        // Handle different encodings for display
                        char* displayText = NULL;
                        
                        switch (g_currentEncoding) {
                            case ENCODING_UTF8: {
                                // Skip UTF-8 BOM if present (3 bytes: EF BB BF)
                                if (bytesRead >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
                                    displayText = (char*)(buffer + 3);
                                    buffer[bytesRead] = '\0'; // Ensure null termination
                                } else {
                                    displayText = (char*)buffer;
                                    buffer[bytesRead] = '\0'; // Ensure null termination
                                }
                                break;
                            }
                            
                            case ENCODING_UTF16LE: {
                                // Skip UTF-16LE BOM if present (2 bytes: FF FE)
                                // For proper display, we would need to convert UTF-16LE to UTF-8
                                // This is a simplified version that just skips the BOM
                                if (bytesRead >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE) {
                                    displayText = (char*)(buffer + 2);
                                } else {
                                    displayText = (char*)buffer;
                                }
                                // For UTF-16LE, we need a proper conversion to display correctly
                                // This is a simplified version that may not display correctly
                                buffer[bytesRead] = '\0';
                                buffer[bytesRead + 1] = '\0'; // Ensure null termination for UTF-16
                                break;
                            }
                            
                            case ENCODING_UTF16BE: {
                                // Skip UTF-16BE BOM if present (2 bytes: FE FF)
                                // For proper display, we would need to convert UTF-16BE to UTF-8
                                // This is a simplified version that just skips the BOM
                                if (bytesRead >= 2 && buffer[0] == 0xFE && buffer[1] == 0xFF) {
                                    displayText = (char*)(buffer + 2);
                                } else {
                                    displayText = (char*)buffer;
                                }
                                // For UTF-16BE, we need a proper conversion to display correctly
                                // This is a simplified version that may not display correctly
                                buffer[bytesRead] = '\0';
                                buffer[bytesRead + 1] = '\0'; // Ensure null termination for UTF-16
                                break;
                            }
                            
                            case ENCODING_ASCII:
                            default: {
                                displayText = (char*)buffer;
                                buffer[bytesRead] = '\0'; // Ensure null termination
                                break;
                            }
                        }
                        
                        SetWindowText(g_hEdit, displayText);
                        
                        // Update the menu to reflect the detected encoding
                        CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 21 + g_currentEncoding, MF_BYCOMMAND);
                        
                        // Update cursor position and status bar after loading file
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
                    
                    // Convert the text to the selected encoding
                    size_t encodedSize = 0;
                    char* encodedBuffer = ConvertToEncoding(buffer, dwTextLength, g_currentEncoding, &encodedSize);
                    
                    if (encodedBuffer) {
                        // Write the encoded text to the file
                        DWORD bytesWritten = 0;
                        WriteFile(hFile, encodedBuffer, (DWORD)encodedSize, &bytesWritten, NULL);
                        free(encodedBuffer);
                    } else {
                        // Fallback to writing the original buffer if encoding conversion failed
                        WriteFile(hFile, buffer, dwTextLength, NULL, NULL);
                    }

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
            MessageBox(g_hWnd, "About CyCharm\nVersion 1.0-preview5\n\nDeveloped by Cyril John Magayaga", "About CyCharm", MB_OK | MB_ICONINFORMATION);
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
                    
                    // Convert the text to the selected encoding
                    size_t encodedSize = 0;
                    char* encodedBuffer = ConvertToEncoding(buffer, dwTextLength, g_currentEncoding, &encodedSize);
                    
                    if (encodedBuffer) {
                        // Write the encoded text to the file
                        DWORD bytesWritten = 0;
                        WriteFile(hFile, encodedBuffer, (DWORD)encodedSize, &bytesWritten, NULL);
                        free(encodedBuffer);
                    } else {
                        // Fallback to writing the original buffer if encoding conversion failed
                        WriteFile(hFile, buffer, dwTextLength, NULL, NULL);
                    }
        
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
        
        case 19: // View License
            // Open the license URL in the default web browser
            ShellExecute(NULL, "open", "https://github.com/Magayaga/CyCharm/blob/master/LICENSE", NULL, NULL, SW_SHOWNORMAL);
            break;
        
        case 20: // Restore Default Zoom
            g_zoomLevel = 100; // Reset zoom level to 100%
            SetZoomLevel(g_zoomLevel);
            break;
            
        case 21: // UTF-8 encoding
            g_currentEncoding = ENCODING_UTF8;
            CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 21, MF_BYCOMMAND);
            UpdateStatusBar();
            break;
            
        case 22: // UTF-16LE encoding
            g_currentEncoding = ENCODING_UTF16LE;
            CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 22, MF_BYCOMMAND);
            UpdateStatusBar();
            break;
            
        case 23: // UTF-16BE encoding
            g_currentEncoding = ENCODING_UTF16BE;
            CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 23, MF_BYCOMMAND);
            UpdateStatusBar();
            break;
            
        case 24: // ASCII encoding
            g_currentEncoding = ENCODING_ASCII;
            CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 24, MF_BYCOMMAND);
            UpdateStatusBar();
            break;
            
        case 25: // ISO-8859-1 encoding
            g_currentEncoding = ENCODING_ISO_8859_1;
            CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 25, MF_BYCOMMAND);
            UpdateStatusBar();
            break;
            
        case 26: // ISO-8859-15 encoding
            g_currentEncoding = ENCODING_ISO_8859_15;
            CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 26, MF_BYCOMMAND);
            UpdateStatusBar();
            break;
            
        case 27: // Windows-1252 encoding
            g_currentEncoding = ENCODING_WINDOWS_1252;
            CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 27, MF_BYCOMMAND);
            UpdateStatusBar();
            break;
            
        case 28: // Shift-JIS encoding
            g_currentEncoding = ENCODING_SHIFT_JIS;
            CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 28, MF_BYCOMMAND);
            UpdateStatusBar();
            break;
            
        case 29: // GB18030 encoding
            g_currentEncoding = ENCODING_GB18030;
            CheckMenuRadioItem(GetSubMenu(hViewMenu, 1), 21, 29, 29, MF_BYCOMMAND);
            UpdateStatusBar();
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
    
    // Update the status bar to show the new zoom level
    UpdateStatusBar();
}
