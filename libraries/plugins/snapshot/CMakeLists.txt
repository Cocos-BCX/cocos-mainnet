file(GLOB HEADERS "include/graphene/snapshot/*.hpp")

add_library( graphene_snapshot
             snapshot.cpp
           )

target_link_libraries( graphene_snapshot graphene_chain graphene_app )
target_include_directories( graphene_snapshot
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   graphene_snapshot

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
