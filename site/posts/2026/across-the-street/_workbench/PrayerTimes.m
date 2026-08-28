#import <UIKit/UIKit.h>

static NSString *const PTSolarPositionDidChangeNotification = @"PTSolarPositionDidChangeNotification";

// normally this would be a UIViewController, but we don't need all that for the demo
@interface PTViewController : NSObject
@end

@implementation PTViewController {
    UIView *_solarView;
}

- (instancetype)init {
    if (self = [super init]) {
        _solarView = [UIView new];

        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_update)
                                                     name:PTSolarPositionDidChangeNotification object:nil];
    }
    return self;
}

- (void)_update {
    _solarView.center = CGPointMake(1, NAN);
}

@end

int main(void) {
    PTViewController *viewController = [PTViewController new];

    [[NSNotificationCenter defaultCenter] postNotificationName:PTSolarPositionDidChangeNotification object:nil];

    (void)viewController; // make sure viewController is still retained

    return 0;
}
