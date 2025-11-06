//by a111
//To remove console terminal opening, go to the project's Properties->Linker->System and change the Subsystem to Windows

//YOU MUST IMPLEMENT IN YOUR CODE:
//void start()                                 -Called on program startup
//void update()                                -Called every frame
//void process(short* buffer, int size)        -Called every audio block (can leave empty if you don't initAudio())

#ifndef A1WL_H
#define A1WL_H

#pragma comment(linker, "/SUBSYSTEM:windows")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma warning(disable: 28251);

#include <windows.h>
#include <dwmapi.h>
#include <mmsystem.h>

#define CHUNK_AMT 4
#define CHUNK_SIZE 1024

typedef struct Window {
    unsigned char quit;
    int width;
    int height;
    int x;
    int y;

	unsigned int* pixels;

    HINSTANCE hInst;
	HWND hwnd;
	BITMAPINFO bmi;
	HBITMAP frame_bitmap;
	HDC frame_device_context;

	unsigned char keyDown[256];
	unsigned char keyPress[256];
	unsigned int clearCol;

    int fps;
    double targetFrameTimeMs;

    HWAVEOUT waveOut;
    WAVEHDR header[CHUNK_AMT];
    short buffers[CHUNK_AMT][CHUNK_SIZE * 2];
    short bufIndex;
    long sample;
} Window;

struct {
    int x, y;
    unsigned char buttonsDown;
    unsigned char buttonsPress;
} mouse;
enum { M_LEFT = 0b1, M_MIDDLE = 0b10, M_RIGHT = 0b100, M_X1 = 0b1000, M_X2 = 0b10000, M_WHEELDOWN = 0b100000, M_WHEELUP = 0b1000000 };

struct Window win;

static LRESULT CALLBACK winProcMsg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void CALLBACK waveOutProc(HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void start();
void update();
void process(short *buffer, int size);
static void winSize(int w, int h);


//WINDOW SETUP + Entry point

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    win.hInst = hInstance;
    win.quit = 0;
    win.width = 600;
    win.height = 400;
    win.x = 640;
    win.y = 300;
    win.targetFrameTimeMs = 1000.0 / 60;
    win.clearCol = 0x00000A0F;

    static wchar_t* winClassName = L"Untitled";
    static WNDCLASSEX wcx = { 0 };
    wcx.cbSize = sizeof(wcx);
    wcx.lpfnWndProc = winProcMsg;
    wcx.hInstance = win.hInst;
    wcx.lpszClassName = winClassName;
    wcx.lpszMenuName = winClassName;
    RegisterClassEx(&wcx);

    win.bmi.bmiHeader.biWidth = 600;
    win.bmi.bmiHeader.biHeight = -400;
    win.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    win.bmi.bmiHeader.biPlanes = 1;
    win.bmi.bmiHeader.biBitCount = 32;
    win.bmi.bmiHeader.biCompression = BI_RGB;
    win.frame_device_context = CreateCompatibleDC(0);

    win.hwnd = CreateWindowEx(WS_EX_STATICEDGE,
        winClassName, winClassName,
        WS_POPUP | WS_VISIBLE | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX,
        win.x, win.y,
        win.width, win.height,
        NULL, NULL, win.hInst, NULL);

    DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(win.hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

    winSize(600, 400);
    start();

    //Update/Render loop
    while (!win.quit) {
        LARGE_INTEGER start_time, end_time, frequency;
        QueryPerformanceFrequency(&frequency); // Get frequency once
        QueryPerformanceCounter(&start_time);

        memset(win.keyPress, 0, 256 * sizeof(win.keyPress[0])); //Resets keyPressed
        mouse.buttonsPress = 0; mouse.buttonsDown &= ~(M_WHEELDOWN | M_WHEELUP);

        static MSG message = { 0 };
        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) { DispatchMessage(&message); }

        update();

        InvalidateRect(win.hwnd, NULL, FALSE); UpdateWindow(win.hwnd);

        QueryPerformanceCounter(&end_time);
        double elapsed_ms = (double)(end_time.QuadPart - start_time.QuadPart) * 1000.0 / frequency.QuadPart;
        if (elapsed_ms < win.targetFrameTimeMs) {
            Sleep((DWORD)(win.targetFrameTimeMs - elapsed_ms));
        }
    }
    return 0;
}
//Windows message handling
static LRESULT CALLBACK winProcMsg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static char winFocus = 1;

    switch (message) {
    case WM_CLOSE:
        DestroyWindow(win.hwnd);
    case WM_QUIT:
    case WM_DESTROY: {
        win.quit = 1;
        PostQuitMessage(0);
    } break;

    case WM_KILLFOCUS: winFocus = 0; memset(win.keyDown, 0, 256 * sizeof(win.keyDown[0])); mouse.buttonsDown = 0; break;
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

        if (keyTran != keyHold) {
            win.keyPress[(unsigned char)wParam] = keyTran;
            win.keyDown[(unsigned char)wParam] = keyTran;
        }
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
            win.frame_device_context,
            paint.rcPaint.left, paint.rcPaint.top,
            SRCCOPY);
        EndPaint(hwnd, &paint);
    } break;

    case WM_SIZE: {
        winSize(LOWORD(lParam), HIWORD(lParam));
    } break;

    default: {
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    }
    return 0;
}


//AUDIO SETUP

void initAudio(int sampleRate) {
    WAVEFORMATEX format;
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = sampleRate;
    format.wBitsPerSample = 16;
    format.cbSize = 0;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    waveOutOpen(&win.waveOut, WAVE_MAPPER, &format, (DWORD_PTR)waveOutProc, (DWORD_PTR)NULL, CALLBACK_FUNCTION);

    waveOutSetVolume(win.waveOut, 0xFFFFFFFF);
    for (int i = 0; i < CHUNK_AMT; ++i) {
        win.bufIndex = i; process(&win.buffers[win.bufIndex], CHUNK_SIZE / 2);
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
        process(&win.buffers[win.bufIndex], CHUNK_SIZE / 2);

        waveOutWrite(win.waveOut, &win.header[win.bufIndex], sizeof(win.header[win.bufIndex]));
        win.bufIndex = (win.bufIndex + 1) % CHUNK_AMT;
    } 
}





//WINDOW FUNCTIONS

static void winFPS(int fps) {
    win.fps = fps;
    win.targetFrameTimeMs = 1000.0 / fps;
}
static void winTitle(unsigned short* title) {
    SetWindowText(win.hwnd, title);
}
static void winPos(int x, int y) {
    win.x = x; win.y = y;
    SetWindowPos(win.hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}
static void winSize(int w, int h) {
    win.width = w; win.height = h;
    SetWindowPos(win.hwnd, HWND_TOP, win.x, win.y, w, h, SWP_NOZORDER);

    win.bmi.bmiHeader.biWidth = w;
    win.bmi.bmiHeader.biHeight = -h;

    if (win.frame_bitmap) DeleteObject(win.frame_bitmap);
    win.frame_bitmap = CreateDIBSection(NULL, &win.bmi, DIB_RGB_COLORS, (void**)&win.pixels, 0, 0);
    SelectObject(win.frame_device_context, win.frame_bitmap);
}
static void window(unsigned short* title, int w, int h, int x, int y) {
    winTitle(title); winPos(x, y); winSize(w, h);
}
static unsigned char keyDown(unsigned char key) {
    if (key >= 97 && key <= 122) { key -= 32; } //Lowercase to standard case
    return win.keyDown[key];
}
static unsigned char keyPress(unsigned char key) {
    if (key >= 97 && key <= 122) { key -= 32; } //Lowercase to standard case
    return win.keyPress[key];
}
static unsigned char mouseDown(unsigned char button) { return mouse.buttonsDown & button; }
static unsigned char mousePress(unsigned char button) { return mouse.buttonsPress & button; }


//DRAWING FUNCTIONS

static inline void pixel(int x, int y, unsigned int col) {
    if (x >= win.width || x < 0 || y >= win.height || y < 0) { return; }
    win.pixels[y * win.width + x] = col;
}
static inline unsigned int col(unsigned int r, unsigned int g, unsigned int b) {
    return (unsigned int)((r << 16) & 0x00FF0000) | ((g << 8) & 0x0000FF00) | (b & 0x000000FF);
}
static inline unsigned int colf(float r, float g, float b) {
    return col((unsigned int)(r * 255.0f), (unsigned int)(g * 255.0f), (unsigned int)(b * 255.0f));
}
static void cls() {
    for (int i = 0; i < win.width * win.height; i++) {
        win.pixels[i] = win.clearCol;
    }
}

//todo: font


#endif