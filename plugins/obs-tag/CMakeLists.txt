project(obs-tag)

include_directories(${OBS_JANSSON_INCLUDE_DIRS})

if(MSVC)
	set(obs-tag_PLATFORM_DEPS
		w32-pthreads)
endif()

set(obs-tag_HEADERS
	tft.h
	lhash.h
	shash.h)

set(obs-tag_SOURCES
	obs-tag.c
	obs-sourceview.c
	tft.c
	shash.c
	lhash.c)

add_library(obs-tag MODULE
	${obs-tag_HEADERS}
	${obs-tag_SOURCES})
target_link_libraries(obs-tag
	libobs
	${obs-tag_PLATFORM_DEPS}
	${OBS_JANSSON_IMPORT})

install_obs_plugin_with_data(obs-tag data)
