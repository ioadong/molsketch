message("======= Welcome to molsKetch build =======")
include(settings.pri)

TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
	libmolsketch/src \
	molsketch/src \
	obabeliface \
	tests

INSTALLS += documentation_en \
	    documentation_cs \
	    documentation_nl \
	    molecule_library \
	    custom_library

documentation_en.files = $$PWD/doc/en/*
documentation_cs.files = $$PWD/doc/cs/*
documentation_nl.files = $$PWD/doc/nl/*
molecule_library.files = $$PWD/library/*
custom_library.files = $$PWD/library/custom/*

documentation_en.path = $${MSK_INSTALL_DOCS}/en
documentation_cs.path = $${MSK_INSTALL_DOCS}/cs
documentation_nl.path = $${MSK_INSTALL_DOCS}/nl
molecule_library.path = $${MSK_INSTALL_LIBRARY}
custom_library.path = $${MSK_INSTALL_CUSTOM}

contains(CONFIG, static) { SUBDIRS -= tests obabeliface }

OTHER_FILES += \
    version \
    versionnick
