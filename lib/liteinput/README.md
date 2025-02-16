Just a modified and trimmed down version of [libvinput](https://github.com/slendidev/libvinput) by slendidev. It relies on evdev so user needs the 
appropriate permissions to use this, because of that,
I hope to replace this in the future, so it doesn't
deserve its own repo.

### Changes:
- Removed support for windows, macos and X11
- Removed keyboard emulation fuctionality
- Added key release detection
- Modified the API to be more suitable for single-threaded applications
- Adjusted the Makefile to build into a single .o file instead of a .so file