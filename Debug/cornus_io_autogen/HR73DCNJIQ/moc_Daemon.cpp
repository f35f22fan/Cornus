/****************************************************************************
** Meta object code from reading C++ file 'Daemon.hpp'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.7.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../io/Daemon.hpp"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Daemon.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.7.2. It"
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

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASScornusSCOPEioSCOPEDaemonENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASScornusSCOPEioSCOPEDaemonENDCLASS = QtMocHelpers::stringData(
    "cornus::io::Daemon",
    "CutURLsToClipboard",
    "",
    "ByteArray*",
    "ba",
    "CopyURLsToClipboard",
    "EmptyTrashRecursively",
    "dir_path",
    "notify_user",
    "LoadDesktopFiles",
    "LoadDesktopFilesFrom",
    "QProcessEnvironment",
    "env",
    "QuitGuiApp",
    "SendAllDesktopFiles",
    "fd",
    "SendDefaultDesktopFileForFullPath",
    "cint",
    "SendDesktopFilesById",
    "cornus::ByteArray*",
    "SendOpenWithList",
    "mime"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASScornusSCOPEioSCOPEDaemonENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   74,    2, 0x0a,    1 /* Public */,
       5,    1,   77,    2, 0x0a,    3 /* Public */,
       6,    2,   80,    2, 0x0a,    5 /* Public */,
       9,    0,   85,    2, 0x0a,    8 /* Public */,
      10,    2,   86,    2, 0x0a,    9 /* Public */,
      13,    0,   91,    2, 0x0a,   12 /* Public */,
      14,    1,   92,    2, 0x0a,   13 /* Public */,
      16,    2,   95,    2, 0x0a,   15 /* Public */,
      18,    2,  100,    2, 0x0a,   18 /* Public */,
      20,    2,  105,    2, 0x0a,   21 /* Public */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Bool, QMetaType::QString, QMetaType::Bool,    7,    8,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 11,    7,   12,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   15,
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 17,    4,   15,
    QMetaType::Void, 0x80000000 | 19, 0x80000000 | 17,    4,   15,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 17,   21,   15,

       0        // eod
};

Q_CONSTINIT const QMetaObject cornus::io::Daemon::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASScornusSCOPEioSCOPEDaemonENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASScornusSCOPEioSCOPEDaemonENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASScornusSCOPEioSCOPEDaemonENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Daemon, std::true_type>,
        // method 'CutURLsToClipboard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<ByteArray *, std::false_type>,
        // method 'CopyURLsToClipboard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<ByteArray *, std::false_type>,
        // method 'EmptyTrashRecursively'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const bool, std::false_type>,
        // method 'LoadDesktopFiles'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'LoadDesktopFilesFrom'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QProcessEnvironment &, std::false_type>,
        // method 'QuitGuiApp'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'SendAllDesktopFiles'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const int, std::false_type>,
        // method 'SendDefaultDesktopFileForFullPath'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<ByteArray *, std::false_type>,
        QtPrivate::TypeAndForceComplete<cint, std::false_type>,
        // method 'SendDesktopFilesById'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<cornus::ByteArray *, std::false_type>,
        QtPrivate::TypeAndForceComplete<cint, std::false_type>,
        // method 'SendOpenWithList'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<cint, std::false_type>
    >,
    nullptr
} };

void cornus::io::Daemon::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Daemon *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->CutURLsToClipboard((*reinterpret_cast< std::add_pointer_t<ByteArray*>>(_a[1]))); break;
        case 1: _t->CopyURLsToClipboard((*reinterpret_cast< std::add_pointer_t<ByteArray*>>(_a[1]))); break;
        case 2: { bool _r = _t->EmptyTrashRecursively((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 3: _t->LoadDesktopFiles(); break;
        case 4: _t->LoadDesktopFilesFrom((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QProcessEnvironment>>(_a[2]))); break;
        case 5: _t->QuitGuiApp(); break;
        case 6: _t->SendAllDesktopFiles((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->SendDefaultDesktopFileForFullPath((*reinterpret_cast< std::add_pointer_t<ByteArray*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<cint>>(_a[2]))); break;
        case 8: _t->SendDesktopFilesById((*reinterpret_cast< std::add_pointer_t<cornus::ByteArray*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<cint>>(_a[2]))); break;
        case 9: _t->SendOpenWithList((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<cint>>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObject *cornus::io::Daemon::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *cornus::io::Daemon::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASScornusSCOPEioSCOPEDaemonENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int cornus::io::Daemon::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}
QT_WARNING_POP
