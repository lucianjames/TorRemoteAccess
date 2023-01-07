add_executable(TorRatServer main.cpp)
#target_include_directories(TorRatServer PRIVATE ..)
target_link_libraries(TorRatServer PRIVATE imtui-ncurses)