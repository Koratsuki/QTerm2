#!/bin/bash
set -e

APP=QTerm2
VERSION=1.0.5
BUILD_DIR=../../build
APPDIR=AppDir

mkdir -p $APPDIR/usr/bin
mkdir -p $APPDIR/usr/share/applications
mkdir -p $APPDIR/usr/share/icons/hicolor/scalable/apps

cp $BUILD_DIR/$APP $APPDIR/usr/bin/
cp ../$APP.desktop $APPDIR/usr/share/applications/
cp ../$APP.svg $APPDIR/usr/share/icons/hicolor/scalable/apps/$APP.svg
ln -s usr/share/applications/$APP.desktop $APPDIR/
ln -s usr/share/icons/hicolor/scalable/apps/$APP.svg $APPDIR/.DirIcon

# Copy dependencies
linuxdeploy --appdir $APPDIR --output appimage \
    --desktop-file $APPDIR/usr/share/applications/$APP.desktop \
    --icon-file $APPDIR/usr/share/icons/hicolor/scalable/apps/$APP.svg
