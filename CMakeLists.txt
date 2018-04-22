cmake_minimum_required(VERSION 2.8)
## Compile as C++11
add_compile_options(-std=c++11)

project( ImgageStitching )
find_package( OpenCV REQUIRED )

add_executable( imageStitch src/imageStitch.cpp )
target_link_libraries( imageStitch ${OpenCV_LIBS} )
