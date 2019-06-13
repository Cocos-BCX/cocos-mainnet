file(GLOB HEADERS "include/graphene/debug_witness/*.hpp")

add_library( graphene_debug_witness
             debug_api.cpp
             debug_witness.cpp
           )

target_link_libraries( graphene_debug_witness graphene_chain graphene_app )
target_include_directories( graphene_debug_witness
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   graphene_debug_witness

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
