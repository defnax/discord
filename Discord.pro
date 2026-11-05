# Build with the same qmake/toolchain as the host RetroShare.

QT += core gui widgets network
TEMPLATE = lib
CONFIG += plugin c++17
TARGET = Discord

include(../Common/retroshare_plugin.pri)
HEADERS += DiscordPlugin.h DiscordBridge.h BridgeMessages.h
SOURCES += DiscordPlugin.cpp DiscordBridge.cpp
RESOURCES += Discord_images.qrc
