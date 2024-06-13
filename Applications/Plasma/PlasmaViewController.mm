
#import "PlasmaViewController.h"
#import "Renderer.h"

#include "pch.h"
#include "Plasma.h"
extern Plasma* gpApp;

@interface PlasmaViewController()
@property (nonatomic, strong) GCMouse* _mouse;
@property (nonatomic, strong) GCKeyboard* _keyboard;
@property (assign) bool _leftMouseButtonDown;
@property (assign) bool _rightMouseButtonDown;
@property (assign) bool _middleMouseButtonDown;
@end

@implementation PlasmaViewController
{
    MTKView *_view;

    Renderer *_renderer;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    _view = (MTKView *)self.view;

    _view.device = MTLCreateSystemDefaultDevice();

    if(!_view.device)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
        return;
    }

    _renderer = [[Renderer alloc] initWithMetalKitView:_view];

    [_renderer mtkView:_view drawableSizeWillChange:_view.drawableSize];

    _view.delegate = _renderer;

    self._leftMouseButtonDown = false;
    self._rightMouseButtonDown = false;
    self._middleMouseButtonDown = false;

    [self setupMouse];
    [self setupKeyboard];
}

- (void)setupKeyboard {
    [[NSNotificationCenter defaultCenter] addObserverForName:GCKeyboardDidConnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification * _Nonnull notification) {
        NSLog(@"Keyboard connected");
        GCKeyboard* keyboard = notification.object;
        self._keyboard = keyboard;
        [self._keyboard.keyboardInput setKeyChangedHandler:^([[maybe_unused]] GCKeyboardInput * _Nonnull keyboard, [[maybe_unused]] GCControllerButtonInput * _Nonnull key, [[maybe_unused]] GCKeyCode keyCode, [[maybe_unused]] BOOL pressed) {
            
            // key up
            if(!pressed) {
                gpApp->onKeyPressed(keyCode);
            }
        }];
    }];
    [[NSNotificationCenter defaultCenter] addObserverForName:GCKeyboardDidDisconnectNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^([[maybe_unused]] NSNotification * _Nonnull notification) {
        NSLog(@"Keyboard disconnected");
    }];
}

- (void)setupMouse {
    if([GCMouse current]) {
        self._mouse = [GCMouse current];
        [self listenToMouseEvents:self._mouse];
    }
        
    [[NSNotificationCenter defaultCenter] addObserverForName:GCMouseDidConnectNotification
                                                      object:nil
                                                       queue:[NSOperationQueue mainQueue]
                                                  usingBlock:^(NSNotification * _Nonnull notification) {
        GCMouse *connectedMouse = notification.object;
        [self listenToMouseEvents:connectedMouse];
    }];
    
    [[NSNotificationCenter defaultCenter] addObserverForName:GCMouseDidDisconnectNotification
                                                      object:nil
                                                       queue:[NSOperationQueue mainQueue]
                                                  usingBlock:^(NSNotification * _Nonnull notification) {
        GCMouse *disconnectedMouse = notification.object;
        if (self._mouse == disconnectedMouse) {
            self._mouse = nil;
        }
    }];
}

- (void)listenToMouseEvents:(GCMouse *)mouse {
    //__weak typeof(self) weakSelf = self;
    mouse.mouseInput.mouseMovedHandler = ^([[maybe_unused]] GCMouseInput * _Nonnull mouse, [[maybe_unused]] float deltaX, [[maybe_unused]] float deltaY) {
        NSPoint location = [NSEvent mouseLocation];
        NSWindow* window = self.view.window;
        NSPoint windowLocation = [window convertPointFromScreen:location];
        if(NSPointInRect(windowLocation, [self.view bounds])) {
            gpApp->onMouseMoved(windowLocation.x, windowLocation.y, self._leftMouseButtonDown, self._middleMouseButtonDown, self._rightMouseButtonDown);
        }
    };
    
    mouse.mouseInput.leftButton.pressedChangedHandler = ^([[maybe_unused]] GCControllerButtonInput * _Nonnull button, [[maybe_unused]] float value, BOOL pressed) {
        self._leftMouseButtonDown = pressed;
    };
    
    mouse.mouseInput.rightButton.pressedChangedHandler = ^([[maybe_unused]] GCControllerButtonInput * _Nonnull button, [[maybe_unused]] float value, BOOL pressed) {
        self._rightMouseButtonDown = pressed;
    };
    
    mouse.mouseInput.middleButton.pressedChangedHandler = ^([[maybe_unused]] GCControllerButtonInput * _Nonnull button, [[maybe_unused]] float value, BOOL pressed) {
        self._middleMouseButtonDown = pressed;
    };
    
    mouse.mouseInput.scroll.valueChangedHandler = ^([[maybe_unused]] GCControllerDirectionPad * _Nonnull dpad, [[maybe_unused]] float xValue, float yValue) {
        gpApp->onMouseWheel(yValue);
    };
}

@end
