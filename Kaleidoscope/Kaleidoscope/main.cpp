// Kaleidoscope - C++ / OpenGL
// Controls: Up/Down = segments, Left/Right = palette, Space = pause, Esc = quit

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>
#include <cstdio>

#pragma comment(lib, "opengl32.lib")

// Basic OpenGL types and constants
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef char GLchar;

#define GL_TRIANGLES 0x0004
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82

// Basic OpenGL functions from opengl32.dll
extern "C" {
__declspec(dllimport) const GLubyte* WINAPI glGetString(GLenum name);
__declspec(dllimport) void WINAPI glViewport(GLint x, GLint y, GLsizei width,
                                             GLsizei height);
__declspec(dllimport) void WINAPI glDrawArrays(GLenum mode, GLint first,
                                               GLsizei count);
}

// Function pointer types for the newer OpenGL functions
typedef GLuint(APIENTRY* PFN_glCreateShader)(GLenum type);
typedef void(APIENTRY* PFN_glShaderSource)(GLuint shader, GLsizei count,
                                           const GLchar* const* str,
                                           const GLint* len);
typedef void(APIENTRY* PFN_glCompileShader)(GLuint shader);
typedef void(APIENTRY* PFN_glGetShaderiv)(GLuint shader, GLenum pname,
                                          GLint* params);
typedef void(APIENTRY* PFN_glGetShaderInfoLog)(GLuint shader, GLsizei maxLen,
                                               GLsizei* len, GLchar* log);
typedef void(APIENTRY* PFN_glDeleteShader)(GLuint shader);
typedef GLuint(APIENTRY* PFN_glCreateProgram)(void);
typedef void(APIENTRY* PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void(APIENTRY* PFN_glLinkProgram)(GLuint program);
typedef void(APIENTRY* PFN_glGetProgramiv)(GLuint program, GLenum pname,
                                           GLint* params);
typedef void(APIENTRY* PFN_glGetProgramInfoLog)(GLuint program, GLsizei maxLen,
                                                GLsizei* len, GLchar* log);
typedef void(APIENTRY* PFN_glUseProgram)(GLuint program);
typedef GLint(APIENTRY* PFN_glGetUniformLocation)(GLuint program,
                                                  const GLchar* name);
typedef void(APIENTRY* PFN_glUniform1f)(GLint loc, GLfloat v0);
typedef void(APIENTRY* PFN_glUniform2f)(GLint loc, GLfloat v0, GLfloat v1);
typedef void(APIENTRY* PFN_glUniform1i)(GLint loc, GLint v0);
typedef void(APIENTRY* PFN_glGenVertexArrays)(GLsizei n, GLuint* arrays);
typedef void(APIENTRY* PFN_glBindVertexArray)(GLuint array);
typedef BOOL(APIENTRY* PFN_wglSwapIntervalEXT)(int interval);

// The newer OpenGL functions (filled in by LoadAllGLFunctions)
PFN_glCreateShader glCreateShader;
PFN_glShaderSource glShaderSource;
PFN_glCompileShader glCompileShader;
PFN_glGetShaderiv glGetShaderiv;
PFN_glGetShaderInfoLog glGetShaderInfoLog;
PFN_glDeleteShader glDeleteShader;
PFN_glCreateProgram glCreateProgram;
PFN_glAttachShader glAttachShader;
PFN_glLinkProgram glLinkProgram;
PFN_glGetProgramiv glGetProgramiv;
PFN_glGetProgramInfoLog glGetProgramInfoLog;
PFN_glUseProgram glUseProgram;
PFN_glGetUniformLocation glGetUniformLocation;
PFN_glUniform1f glUniform1f;
PFN_glUniform2f glUniform2f;
PFN_glUniform1i glUniform1i;
PFN_glGenVertexArrays glGenVertexArrays;
PFN_glBindVertexArray glBindVertexArray;
PFN_wglSwapIntervalEXT wglSwapIntervalEXT;

// Finds one OpenGL function by name in the graphics driver
static PROC LoadGLFunction(const char* name) {
  PROC p = wglGetProcAddress(name);
  if (p == nullptr || p == (PROC)1 || p == (PROC)2 || p == (PROC)3 ||
      p == (PROC)-1) {
    p = GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
  }
  return p;
}

// Loads all the newer OpenGL functions, returns false if one is missing
static bool LoadAllGLFunctions() {
#define LOAD(fn)                                       \
  fn = (PFN_##fn)LoadGLFunction(#fn);                  \
  if (!fn) {                                           \
    std::printf("Missing OpenGL function: %s\n", #fn); \
    return false;                                      \
  }
  LOAD(glCreateShader)
  LOAD(glShaderSource) LOAD(glCompileShader) LOAD(glGetShaderiv)
      LOAD(glGetShaderInfoLog) LOAD(glDeleteShader) LOAD(glCreateProgram)
          LOAD(glAttachShader) LOAD(glLinkProgram) LOAD(glGetProgramiv)
              LOAD(glGetProgramInfoLog) LOAD(glUseProgram)
                  LOAD(glGetUniformLocation) LOAD(glUniform1f) LOAD(glUniform2f)
                      LOAD(glUniform1i) LOAD(glGenVertexArrays)
                          LOAD(glBindVertexArray)
#undef LOAD
                              wglSwapIntervalEXT =
                                  (PFN_wglSwapIntervalEXT)LoadGLFunction(
                                      "wglSwapIntervalEXT");
  return true;
}

// Vertex shader: draws one triangle that covers the whole window
static const char* VERTEX_SHADER = R"(
#version 330 core
void main()
{
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Fragment shader: calculates the color of every pixel
static const char* FRAGMENT_SHADER = R"(
#version 330 core
out vec4 fragColor;

uniform vec2  uResolution;
uniform float uTime;
uniform int   uSegments;
uniform int   uPalette;

const float PI = 3.14159265359;

// Turns a number into a color from the chosen palette
vec3 palette(float t)
{
    vec3 a, b, c, d;
    if      (uPalette == 0) { a = vec3(0.5); b = vec3(0.5); c = vec3(1.0);           d = vec3(0.00, 0.33, 0.67); } // Rainbow
    else if (uPalette == 1) { a = vec3(0.5); b = vec3(0.5); c = vec3(1.0);           d = vec3(0.26, 0.42, 0.56); } // Neon
    else if (uPalette == 2) { a = vec3(0.5); b = vec3(0.5); c = vec3(1.0, 0.7, 0.4); d = vec3(0.00, 0.15, 0.20); } // Sunset
    else if (uPalette == 3) { a = vec3(0.5); b = vec3(0.5); c = vec3(1.0, 1.0, 0.5); d = vec3(0.80, 0.90, 0.30); } // Tropical
    else if (uPalette == 4) { a = vec3(0.5); b = vec3(0.5); c = vec3(2.0, 1.0, 0.0); d = vec3(0.50, 0.20, 0.25); } // Candy
    else                    { a = vec3(0.5); b = vec3(0.5); c = vec3(1.0, 1.0, 1.0); d = vec3(0.30, 0.20, 0.20); } // Electric
    return a + b * cos(2.0 * PI * (c * t + d));
}

void main()
{
    // Center the pixel position
    vec2 uv = (gl_FragCoord.xy * 2.0 - uResolution) / uResolution.y;

    // Zoom in and out
    uv *= 1.0 + 0.15 * sin(uTime * 0.3);

    // Mirror into slices (the kaleidoscope effect)
    float radius = length(uv);
    float angle  = atan(uv.y, uv.x) + uTime * 0.12;
    float slice  = 2.0 * PI / float(uSegments);
    angle = mod(angle, slice);
    angle = abs(angle - slice * 0.5);
    uv = vec2(cos(angle), sin(angle)) * radius;

    // Build the glowing pattern
    vec2 uv0   = uv;
    vec3 color = vec3(0.0);

    for (int i = 0; i < 4; i++)
    {
        uv = fract(uv * 1.5) - 0.5;

        float d = length(uv) * exp(-length(uv0));

        vec3 c = palette(length(uv0) + float(i) * 0.4 + uTime * 0.4);

        d = sin(d * 8.0 + uTime) / 8.0;
        d = abs(d);
        d = pow(0.01 / d, 1.2);

        color += c * d;
    }

    // Keep bright areas from turning white
    color = 1.0 - exp(-color * 1.2);

    fragColor = vec4(color, 1.0);
}
)";

// Compiles one shader and prints errors if it fails
static GLuint CompileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::printf("Shader compile error:\n%s\n", log);
    return 0;
  }
  return shader;
}

// Compiles both shaders and links them into one program
static GLuint CreateShaderProgram() {
  GLuint vs = CompileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
  GLuint fs = CompileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
  if (!vs || !fs) return 0;

  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    std::printf("Shader link error:\n%s\n", log);
    return 0;
  }
  return program;
}

// Settings changed with the keyboard
static const int PALETTE_COUNT = 6;
static const char* PALETTE_NAMES[PALETTE_COUNT] = {
    "Rainbow", "Neon", "Sunset", "Tropical", "Candy", "Electric"};

static int g_segments = 8;
static int g_palette = 0;
static bool g_paused = false;
static bool g_running = true;
static bool g_titleDirty = true;

// Handles window events like key presses and closing the window
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                   LPARAM lParam) {
  switch (msg) {
    case WM_KEYDOWN:
      switch (wParam) {
        case VK_UP:
          if (g_segments < 32) g_segments++;
          break;
        case VK_DOWN:
          if (g_segments > 2) g_segments--;
          break;
        case VK_RIGHT:
          g_palette = (g_palette + 1) % PALETTE_COUNT;
          break;
        case VK_LEFT:
          g_palette = (g_palette + PALETTE_COUNT - 1) % PALETTE_COUNT;
          break;
        case VK_SPACE:
          g_paused = !g_paused;
          break;
        case VK_ESCAPE:
          g_running = false;
          break;
      }
      g_titleDirty = true;
      return 0;

    case WM_CLOSE:
    case WM_DESTROY:
      g_running = false;
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Creates the window, sets up OpenGL and runs the main loop
int main() {
  HINSTANCE hInstance = GetModuleHandleW(nullptr);

  // Create the window
  WNDCLASSW wc = {};
  wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"KaleidoscopeWindow";
  RegisterClassW(&wc);

  RECT rect = {0, 0, 1280, 720};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  HWND hwnd = CreateWindowExW(
      0, wc.lpszClassName, L"Kaleidoscope", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
      rect.bottom - rect.top, nullptr, nullptr, hInstance, nullptr);
  if (!hwnd) {
    std::printf("Could not create window.\n");
    return 1;
  }

  // Set up OpenGL
  HDC hdc = GetDC(hwnd);

  PIXELFORMATDESCRIPTOR pfd = {};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.iLayerType = PFD_MAIN_PLANE;
  SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);

  HGLRC glContext = wglCreateContext(hdc);
  wglMakeCurrent(hdc, glContext);

  std::printf("OpenGL version: %s\n", (const char*)glGetString(GL_VERSION));
  std::printf("Graphics card : %s\n\n", (const char*)glGetString(GL_RENDERER));

  if (!LoadAllGLFunctions()) {
    std::printf(
        "Your graphics driver does not support OpenGL 3.3. Try updating it.\n");
    std::getchar();
    return 1;
  }
  if (wglSwapIntervalEXT) wglSwapIntervalEXT(1);

  // Build the shader program
  GLuint program = CreateShaderProgram();
  if (!program) {
    std::printf("Press Enter to exit.\n");
    std::getchar();
    return 1;
  }
  glUseProgram(program);

  GLint locResolution = glGetUniformLocation(program, "uResolution");
  GLint locTime = glGetUniformLocation(program, "uTime");
  GLint locSegments = glGetUniformLocation(program, "uSegments");
  GLint locPalette = glGetUniformLocation(program, "uPalette");

  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  std::printf("Controls:\n");
  std::printf("  Up/Down     : change symmetry\n");
  std::printf("  Left/Right  : change palette\n");
  std::printf("  Space       : pause\n");
  std::printf("  Esc         : quit\n");

  // Main loop
  using Clock = std::chrono::steady_clock;
  auto lastFrame = Clock::now();
  float animTime = 0.0f;

  while (g_running) {
    // Handle window events
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if (!g_running) break;

    // Update the time
    auto now = Clock::now();
    float dt = std::chrono::duration<float>(now - lastFrame).count();
    lastFrame = now;
    if (!g_paused) animTime += dt;

    // Get the window size
    RECT client;
    GetClientRect(hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) {
      Sleep(10);
      continue;
    }

    // Update the window title
    if (g_titleDirty) {
      wchar_t title[128];
      swprintf(title, 128, L"Kaleidoscope  |  Segments: %d  |  Palette: %hs%ls",
               g_segments, PALETTE_NAMES[g_palette],
               g_paused ? L"  |  PAUSED" : L"");
      SetWindowTextW(hwnd, title);
      g_titleDirty = false;
    }

    // Send the settings to the shader and draw
    glViewport(0, 0, width, height);
    glUniform2f(locResolution, (float)width, (float)height);
    glUniform1f(locTime, animTime);
    glUniform1i(locSegments, g_segments);
    glUniform1i(locPalette, g_palette);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    SwapBuffers(hdc);
  }

  // Clean up
  wglMakeCurrent(nullptr, nullptr);
  wglDeleteContext(glContext);
  ReleaseDC(hwnd, hdc);
  DestroyWindow(hwnd);
  return 0;
}
