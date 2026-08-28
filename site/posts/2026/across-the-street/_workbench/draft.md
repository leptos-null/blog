# Across the street (a debugging story)

This story takes place in 2018, while I was staying in a hotel in California for a few days.
I was working on code that would eventually make its way into [PrayerTimes](<https://github.com/leptos-null/PrayerTimes>).

On the first evening that I was staying at the hotel, I walked across the street to where a few restaurants were.
When I got back, I saw a crash report for my app- it looked something like this:

```
0   CoreFoundation           __exceptionPreprocess + 176
1   libobjc.A.dylib          objc_exception_throw + 88
2   CoreFoundation           +[NSException exceptionWithName:reason:userInfo:] + 0
3   QuartzCore               _ZN2CA5Layer12set_positionERKNS_4Vec2IdEEb + 172
4   QuartzCore               -[CALayer setPosition:] + 52
5   UIKitCore                -[UIView(_UIViewBacking) _backing_setPosition:] + 168
6   UIKitCore                -[UIView setCenter:] + 212
7   PrayerTimes              -[PTViewController _update] + 76
8   CoreFoundation           __CFNOTIFICATIONCENTER_IS_CALLING_OUT_TO_AN_OBSERVER__ + 148
9   CoreFoundation           ___CFXRegistrationPost_block_invoke + 92
10  CoreFoundation           _CFXRegistrationPost + 440
11  CoreFoundation           _CFXNotificationPost + 740
```

(note: in HTML, this backtrace should have some highlighting - consider checking <https://leptos-null.github.io/ips-page/structured>)

It seemed that the crash was in UIKit (or CoreAnimation). I thought: perhaps I had been in the debugger before and messed with some memory that resulted in this crash.
I didn't look more into it and the app seemed to be running fine back in the hotel.

On the second day, I go across the street to get food again. When I get back, I again notice there's a crash report. The backtrace looks the same as the one I saw yesterday. That certainly seems suspicious.

If you're not familiar: [PrayerTimes](<https://github.com/leptos-null/PrayerTimes>) shows you the times of the prayers for Muslims.
These times are based off of the angle of the Sun from the horizon. Calculating these times requires: a date and position on Earth.

The crash is clearly in the UI part of the code, but with a lack of other hints, I run the app while simulating different times throughout the day as the input to the solar calculations. No crash.
I try simulating the coordinates of the restaurant for good measure; no crash, unsurprisingly- we're in California- nowhere near the equator or the prime meridian or anything that I would expect to considerably change the math.

It occurs to me there could be an issue with the coordinates the app is receiving - maybe there's some weird radio interference in the area.
I simulate a bunch of coordinates around the world. Still not seeing the crash.
I add some logging that persists to the file system, since I won't have a console attached when I go across the street again.
The logging includes the parameters to the main computation (date and position on Earth) and the point passed into `-[UIView setCenter:]`.

On the third day, I go across the street and pay a little bit more attention to when the app crashes- it's right as I'm crossing the street. Back at my laptop, I check the logs: dates and coordinates all look as expected. The `-[UIView setCenter:]` logs give some more information: immediately before the crash, the point is `{1, nan}`. I could see that being a problem. But how did that happen? The dates and locations all look normal. I trace through the code; I check the logs some more.

I found the issue: the `sqrt` (square root) function in the C math library returns `NaN` when the parameter is negative. The code takes the `sqrt` of the elevation to adjust the observed horizon. The hotel and restaurant were just above sea level and the road between them was below sea level, meaning the elevation was negative. When I simulated a bunch of locations on the second day, I only set the coordinates, effectively using a `0` elevation. Up until that point, while working on the project, I hadn't been near sea level at all, and evidently not below it.

I enjoy this story because the issue came down to a very tangible logic bug and partially because the real-world input was fun (being below sea level).
