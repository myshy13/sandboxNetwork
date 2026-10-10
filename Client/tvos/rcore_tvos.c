/**********************************************************************************************
 *
 *   rcore_tvos - Functions to manage window, graphics device and touch inputs
 *
 *   PLATFORM: TVOS
 *       - tvOS (arm64)
 *
 *   LIMITATIONS:
 *       - No keyboard input support
 *       - Gamepads: extended gamepads only, polled from GameController into raylib's gamepad
 *         state (the Siri Remote is a micro gamepad and is not read)
 *
 *   ADDITIONAL NOTES:
 *       - Adapted from raylib-iOS's rcore_ios.c. The UIKit/lifecycle glue is copied nearly
 *         verbatim (tvOS shares that API surface with iOS); the graphics bring-up is
 *         rewritten from ANGLE/EGL to native EAGLContext + CAEAGLLayer, because tvOS's SDK
 *         still ships a real OpenGLES.framework (unlike current iOS, which dropped it and
 *         requires ANGLE to translate GLES calls to Metal). No ANGLE binaries needed here.
 *       - The view controller is a GCEventViewController with controllerUserInteractionEnabled
 *         = NO, so controller buttons (Menu included) reach the game through GameController
 *         instead of UIKit's focus engine and the system's "Menu leaves the app".
 *       - TRACELOG() function is located in raylib [utils] module
 *
 *   DEPENDENCIES:
 *       - OpenGLES.framework, GameController.framework (system, tvOS SDK)
 *       - gestures: Gestures system for touch-ready devices (or simulated from mouse inputs)
 *
 *   LICENSE: zlib/libpng
 *
 *   Copyright (c) 2013-2024 Ramon Santamaria (@raysan5) and contributors
 *
 *   This software is provided "as-is", without any express or implied warranty. In no event
 *   will the authors be held liable for any damages arising from the use of this software.
 *
 *   Permission is granted to anyone to use this software for any purpose, including commercial
 *   applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not claim that you
 *     wrote the original software. If you use this software in a product, an acknowledgment
 *     in the product documentation would be appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *     as being the original software.
 *
 *     3. This notice may not be removed or altered from any source distribution.
 *
 **********************************************************************************************/

#import <OpenGLES/EAGL.h>
#import <QuartzCore/QuartzCore.h>

// tvOS only supports callbacks
// We are not able to give users full control of the game loop
extern void ios_ready();
extern void ios_update(bool);
extern void ios_destroy();

#import <GameController/GameController.h>
#import <UIKit/UIKit.h>

// MARK: - GameViewController interface

@interface GameViewController : GCEventViewController
- (void)update;
@end

// MARK: - SceneDelegate interface

// tvOS mandates the Scene lifecycle (unlike the classic single-window AppDelegate
// pattern rcore_ios.c uses): the system creates a UIWindowScene regardless of Info.plist
// opt-in, and traps if there's no UISceneDelegate to hand it to. So the window is set up
// here instead of in AppDelegate; see the matching UIApplicationSceneManifest in
// tvos/Info.plist, which points the system at this class by name.
@interface SceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(strong, nonatomic) UIWindow* window;
@end

// MARK: - AppDelegate interface

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property(strong, nonatomic) UIWindow* window;
@end

// MARK: -

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct {
    GameViewController* viewController;  // Root view controller

    // Display data
    EAGLContext* context;         // Native GLES context (no EGL/ANGLE needed on tvOS)
    GLuint framebuffer;           // Default framebuffer object
    GLuint colorRenderbuffer;     // Color renderbuffer, storage provided by the CAEAGLLayer
    GLuint depthRenderbuffer;     // Depth renderbuffer
} PlatformData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
extern CoreData CORE;  // Global CORE state context

static PlatformData platform = {0};  // Platform specific data

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
static int MapPointId(UITouch* touch) {
    static UITouch* touchs[MAX_TOUCH_POINTS] = {0};
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) {
        if (touchs[i] == touch) return i + 1;
    }

    // clear unused touch pairs before insert
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) {
        if (touchs[i] == NULL) continue;
        bool found = false;
        for (int j = 0; j < MAX_TOUCH_POINTS; j++) {
            if (CORE.Input.Touch.pointId[j] == i + 1) {
                found = true;
                break;
            }
        }
        if (!found) touchs[i] = NULL;
    }

    for (int i = 0; i < MAX_TOUCH_POINTS; i++) {
        if (touchs[i] == NULL) {
            touchs[i] = touch;
            return i + 1;
        }
    }

    TRACELOG(LOG_ERROR, "Touch point id overflow. This may be a bug!");
    return 0;
}

int InitPlatform(void);     // Initialize platform (graphics, inputs and more)
void ClosePlatform(void);   // Close platform

// Check if application should close
bool WindowShouldClose(void) {
    if (CORE.Window.ready)
        return CORE.Window.shouldClose;
    else
        return true;
}

// Toggle fullscreen mode
void ToggleFullscreen(void) {
    TRACELOG(LOG_WARNING, "ToggleFullscreen() not available on target platform");
}

// Toggle borderless windowed mode
void ToggleBorderlessWindowed(void) {
    TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available on target platform");
}

// Set window state: maximized, if resizable
void MaximizeWindow(void) {
    TRACELOG(LOG_WARNING, "MaximizeWindow() not available on target platform");
}

// Set window state: minimized
void MinimizeWindow(void) {
    TRACELOG(LOG_WARNING, "MinimizeWindow() not available on target platform");
}

// Set window state: not minimized/maximized
void RestoreWindow(void) {
    TRACELOG(LOG_WARNING, "RestoreWindow() not available on target platform");
}

// Set window configuration state using flags
void SetWindowState(unsigned int flags) {
    TRACELOG(LOG_WARNING, "SetWindowState() not available on target platform");
}

// Clear window configuration state flags
void ClearWindowState(unsigned int flags) {
    TRACELOG(LOG_WARNING, "ClearWindowState() not available on target platform");
}

// Set icon for window
void SetWindowIcon(Image image) {
    TRACELOG(LOG_WARNING, "SetWindowIcon() not available on target platform");
}

// Set icon for window
void SetWindowIcons(Image* images, int count) {
    TRACELOG(LOG_WARNING, "SetWindowIcons() not available on target platform");
}

// Set title for window
void SetWindowTitle(const char* title) {
    CORE.Window.title = title;
}

// Set window position on screen (windowed mode)
void SetWindowPosition(int x, int y) {
    TRACELOG(LOG_WARNING, "SetWindowPosition() not available on target platform");
}

// Set monitor for the current window
void SetWindowMonitor(int monitor) {
    TRACELOG(LOG_WARNING, "SetWindowMonitor() not available on target platform");
}

// Set window minimum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMinSize(int width, int height) {
    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;
}

// Set window maximum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMaxSize(int width, int height) {
    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;
}

// Set window dimensions
void SetWindowSize(int width, int height) {
    TRACELOG(LOG_WARNING, "SetWindowSize() not available on target platform");
}

// Set window opacity, value opacity is between 0.0 and 1.0
void SetWindowOpacity(float opacity) {
    TRACELOG(LOG_WARNING, "SetWindowOpacity() not available on target platform");
}

// Set window focused
void SetWindowFocused(void) {
    TRACELOG(LOG_WARNING, "SetWindowFocused() not available on target platform");
}

// Get native window handle
void* GetWindowHandle(void) {
    TRACELOG(LOG_WARNING, "GetWindowHandle() not implemented on target platform");
    return NULL;
}

// Get number of monitors
int GetMonitorCount(void) {
    return 1;
}

// Get current monitor id
int GetCurrentMonitor(void) {
    return 0;
}

// Get selected monitor position
Vector2 GetMonitorPosition(int monitor) {
    return (Vector2){0, 0};
}

// Get selected monitor width (currently used by monitor)
int GetMonitorWidth(int monitor) {
    return CORE.Window.screen.width;
}

// Get selected monitor height (currently used by monitor)
int GetMonitorHeight(int monitor) {
    return CORE.Window.screen.height;
}

// Get selected monitor physical width in millimetres
int GetMonitorPhysicalWidth(int monitor) {
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalWidth() not implemented on target platform");
    return 0;
}

// Get selected monitor physical height in millimetres
int GetMonitorPhysicalHeight(int monitor) {
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalHeight() not implemented on target platform");
    return 0;
}

// Get selected monitor refresh rate
int GetMonitorRefreshRate(int monitor) {
    TRACELOG(LOG_WARNING, "GetMonitorRefreshRate() not implemented on target platform");
    return 0;
}

// Get the human-readable, UTF-8 encoded name of the selected monitor
const char* GetMonitorName(int monitor) {
    TRACELOG(LOG_WARNING, "GetMonitorName() not implemented on target platform");
    return "";
}

// Get window position XY on monitor
Vector2 GetWindowPosition(void) {
    TRACELOG(LOG_WARNING, "GetWindowPosition() not implemented on target platform");
    return (Vector2){0, 0};
}

// Get window scale DPI factor for current monitor
Vector2 GetWindowScaleDPI(void) {
    CGFloat scale = [[UIScreen mainScreen] nativeScale];
    return (Vector2){scale, scale};
}

// Set clipboard text content
// NOTE: UIPasteboard is unavailable on tvOS -- there's no system clipboard to set.
void SetClipboardText(const char* text) {
    TRACELOG(LOG_WARNING, "SetClipboardText() not available on target platform");
}

// Get clipboard text content
// NOTE: UIPasteboard is unavailable on tvOS -- there's no system clipboard to read.
const char* GetClipboardText(void) {
    TRACELOG(LOG_WARNING, "GetClipboardText() not available on target platform");
    return "";
}

// Get clipboard image
// NOTE: UIPasteboard is unavailable on tvOS -- there's no system clipboard to read.
Image GetClipboardImage(void) {
    TRACELOG(LOG_WARNING, "GetClipboardImage() not available on target platform");
    return (Image){0};
}

// Show mouse cursor
void ShowCursor(void) {
    CORE.Input.Mouse.cursorHidden = false;
}

// Hides mouse cursor
void HideCursor(void) {
    CORE.Input.Mouse.cursorHidden = true;
}

// Enables cursor (unlock cursor)
void EnableCursor(void) {
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width / 2, CORE.Window.screen.height / 2);

    CORE.Input.Mouse.cursorHidden = false;
}

// Disables cursor (lock cursor)
void DisableCursor(void) {
    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width / 2, CORE.Window.screen.height / 2);

    CORE.Input.Mouse.cursorHidden = true;
}

// Swap back buffer with front buffer (screen drawing)
void SwapScreenBuffer(void) {
    if (platform.context != nil) {
        [EAGLContext setCurrentContext:platform.context];
        glBindRenderbuffer(GL_RENDERBUFFER, platform.colorRenderbuffer);
        BOOL presented = [platform.context presentRenderbuffer:GL_RENDERBUFFER];
        static int logCount = 0;
        if (logCount < 5) {
            TRACELOG(LOG_INFO, "DEBUG: presentRenderbuffer returned %d, layer=%p, bounds=%f,%f fb=%u colorRb=%u",
                     (int)presented, (__bridge void*)platform.viewController.view.layer,
                     platform.viewController.view.bounds.size.width, platform.viewController.view.bounds.size.height,
                     platform.framebuffer, platform.colorRenderbuffer);
            logCount++;
        }
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Misc
//----------------------------------------------------------------------------------

// Get elapsed time measure in seconds since InitTimer()
double GetTime(void) {
    double time = 0.0;
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long int nanoSeconds = (unsigned long long int)ts.tv_sec * 1000000000LLU + (unsigned long long int)ts.tv_nsec;

    time = (double)(nanoSeconds - CORE.Time.base) * 1e-9;  // Elapsed time since InitTimer()

    return time;
}

// Open URL with default system browser (if available)
// NOTE: This function is only safe to use if you control the URL given.
// A user could craft a malicious string performing another action.
// Only call this function yourself not with user input or make sure to check the string yourself.
// Ref: https://github.com/raysan5/raylib/issues/686
void OpenURL(const char* url) {
    NSURL* nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
    [[UIApplication sharedApplication] openURL:nsurl options:@{} completionHandler:nil];
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Inputs
//----------------------------------------------------------------------------------

// Set internal gamepad mappings
int SetGamepadMappings(const char* mappings) {
    TRACELOG(LOG_WARNING, "SetGamepadMappings() not implemented on target platform");
    return 0;
}

// Set gamepad vibration
void SetGamepadVibration(int gamepad, float leftMotor, float rightMotor, float duration) {
    TRACELOG(LOG_WARNING, "GamepadSetVibration() not implemented on target platform");
}

// Set mouse position XY
void SetMousePosition(int x, int y) {
    CORE.Input.Mouse.currentPosition = (Vector2){(float)x, (float)y};
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
}

// Set mouse cursor
void SetMouseCursor(int cursor) {
    TRACELOG(LOG_WARNING, "SetMouseCursor() not implemented on target platform");
}

// Get physical key name.
const char *GetKeyName(int key) {
    TRACELOG(LOG_WARNING, "GetKeyName() not implemented on target platform");
    return "";
}

// Register all input events
// Copies each connected extended gamepad into raylib's gamepad state, in raylib's conventions
// (up on a stick is negative, triggers run -1 released to 1 pressed).
static void PollGamepads(void) {
    NSArray<GCController*>* controllers = [GCController controllers];

    for (int i = 0; i < MAX_GAMEPADS; i++) {
        GCExtendedGamepad* pad = (i < (int)controllers.count) ? controllers[i].extendedGamepad : nil;
        CORE.Input.Gamepad.ready[i] = (pad != nil);
        if (pad == nil) continue;

        const char* name = controllers[i].vendorName.UTF8String;
        snprintf(CORE.Input.Gamepad.name[i], MAX_GAMEPAD_NAME_LENGTH, "%s", name ? name : "Gamepad");

        for (int k = 0; k < MAX_GAMEPAD_BUTTONS; k++) {
            CORE.Input.Gamepad.previousButtonState[i][k] = CORE.Input.Gamepad.currentButtonState[i][k];
        }

        // Home, options and the stick presses are nil on some pads; messaging nil reads as not pressed
        const struct { int button; GCControllerButtonInput* input; } buttons[] = {
            {GAMEPAD_BUTTON_LEFT_FACE_UP,     pad.dpad.up},
            {GAMEPAD_BUTTON_LEFT_FACE_RIGHT,  pad.dpad.right},
            {GAMEPAD_BUTTON_LEFT_FACE_DOWN,   pad.dpad.down},
            {GAMEPAD_BUTTON_LEFT_FACE_LEFT,   pad.dpad.left},
            {GAMEPAD_BUTTON_RIGHT_FACE_UP,    pad.buttonY},
            {GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, pad.buttonB},
            {GAMEPAD_BUTTON_RIGHT_FACE_DOWN,  pad.buttonA},
            {GAMEPAD_BUTTON_RIGHT_FACE_LEFT,  pad.buttonX},
            {GAMEPAD_BUTTON_LEFT_TRIGGER_1,   pad.leftShoulder},
            {GAMEPAD_BUTTON_LEFT_TRIGGER_2,   pad.leftTrigger},
            {GAMEPAD_BUTTON_RIGHT_TRIGGER_1,  pad.rightShoulder},
            {GAMEPAD_BUTTON_RIGHT_TRIGGER_2,  pad.rightTrigger},
            {GAMEPAD_BUTTON_MIDDLE_LEFT,      pad.buttonOptions},
            {GAMEPAD_BUTTON_MIDDLE,           pad.buttonHome},
            {GAMEPAD_BUTTON_MIDDLE_RIGHT,     pad.buttonMenu},
            {GAMEPAD_BUTTON_LEFT_THUMB,       pad.leftThumbstickButton},
            {GAMEPAD_BUTTON_RIGHT_THUMB,      pad.rightThumbstickButton},
        };

        for (size_t k = 0; k < sizeof(buttons) / sizeof(buttons[0]); k++) {
            int button = buttons[k].button;
            bool down = buttons[k].input.isPressed;
            CORE.Input.Gamepad.currentButtonState[i][button] = down;
            if (down && !CORE.Input.Gamepad.previousButtonState[i][button]) CORE.Input.Gamepad.lastButtonPressed = button;
        }

        CORE.Input.Gamepad.axisState[i][GAMEPAD_AXIS_LEFT_X]        = pad.leftThumbstick.xAxis.value;
        CORE.Input.Gamepad.axisState[i][GAMEPAD_AXIS_LEFT_Y]        = -pad.leftThumbstick.yAxis.value;
        CORE.Input.Gamepad.axisState[i][GAMEPAD_AXIS_RIGHT_X]       = pad.rightThumbstick.xAxis.value;
        CORE.Input.Gamepad.axisState[i][GAMEPAD_AXIS_RIGHT_Y]       = -pad.rightThumbstick.yAxis.value;
        CORE.Input.Gamepad.axisState[i][GAMEPAD_AXIS_LEFT_TRIGGER]  = pad.leftTrigger.value * 2.0f - 1.0f;
        CORE.Input.Gamepad.axisState[i][GAMEPAD_AXIS_RIGHT_TRIGGER] = pad.rightTrigger.value * 2.0f - 1.0f;
        CORE.Input.Gamepad.axisCount[i] = 6;
    }
}

void PollInputEvents(void) {
#if defined(SUPPORT_GESTURES_SYSTEM)
    // NOTE: Gestures update must be called every frame to reset gestures correctly
    // because ProcessGestureEvent() is just called on an event, not every frame
    UpdateGestures();
#endif

    // Reset keys/chars pressed registered
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;

    // Reset key repeats
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;

    // Reset last gamepad button/axis registered state
    CORE.Input.Gamepad.lastButtonPressed = 0;  // GAMEPAD_BUTTON_UNKNOWN
    // CORE.Input.Gamepad.axisCount = 0;

    // Register previous touch states
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    // NOTE: Touch positions NOT reset here on purpose.
    // On tvOS, touch[0] maps to mouse. Resetting to (0,0) would snap mouse to origin
    // between frames when no touch is active. Last known position persists instead.

    // Register previous keys states
    // NOTE: Android supports up to 260 keys
    for (int i = 0; i < 260; i++) {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
        CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    }

    PollGamepads();

    // Touches arrive through the UIKit callbacks below
    // https://developer.apple.com/documentation/uikit/touches_presses_and_gestures
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

void SetupWindowSizes(int width, int height) {
    Vector2 scale = GetWindowScaleDPI();
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;
    CORE.Window.display.width = CORE.Window.screen.width * scale.x;
    CORE.Window.display.height = CORE.Window.screen.height * scale.y;
    CORE.Window.render.width = CORE.Window.display.width;
    CORE.Window.render.height = CORE.Window.display.height;
    CORE.Window.currentFbo.width = CORE.Window.display.width;
    CORE.Window.currentFbo.height = CORE.Window.display.height;
    CORE.Window.screenScale = MatrixScale(scale.x, scale.y, 1.0f);
}

// (Re)create the GLES framebuffer/renderbuffers backed by the view's CAEAGLLayer, and size
// them to match. Called once from InitPlatform() and again from RecreatePlatformSurface()
// whenever the layer size changes.
static bool CreateFramebuffer(CAEAGLLayer* layer) {
    [EAGLContext setCurrentContext:platform.context];

    if (platform.framebuffer == 0) glGenFramebuffers(1, &platform.framebuffer);
    if (platform.colorRenderbuffer == 0) glGenRenderbuffers(1, &platform.colorRenderbuffer);
    if (platform.depthRenderbuffer == 0) glGenRenderbuffers(1, &platform.depthRenderbuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, platform.framebuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, platform.colorRenderbuffer);
    [platform.context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, platform.colorRenderbuffer);

    GLint width = 0, height = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);

    glBindRenderbuffer(GL_RENDERBUFFER, platform.depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, platform.depthRenderbuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TRACELOG(LOG_WARNING, "DISPLAY: Failed to create complete GLES framebuffer");
        return false;
    }

    return true;
}

// Initialize platform: graphics, inputs and more
int InitPlatform(void) {
    CORE.Window.flags |= FLAG_FULLSCREEN_MODE;

    platform.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (platform.context == nil) {
        TRACELOG(LOG_WARNING, "DISPLAY: Failed to create EAGL context");
        return -1;
    }

    CAEAGLLayer* layer = (CAEAGLLayer*)platform.viewController.view.layer;
    layer.opaque = YES;
    layer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking : @NO,
        kEAGLDrawablePropertyColorFormat : kEAGLColorFormatRGBA8
    };

    if (!CreateFramebuffer(layer)) return -1;
    rlSetDefaultFramebuffer(platform.framebuffer);  // rlgl's "back to the screen" is this fbo, not 0

    CGSize screenSize = [[UIScreen mainScreen] bounds].size;
    SetupWindowSizes(screenSize.width, screenSize.height);

    TRACELOG(LOG_INFO, "DISPLAY: Device initialized successfully");
    TRACELOG(LOG_INFO, "    > Display size: %i x %i", CORE.Window.display.width, CORE.Window.display.height);
    TRACELOG(LOG_INFO, "    > Screen size:  %i x %i", CORE.Window.screen.width, CORE.Window.screen.height);
    TRACELOG(LOG_INFO, "    > Render size:  %i x %i", GetRenderWidth(), GetRenderHeight());
    TRACELOG(LOG_INFO, "    > Viewport offsets: %i, %i", CORE.Window.renderOffset.x, CORE.Window.renderOffset.y);

    //----------------------------------------------------------------------------
    // NOTE: No extension loader needed here (unlike EGL/ANGLE platforms): GLES
    // symbols are statically linked against OpenGLES.framework, not resolved via
    // a proc-address loader.
    rlLoadExtensions(NULL);

    CORE.Window.ready = true;

    //----------------------------------------------------------------------------
    // Initialize OpenGL context (states and resources)
    // NOTE: CORE.Window.currentFbo.width and CORE.Window.currentFbo.height not used, just stored as globals in rlgl
    rlglInit(CORE.Window.currentFbo.width, CORE.Window.currentFbo.height);

    LoadFontDefault();

    // Setup default viewport
    // NOTE: It updated CORE.Window.render.width and CORE.Window.render.height
    SetupViewport(CORE.Window.currentFbo.width, CORE.Window.currentFbo.height);

    InitTimer();
    CORE.Storage.basePath = GetWorkingDirectory();
    TRACELOG(LOG_INFO, "PLATFORM: TVOS: Initialized successfully");
    return 0;
}

// Close platform
void ClosePlatform(void) {
    // NOTE: raylib-level cleanup (rlglClose, UnloadFontDefault) is done in CloseWindow()
    // before calling ClosePlatform(). Here we only tear down GLES resources.

    if (platform.context != nil) {
        [EAGLContext setCurrentContext:platform.context];

        if (platform.framebuffer != 0) {
            glDeleteFramebuffers(1, &platform.framebuffer);
            platform.framebuffer = 0;
        }
        if (platform.colorRenderbuffer != 0) {
            glDeleteRenderbuffers(1, &platform.colorRenderbuffer);
            platform.colorRenderbuffer = 0;
        }
        if (platform.depthRenderbuffer != 0) {
            glDeleteRenderbuffers(1, &platform.depthRenderbuffer);
            platform.depthRenderbuffer = 0;
        }

        [EAGLContext setCurrentContext:nil];
        platform.context = nil;
    }
}

void RecreatePlatformSurface(void* layer, int width, int height) {
    CreateFramebuffer((__bridge CAEAGLLayer*)layer);

    SetupWindowSizes(width, height);

    SetupViewport(CORE.Window.currentFbo.width, CORE.Window.currentFbo.height);

    TRACELOG(LOG_INFO, "SIZE CHANGED: Surface recreated. New size: %d x %d", width, height);

    ios_update(true);
}

// MARK: - GameView

// +layerClass is a UIView class method, not a UIViewController one -- it has to live on
// the view itself to back self.view.layer with a CAEAGLLayer instead of the default
// CALayer, which renderbufferStorage:fromDrawable: requires.
@interface GameView : UIView
@end

@implementation GameView

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

@end

// MARK: - GameViewController implementation

@implementation GameViewController

- (void)loadView {
    self.view = [[GameView alloc] initWithFrame:[UIScreen mainScreen].bounds];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    platform.viewController = self;
    self.view.contentScaleFactor = [[UIScreen mainScreen] nativeScale];

    // Controllers go to GameController, not the focus engine; otherwise Menu leaves the app
    self.controllerUserInteractionEnabled = NO;
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context) {
      RecreatePlatformSurface((__bridge void*)self.view.layer, (int)size.width, (int)size.height);
    } completion:nil];
}

- (void)update {
    if (IsWindowFocused())
        ios_update(false);
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
    return false;
}

static void SyncAllTouches(UIEvent* event) {
    CORE.Input.Touch.pointCount = (int)event.allTouches.count;
    int i = 0;
    for (UITouch* touch in event.allTouches) {
        CGPoint location = [touch locationInView:platform.viewController.view];
        CORE.Input.Touch.position[i] = (Vector2){location.x, location.y};
        CORE.Input.Touch.pointId[i] = MapPointId(touch);
        i++;
        if (i >= MAX_TOUCH_POINTS) break;
    }
    // NOTE: Touch positions from UIKit are in view points, which already match
    // CORE.Window.screen.width/height on fullscreen. No normalization needed.
    // The contentScaleFactor is set to nativeScale, so view bounds == screen bounds.
}

static int IndexOf(int needle, int* haystack, int size) {
    for (int i = 0; i < size; i++)
        if (haystack[i] == needle) return i;
    return -1;
}

static void SendGestureEvent(NSSet<UITouch*>* touches, int action) {
#if defined(SUPPORT_GESTURES_SYSTEM)
    GestureEvent gestureEvent = {0};

    gestureEvent.pointCount = CORE.Input.Touch.pointCount;

    // Register touch actions
    gestureEvent.touchAction = action;

    for (int i = 0; (i < gestureEvent.pointCount) && (i < MAX_TOUCH_POINTS); i++) {
        gestureEvent.pointId[i] = CORE.Input.Touch.pointId[i];
        gestureEvent.position[i] = CORE.Input.Touch.position[i];
        gestureEvent.position[i].x /= (float)GetScreenWidth();
        gestureEvent.position[i].y /= (float)GetScreenHeight();
    }

    // Gesture data is sent to gestures system for processing
    ProcessGestureEvent(gestureEvent);
#endif

    if (action == TOUCH_ACTION_UP) {
        // One of the touchpoints is released, remove it from touch point arrays
        for (UITouch* touch in touches) {
            int size = CORE.Input.Touch.pointCount;
            if (size > MAX_TOUCH_POINTS) size = MAX_TOUCH_POINTS;
            int i = IndexOf(MapPointId(touch), CORE.Input.Touch.pointId, size);
            if (i >= 0) {
                // remove i-th touch point
                for (int j = i; j < size - 1; j++) {
                    CORE.Input.Touch.pointId[j] = CORE.Input.Touch.pointId[j + 1];
                    CORE.Input.Touch.position[j] = CORE.Input.Touch.position[j + 1];
                }
                CORE.Input.Touch.pointCount--;
            } else {
                TRACELOG(LOG_WARNING, "Touch point not found. This may be a bug!");
            }
        }
    }

    if (CORE.Input.Touch.pointCount > 0)
        CORE.Input.Touch.currentTouchState[MOUSE_BUTTON_LEFT] = 1;
    else
        CORE.Input.Touch.currentTouchState[MOUSE_BUTTON_LEFT] = 0;

    if (action == TOUCH_ACTION_MOVE) {
        CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
    } else {
        CORE.Input.Mouse.previousPosition = CORE.Input.Touch.position[0];
    }

    // Map touch[0] as mouse input for convenience
    CORE.Input.Mouse.currentPosition = CORE.Input.Touch.position[0];
    CORE.Input.Mouse.currentWheelMove = (Vector2){0.0f, 0.0f};
}

// touch callbacks
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    SyncAllTouches(event);
    SendGestureEvent(touches, TOUCH_ACTION_DOWN);
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    SyncAllTouches(event);
    SendGestureEvent(touches, TOUCH_ACTION_UP);
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    SyncAllTouches(event);
    SendGestureEvent(touches, TOUCH_ACTION_MOVE);
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    SyncAllTouches(event);
    SendGestureEvent(touches, TOUCH_ACTION_CANCEL);
}

@end

// MARK: - SceneDelegate implementation

@implementation SceneDelegate

- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions {
    if (![scene isKindOfClass:[UIWindowScene class]]) return;
    UIWindowScene* windowScene = (UIWindowScene*)scene;

    self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
    self.window.rootViewController = [[GameViewController alloc] init];
    [self.window makeKeyAndVisible];
    ios_ready();
    CADisplayLink* displayLink = [CADisplayLink displayLinkWithTarget:self.window.rootViewController selector:@selector(update)];
    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)sceneWillResignActive:(UIScene*)scene {
    CORE.Window.flags |= FLAG_WINDOW_UNFOCUSED;
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
    CORE.Window.flags &= ~FLAG_WINDOW_UNFOCUSED;
}

@end

// MARK: - AppDelegate implementation

@implementation AppDelegate

- (UISceneConfiguration*)application:(UIApplication*)application configurationForConnectingSceneSession:(UISceneSession*)connectingSceneSession options:(UISceneConnectionOptions*)options {
    UISceneConfiguration* config = [[UISceneConfiguration alloc] initWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
    config.delegateClass = [SceneDelegate class];
    return config;
}

- (void)applicationWillTerminate:(UIApplication*)application {
    ios_destroy();

    if (platform.context != nil) {
        // the user does not call CloseWindow() before exiting
        TRACELOG(LOG_ERROR, "DISPLAY: CloseWindow() should be called before terminating the application");
    }
    // If 'platform.context' is already nil, the user has already called 'CloseWindow()'
}

@end

/* main() */
int main(int argc, char* argv[]) {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
}

// EOF
