# Auto-generated by IDE. Any changes made by user will be lost!
BASEDIR =  $$quote($$_PRO_FILE_PWD_)

device {
    CONFIG(debug, debug|release) {
        SOURCES +=  $$quote($$BASEDIR/src/AppSettings.cpp) \
                 $$quote($$BASEDIR/src/FormatDropDown.cpp) \
                 $$quote($$BASEDIR/src/LogicPasteApp.cpp) \
                 $$quote($$BASEDIR/src/PasteFormatter.cpp) \
                 $$quote($$BASEDIR/src/PasteListing.cpp) \
                 $$quote($$BASEDIR/src/PasteModel.cpp) \
                 $$quote($$BASEDIR/src/Pastebin.cpp) \
                 $$quote($$BASEDIR/src/ViewPastePage.cpp) \
                 $$quote($$BASEDIR/src/main.cpp)

        HEADERS +=  $$quote($$BASEDIR/src/AppSettings.h) \
                 $$quote($$BASEDIR/src/FormatDropDown.h) \
                 $$quote($$BASEDIR/src/LogicPasteApp.h) \
                 $$quote($$BASEDIR/src/PasteFormatter.h) \
                 $$quote($$BASEDIR/src/PasteListing.h) \
                 $$quote($$BASEDIR/src/PasteModel.h) \
                 $$quote($$BASEDIR/src/Pastebin.h) \
                 $$quote($$BASEDIR/src/ViewPastePage.h) \
                 $$quote($$BASEDIR/src/config.h)
    }

    CONFIG(release, debug|release) {
        SOURCES +=  $$quote($$BASEDIR/src/AppSettings.cpp) \
                 $$quote($$BASEDIR/src/FormatDropDown.cpp) \
                 $$quote($$BASEDIR/src/LogicPasteApp.cpp) \
                 $$quote($$BASEDIR/src/PasteFormatter.cpp) \
                 $$quote($$BASEDIR/src/PasteListing.cpp) \
                 $$quote($$BASEDIR/src/PasteModel.cpp) \
                 $$quote($$BASEDIR/src/Pastebin.cpp) \
                 $$quote($$BASEDIR/src/ViewPastePage.cpp) \
                 $$quote($$BASEDIR/src/main.cpp)

        HEADERS +=  $$quote($$BASEDIR/src/AppSettings.h) \
                 $$quote($$BASEDIR/src/FormatDropDown.h) \
                 $$quote($$BASEDIR/src/LogicPasteApp.h) \
                 $$quote($$BASEDIR/src/PasteFormatter.h) \
                 $$quote($$BASEDIR/src/PasteListing.h) \
                 $$quote($$BASEDIR/src/PasteModel.h) \
                 $$quote($$BASEDIR/src/Pastebin.h) \
                 $$quote($$BASEDIR/src/ViewPastePage.h) \
                 $$quote($$BASEDIR/src/config.h)
    }
}

simulator {
    CONFIG(debug, debug|release) {
        SOURCES +=  $$quote($$BASEDIR/src/AppSettings.cpp) \
                 $$quote($$BASEDIR/src/FormatDropDown.cpp) \
                 $$quote($$BASEDIR/src/LogicPasteApp.cpp) \
                 $$quote($$BASEDIR/src/PasteFormatter.cpp) \
                 $$quote($$BASEDIR/src/PasteListing.cpp) \
                 $$quote($$BASEDIR/src/PasteModel.cpp) \
                 $$quote($$BASEDIR/src/Pastebin.cpp) \
                 $$quote($$BASEDIR/src/ViewPastePage.cpp) \
                 $$quote($$BASEDIR/src/main.cpp)

        HEADERS +=  $$quote($$BASEDIR/src/AppSettings.h) \
                 $$quote($$BASEDIR/src/FormatDropDown.h) \
                 $$quote($$BASEDIR/src/LogicPasteApp.h) \
                 $$quote($$BASEDIR/src/PasteFormatter.h) \
                 $$quote($$BASEDIR/src/PasteListing.h) \
                 $$quote($$BASEDIR/src/PasteModel.h) \
                 $$quote($$BASEDIR/src/Pastebin.h) \
                 $$quote($$BASEDIR/src/ViewPastePage.h) \
                 $$quote($$BASEDIR/src/config.h)
    }
}

INCLUDEPATH +=  $$quote($$BASEDIR/src)

CONFIG += precompile_header

PRECOMPILED_HEADER =  $$quote($$BASEDIR/precompiled.h)

lupdate_inclusion {
    SOURCES +=  $$quote($$BASEDIR/../src/*.c) \
             $$quote($$BASEDIR/../src/*.c++) \
             $$quote($$BASEDIR/../src/*.cc) \
             $$quote($$BASEDIR/../src/*.cpp) \
             $$quote($$BASEDIR/../src/*.cxx) \
             $$quote($$BASEDIR/../assets/*.qml) \
             $$quote($$BASEDIR/../assets/*.js) \
             $$quote($$BASEDIR/../assets/*.qs)

    HEADERS +=  $$quote($$BASEDIR/../src/*.h) \
             $$quote($$BASEDIR/../src/*.h++) \
             $$quote($$BASEDIR/../src/*.hh) \
             $$quote($$BASEDIR/../src/*.hpp) \
             $$quote($$BASEDIR/../src/*.hxx)
}

TRANSLATIONS =  $$quote($${TARGET}.ts)
