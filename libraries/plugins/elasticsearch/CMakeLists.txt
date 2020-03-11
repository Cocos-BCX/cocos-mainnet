file(GLOB HEADERS "include/graphene/elasticsearch/*.hpp")

add_library( graphene_elasticsearch
        elasticsearch_plugin.cpp
           )

find_package( CURL )
if ( CURL_FOUND )
  include_directories ( ${CURL_INCLUDE_DIR} )
  target_link_libraries ( graphene_elasticsearch ${CURL_LIBRARY} )
else ( CURL_FOUND )
  message( FATAL_ERROR "curl not found!" )
endif ( CURL_FOUND )

target_link_libraries( graphene_elasticsearch graphene_chain graphene_app )
target_include_directories( graphene_elasticsearch
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties(elasticsearch_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_elasticsearch

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/elasticsearch" )

