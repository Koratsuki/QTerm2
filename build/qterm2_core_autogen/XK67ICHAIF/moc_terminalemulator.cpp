/****************************************************************************
** Meta object code from reading C++ file 'terminalemulator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/terminal/terminalemulator.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'terminalemulator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16TerminalEmulatorE_t {};
} // unnamed namespace

template <> constexpr inline auto TerminalEmulator::qt_create_metaobjectdata<qt_meta_tag_ZN16TerminalEmulatorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TerminalEmulator",
        "updated",
        "",
        "screenReset",
        "titleChanged",
        "title",
        "bellRang",
        "workingDirectoryChanged",
        "path",
        "shellIntegrationEvent",
        "event",
        "response",
        "data",
        "cursorStyleRequested",
        "TerminalEmulator::CursorStyle",
        "style",
        "blink",
        "isDefault",
        "CursorStyle",
        "Block",
        "IBeam",
        "Underscore"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'updated'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'screenReset'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'titleChanged'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'bellRang'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'workingDirectoryChanged'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Signal 'shellIntegrationEvent'
        QtMocHelpers::SignalData<void(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Signal 'response'
        QtMocHelpers::SignalData<void(const QByteArray &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 12 },
        }}),
        // Signal 'cursorStyleRequested'
        QtMocHelpers::SignalData<void(TerminalEmulator::CursorStyle, bool, bool)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 14, 15 }, { QMetaType::Bool, 16 }, { QMetaType::Bool, 17 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'CursorStyle'
        QtMocHelpers::EnumData<enum CursorStyle>(18, 18, QMC::EnumIsScoped).add({
            {   19, CursorStyle::Block },
            {   20, CursorStyle::IBeam },
            {   21, CursorStyle::Underscore },
        }),
    };
    return QtMocHelpers::metaObjectData<TerminalEmulator, qt_meta_tag_ZN16TerminalEmulatorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TerminalEmulator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16TerminalEmulatorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16TerminalEmulatorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16TerminalEmulatorE_t>.metaTypes,
    nullptr
} };

void TerminalEmulator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TerminalEmulator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->updated(); break;
        case 1: _t->screenReset(); break;
        case 2: _t->titleChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->bellRang(); break;
        case 4: _t->workingDirectoryChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->shellIntegrationEvent((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->response((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 7: _t->cursorStyleRequested((*reinterpret_cast<std::add_pointer_t<TerminalEmulator::CursorStyle>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TerminalEmulator::*)()>(_a, &TerminalEmulator::updated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalEmulator::*)()>(_a, &TerminalEmulator::screenReset, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalEmulator::*)(const QString & )>(_a, &TerminalEmulator::titleChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalEmulator::*)()>(_a, &TerminalEmulator::bellRang, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalEmulator::*)(const QString & )>(_a, &TerminalEmulator::workingDirectoryChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalEmulator::*)(const QString & )>(_a, &TerminalEmulator::shellIntegrationEvent, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalEmulator::*)(const QByteArray & )>(_a, &TerminalEmulator::response, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalEmulator::*)(TerminalEmulator::CursorStyle , bool , bool )>(_a, &TerminalEmulator::cursorStyleRequested, 7))
            return;
    }
}

const QMetaObject *TerminalEmulator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TerminalEmulator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16TerminalEmulatorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int TerminalEmulator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void TerminalEmulator::updated()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void TerminalEmulator::screenReset()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void TerminalEmulator::titleChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void TerminalEmulator::bellRang()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void TerminalEmulator::workingDirectoryChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void TerminalEmulator::shellIntegrationEvent(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void TerminalEmulator::response(const QByteArray & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void TerminalEmulator::cursorStyleRequested(TerminalEmulator::CursorStyle _t1, bool _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
