# window-base
This is a small glue library for GLFW, ImGui and some additional ImGui controls like a filepicker, as well as additonal utilities.

The compiled library. depends only on OpenGL and standard libraries, all other dependencies are statically compiled into the library.

## Usage

```cmake
add_subdirectory(path/to/window-base)
target_link_libraries(your-target PRIVATE window-base-1)
target_include_directories(your-target PRIVATE ${window-base-1_SOURCES_DIR} ${window-base-1_INCLUDE_DIRS})
```

## Compiling

```sh
$ cmake <dir with CMakeLists.txt> -B <output dir>
$ cmake --build <output dir>
```
