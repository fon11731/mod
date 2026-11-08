# ============================================================
# 把下面片段合并进项目根目录的 CMakeLists.txt
# ============================================================

# ---- WebUI: cpp-httplib (header-only) ----
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(httplib)

# 在定义 sdvx_training（或你的 DLL 目标）之后追加：
# （下面假设目标名是 sdvx_training，请按实际名称修改）

target_sources(sdvx_training PRIVATE
  src/web/server.cpp
)

target_include_directories(sdvx_training PRIVATE
  ${httplib_SOURCE_DIR}
  # src 一般已经在 include path 里；若没有则加上
  # ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Windows 套接字
if(WIN32)
  target_link_libraries(sdvx_training PRIVATE ws2_32)
endif()

# 可选：MSVC 下关闭一些 httplib 相关警告
# if(MSVC)
#   target_compile_options(sdvx_training PRIVATE /wd4244 /wd4267)
# endif()
