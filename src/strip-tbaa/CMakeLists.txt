add_executable(strip-tbaa
  main.cc
)

target_link_libraries(
  strip-tbaa
  PRIVATE
  ${LLVM_LIBS}
)

install(
  TARGETS strip-tbaa
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
