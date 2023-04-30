Clither
=========
A  cross-platform multi-player 2D arcade  game  implemented  in  plain  C89  and
Vulkan.

How to Build
------------
The tools you will need to build this project are:
+ [CMake](http://www.cmake.org/).
+ A C89 compliant C compiler.
+ [Make](http://www.gnu.org/software/make/) (If you're on Mac/linux).
+ [Git](http://git-scm.com/) (if you want to make to make updating the code easy).

**Windows MSVC Developers** should start the "Visual Studio Command Prompt" from the
start menu instead of CMD. It sets the required environment variables. If you are
using MSYS then you can use regular CMD (or bash).

The general procedure is as follows:
+ cd into the source directory of lightship (where this README is located).
+ Make a new directory called *build*.
+ cd into that directory with your command prompt/terminal/whatever.
+ Type ```cmake ..``` (Two dots are important)

This will configure the project for your platform. Linux/Mac users now type *make* to build.

If you're on Windows, there will now be Visual Studio project files, inside the *build*
directory. So go in there and open them up. Set the target "lightship" as the executable target and build. You're good to go.

