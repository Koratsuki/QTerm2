cmake_minimum_required(VERSION 3.16)
project(QTerm2 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Sql Network)

# ---------------------------------------------------------------- libsecret ----
# Credential storage is optional.  When libsecret is missing the application
# still builds and runs; it simply refuses to persist secrets (OpenSSH keeps
# prompting inside the PTY).
find_package(PkgConfig QUIET)
set(QTERM2_HAVE_LIBSECRET OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
endif()
if(LIBSECRET_FOUND)
    set(QTERM2_HAVE_LIBSECRET ON)
    message(STATUS "QTerm2: libsecret found, Secret Service backend enabled")
else()
    message(STATUS "QTerm2: libsecret not found, secrets will not be persisted")
endif()

# ------------------------------------------------------------------- core -----
add_library(qterm2_core STATIC
    # app composition root (depends only on core services)
    src/app/applicationcontext.h    src/app/applicationcontext.cpp
    # core
    src/core/apppaths.h             src/core/apppaths.cpp
    src/core/status.h
    src/core/pathutils.h            src/core/pathutils.cpp
    src/core/processspec.h          src/core/processspec.cpp
    src/core/commandregistry.h      src/core/commandregistry.cpp
    # domain
    src/domain/session.h            src/domain/session.cpp
    src/domain/snippet.h            src/domain/snippet.cpp
    # storage
    src/storage/database.h          src/storage/database.cpp
    src/storage/sessionrepository.h
    src/storage/sqlitesessionrepository.h src/storage/sqlitesessionrepository.cpp
    src/storage/snippetrepository.h
    src/storage/sqlitesnippetrepository.h src/storage/sqlitesnippetrepository.cpp
    src/storage/settingsstore.h     src/storage/settingsstore.cpp
    src/storage/legacymigration.h   src/storage/legacymigration.cpp
    src/storage/syncmanager.h       src/storage/syncmanager.cpp
    # security
    src/security/credentialstore.h  src/security/credentialstore.cpp
    src/security/libsecretcredentialstore.h src/security/libsecretcredentialstore.cpp
    src/security/crypto.h          src/security/crypto.cpp
    # ssh
    src/ssh/keymanager.h            src/ssh/keymanager.cpp
    src/ssh/knownhosts.h            src/ssh/knownhosts.cpp
    src/ssh/sshagent.h              src/ssh/sshagent.cpp
    src/ssh/sshcommandbuilder.h     src/ssh/sshcommandbuilder.cpp
    src/ssh/libssh/libsshsession.h  src/ssh/libssh/libsshsession.cpp
    src/telemetry/telemetry.h       src/telemetry/telemetry.cpp
    src/ssh/sshconfig.h             src/ssh/sshconfig.cpp
    # sessions
    src/sessions/sessionstore.h     src/sessions/sessionstore.cpp
    src/sessions/snippetstore.h     src/sessions/snippetstore.cpp
    src/sessions/sessionlauncher.h  src/sessions/sessionlauncher.cpp
    # terminal
    src/terminal/ptyprocess.h       src/terminal/ptyprocess.cpp
    src/terminal/terminalsession.h
    src/terminal/ptyterminalsession.h src/terminal/ptyterminalsession.cpp
    src/terminal/terminalemulator.h src/terminal/terminalemulator.cpp
    src/terminal/terminalwidget.h   src/terminal/terminalwidget.cpp
    # plugins
    src/plugins/pluginapi.h        src/plugins/pluginapi.cpp
    src/plugins/pluginpermissions.h src/plugins/pluginpermissions.cpp
    src/plugins/pluginmanager.h     src/plugins/pluginmanager.cpp
    src/plugins/pluginmanager_v2.h src/plugins/pluginmanager_v2.cpp
    # ui primitives shared by app and tests
    src/ui/theme.h                  src/ui/theme.cpp
    src/ui/statusbar/statusbarcomponent.h src/ui/statusbar/statusbarcomponent.cpp
    src/ui/statusbar/statusbarmanager.h src/ui/statusbar/statusbarmanager.cpp
    src/ui/statusbar/components/basiccomponents.h src/ui/statusbar/components/basiccomponents.cpp
    src/ui/statusbar/components/systemcomponents.h src/ui/statusbar/components/systemcomponents.cpp
    src/ui/sftp/transferpanel.h         src/ui/sftp/transferpanel.cpp
    src/ui/editor/remoteeditordialog.h  src/ui/editor/remoteeditordialog.cpp
    src/ui/tunnels/tunneldialog.h       src/ui/tunnels/tunneldialog.cpp
    src/tunnels/tunnelmanager.h        src/tunnels/tunnelmanager.cpp
)

target_include_directories(qterm2_core PUBLIC src)
target_link_libraries(qterm2_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network)

if(UNIX AND NOT APPLE)
    target_link_libraries(qterm2_core PUBLIC util)
endif()

if(QTERM2_HAVE_LIBSECRET)
    target_compile_definitions(qterm2_core PRIVATE QTERM2_HAVE_LIBSECRET)
    target_link_libraries(qterm2_core PRIVATE PkgConfig::LIBSECRET)
endif()

# ------------------------------------------------------------- application ----
add_library(qterm2_app STATIC
    src/ui/mainwindow.h             src/ui/mainwindow.cpp
    src/ui/mainwindow.ui
    src/ui/sidebar.h                src/ui/sidebar.cpp
    src/ui/terminalpane.h           src/ui/terminalpane.cpp
    src/ui/sessiondialog.h          src/ui/sessiondialog.cpp
    src/ui/sshconfigdialog.h        src/ui/sshconfigdialog.cpp
    src/ui/knownhostsdialog.h       src/ui/knownhostsdialog.cpp
    src/ui/sshagentdialog.h         src/ui/sshagentdialog.cpp
    src/ui/commandpalettedialog.h   src/ui/commandpalettedialog.cpp
    src/ui/snippetdialog.h          src/ui/snippetdialog.cpp
    src/ui/globalsearchdialog.h     src/ui/globalsearchdialog.cpp
    src/ui/networkdiscoverydialog.h src/ui/networkdiscoverydialog.cpp
    src/ui/servermonitor.h src/ui/servermonitor.cpp
    src/ui/applockdialog.h src/ui/applockdialog.cpp
)
target_link_libraries(qterm2_app PUBLIC qterm2_core)

qt_add_executable(QTerm2 src/main.cpp src/resources.qrc)
target_link_libraries(QTerm2 PRIVATE qterm2_app)

install(TARGETS QTerm2 RUNTIME DESTINATION bin)
install(FILES packaging/QTerm2.desktop DESTINATION share/applications OPTIONAL)
install(FILES packaging/QTerm2.svg
        DESTINATION share/icons/hicolor/scalable/apps
        OPTIONAL)

# ------------------------------------------------------------------ tests -----
enable_testing()

qt_add_executable(qterm2_smoketest tests/smoketest.cpp)
target_link_libraries(qterm2_smoketest PRIVATE qterm2_app)
add_test(NAME smoketest COMMAND qterm2_smoketest)
set_tests_properties(smoketest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(qterm2_uitest tests/uitest.cpp)
target_link_libraries(qterm2_uitest PRIVATE qterm2_app)
add_test(NAME uitest COMMAND qterm2_uitest)
set_tests_properties(uitest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(qterm2_archtest tests/archtest.cpp)
target_link_libraries(qterm2_archtest PRIVATE qterm2_core)
add_test(NAME archtest COMMAND qterm2_archtest)
set_tests_properties(archtest PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# UI files
