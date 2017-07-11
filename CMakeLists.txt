project(SpringMapConvNG)
cmake_minimum_required(VERSION 2.6)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(DevIL REQUIRED)


include_directories(${IL_INCLUDE_DIR})

set(MAPCONV_FILES
	src/CRC.cpp
	src/Image.cpp
	src/SMFMap.cpp
	src/TileStorage.cpp
)

add_executable(springMapConvNG src/main.cpp ${MAPCONV_FILES})
add_executable(smfdecompiler src/decompiler.cpp ${MAPCONV_FILES})

target_link_libraries(springMapConvNG ${IL_LIBRARIES})
target_link_libraries(smfdecompiler ${IL_LIBRARIES})

install(TARGETS springMapConvNG smfdecompiler RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )

