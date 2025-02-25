// DO NOT INCLUDE THIS

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

//

inline QString software_name = "NekoRay";
inline QString software_core_name = "V2Ray";

// Main Functions

inline std::function<void()> MF_release_runguard;

// MainWindow functions

inline QWidget *mainwindow;
inline std::function<void(QString)> MW_show_log;
inline std::function<void(QString, QString)> MW_show_log_ext;
inline std::function<void(QString)> MW_show_log_ext_vt100;
inline std::function<void(QString, QString)> MW_dialog_message;

// Utils

#define QJSONARRAY_ADD(arr, add) for(const auto &a: (add)) { (arr) += a; }
#define QJSONOBJECT_COPY(src, dst, key) if (src.contains(key)) dst[key] = src[key];
#define QJSONOBJECT_COPY2(src, dst, src_key, dst_key) if (src.contains(src_key)) dst[dst_key] = src[src_key];

#define Int2String(num) QString::number(num)

inline QString SubStrBefore(QString str, const QString &sub) {
    if (!str.contains(sub)) return str;
    return str.left(str.indexOf(sub));
}

inline QString SubStrAfter(QString str, const QString &sub) {
    if (!str.contains(sub)) return str;
    return str.right(str.length() - str.indexOf(sub) - sub.length());
}

inline QString QStringList2Command(const QStringList &list) {
    QStringList new_list;
    for (auto str: list) {
        auto q = "\"" + str.replace("\"", "\\\"") + "\"";
        new_list << q;
    }
    return new_list.join(" ");
}

inline QString
DecodeB64IfValid(const QString &input, QByteArray::Base64Option options = QByteArray::Base64Option::Base64Encoding) {
    auto result = QByteArray::fromBase64Encoding(input.toUtf8(),
                                                 options | QByteArray::Base64Option::AbortOnBase64DecodingErrors);
    if (result) {
        return result.decoded;
    }
    return "";
}

#define GetQuery(url) QUrlQuery((url).query(QUrl::ComponentFormattingOption::FullyDecoded));

QString GetQueryValue(const QUrlQuery &q, const QString &key, const QString &def = "");

QString GetRandomString(int randomStringLength);

// QString >> QJson
inline QJsonObject QString2QJsonObject(const QString &jsonString) {
    QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonString.toUtf8());
    QJsonObject jsonObject = jsonDocument.object();
    return jsonObject;
}

// QJson >> QString
inline QString QJsonObject2QString(const QJsonObject &jsonObject, bool compact) {
    return QString(QJsonDocument(jsonObject).toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented));
}

template<typename T>
inline QJsonArray QList2QJsonArray(const QList<T> &list) {
    QVariantList list2;
    for (auto &item: list)
        list2.append(item);
    return QJsonArray::fromVariantList(list2);
}

inline QList<int> QJsonArray2QListInt(const QJsonArray &arr) {
    QList<int> list2;
    for (auto item: arr)
        list2.append(item.toInt());
    return list2;
}

inline QList<QString> QJsonArray2QListString(const QJsonArray &arr) {
    QList<QString> list2;
    for (auto item: arr)
        list2.append(item.toString());
    return list2;
}

inline QString UrlSafe_encode(const QString &s) {
    return s.toUtf8().toPercentEncoding().replace(" ", "%20");
}

inline bool InRange(unsigned x, unsigned low, unsigned high) {
    return (low <= x && x <= high);
}

inline QStringList SplitLines(const QString &_string) {
    return _string.split(QRegularExpression("[\r\n]"), Qt::SplitBehaviorFlags::SkipEmptyParts);
}

// Files

QByteArray ReadFile(const QString &path);

QString ReadFileText(const QString &path);

// Net

int MkPort();

// Validators

bool IsIpAddress(const QString &str);

bool IsIpAddressV4(const QString &str);

bool IsIpAddressV6(const QString &str);

// [2001:4860:4860::8888] -> 2001:4860:4860::8888
inline QString UnwrapIPV6Host(QString &str) {
    return str.replace("[", "").replace("]", "");
}

// [2001:4860:4860::8888] or 2001:4860:4860::8888 -> [2001:4860:4860::8888]
inline QString WrapIPV6Host(QString &str) {
    if (!IsIpAddressV6(str)) return str;
    return "[" + UnwrapIPV6Host(str) + "]";
}

inline QString DisplayAddress(QString serverAddress, int serverPort) {
    if (serverAddress.isEmpty() && serverPort == 0) return {};
    return WrapIPV6Host(serverAddress) + ":" + Int2String(serverPort);
};

// Format

inline QString DisplayTime(long long time, QLocale::FormatType formatType = QLocale::LongFormat) {
    QDateTime t;
    t.setSecsSinceEpoch(time);
    return QLocale().toString(t, formatType);
}

inline QString ReadableSize(const qint64 &size) {
    double sizeAsDouble = size;
    static QStringList measures;
    if (measures.isEmpty())
        measures << "B"
                 << "KiB"
                 << "MiB"
                 << "GiB"
                 << "TiB"
                 << "PiB"
                 << "EiB"
                 << "ZiB"
                 << "YiB";
    QStringListIterator it(measures);
    QString measure(it.next());
    while (sizeAsDouble >= 1024.0 && it.hasNext()) {
        measure = it.next();
        sizeAsDouble /= 1024.0;
    }
    return QString::fromLatin1("%1 %2").arg(sizeAsDouble, 0, 'f', 2).arg(measure);
}

// UI

QWidget *GetMessageBoxParent();

int MessageBoxWarning(const QString &title, const QString &text);

int MessageBoxInfo(const QString &title, const QString &text);

void runOnUiThread(const std::function<void()> &callback, QObject *parent = nullptr);

void runOnNewThread(const std::function<void()> &callback);

template<typename EMITTER, typename SIGNAL, typename RECEIVER, typename ReceiverFunc>
inline void connectOnce(EMITTER *emitter, SIGNAL signal, RECEIVER *receiver, ReceiverFunc f,
                        Qt::ConnectionType connectionType = Qt::AutoConnection) {
    auto connection = std::make_shared<QMetaObject::Connection>();
    auto onTriggered = [connection, f](auto... arguments) {
        std::invoke(f, arguments...);
        QObject::disconnect(*connection);
    };

    *connection = QObject::connect(emitter, signal, receiver, onTriggered, connectionType);
}

void setTimeout(const std::function<void()> &callback, QObject *obj, int timeout = 0);
