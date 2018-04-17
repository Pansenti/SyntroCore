# SyntroCore

The core Syntro libraries and applications for running a Syntro cloud.

#### Applications

* SyntroControl
* SyntroDB
* SyntroExec
* SyntroLog

#### Libraries

* SyntroControlLib
* SyntroLib
* SyntroGUI

#### C++ Headers

* /usr/include/syntro

#### Package Config

* /usr/lib/pkgconfig/syntro.pc


### Build Dependencies

* Qt5 development libraries and headers
* pkgconfig

### Fetch

        git clone git://github.com/Syntro/SyntroCore.git


### Build (Linux)

        qmake 
        make 
        sudo make install


After the install step on Linux you will be able to build and run [SyntroLCam][1]
and [SyntroView][2].

There are VS2015 solution files for building SyntroCore binaries on Windows.
They are configured for use with Qt5 libraries.

For embedded Linux there is a Yocto meta-layer available [meta-syntro][3] 

### Run

TODO

[1]: https://github.com/Pansenti/SyntroLCam
[2]: https://github.com/Pansenti/SyntroView
[3]: https://github.com/Pansenti/meta-syntro

