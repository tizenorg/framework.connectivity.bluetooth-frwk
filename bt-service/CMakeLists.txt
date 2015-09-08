CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(bt-service C)

SET(SRCS
marshal.c
bt-service-main.c
bt-service-event-sender.c
bt-service-headset-connection.c
bt-service-event-receiver.c
bt-service-common.c
bt-service-util.c
bt-service-adapter.c
bt-service-adapter-le.c
bt-service-device.c
bt-service-hid.c
bt-service-network.c
bt-service-audio.c
bt-service-oob.c
bt-service-obex-agent.c
bt-service-opp-client.c
bt-service-obex-server.c
bt-service-rfcomm-client.c
bt-service-rfcomm-server.c
bt-service-avrcp.c
bt-service-avrcp-controller.c
bt-request-handler.c
bt-service-agent.c
bt-service-gap-agent.c
bt-service-pbap.c
)

IF("${CMAKE_BUILD_TYPE}" STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Release")
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "")
MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)

SET(PKG_MODULES
	vconf
	aul
	vconf
	libprivilege-control
	syspopup-caller
	dbus-glib-1
	alarm-service
	security-server
	capi-content-mime-type
	capi-network-connection
	pkgmgr
	journal
)

IF("$ENV{CFLAGS}" MATCHES "-DTIZEN_NETWORK_TETHERING_ENABLE")
SET(PKG_MODULES ${PKG_MODULES}
	capi-network-tethering
)
ENDIF()

INCLUDE(FindPkgConfig)
pkg_check_modules(service_pkgs REQUIRED ${PKG_MODULES})

FOREACH(flag ${service_pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag} -Wall")
ENDFOREACH(flag)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIC")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_C_FLAGS_RELEASE "-O2")
SET(APP_SYSCONFDIR /opt/var/lib/bluetooth)

FIND_PROGRAM(UNAME NAMES uname)
EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARCH")
IF("${ARCH}" STREQUAL "arm")
	ADD_DEFINITIONS("-DTARGET")
	MESSAGE("add -DTARGET")
ENDIF("${ARCH}" STREQUAL "arm")

FIND_PROGRAM(MARSHALTOOL NAMES glib-genmarshal)
EXEC_PROGRAM("${MARSHALTOOL}" ARGS "--prefix=marshal ${CMAKE_CURRENT_SOURCE_DIR}/marshal.list --header > ${CMAKE_CURRENT_SOURCE_DIR}/include/marshal.h")
EXEC_PROGRAM("${MARSHALTOOL}" ARGS "--prefix=marshal ${CMAKE_CURRENT_SOURCE_DIR}/marshal.list --body > ${CMAKE_CURRENT_SOURCE_DIR}/marshal.c")

FIND_PROGRAM(DBUS_BINDING_TOOL NAMES dbus-binding-tool)
EXEC_PROGRAM("${DBUS_BINDING_TOOL}" ARGS "--prefix=bt ${CMAKE_CURRENT_SOURCE_DIR}/bt-request-service.xml --mode=glib-server --output=${CMAKE_CURRENT_SOURCE_DIR}/include/bt-service-method.h")
EXEC_PROGRAM("${DBUS_BINDING_TOOL}" ARGS "--prefix=bt_obex_agent ${CMAKE_CURRENT_SOURCE_DIR}/bt-service-obex-agent.xml --mode=glib-server --output=${CMAKE_CURRENT_SOURCE_DIR}/include/bt-obex-agent-method.h")

EXEC_PROGRAM("${DBUS_BINDING_TOOL}" ARGS "--prefix=gap_agent ${CMAKE_CURRENT_SOURCE_DIR}/bt-service-gap-agent1.xml --mode=glib-server --output=${CMAKE_CURRENT_SOURCE_DIR}/include/bt-gap-agent-method.h")

EXEC_PROGRAM("${DBUS_BINDING_TOOL}" ARGS "--prefix=bt_media_agent ${CMAKE_CURRENT_SOURCE_DIR}/bt-service-media-agent.xml --mode=glib-server --output=${CMAKE_CURRENT_SOURCE_DIR}/include/bt-media-agent-method.h")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DVENDOR=\"${APP_VENDOR}\"")
ADD_DEFINITIONS("-DAPPNAME=\"${APP_NAME}\"")
ADD_DEFINITIONS("-DAPP_DIR=\"${APP_DIR}\"")
ADD_DEFINITIONS("-DAPP_LOCALEDIR=\"${APP_LOCALEDIR}\"")
ADD_DEFINITIONS("-DAPP_SYSCONFDIR=\"${APP_SYSCONFDIR}\"")

SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -pie")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${service_pkgs_LDFLAGS})

INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/bluetooth-frwk-service DESTINATION /etc/rc.d/init.d)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/org.projectx.bt.service DESTINATION share/dbus-1/services)
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/auto-pair-blacklist DESTINATION /opt/var/lib/bluetooth/)
IF (TIZEN_WEARABLE)
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/wearable/stack_info DESTINATION /usr/etc/bluetooth/)
ELSE ()
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/mobile/stack_info DESTINATION /usr/etc/bluetooth/)
ENDIF (TIZEN_WEARABLE)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})

