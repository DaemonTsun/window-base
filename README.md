# window-template
This is a template repository for GLFW, ImGui, some of my configuration files and some of my utility libraries.

The compiled executable depends only on OpenGL and standard libraries, all other dependencies are statically compiled into the executable.

## Usage

Clone / use as template, change the project name within [CMakeLists.txt](CMakeLists.txt), possibly modify [src/main.cpp](src/main.cpp) to use the new generated project name macro and insert your code into the [`_update`](src/main.cpp#L18) function.

## Compiling

```sh
$ cmake <dir with CMakeLists.txt> -B <output dir>
$ cmake --build <output dir>
```
