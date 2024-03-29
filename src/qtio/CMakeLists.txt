##############################
###### QTIO library  #########
##############################
if (BUILD_QTIO)
  ###### QTIO Input video suppport  #########
  file(GLOB QTIO_HEADERS_TO_MOC ../../include/qtio/qt_io.hpp)
  qt6_wrap_cpp(QTIO_PROCESSED_MOCS                                                                                                                                                                                                                                                                    
                  ${QTIO_HEADERS_TO_MOC}                                                   
                  TARGET qtio
                  OPTIONS --no-notes) # Don't display a note for the headers which don't produce a moc_*.cpp

  add_library(qtio SHARED qt_input.cpp ${QTIO_PROCESSED_MOCS} )
  target_include_directories(qtio PRIVATE "include/" Qt6::Core Qt6::Multimedia)
  target_compile_options(qtio PRIVATE -fPIC -fvisibility=hidden)
  target_link_libraries(qtio PRIVATE Qt6::Core Qt6::Multimedia FunctionalDag::functional_dag)

  ###### QTIO Output image suppport  #########
  if (CLI_ONLY EQUAL 0)
    file(GLOB HEADERS_TO_MOC ../../include/qtio/image_view.hpp)
    qt6_wrap_cpp(PROCESSED_MOCS                                                                                                                                                                                                                                                                    
                    ${HEADERS_TO_MOC}                                                   
                    TARGET imageview
                    OPTIONS --no-notes) # Don't display a note for the headers which don't produce a moc_*.cpp

    add_library(imageview SHARED imageview_output.cpp  image_view.cpp ${PROCESSED_MOCS})
    target_include_directories(imageview PRIVATE "include/" Qt6::Widgets)
    target_compile_options(imageview PRIVATE -fPIC -fvisibility=hidden)
    target_link_libraries(imageview PRIVATE Qt6::Core Qt6::Widgets FunctionalDag::functional_dag)

    install(TARGETS imageview 
        LIBRARY DESTINATION lib/vin_libs
        ARCHIVE DESTINATION lib/vin_libs
        RUNTIME DESTINATION bin)
  endif ()
  
  install(TARGETS qtio 
      LIBRARY DESTINATION lib/vin_libs
      ARCHIVE DESTINATION lib/vin_libs
      RUNTIME DESTINATION bin)
endif ()

##############################
###### QTOP library  #########
##############################
if (BUILD_QTOP)
  add_library(qtop SHARED qt_ops.cpp)
  target_include_directories(qtop PRIVATE "include/" Qt6::Core Qt6::Multimedia)
  target_compile_options(qtop PRIVATE -fPIC -fvisibility=hidden)
  target_link_libraries(qtop PRIVATE Qt6::Core Qt6::Multimedia FunctionalDag::functional_dag)

  install(TARGETS qtop 
      LIBRARY DESTINATION lib/vin_libs
      ARCHIVE DESTINATION lib/vin_libs
      RUNTIME DESTINATION bin)
endif ()