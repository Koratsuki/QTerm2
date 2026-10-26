/****************************************************************************
** Meta object code from reading C++ file 'terminalwidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/terminalwidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'terminalwidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14TerminalWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto TerminalWidget::qt_create_metaobjectdata<qt_meta_tag_ZN14TerminalWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TerminalWidget",
        "titleChanged",
        "",
        "title",
        "terminalFinished",
        "bellRang",
        "searchResult",
        "found",
        "selectionChanged",
        "hasSelection",
        "workingDirectoryChanged",
        "path",
        "CursorStyle",
        "Block",
        "IBeam",
        "Underscore"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'titleChanged'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'terminalFinished'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'bellRang'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'searchResult'
        QtMocHelpers::SignalData<void(bool)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 7 },
        }}),
        // Signal 'selectionChanged'
        QtMocHelpers::SignalData<void(bool)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 9 },
        }}),
        // Signal 'workingDirectoryChanged'
        QtMocHelpers::SignalData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'CursorStyle'
        QtMocHelpers::EnumData<enum CursorStyle>(12, 12, QMC::EnumIsScoped).add({
            {   13, CursorStyle::Block },
            {   14, CursorStyle::IBeam },
            {   15, CursorStyle::Underscore },
        }),
    };
    return QtMocHelpers::metaObjectData<TerminalWidget, qt_meta_tag_ZN14TerminalWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TerminalWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14TerminalWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14TerminalWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14TerminalWidgetE_t>.metaTypes,
    nullptr
} };

void TerminalWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TerminalWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->titleChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->terminalFinished(); break;
        case 2: _t->bellRang(); break;
        case 3: _t->searchResult((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->selectionChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->workingDirectoryChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TerminalWidget::*)(const QString & )>(_a, &TerminalWidget::titleChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalWidget::*)()>(_a, &TerminalWidget::terminalFinished, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalWidget::*)()>(_a, &TerminalWidget::bellRang, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalWidget::*)(bool )>(_a, &TerminalWidget::searchResult, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalWidget::*)(bool )>(_a, &TerminalWidget::selectionChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (TerminalWidget::*)(const QString & )>(_a, &TerminalWidget::workingDirectoryChanged, 5))
            return;
    }
}

const QMetaObject *TerminalWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TerminalWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14TerminalWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int TerminalWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void TerminalWidget::titleChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void TerminalWidget::terminalFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void TerminalWidget::bellRang()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void TerminalWidget::searchResult(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void TerminalWidget::selectionChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void TerminalWidget::workingDirectoryChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
