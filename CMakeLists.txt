cmake_minimum_required(VERSION 3.16)
# Keep CMake, main.cpp, the Debian changelog and the RPM spec on one version.
project(QTerm2 VERSION 1.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------------------------------------------------------------- warnings ----
# The whole tree is warning-clean today; CI turns this into -Werror so it stays
# that way.  MSVC is left alone (different flag names).
option(QTERM2_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(QTERM2_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Sql Network Concurrent)

# lrelease compiles .ts -> .qm.  The LinguistTools CMake package may not be
# installed even when the binary is present, so locate the executable directly.
# Prefer Qt 6 specific binaries: on Ubuntu /usr/bin/lrelease may be a qtchooser
# wrapper that fails in CI with "could not find a Qt installation of ''".
find_program(QTERM2_LRELEASE_EXECUTABLE
    NAMES lrelease-qt6 lrelease
    HINTS
        ${QT6_INSTALL_PREFIX}/bin
        ${QT6_INSTALL_PREFIX}/libexec
        /usr/lib/qt6/bin
        /usr/lib/qt6/libexec
        /usr/lib64/qt6/bin
        /usr/lib/x86_64-linux-gnu/qt6/bin
        /usr/lib/x86_64-linux-gnu/qt6/libexec
    DOC "Qt lrelease tool for compiling translations")
if(QTERM2_LRELEASE_EXECUTABLE)
    execute_process(
        COMMAND "${QTERM2_LRELEASE_EXECUTABLE}" -version
        RESULT_VARIABLE _qterm2_lrelease_ver_result
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT _qterm2_lrelease_ver_result EQUAL 0)
        message(WARNING "QTerm2: ${QTERM2_LRELEASE_EXECUTABLE} is not usable, falling back to checked-in translations/qterm2_en.qm")
        unset(QTERM2_LRELEASE_EXECUTABLE CACHE)
    endif()
endif()

# ------------------------------------------------------------------ OpenSSL ---
# Encrypted session export uses AES-256-GCM + PBKDF2-HMAC-SHA256.  There is no
# weaker fallback: without OpenSSL the app still builds, but syncing encrypted
# files reports that encryption is unavailable instead of pretending to work.
find_package(OpenSSL QUIET COMPONENTS Crypto)
set(QTERM2_HAVE_OPENSSL OFF)
if(OpenSSL_Crypto_FOUND)
    set(QTERM2_HAVE_OPENSSL ON)
    message(STATUS "QTerm2: OpenSSL found, encrypted session sync enabled")
else()
    message(STATUS "QTerm2: OpenSSL not found, encrypted session sync disabled")
endif()

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
    src/terminal/terminalwidget_search.cpp
    src/terminal/terminalwidget_selection.cpp
    src/terminal/terminalwidget_events.cpp
    src/terminal/broadcast/broadcastmanager.h src/terminal/broadcast/broadcastmanager.cpp
    # tunnels
    src/tunnels/tunnelmanager.h     src/tunnels/tunnelmanager.cpp
    # sftp (experimental service layer)
    src/sftp/sftpservice.h          src/sftp/sftpservice.cpp
    src/sftp/sftpsession.h          src/sftp/sftpsession.cpp
    src/sftp/transferqueue.h        src/sftp/transferqueue.cpp
    # plugins
    src/plugins/pluginapi.h        src/plugins/pluginapi.cpp
    src/plugins/pluginpermissions.h src/plugins/pluginpermissions.cpp
    src/plugins/pluginmanager.h     src/plugins/pluginmanager.cpp
    # ui primitives shared by app and tests
    src/ui/theme.h                  src/ui/theme.cpp
    src/ui/statusbar/statusbarcomponent.h src/ui/statusbar/statusbarcomponent.cpp
    src/ui/statusbar/statusbarmanager.h src/ui/statusbar/statusbarmanager.cpp
    src/ui/statusbar/components/basiccomponents.h src/ui/statusbar/components/basiccomponents.cpp
    src/ui/statusbar/components/systemcomponents.h src/ui/statusbar/components/systemcomponents.cpp
)

target_include_directories(qterm2_core PUBLIC src)
target_link_libraries(qterm2_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Concurrent)

if(UNIX AND NOT APPLE)
    target_link_libraries(qterm2_core PUBLIC util)
endif()

if(QTERM2_HAVE_LIBSECRET)
    target_compile_definitions(qterm2_core PRIVATE QTERM2_HAVE_LIBSECRET)
    target_link_libraries(qterm2_core PRIVATE PkgConfig::LIBSECRET)
endif()

if(QTERM2_HAVE_OPENSSL)
    target_compile_definitions(qterm2_core PRIVATE QTERM2_HAVE_OPENSSL)
    target_link_libraries(qterm2_core PRIVATE OpenSSL::Crypto)
endif()

# ------------------------------------------------------------- application ----
add_library(qterm2_app STATIC
    src/ui/mainwindow.h             src/ui/mainwindow.cpp
    src/ui/mainwindow_sessions.cpp
    src/ui/mainwindow_tools.cpp
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
    src/ui/preferencesdialog.h      src/ui/preferencesdialog.cpp
    src/ui/sshintegration.h         src/ui/sshintegration.cpp
    src/ui/tunnels/tunneldialog.h   src/ui/tunnels/tunneldialog.cpp
    src/ui/sftp/sftpdialog.h        src/ui/sftp/sftpdialog.cpp
    src/ui/networkdiscoverydialog.h src/ui/networkdiscoverydialog.cpp
)
target_link_libraries(qterm2_app PUBLIC qterm2_core Qt6::Concurrent)

qt_add_executable(QTerm2 src/main.cpp src/resources.qrc)
target_link_libraries(QTerm2 PRIVATE qterm2_app)

# Compile the .ts at build time: editing a translation must change the .qm that
# is embedded through resources.qrc, instead of relying on a checked-in blob.
if(QTERM2_LRELEASE_EXECUTABLE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/translations/qterm2_en.qm
        COMMAND ${QTERM2_LRELEASE_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/translations/qterm2_en.ts
                -qm ${CMAKE_CURRENT_SOURCE_DIR}/translations/qterm2_en.qm
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/translations/qterm2_en.ts
        COMMENT "Compiling Qt translations"
        VERBATIM)
    add_custom_target(qterm2_translations ALL
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/translations/qterm2_en.qm)
    add_dependencies(QTerm2 qterm2_translations)
else()
    message(WARNING "QTerm2: lrelease not found, using the checked-in translations/qterm2_en.qm")
endif()

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
