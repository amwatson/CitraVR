<h1 align="center">
  <br>
  <a href="https://citra-emu.org/"><img src="assets/citravr_logo.png" alt="CitraVR" width="200"></a>
  <br>
  <b>CitraVR (Beta)</b>
  <br>
</h1>

<h4 align="center"> Play 3DS homebrew and personal game backups in 3D on the go with your Quest.
</br>
  CitraVR is a GPL-licensed, engineless OpenXR application with all source code publicly available.
</h4>

<p align="center">
  <a href="#compatibility">Compatibility</a> |
  <a href="#releases">Releases</a> |
  <a href="#known-issues">Known Issues</a> |
  <a href="#how-to-install-and-run">How to Install and Run</a> |
  <a href="#building">Building</a> |
  <a href="#need-help">Need Help?</a> |
  <a href="#support">Support</a> |
  <a href="#license">License</a>
</p>

## Introduction
I originally created this project to a be a proof-of-concept of some techniques XR developers were curious about. 
Specifically:
- Building a 2D/3D hybrid app
- Rendering a 2D interactive window of non-VR content in VR
- Using VR layers to get sharp, crisp text and visuals.

A Quest-native (i.e. OpenXR, without a third-party game engine) port of the [Citra 3DS emultor](https://github.com/citra-emu/citra) Seemed like a great and fun way to demonstrate all these things at once.

The project is still small, but I'm looking for ways to improve it as time goes on.

## Features
- Stereoscopic rendering
- Broad controller support
- Large, moveable/resizeable screen
- Playable in mixed reality
- low-overhead port
- Fully-GPL-licesed, 100% independent of the Meta SDK

## Compatibility

### HMDs
CitraVR supports the following devices:
- Meta Quest 2
- Meta Quest Pro
- Meta Quest 3

### Games
For a full list of games that work well on CitraVR, please visit the [CitraVR compatability list](https://docs.google.com/document/d/1Jezf64_s5m1lbj3mD--Ye2ew3su0hTAeBWNx3ViMtHI/edit?usp=sharing)

### Controllers/Input 
CitraVR maps the Quest controllers in a way that makes most games on the platform playable with the default mapping. 

For games that need access to more inputs, or if a player needs to access more inputs faster, CitraVR also supports a multitude of 3rd party wired USB and wireless bluetooth controllers. 

## Releases
Grab the latest release [here](https://github.com/amwatson/CitraVR/releases)

## Known Issues
See the [CitraVR Known Issues](https://github.com/amwatson/CitraVR/wiki/CitraVR-Known-Issues)

## How to Install and Run
- [How to install and run CitraVR on Quest](https://github.com/amwatson/CitraVR/wiki/Install-Run-on-Quest)
- [How to back up 3DS Games](https://github.com/amwatson/CitraVR/wiki/Backing-up-3DS-Games)

## Building
[Building for Quest](https://github.com/amwatson/CitraVR/wiki/Building-for-Quest)

## Need Help? 
[Submit an issue](https://github.com/amwatson/CitraVR/issues), or join the cvr-support channel on [Discord](https://discord.com/channels/747967102895390741/1196505250102792232)

## Support
[Buy me a beer](https://www.buymeacoffee.com/fewerwrong)

You can also [buy the original \(non-VR\) Citra project a beer](https://www.patreon.com/citraemu)

## License
CitraVR is licensed under the GPLv3 (or any later version). Refer to the [LICENSE.txt](https://github.com/amwatson/CitraVR/blob/master/license.txt) file.
