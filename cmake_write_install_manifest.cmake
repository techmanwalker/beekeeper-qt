file(WRITE "${CMAKE_INSTALL_MANIFEST}" "")
foreach(file ${CMAKE_INSTALL_MANIFEST_FILES})
    file(APPEND "${CMAKE_INSTALL_MANIFEST}" "${file}\n")
endforeach()
