set(LLVM_LINK_COMPONENTS
  Support
  AsmParser
  BitReader
  Mc
  Option
  )

add_clang_executable(decl-scope
  decl-scope_main.cpp
)

target_link_libraries(decl-scope
  PRIVATE
  clangBasic
  clangFrontend
  clangTooling
  clangSerialization
  clangDriver
  clangRewriteFrontend
  clangRewrite
  clangParse
  clangSema
  clangAnalysis
  clangAST
  clangASTMatchers
  clangEdit
  clangLex
  )

if(UNIX)
  set(CLANGXX_LINK_OR_COPY create_symlink)
else()
  set(CLANGXX_LINK_OR_COPY copy)
endif()
