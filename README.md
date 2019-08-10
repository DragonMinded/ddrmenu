# DDRMenu

A launcher menu for DDR that uses the menu buttons to select games.

Includes a Visual Studio 2008 solution that compiles to a static executable which runs on XPE, suitable for use on a DDR cabinet.

The ini file format is simple. For each game, there should be a section which is the game name. For each section, there should be a "launch" key which points to the full path of the executable or batch file to execute when selecting this option. An example is below:

```
[2014]
launch=D:\2014\contents\gamestart.bat

[2013]
launch=D:\2013\contents\gamestart.bat

[X3 Vs. 2nd Mix]
launch=D:\X3\contents\gamestart.bat

[X2]
launch=D:\X2\contents\gamestart.bat
```

To correctly execute the built code, run the executable with one parameter specifying the location of the INI file. An example invocation is as follows:

```
DDRMenu.exe games.ini
```
