file(GLOB SOURCES "src/*.cpp")
include_directories(inc
                    /usr/include
                    )
IF(DEFINE_VERSION)
    ADD_DEFINITIONS(-DDEFINE_VERSION)
ENDIF(DEFINE_VERSION)
ADD_LIBRARY(lua 

        ${SOURCES}
	)
target_link_libraries( lua PUBLIC  m  )
install( TARGETS
   lua

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
