
#import "PlasmaView.h"

@implementation PlasmaView {
}

// "handle" keyUp and keyDown ecents to avoid the system beeps when we press keys

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent*)event {
    
}

- (void)keyUp:(NSEvent*) event {
    
}

@end
