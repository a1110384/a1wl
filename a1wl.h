//by a111

//To remove console terminal opening, 
//(If using Visual Studio): Go to the project's Properties->Linker->System and change the Subsystem to Windows
//(If using GCC): add '-mwindows' OR '-Wl,--subsystem,windows' to your compile command

//FUNCTIONS:
//void start()                                 -Called on program startup (must implement)
//void update()                                -Called every frame (must implement)
//void process(short* buffer, int size)        -Called every audio block (only need if calling initAudio())

#ifndef A1WL_H
#define A1WL_H

#pragma comment(linker, "/SUBSYSTEM:windows")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma warning(disable: 28251)

#include <windows.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdarg.h>

#define WIN_WIDTH 900
#define WIN_HEIGHT 540

#define CHUNK_AMT 4
#define CHUNK_SIZE 1024

typedef struct Window {
    unsigned char quit;
    int width, height;
    int x, y;

	unsigned int* pixels;
    int fontHeight, fontSpacing, fontRSpacing;
    unsigned int textCol;
    unsigned int clearCol;

    HINSTANCE hInst;
	HWND hwnd;
	BITMAPINFO bmi;
	HBITMAP frame_bitmap;
	HDC hdc;
    HDC drawHDC;

	unsigned char keyDown[256], keyPress[256], keyHeld[256];
    unsigned short keyHeldTimer[256];

    int targetFPS;
    double targetFrameTimeMs;

    //Audio
    HWAVEOUT waveOut;
    WAVEHDR header[CHUNK_AMT];
    short buffers[CHUNK_AMT][CHUNK_SIZE * 2], bufIndex;
    unsigned int sample;
    void (*process)(short* buffer, int size, int start);
    void (*close)();

} Window;

struct { int x, y; unsigned char buttonsDown, buttonsPress; } mouse;
enum { M_LEFT = 0b1, M_MIDDLE = 0b10, M_RIGHT = 0b100, M_X1 = 0b1000, M_X2 = 0b10000, M_WHEELDOWN = 0b100000, M_WHEELUP = 0b1000000 };

struct Window win;

LRESULT CALLBACK winProcMsg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void start();
void update();
void winSize(int w, int h);



//WINDOW SETUP + Entry point

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    win.hInst = hInstance;
    win.quit = 0;
    win.width = WIN_WIDTH;
    win.height = WIN_HEIGHT;
    win.x = 640;
    win.y = 300;
    win.targetFPS = 60;
    win.targetFrameTimeMs = 1000.0 / win.targetFPS;
    win.clearCol = 0x000A0F;
    win.textCol = 0xFFFFFF;
    win.fontHeight = 12; win.fontSpacing = 1;
    win.fontRSpacing = ((int)(win.fontHeight * 0.7f) + win.fontSpacing);

    static const wchar_t* winClassName = L"Untitled";
    static WNDCLASSEX wcx = { 0 };
    wcx.cbSize = sizeof(wcx);
    wcx.lpfnWndProc = winProcMsg;
    wcx.hInstance = win.hInst;
    wcx.lpszClassName = winClassName;
    wcx.lpszMenuName = winClassName;
    RegisterClassEx(&wcx);

    win.bmi.bmiHeader.biWidth = WIN_WIDTH;
    win.bmi.bmiHeader.biHeight = -WIN_HEIGHT;
    win.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    win.bmi.bmiHeader.biPlanes = 1;
    win.bmi.bmiHeader.biBitCount = 32;
    win.bmi.bmiHeader.biCompression = BI_RGB;
    win.hdc = CreateCompatibleDC(0);

    win.hwnd = CreateWindowEx(WS_EX_STATICEDGE,
        winClassName, winClassName,
        WS_POPUP | WS_VISIBLE | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX,
        win.x, win.y,
        win.width, win.height,
        NULL, NULL, win.hInst, NULL);

    DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(win.hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

    winSize(WIN_WIDTH, WIN_HEIGHT);
    start();

    //Update/Render loop
    while (!win.quit) {
        LARGE_INTEGER start_time, end_time, frequency;
        QueryPerformanceFrequency(&frequency); // Get frequency once
        QueryPerformanceCounter(&start_time);

        for (int i = 0; i < 256; i++) {
        }

        memset(win.keyPress, 0, 256 * sizeof(win.keyPress[0])); //Resets keyPressed
        mouse.buttonsPress = 0; mouse.buttonsDown &= ~(M_WHEELDOWN | M_WHEELUP);

        static MSG message = { 0 };
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) { DispatchMessage(&message); }

        update();

        InvalidateRect(win.hwnd, NULL, FALSE); UpdateWindow(win.hwnd);

        QueryPerformanceCounter(&end_time);
        double elapsed_ms = (double)(end_time.QuadPart - start_time.QuadPart) * 1000.0 / frequency.QuadPart;
        if (elapsed_ms < win.targetFrameTimeMs) { Sleep((DWORD)(win.targetFrameTimeMs - elapsed_ms)); }
    }
    if (!&win.close) { win.close(); }
    return 0;
}
//Windows message handling
static LRESULT CALLBACK winProcMsg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static char winFocus = 1;

    switch (message) {
    case WM_CLOSE: DestroyWindow(win.hwnd);
    case WM_QUIT:
    case WM_DESTROY: {
        win.quit = 1; PostQuitMessage(0);
    } break;

    case WM_KILLFOCUS: winFocus = 0; 
        memset(win.keyDown, 0, 256 * sizeof(win.keyDown[0])); 
        memset(win.keyHeld, 0, 256 * sizeof(win.keyHeld[0]));
        mouse.buttonsDown = 0; 
        break;
    case WM_SETFOCUS: winFocus = 1; break;

    //KEYBOARD CONTROLS
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
        if (!winFocus) { break; }
        static char keyTran, keyHold;
        keyTran = ((lParam & (1 << 31)) == 0);
        keyHold = ((lParam & (1 << 30)) != 0);

        if (keyTran == keyHold) { break; }
        win.keyPress[(unsigned char)wParam] = keyTran;
        win.keyDown[(unsigned char)wParam] = keyTran;

    } break;

    //MOUSE CONTROLS
    case WM_MOUSEMOVE: {
        mouse.x = LOWORD(lParam);
        mouse.y = HIWORD(lParam);
    } break;

    case WM_LBUTTONDOWN: mouse.buttonsDown |= M_LEFT; mouse.buttonsPress = mouse.buttonsDown;  break;
    case WM_LBUTTONUP:   mouse.buttonsDown &= ~M_LEFT;   break;
    case WM_MBUTTONDOWN: mouse.buttonsDown |= M_MIDDLE; mouse.buttonsPress = mouse.buttonsDown; break;
    case WM_MBUTTONUP:   mouse.buttonsDown &= ~M_MIDDLE; break;
    case WM_RBUTTONDOWN: mouse.buttonsDown |= M_RIGHT; mouse.buttonsPress = mouse.buttonsDown;  break;
    case WM_RBUTTONUP:   mouse.buttonsDown &= ~M_RIGHT;  break;

    case WM_XBUTTONDOWN: {
        if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) { mouse.buttonsDown |= M_X1; }
        else { mouse.buttonsDown |= M_X2; }
        mouse.buttonsPress = mouse.buttonsDown;
    } break;
    case WM_XBUTTONUP: {
        if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) { mouse.buttonsDown &= ~M_X1; }
        else { mouse.buttonsDown &= ~M_X2; }
    } break;

    case WM_MOUSEWHEEL: {
        if (wParam & 0b10000000000000000000000000000000) { mouse.buttonsDown |= M_WHEELDOWN; }
        else { mouse.buttonsDown |= M_WHEELUP; }
        mouse.buttonsPress = mouse.buttonsDown;
    } break;


    case WM_PAINT: {
        static PAINTSTRUCT paint;
        static HDC device_context;
        device_context = BeginPaint(hwnd, &paint);

        BitBlt(device_context,
            paint.rcPaint.left, paint.rcPaint.top,
            paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top,
            win.hdc,
            paint.rcPaint.left, paint.rcPaint.top,
            SRCCOPY);

        EndPaint(hwnd, &paint);
    } break;

    case WM_SIZE: winSize(LOWORD(lParam), HIWORD(lParam)); break;

    case WM_SHOWWINDOW: {
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd); //ensures it comes to the front, optional
        }
    } break;

    default: return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}


//AUDIO SETUP

void initAudio(int sampleRate, void (*proc)(short*, int, int)) {
    win.process = proc;

    WAVEFORMATEX format;
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = sampleRate;
    format.wBitsPerSample = 16;
    format.cbSize = 0;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    waveOutOpen(&win.waveOut, WAVE_MAPPER, &format, (DWORD_PTR)waveOutProc, (DWORD_PTR)NULL, CALLBACK_FUNCTION);

    //Inital formatting of the waveOut header
    waveOutSetVolume(win.waveOut, 0xFFFFFFFF);
    for (int i = 0; i < CHUNK_AMT; ++i) {
        win.bufIndex = i; win.process(win.buffers[win.bufIndex], CHUNK_SIZE >> 1, 0);
        win.header[i].lpData = (CHAR*)win.buffers[i];
        win.header[i].dwBufferLength = CHUNK_SIZE * 2;

        waveOutPrepareHeader(win.waveOut, &win.header[i], sizeof(win.header[i]));
        waveOutWrite(win.waveOut, &win.header[i], sizeof(win.header[i]));
    }
    win.bufIndex = 0;
    win.sample = 0;

    
}
void CALLBACK waveOutProc(HWAVEOUT wave_out_handle, UINT message, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR param2) {
    if (message == WOM_DONE) 
    {
        //Calls your 'process' function to fill the buffer, writes the buffer to waveOut device, then advances the buffer index
        win.process(win.buffers[win.bufIndex], CHUNK_SIZE >> 1, win.sample);
        waveOutWrite(win.waveOut, &win.header[win.bufIndex], sizeof(win.header[win.bufIndex]));
        win.bufIndex = (win.bufIndex + 1) % CHUNK_AMT;
    }  
}
char* deviceName() {
    WAVEOUTCAPS caps;
    MMRESULT hr = waveOutGetDevCaps(0, &caps, sizeof(caps));
    char name[32];
    for (int i = 0; i < 32; i++) {
        name[i] = (char)(caps.szPname[i]);
    }
    return name;
}





//WINDOW FUNCTIONS

static void winFPS(int fps) {
    win.targetFPS = fps;
    win.targetFrameTimeMs = 1000.0 / fps;
}
static void winTitle(LPCWSTR title) {
    SetWindowText(win.hwnd, title);
}
static void winPos(int x, int y) {
    win.x = x; win.y = y;
    SetWindowPos(win.hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}
//DONT CALL WINSIZE DOESNT WORK REALLY
static void winSize(int w, int h) {
    win.width = w; win.height = h;
    SetWindowPos(win.hwnd, HWND_TOP, win.x, win.y, w, h, SWP_NOZORDER);

    win.bmi.bmiHeader.biWidth = w;
    win.bmi.bmiHeader.biHeight = -h;

    if (win.frame_bitmap) DeleteObject(win.frame_bitmap);
    win.frame_bitmap = CreateDIBSection(NULL, &win.bmi, DIB_RGB_COLORS, (void**)&win.pixels, 0, 0);
    if (win.frame_bitmap) { SelectObject(win.hdc, win.frame_bitmap); }
}
static int screenWidth() { return GetSystemMetrics(SM_CXSCREEN); }
static int screenHeight() { return GetSystemMetrics(SM_CYSCREEN); }
static void window(LPCWSTR title, int w, int h, int x, int y) {
    winTitle(title); winPos(x, y); winSize(w, h);
}
static unsigned char toLower(unsigned char key) { return (key >= 97 && key <= 122) ? key - 32 : key; }
static unsigned char keyDown(unsigned char key) { return win.keyDown[toLower(key)]; }
static unsigned char keyPress(unsigned char key) { return win.keyPress[toLower(key)]; }
static unsigned char keyType(unsigned char key) { return win.keyHeld[toLower(key)]; }

static unsigned char mouseDown(unsigned char button) { return mouse.buttonsDown & button; }
static unsigned char mousePress(unsigned char button) { return mouse.buttonsPress & button; }


//DRAWING FUNCTIONS

static inline void pixel(int x, int y, unsigned int col) {
    if (x >= win.width || x < 0 || y >= win.height || y < 0) { return; }
    win.pixels[y * win.width + x] = col;
}
static inline unsigned int col(unsigned int r, unsigned int g, unsigned int b) { return (unsigned int)((r << 16) & 0xFF0000) | ((g << 8) & 0xFF00) | (b & 0xFF); }
static inline unsigned int colf(float r, float g, float b) { return col(r*255.0f, g*255.0f, b*255.0f); }
static inline COLORREF col2ref(unsigned int col) { return RGB((col & 0xFF0000) >> 16, (col & 0xFF00) >> 8, col & 0xFF); }

static void rect(int x, int y, int w, int h, unsigned int col) {
    SelectObject(win.hdc, GetStockObject(DC_BRUSH)); SetDCBrushColor(win.hdc, col2ref(col));
    Rectangle(win.hdc, x, y, x + w, y + h);
}
static void cls() { rect(0, 0, win.width, win.height, win.clearCol); }


static void rendChars(int x, int y, const char* text) {
    LOGFONT lf; HFONT hFont;

    ZeroMemory(&lf, sizeof(LOGFONT));
    lf.lfHeight = -win.fontHeight;
    lf.lfWidth = 0; lf.lfWeight = FW_NORMAL; lf.lfCharSet = ANSI_CHARSET; lf.lfQuality = PROOF_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lf.lfOutPrecision = OUT_RASTER_PRECIS;
    lstrcpy(lf.lfFaceName, TEXT("System"));

    hFont = (HFONT)SelectObject(win.hdc, CreateFontIndirect(&lf));
    SetTextColor(win.hdc, col2ref(win.textCol));
    SetBkMode(win.hdc, TRANSPARENT);
    SetTextCharacterExtra(win.hdc, win.fontSpacing);
    TextOutA(win.hdc, x, y, text, lstrlenA(text));
    DeleteObject(hFont);
}

static void text(int x, int y, const char* format, ...) {
    va_list args; va_start(args, format);
    char temp[400]; vsprintf_s(temp, 400, format, args);
    va_end(args); rendChars(x, y, temp);
}
//For "console-like" positioning (each x/y coordinate is size of 1 character)
static void textC(int x, int y, const char* format, ...) {
    va_list args; va_start(args, format);
    char temp[400]; vsprintf_s(temp, 400, format, args);
    va_end(args); rendChars(x * win.fontRSpacing, y * win.fontHeight + 3, temp);
}
 
#endif