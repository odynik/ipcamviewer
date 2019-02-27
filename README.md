# ipcamviewer
This is a simple IP camera viewer application over rtsp. The application is based on gstreamer. It reads a hard coded uri from the IP camera and outputs the videostream.

# Dependencies
The current application is based on the gstreamer library. You need to install,
1. [gstreamer-1.14.4](https://gstreamer.freedesktop.org/data/pkg/windows/1.14.4/) 
2. [gstreamer-dev-1.14.4](https://gstreamer.freedesktop.org/data/pkg/windows/1.14.4/gstreamer-1.0-devel-x86_64-1.14.4.msi)

The cmake files are using the PkgConfig,

In a Windows environment (it doesn't mean that it cannot work in other OSes but is not tested yet),

1. [pkg-config](https://sourceforge.net/projects/pkgconfiglite/)
2. or may be better from [here](https://pkg-config.freedesktop.org/releases/)

Current development environment:
* Processor Arch: x86_64
* Target Machine OS: Windows 10
* Compiler: MinGW x86_64-8.1.0-posix-seh-rt_v6-rev0
* IDE project: JetBrains CLion v2018.3.4

 
Future goals,
* Multiple targets with different OSes and Processor Archs

# Gstreamer Pipeline example
This pipeline visualizes the gstreamer pipeline coded in the main.c file. It is going to change as the project evolves.
Gstreamer elements,
1. rtspsrc - IP camera rstp feed.
2. rtph264depay - Depayload the rtp packets to acquire the encoded video data.
3. decodebin - Flexible decoder to decode the video packets.
4. videoconvert -  
5. autovideosink - Show the video stream on the screen.

![Alt](draw.io/images/ipcam_pipeline.png)
