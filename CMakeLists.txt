### Generic native Skirmish AI config
#

GetNativeSourcesRecursive(luaSources "${CMAKE_SOURCE_DIR}/rts/lib/lua/src" "")

set(mySourceDirRel         "") # Common values are "" or "src"
set(additionalSources      ${CUtils_SRC} ${luaSources} ${CMAKE_SOURCE_DIR}/rts/System/Util)
## FIXME:
##     "undefined symbol: _ZTIN4creg18IMemberRegistratorE"
##     ("typeinfo for creg::IMemberRegistrator") on dlopen
##     after Spring b1152310caff2a928635f6f1d9e6fe936076176f
##     and/or KAIK 000c7bf2659ceb4d50666cb32b342bcf26ada243
## set(additionalCompileFlags "-DUSING_CREG")
## set(additionalLibraries    ${LegacyCpp_Creg_AIWRAPPER_TARGET})
set(additionalLibraries    ${LegacyCpp_AIWRAPPER_TARGET})
set(additionalCompileFlags "${additionalCompileFlags} -I ${CMAKE_SOURCE_DIR}/rts/lib/lua/include/")
set(additionalCompileFlags "${additionalCompileFlags} -I ${CMAKE_SOURCE_DIR}/rts/lib/lua/src/")
set(additionalCompileFlags "${additionalCompileFlags} -I ${CMAKE_SOURCE_DIR}/rts/lib/streflop/")


ConfigureNativeSkirmishAI(mySourceDirRel additionalSources additionalCompileFlags additionalLibraries)
