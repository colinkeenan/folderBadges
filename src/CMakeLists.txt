# ---- core: type table, classifier, xattr store (shared by daemon, CLI, tests) ----
add_library(folderbadges-core STATIC
    core/types.cpp
    core/classifier.cpp
    core/store.cpp
    core/settings.cpp
    ../data/folderbadges.qrc
)
target_include_directories(folderbadges-core PUBLIC core)
target_link_libraries(folderbadges-core PUBLIC Qt6::Core)

# ---- folderbadgesd: the background classifier, D-Bus activated ----
add_executable(folderbadgesd
    daemon/main.cpp
    daemon/service.cpp
)
target_link_libraries(folderbadgesd PRIVATE folderbadges-core Qt6::DBus)
install(TARGETS folderbadgesd DESTINATION ${KDE_INSTALL_BINDIR})

configure_file(daemon/io.github.folderbadges.service.in
               ${CMAKE_CURRENT_BINARY_DIR}/io.github.folderbadges.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.folderbadges.service
        DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})

# ---- folderbadges: command line tool ----
add_executable(folderbadges cli/main.cpp)
target_link_libraries(folderbadges PRIVATE folderbadges-core Qt6::DBus)
install(TARGETS folderbadges DESTINATION ${KDE_INSTALL_BINDIR})

# ---- Dolphin (KIO) overlay icon plugin: only renders what the daemon decided ----
kcoreaddons_add_plugin(folderbadgesoverlay
    SOURCES plugin/folderbadgesplugin.cpp
    INSTALL_NAMESPACE "kf6/overlayicon"
)
target_link_libraries(folderbadgesoverlay PRIVATE
    KF6::KIOCore Qt6::DBus Qt6::Gui)
