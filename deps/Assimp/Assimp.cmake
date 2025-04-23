set(patch_command git init && ${PATCH_CMD} ${CMAKE_CURRENT_LIST_DIR}/0001-.patch)
orcaslicer_add_cmake_project(Assimp
  URL                 https://github.com/assimp/assimp/archive/refs/tags/v5.4.3.zip
  URL_HASH            SHA256=795C29716F4AC123B403E53B677E9F32A8605C4A7B2D9904BFAAE3F4053B506D
  PATCH_COMMAND ${patch_command}
#  CMAKE_ARGS
#        -DASSIMP_BUILD_ZLIB=OFF
		
)

#add_dependencies(dep_ZLIB)
if (MSVC)
    add_debug_dep(dep_Assimp)
endif ()