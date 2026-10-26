/****************************************************************************
** Meta object code from reading C++ file 'ptyterminalsession.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/terminal/ptyterminalsession.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ptyterminalsession.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN6qterm218PtyTerminalSessionE_t {};
} // unnamed namespace

template <> constexpr inline auto qterm2::PtyTerminalSession::qt_create_metaobjectdata<qt_meta_tag_ZN6qterm218PtyTerminalSessionE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "qterm2::PtyTerminalSession"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PtyTerminalSession, qt_meta_tag_ZN6qterm218PtyTerminalSessionE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject qterm2::PtyTerminalSession::staticMetaObject = { {
    QMetaObject::SuperData::link<TerminalSession::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6qterm218PtyTerminalSessionE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6qterm218PtyTerminalSessionE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6qterm218PtyTerminalSessionE_t>.metaTypes,
    nullptr
} };

void qterm2::PtyTerminalSession::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PtyTerminalSession *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *qterm2::PtyTerminalSession::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *qterm2::PtyTerminalSession::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6qterm218PtyTerminalSessionE_t>.strings))
        return static_cast<void*>(this);
    return TerminalSession::qt_metacast(_clname);
}

int qterm2::PtyTerminalSession::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = TerminalSession::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
