project(SpringMapConvNG)
cmake_minimum_required(VERSION 3.1)

set (CMAKE_CXX_STANDARD 11)
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

if(PREFER_STATIC_LIBS) # inside spring build
	add_definitions(-DIL_STATIC_LIB)
	set(LINKLIBS ${PNG_LIBRARY_RELEASE} ${TIFF_LIBRARY_RELEASE} ${JPEG_LIBRARY} ${ZLIB_LIBRARY_RELEASE})
	set_target_properties(springMapConvNG PROPERTIES LINK_FLAGS "-static" )
	set_target_properties(smfdecompiler PROPERTIES LINK_FLAGS "-static" )
endif()

target_link_libraries(springMapConvNG ${IL_LIBRARIES} ${ILU_LIBRARIES} ${LINKLIBS})
target_link_libraries(smfdecompiler ${IL_LIBRARIES} ${ILU_LIBRARIES} ${LINKLIBS})



install(TARGETS springMapConvNG smfdecompiler RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )

