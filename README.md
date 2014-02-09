=====
GcEAD
=====

DIRECTORIES
===========

Core:
	A few semi-generic files that may be used by all sub-projects
Idac:
	Classes to interface between the hardware drivers and the main application code.
	IdacFactory and IdacProxy are the only two IDAC classes that should be used by
	the main application (other than the settings and capabilities structures).
	IdacProxy runs in the main application thread and shields the main application
	programmer from all driver threads and details.
IdacDriver:
	The general driver code.  This is not really a "driver", since it runs in
	user-space via libusb.
IdacDriver2:
	USB protocol implementation for the IDAC2
IdacDriver4:
	USB protocol implementation for the IDAC4
Model:
	The basic data classes for GC-EAD
Scope:
	Classes to implement non-UI code to support the UI.  Basically, this is a
	representation of the UI in a form that can potentially be unit-tested without
	user-interaction.  This approach wasn't taken until near the end of the project,
	so there's not much in here.
View:
	UI code and main()


USB DRIVERS
===========

The IDAC drivers were hacked together very quickly.  Part of the source-code
for the old proprietary IDAC4 driver was available to me, so that helped.  I
didn't have any source code for IDAC2 though, so I had to make guesses based
on analyzing the USB transfer stream of the proprietary drivers and use what
I had gleaned from the old IDAC4 sourcecode.  There are still a couple loose
ends in the IDAC2 code.


Recording objects:
	Idac: interfaces with hardware
	RecordHandler: converts idac data to raw and display data
	RecordDialog: shows preview of the incoming EAD data and lets user adjust settings
	RecordSettingsDialog: for setting the recording options
	MainScope:
		adds data to the EadFile
		tells ChartWidget to update itself when in Recording view
		auto-stops Idac recording when duration is reached


INSTALL PROGRAMS
================

Installer for windows
---------------------

The installer for Windows is build with NSI.  The following instructions are no longer valid...

* Install Qt SDK.  I installed it in C:\QtSDK with Qt 4.8.1.  Adapt Setup/GcEad-windows.nsi for newer versions or different directories.
* build the project (e.g. by opening GcEad.pro with Qt Creator)
* Install NSIS (I have v2.46 installed)
* Open Setup/GcEad-windows.nsi and compile it.
* Upload Setup/GcEad-x.x.x-windows-installer.exe to sourceforge.net.

Installer for linux
-------------------

I used BitRock InstallBuilder to create the installer for Linux.

* build the project
* cd Setup
* run: ./copylibs
* run: $path_to_builder build GcEad-linux.xml linux

Installer for mac
-----------------

* make sure the Qt SDK is installed
* build the project
* cd release
* macdeployqt GcEad.app -dmg
* since I installed the Qt SDK to ~/local/QtSDK, the above command line is
  ``~/local/QtSDK/Desktop/Qt/4.8.1/gcc/bin/macdeployqt GcEad.app -dmg``
* GcEad.dmg is now ready for deployment