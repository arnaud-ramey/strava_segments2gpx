# strava_segments2gpx

Batch-export Strava segments around a point of interest into GPX format for hand-held GPS

Licence
=======

LGPL v3, check file ```LICENCE```.

Dependencies
============

You need the following libraries before compiling :

  * cmake  ( ```$ sudo apt-get install cmake``` ),

How to build the program
========================

The project is based on a ```CMakeLists```.
It is easy to build the program on a Unix computer.
Go in the source folder and type:
```bash
$ mkdir build
$ cd build
$ cmake ..
$ make
```

For Windows users, some instructions are available on OpenCV website:
http://opencv.willowgarage.com/wiki/Getting_started .

How to use the program
======================

To display the help,
from the main folder, run the generated executable '```build/test_strava_segments2gpx``` ' with no arguments.
It will display the help of the program.

Authors
=======

Arnaud Ramey <arnaud.a.ramey@gmail.com>
  -- Robotics Lab, University Carlos III of Madrid
