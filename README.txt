Group: Matthew Pettersson, Andreas Johansson.
Dependencies: Maya Api, Gameplay 3D.


Solution Setup:
The properties in MayaViewerPlugin project needs to be modified.
Additional Include Directories: File path that leads to Maya's "include" folder.
Additional Library Directories: File path that leads to Maya's "lib" folder.

In LoadPlugin.py on line 23 needs a file path to the Output Directory,
aka, where the "MayaViewerPlugin.mll" file is compiled.
File Path: $(SolutionDir)\bin\$(Configuration)-$(PlatformShortName)\

Set MayaViewer project as Startup Project.


Debuging:
Set the port in Maya to "1234". There is a "SetPort.mel" script inside this folder that you can run.
In LoadPlugin.py on line 23, the file path should lead to the Debug virsion if the purpose is to Debug.
Within MayaRun.cpp on line 719 needs a file path to the Output Directory for the Debug virsion.
Build the MayaViewerPlugin project and it should connect.
Run MayaViewer through Visual Studio.


Release:
MayaViewerPlugin can not be run from Visual Studio.
It should be compiled and then the "MayaViewerPlugin.mll" should be placed in Maya's "plug-ins" folder.
Inside Maya, you need to load the plugin by running (loadPlugin "MayaViewerPlugin.mll"). 
Alternatively, you can run the "LoadPlugin.mel" script inside this folder.
Run MayaViewer through Visual Studio or run "MayaViewer.exe" in the Output Directory.

