file(GLOB HEADERS "include/graphene/witness/*.hpp")

add_library( graphene_witness 
             witness.cpp
           )

target_link_libraries( graphene_witness graphene_chain graphene_app )
target_include_directories( graphene_witness
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   graphene_witness

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
