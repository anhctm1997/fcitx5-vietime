add_library(fcitx5-vietime-applications-editor MODULE
    main.cpp applicationseditor.cpp)
set_target_properties(fcitx5-vietime-applications-editor PROPERTIES
    AUTOMOC TRUE PREFIX "")
target_compile_features(fcitx5-vietime-applications-editor PRIVATE cxx_std_20)
target_link_libraries(fcitx5-vietime-applications-editor PRIVATE
    Qt6::Core Qt6::DBus Qt6::Widgets Fcitx5Qt6::WidgetsAddons)
install(TARGETS fcitx5-vietime-applications-editor
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5/qt6")
