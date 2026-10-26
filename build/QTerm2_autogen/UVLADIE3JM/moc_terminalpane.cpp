/****************************************************************************
** Meta object code from reading C++ file 'terminalpane.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/terminalpane.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'terminalpane.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12TerminalPaneE_t {};
} // unnamed namespace

template <> constexpr inline auto TerminalPane::qt_create_metaobjectdata<qt_meta_tag_ZN12TerminalPaneE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TerminalPane",
        "titleChanged",
        "",
        "lastPaneClosed",
        "activeTerminalChanged",
        "TerminalWidget*",
        "terminal",
        "terminalFinished",
        "bellRang"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'titleChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'lastPaneClosed'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'activeTerminalChanged'
        QtMocHelpers::SignalData<void(TerminalWidget *)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'terminalFinished'
        QtMocHelpers::SignalData<void(TerminalWidget *)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'bellRang'
        QtMocHelpers::SignalData<void(TerminalWidget *)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TerminalPane, qt_meta_tag_ZN12TerminalPaneE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TerminalPane::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12TerminalPaneE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12TerminalPaneE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12TerminalPaneE_t>.metaTypes,
    nullptr
} };

void TerminalPane::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TerminalPane *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->titleChanged(); break;
        case 1: _t->lastPaneClosed(); break;
        case 2: _t->activeTerminalChanged((*reinterpret_cast<std::add_pointer_t<TerminalWidget*>>(_a[1]))); break;
        case 3: _t->terminalFinished((*reinterpret_cast<std::add_pointer_t<TerminalWidget*>>(_a[1]))); break;
        case 4: _t->bellRang((*reinterpret_cast<std::add_pointer_t<TerminalWidget*>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TerminalPane::*)()>(_a, &TerminalPane::titleChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalPane::*)()>(_a, &TerminalPane::lastPaneClosed, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalPane::*)(TerminalWidget * )>(_a, &TerminalPane::activeTerminalChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalPane::*)(TerminalWidget * )>(_a, &TerminalPane::terminalFinished, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalPane::*)(TerminalWidget * )>(_a, &TerminalPane::bellRang, 4))
            return;
    }
}

const QMetaObject *TerminalPane::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TerminalPane::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12TerminalPaneE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int TerminalPane::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void TerminalPane::titleChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void TerminalPane::lastPaneClosed()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void TerminalPane::activeTerminalChanged(TerminalWidget * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void TerminalPane::terminalFinished(TerminalWidget * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void TerminalPane::bellRang(TerminalWidget * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
