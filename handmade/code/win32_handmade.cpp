#include <windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>

#define internal static
#define local_persist static
#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

struct win32_ofscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};
struct win32_window_dimension {
  int Width;
  int Height;
};

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
  return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
  return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

// TODO: this is a global for now
global_variable bool GlobalRunning;
global_variable win32_ofscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

internal void
Win32LoadXInput(void) {
  // TODO: test this on windows 8
  HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
  if (!XInputLibrary) {
    XInputLibrary = LoadLibraryA("xinput1_3.dll");
  }
  if (XInputLibrary) {
    XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    if (!XInputGetState) {
      XInputGetState = XInputGetStateStub;
    }
    XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    if (!XInputSetState) {
      XInputSetState = XInputSetStateStub;
    }
  }
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize) {
  WAVEFORMATEX WaveFormat = {};
  WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
  WaveFormat.nChannels = 2;
  WaveFormat.nSamplesPerSec = SamplesPerSecond;
  WaveFormat.wBitsPerSample = 16;
  // API should know this
  WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
  // API should know this
  WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
  WaveFormat.cbSize = 0;
  // actually load the library
  HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
  if (DSoundLibrary) {
    direct_sound_create *DirectSoundCreate = (direct_sound_create *)
    GetProcAddress(DSoundLibrary, "DirectSoundCreate");
    // check if works with different windows versions
    LPDIRECTSOUND DirectSound;
    if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
      if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
        // create the primary buffer
        // ths bufffer is anachronistic - its only purpose is to call SetFormat
        DSBUFFERDESC BufferDescription = {};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))) {
          if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
            // we have finally set the format!
            OutputDebugStringA("Primary buffer format was set.\n");
          }
          else {
            // diagnostics
          }
        }
      }
      else {
        // diagnostics
      }
    }
    else {
      // diagnostic
    }

    DSBUFFERDESC BufferDescription = {};
    BufferDescription.dwSize = sizeof(BufferDescription);
    BufferDescription.dwFlags = 0;
    BufferDescription.dwBufferBytes = BufferSize;
    BufferDescription.lpwfxFormat = &WaveFormat;
    HRESULT result = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
    if (SUCCEEDED(result)) {
      OutputDebugStringA("Secondary buffer created successfully.\n");
    }
  }
  else {
    // diagnostic
  }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window) {
  win32_window_dimension Result;
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Result.Width = ClientRect.right - ClientRect.left;
  Result.Height = ClientRect.bottom - ClientRect.top;
  return(Result);
}

internal void 
RenderWeirdGradient(win32_ofscreen_buffer *Buffer, int BlueOffest, int GreenOffset) {
  uint8 *Row = (uint8 *)Buffer->Memory;  
  for(int Y = 0; Y < Buffer->Height; ++Y) {
    uint32 *Pixel = (uint32 *)Row;
    for(int X = 0; X < Buffer->Width; ++X) {
      /*
        MEMORY ORDER:      RR GG BB xx
        LOADED IN:         xx BB GG RR
        WANTED:            xx RR GG BB
        WIN MEMORY ORDER:  BB GG RR xx
      */
      uint8 Blue = (X + BlueOffest);
      uint8 Green = (Y + GreenOffset);
      // uint8 Red = 150;
      *Pixel++ = (Green << 8) | Blue;
    }
    Row += Buffer->Pitch;
    // Row = (uint8 *)Pixel; <-- also works
  }
}

internal void 
Win32ResizeDIBSection(win32_ofscreen_buffer *Buffer, int Width, int Height) {
  // TODO: bulletproof this
  //  maybe dont free first, free after, then free first if that fails

  if (Buffer->Memory) {
    // TODO: learn what this function does
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  Buffer->Width = Width;
  Buffer->Height = Height;

  // when the biHeight field is negative, this is the clue to
  // Windows to treat this bitmap as top-down, not bottom-up, meaning that the
  // first three bytes of the image are the color for the top left pixel in the 
  // bitmap, not the bottom left!
  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Buffer->Width;
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
  Buffer->Info.bmiHeader.biPlanes = 1;
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;

  // TODO: maybe we can just allocate this ourselves?
  int BytesPerPixel = 4;
  int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;
  // TODO: learn what this function does
  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
  Buffer->Pitch = Buffer->Width * BytesPerPixel;
}

internal void 
Win32DisplayBufferInWindow(win32_ofscreen_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight) {
  // TODO: aspect ratio correction
  // TODO: learn what this function does
  StretchDIBits(
    DeviceContext, 
    0, 0, WindowWidth, WindowHeight,
    0, 0, Buffer->Width, Buffer->Height,
    Buffer->Memory,
    &Buffer->Info,
    DIB_RGB_COLORS,
    SRCCOPY
  );
}

internal LRESULT CALLBACK 
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
  LRESULT Result = 0;
  switch(Message) {
    case WM_SIZE: {

    } break;

    case WM_CLOSE: {
      // TODO: Handle this with a message to the user?
      GlobalRunning = false;
    } break;

    case WM_ACTIVATEAPP: {
    } break;

    case WM_DESTROY: {
      // TODO: Handle this as an error - recreate window?
      GlobalRunning = false;
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
      uint32 VKCode = WParam;
      #define KeyMessageWasDownBit (1 << 30)
      #define KeyMessageIsDownBit (1 << 31)
      bool WasDown = ((LParam & KeyMessageWasDownBit) != 0);
      bool IsDown = ((LParam & KeyMessageIsDownBit) == 0);
      if (WasDown != IsDown) {
        if (VKCode == 'W') {
          
        }
        else if (VKCode == 'A') {

        }
        else if (VKCode == 'S') {

        }
        else if (VKCode == 'D') {

        }
        else if (VKCode == 'Q') {

        }
        else if (VKCode == 'E') {

        }
        else if (VKCode == VK_UP) {

        }
        else if (VKCode == VK_LEFT) {

        }
        else if (VKCode == VK_DOWN) {

        }
        else if (VKCode == VK_RIGHT) {

        }
        else if (VKCode == VK_ESCAPE) {
          OutputDebugStringA("ESCAPE: ");
          if (IsDown) {
            OutputDebugStringA("IsDown ");
          }
          if (WasDown) {
            OutputDebugStringA("WasDown");
          }
          OutputDebugStringA("\n");
        }
        else if (VKCode == VK_SPACE) {

        }
      }
      bool32 AltKeyWasDown = LParam & (1 << 29);
      if (VKCode == VK_F4 && AltKeyWasDown) {
        GlobalRunning = false;
      }
    }


    case WM_PAINT: {
      PAINTSTRUCT Paint;
      HDC DeviceContext = BeginPaint(Window, &Paint);
      win32_window_dimension Dimension = Win32GetWindowDimension(Window);
      Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
      EndPaint(Window, &Paint);
    } break;

    default: {
      Result = DefWindowProc(Window, Message, WParam, LParam);
    } break;
  }
  return(Result);
}

internal int CALLBACK 
WinMain(
  HINSTANCE Instance, 
  HINSTANCE PrevInstance, 
  LPSTR CommandLine, 
  int ShowCode
) {
  Win32LoadXInput();
  WNDCLASSA WindowClass = {};
  Win32ResizeDIBSection(&GlobalBackBuffer, 1288, 728);
  WindowClass.style = CS_HREDRAW|CS_VREDRAW;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  // WindowClass.lpszMenuName = ;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  if (RegisterClassA(&WindowClass)) {
    HWND Window = CreateWindowEx(
      0,
      WindowClass.lpszClassName,
      "Handmade Hero",
      WS_OVERLAPPEDWINDOW|WS_VISIBLE,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      0,
      0,
      Instance,
      0
    );
    if (Window) {
      GlobalRunning = true;

      // graphics test
      int XOffset = 0;
      int YOffset = 0;

      // sound test
      int SamplesPerSecond = 48000;
      int ToneHz = 256;
      int ToneVolume = 10;
      uint32 RunningSampleIndex = 0;
      int SquareWavePeriod = SamplesPerSecond / ToneHz;
      int HalfSquareWavePeriod = SquareWavePeriod / 2;
      int BytesPerSample = sizeof(int16)*2;
      int SecondaryBufferSize = SamplesPerSecond * BytesPerSample;

      Win32InitDSound(Window, SamplesPerSecond, SecondaryBufferSize);
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
      while (GlobalRunning) {
        MSG Message;
        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
          if (Message.message == WM_QUIT) {
            GlobalRunning = false;
          }
          TranslateMessage(&Message);
          DispatchMessageA(&Message);
        }

        // should we do this more frequently?
        for(DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ControllerIndex++) {
          XINPUT_STATE ControllerState;
          if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
            
            bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
            bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
            bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
            bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
            bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
            bool LShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
            bool RShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
            bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
            bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
            bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
            bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

            int16 StickX = Pad->sThumbLX;
            int16 StickY = Pad->sThumbLY;

            XINPUT_VIBRATION Vibration;

            XOffset += StickX >> 12;
            YOffset += StickY >> 12;
          }
          else {
            // diagnostics
          }
        }

        RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);
        // direct sound output test
        DWORD PlayCursor;
        DWORD WriteCursor;
        if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
          DWORD ByteToLock = RunningSampleIndex * BytesPerSample % SecondaryBufferSize;
          DWORD BytesToWrite;
          if (ByteToLock == PlayCursor) {
            BytesToWrite = SecondaryBufferSize;
          }
          // region is split into two - front and back
          else if (ByteToLock > PlayCursor) {
            BytesToWrite = SecondaryBufferSize - ByteToLock;
            BytesToWrite =+ PlayCursor;
          }
          // one region
          else {
            BytesToWrite = PlayCursor - ByteToLock;
          }
          // int16 int16 int16 ...
          // LEFT RIGHT LEFT RIGHT LEFT ...
          VOID *Region1;
          DWORD Region1Size;
          VOID *Region2;
          DWORD Region2Size;

          HRESULT LockResult = GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0);
          if (SUCCEEDED(LockResult)) {
            int16 *SampleOut = (int16 *)Region1;
            DWORD Region1SampleCount = Region1Size / BytesPerSample;
            // TODO: collapse these two loops
            for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++) {
              int16 SampleValue = (RunningSampleIndex++ / HalfSquareWavePeriod) % 2 ? ToneVolume : -ToneVolume;
              *SampleOut++ = SampleValue;
              *SampleOut++ = SampleValue;
            }
            DWORD Region2SampleCount = Region2Size / BytesPerSample;
            SampleOut = (int16 *)Region2;
            for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++) {
              int16 SampleValue = (RunningSampleIndex++ / HalfSquareWavePeriod) % 2 ? ToneVolume : -ToneVolume;
              *SampleOut++ = SampleValue;
              *SampleOut++ = SampleValue;
            }
            GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
          }
          else {
            // diagnostics
          }
        }
        else {
          // diagnostics
        }

        HDC DeviceContext = GetDC(Window);
        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
        // TODO: learn what this function does
        ReleaseDC(Window, DeviceContext);
      }
    }
    else {
      // TODO: logging
    }
  }
  else {
    // TODO: logging
  }

  return(0);
}

