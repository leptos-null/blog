ARCH ?= arm64
IOS_MIN ?= 17.0
SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
IOS_SUPPORT := $(SDKROOT)/System/iOSSupport
CC := clang
CFLAGS := -target $(ARCH)-apple-ios$(IOS_MIN)-macabi -isysroot $(SDKROOT) -fobjc-arc \
	-Wall \
	-iframework $(IOS_SUPPORT)/System/Library/Frameworks \
	-I$(IOS_SUPPORT)/usr/include
LDFLAGS := -F$(IOS_SUPPORT)/System/Library/Frameworks \
	-L$(IOS_SUPPORT)/usr/lib

PrayerTimes: PrayerTimes.m
	$(CC) $(CFLAGS) $(LDFLAGS) -framework Foundation -framework UIKit -framework CoreGraphics $< -o $@

NanDate: NanDate.m
	$(CC) $(CFLAGS) $(LDFLAGS) -framework Foundation $< -o $@

run-original-crash: PrayerTimes
	./PrayerTimes

run-nan-date: NanDate
	./NanDate

clean:
	rm -f PrayerTimes NanDate

.PHONY: run-original-crash run-nan-date clean
