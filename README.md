<p align="center">
  <img src="media/molly.png" alt="TITLE" width="700">
</p>

A simple, yet useful audio playback application built for manual transcription. It is designed to
be used with [this](https://github.com/SuckDuck/PIC16-PS-2-Pedal) footpedal, which I created to control multiple 
functions with just one switch. However, it should work with any other footswitch as long as it uses the 
keyboard media keys.

## Key features include:
- MP3, FLAC and WAV support
- Audio loading by chunks
- Back and forth playback
- Speed control via GUI
- Volume control via GUI
- App control via keyboard media keys, even in the background

## Requirements:
- libevdev
- libxbcommon
- SDL2
- SDL2_ttf

## Build and use
1. ensure you have all dependencies installed on your system
2. ensure your user has the appropriate permissions for reading /dev/input
3. run `python build.py` for creating the binary
4. run `python build.py install` for installing
5. run `python build.py uninstall` for uninstalling
6. run `python build.py clear` for clearing

<br>
<p align="center">
  <img src="media/demo.gif" alt="DEMO">
</p>

### Notes:
* If your user doesn't have the appropriate permissions to access the /dev/input events, 
you can still use this software, but you will be prompted to enter your password, so 
your user must be in the sudo group.

* fun fact: I actually made this for my mom!