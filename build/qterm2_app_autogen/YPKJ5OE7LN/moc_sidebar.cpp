/****************************************************************************
** Meta object code from reading C++ file 'sidebar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/sidebar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'sidebar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7SidebarE_t {};
} // unnamed namespace

template <> constexpr inline auto Sidebar::qt_create_metaobjectdata<qt_meta_tag_ZN7SidebarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Sidebar",
        "connectRequested",
        "",
        "id",
        "editRequested",
        "deleteRequested",
        "duplicateRequested",
        "favoriteToggleRequested",
        "installKeyRequested",
        "copyCommandRequested",
        "newSshRequested",
        "newLocalRequested",
        "hideRequested"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'connectRequested'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'editRequested'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'deleteRequested'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'duplicateRequested'
        QtMocHelpers::SignalData<void(const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'favoriteToggleRequested'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'installKeyRequested'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'copyCommandRequested'
        QtMocHelpers::SignalData<void(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'newSshRequested'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'newLocalRequested'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'hideRequested'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Sidebar, qt_meta_tag_ZN7SidebarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Sidebar::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7SidebarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7SidebarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7SidebarE_t>.metaTypes,
    nullptr
} };

void Sidebar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Sidebar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->connectRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->editRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->deleteRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->duplicateRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->favoriteToggleRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->installKeyRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->copyCommandRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->newSshRequested(); break;
        case 8: _t->newLocalRequested(); break;
        case 9: _t->hideRequested(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)(const QString & )>(_a, &Sidebar::connectRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)(const QString & )>(_a, &Sidebar::editRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)(const QString & )>(_a, &Sidebar::deleteRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)(const QString & )>(_a, &Sidebar::duplicateRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)(const QString & )>(_a, &Sidebar::favoriteToggleRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)(const QString & )>(_a, &Sidebar::installKeyRequested, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)(const QString & )>(_a, &Sidebar::copyCommandRequested, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)()>(_a, &Sidebar::newSshRequested, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)()>(_a, &Sidebar::newLocalRequested, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (Sidebar::*)()>(_a, &Sidebar::hideRequested, 9))
            return;
    }
}

const QMetaObject *Sidebar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Sidebar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7SidebarE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int Sidebar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void Sidebar::connectRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void Sidebar::editRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void Sidebar::deleteRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void Sidebar::duplicateRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void Sidebar::favoriteToggleRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void Sidebar::installKeyRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void Sidebar::copyCommandRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void Sidebar::newSshRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void Sidebar::newLocalRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void Sidebar::hideRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}
QT_WARNING_POP
