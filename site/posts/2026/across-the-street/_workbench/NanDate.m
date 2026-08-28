#import <Foundation/Foundation.h>

int main(void) {
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:NAN];

    NSDateFormatter *formatter = [NSDateFormatter new];
    formatter.dateStyle = NSDateFormatterFullStyle;
    formatter.timeStyle = NSDateFormatterFullStyle;

    NSLog(@"date: %@", date);
    NSLog(@"timeIntervalSinceReferenceDate: %f", date.timeIntervalSinceReferenceDate);
    NSLog(@"formatted: %@", [formatter stringFromDate:date]);

    return 0;
}
