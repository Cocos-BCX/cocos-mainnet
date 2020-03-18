#
# Execute extra library operation command.

PROJECT( extra_library )
CMAKE_MINIMUM_REQUIRED( VERSION 2.8.12 )

FUNCTION( COPY_FILE SRC DEST )

  execute_process(COMMAND cp ${SRC} ${DEST}
    RESULT_VARIABLE COPY_RET
    ERROR_VARIABLE COPY_ERROR
  )

  if ( COPY_RET EQUAL 0 ) 
    set(COPY_OK TRUE PARENT_SCOPE)
    message( STATUS "${SRC} => ${DEST} copy OK" )
  else ( COPY_RET EQUAL 0 )
    message( WARNING "${SRC} => ${DEST} copy Failed - ${COPY_ERROR}" )
  endif ( COPY_RET EQUAL 0 )
  
ENDFUNCTION( COPY_FILE SRC DEST )

if (UNIX)

  if (APPLE)

    set (CMAKE_OS_NAME "OSX" CACHE STRING "Operating system name" FORCE)
	COPY_FILE ( ${CMAKE_CURRENT_SOURCE_DIR}/libprocess_encryption_sierra.a ${CMAKE_CURRENT_BINARY_DIR}/libprocess_encryption.a)

  else (APPLE)

    ## Check for Debian GNU/Linux ________________

    find_file (DEBIAN_FOUND debian_version debconf.conf
      PATHS /etc
      )
    if (DEBIAN_FOUND)
      set (CMAKE_OS_NAME "Debian" CACHE STRING "Operating system name" FORCE)
    endif (DEBIAN_FOUND)

    ##  Check for Fedora _________________________

    find_file (FEDORA_FOUND fedora-release
      PATHS /etc
      )
    if (FEDORA_FOUND)
      set (CMAKE_OS_NAME "Fedora" CACHE STRING "Operating system name" FORCE)
    endif (FEDORA_FOUND)

    ##  Check for RedHat _________________________

    find_file (REDHAT_FOUND redhat-release inittab.RH
      PATHS /etc
      )
    if (REDHAT_FOUND)
      set (CMAKE_OS_NAME "RedHat" CACHE STRING "Operating system name" FORCE)
	  COPY_FILE ( ${CMAKE_CURRENT_SOURCE_DIR}/libprocess_encryption_centos7.a ${CMAKE_CURRENT_BINARY_DIR}/libprocess_encryption.a)
    endif (REDHAT_FOUND)
    
    ## Extra check for Ubuntu ____________________
    
    if (DEBIAN_FOUND)

      ## At its core Ubuntu is a Debian system, with
      ## a slightly altered configuration; hence from
      ## a first superficial inspection a system will
      ## be considered as Debian, which signifies an
      ## extra check is required.

      find_file (UBUNTU_EXTRA legal issue
	    PATHS /etc
	  )

      if (UBUNTU_EXTRA)
	    ## Scan contents of file
	    file (STRINGS ${UBUNTU_EXTRA} UBUNTU_FOUND
	      REGEX Ubuntu
	    )
		
	    ## Check result of string search
	    if (UBUNTU_FOUND)
	      set (CMAKE_OS_NAME "Ubuntu" CACHE STRING "Operating system name" FORCE)
	      set (DEBIAN_FOUND FALSE)
		  
          execute_process(COMMAND lsb_release -cs
		    OUTPUT_VARIABLE RELEASE_CODENAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
          )
		  
		  set (UBUNTU_CODENAME "${RELEASE_CODENAME}" CACHE STRING "Ubuntu code name" FORCE)
		  COPY_FILE ( ${CMAKE_CURRENT_SOURCE_DIR}/libprocess_encryption_${UBUNTU_CODENAME}.a ${CMAKE_CURRENT_BINARY_DIR}/libprocess_encryption.a)
		  
	    endif (UBUNTU_FOUND)

      endif (UBUNTU_EXTRA)
      
    endif (DEBIAN_FOUND)
    
  endif (APPLE)

endif (UNIX)

if ( COPY_OK )
  set ( PE_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE) 
endif ( COPY_OK )