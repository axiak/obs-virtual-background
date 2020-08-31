cmake_minimum_required(VERSION 3.5)
project(obs-virtual-background)


include(external/FindLibObs.cmake)
find_package(LibObs REQUIRED)

set (CMAKE_CXX_STANDARD 11)
#set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(FFmpeg REQUIRED
		COMPONENTS avformat avutil swscale swresample
		OPTIONAL_COMPONENTS avcodec)


set(virtualoutput_SOURCES
		src/virtual-background.c
		src/segmentation_client.c src/segmentation_client.h src/virtual-background.h src/scale.h src/scale.c src/segmentation_thread.c src/segmentation_thread.h src/imgarray.c src/imgarray.h)

add_library(virtual-background MODULE
	${virtualoutput_SOURCES}
	${virtualoutput_HEADERS})
	
target_link_libraries(virtual-background
	libobs
	swscale)

if(ARCH EQUAL 64)
	set(ARCH_NAME "x86_64")
else()
	set(ARCH_NAME "i686")
endif()

set_target_properties(virtual-background PROPERTIES PREFIX "")

install(TARGETS virtual-background
	LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/obs-plugins)

install(DIRECTORY locale/
	DESTINATION "${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/virtual-background/locale")

install(FILES src/data/virtual-background.effect
		DESTINATION "${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/virtual-background")