#include "gRPC.h"

#include <utility>
#include <QStringList>

#ifndef NKR_NO_GRPC

#include "main/NekoRay.hpp"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QtEndian>
#include <QThread>
#include <QMutex>

namespace QtGrpc {
    const char *GrpcAcceptEncodingHeader = "grpc-accept-encoding";
    const char *AcceptEncodingHeader = "accept-encoding";
    const char *TEHeader = "te";
    const char *GrpcStatusHeader = "grpc-status";
    const char *GrpcStatusMessage = "grpc-message";
    const int GrpcMessageSizeHeaderSize = 5;

    class Http2GrpcChannelPrivate : public QObject {
    private:
        QString url_base;
        QThread *thread;
        QNetworkAccessManager *nm;

        QString nekoray_auth;
        QString serviceName;

        // TODO WTF
        // https://github.com/semlanik/qtprotobuf/issues/116
//        setCachingEnabled:  5  bytesDownloaded
//        QNetworkReplyImpl: backend error: caching was enabled after some bytes had been written

        // async
        QNetworkReply *post(const QString &method, const QString &service, const QByteArray &args) {
            QUrl callUrl = url_base + "/" + service + "/" + method;
//            qDebug() << "Service call url: " << callUrl;

            QNetworkRequest request(callUrl);
            request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
            request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String{"application/grpc"});
            request.setRawHeader("Cache-Control", "no-store");
            request.setRawHeader(GrpcAcceptEncodingHeader, QByteArray{"identity,deflate,gzip"});
            request.setRawHeader(AcceptEncodingHeader, QByteArray{"identity,gzip"});
            request.setRawHeader(TEHeader, QByteArray{"trailers"});
            request.setAttribute(QNetworkRequest::Http2DirectAttribute, true);
            request.setRawHeader("nekoray_auth", nekoray_auth.toLatin1());

            QByteArray msg(GrpcMessageSizeHeaderSize, '\0');
            *reinterpret_cast<int *>(msg.data() + 1) = qToBigEndian((int) args.size());
            msg += args;
            // qDebug() << "SEND: " << msg.size();

            QNetworkReply *networkReply = nm->post(request, msg);
            return networkReply;
        }

        static QByteArray processReply(QNetworkReply *networkReply, QNetworkReply::NetworkError &statusCode) {
            // Check if no network error occured
            if (networkReply->error() != QNetworkReply::NoError) {
                statusCode = networkReply->error();
                return {};
            }

            // Check if server answer with error
            auto errCode = networkReply->rawHeader(GrpcStatusHeader).toInt();
            if (errCode != 0) {
                QStringList errstr;
                errstr << "grpc-status error code:" << Int2String(errCode) << ", error msg:"
                       << QLatin1String(networkReply->rawHeader(GrpcStatusMessage));
                MW_show_log(errstr.join(" "));
                statusCode = QNetworkReply::NetworkError::ProtocolUnknownError;
                return {};
            }
            statusCode = QNetworkReply::NetworkError::NoError;
            return networkReply->readAll().mid(GrpcMessageSizeHeaderSize);
        }

        QNetworkReply::NetworkError
        call(const QString &method, const QString &service, const QByteArray &args, QByteArray &qByteArray,
             int timeout_ms) {
            QNetworkReply *networkReply = post(method, service, args);

            QTimer *abortTimer = nullptr;
            if (timeout_ms > 0) {
                abortTimer = new QTimer;
                abortTimer->setSingleShot(true);
                abortTimer->setInterval(timeout_ms);
                connect(abortTimer, &QTimer::timeout, abortTimer, [=]() {
                    networkReply->abort();
                });
                abortTimer->start();
            }

            {
                QEventLoop loop;
                QObject::connect(networkReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                loop.exec();
            }

            if (abortTimer != nullptr) {
                abortTimer->stop();
                abortTimer->deleteLater();
            }

            auto grpcStatus = QNetworkReply::NetworkError::ProtocolUnknownError;
            qByteArray = processReply(networkReply, grpcStatus);
            // qDebug() << __func__ << "RECV: " << qByteArray.toHex() << "grpcStatus" << grpcStatus;

            networkReply->deleteLater();
            return grpcStatus;
        }

    public:
        Http2GrpcChannelPrivate(const QString &url_, const QString &nekoray_auth_, const QString &serviceName_) {
            url_base = "http://" + url_;
            nekoray_auth = nekoray_auth_;
            serviceName = serviceName_;
            //
            thread = new QThread;
            nm = new QNetworkAccessManager();
            nm->moveToThread(thread);
            thread->start();
        }

        QNetworkReply::NetworkError Call(const QString &methodName,
                                         const google::protobuf::Message &req, google::protobuf::Message *rsp,
                                         int timeout_ms = 0) {
            if (!NekoRay::dataStore->core_running) return QNetworkReply::NetworkError(-1919);

            std::string reqStr;
            req.SerializeToString(&reqStr);
            auto requestArray = QByteArray::fromStdString(reqStr);

            QByteArray responseArray;
            QNetworkReply::NetworkError err;
            QMutex lock;
            lock.lock();

            runOnUiThread([&] {
                err = call(methodName, serviceName, requestArray, responseArray, timeout_ms);
                lock.unlock();
            }, nm);

            lock.lock();
            lock.unlock();
//            qDebug() << "rsp err" << err;
//            qDebug() << "rsp array" << responseArray;

            if (err != QNetworkReply::NetworkError::NoError) {
                return err;
            }
            if (!rsp->ParseFromArray(responseArray.data(), responseArray.size())) {
                return QNetworkReply::NetworkError(-114514);
            }
            return QNetworkReply::NetworkError::NoError;
        }

    };
}

namespace NekoRay::rpc {
    Client::Client(std::function<void(const QString &)> onError, const QString &target, const QString &token) {
        this->grpc_channel = std::make_unique<QtGrpc::Http2GrpcChannelPrivate>(target, token, "libcore.LibcoreService");
        this->onError = std::move(onError);
    }

#define NOT_OK *rpcOK = false; \
    onError( \
            QString("QNetworkReply::NetworkError code: %1\n").arg(status) \
    );

    void Client::Exit() {
        libcore::EmptyReq request;
        libcore::EmptyResp reply;
        grpc_channel->Call("Exit", request, &reply, 500);
    }

    QString Client::Start(bool *rpcOK, const libcore::LoadConfigReq &request) {
        libcore::ErrorResp reply;
        auto status = grpc_channel->Call("Start", request, &reply, 3000);

        if (status == QNetworkReply::NoError) {
            *rpcOK = true;
            return {reply.error().c_str()};
        } else {
            NOT_OK
            return "";
        }
    }

    QString Client::Stop(bool *rpcOK) {
        libcore::EmptyReq request;
        libcore::ErrorResp reply;
        auto status = grpc_channel->Call("Stop", request, &reply, 3000);

        if (status == QNetworkReply::NoError) {
            *rpcOK = true;
            return {reply.error().c_str()};
        } else {
            NOT_OK
            return "";
        }
    }

    bool Client::KeepAlive() {
        libcore::EmptyReq request;
        libcore::EmptyResp reply;
        auto status = grpc_channel->Call("KeepAlive", request, &reply, 500);

        if (status == QNetworkReply::NoError) {
            return true;
        } else {
            return false;
        }
    }

    long long Client::QueryStats(const std::string &tag, const std::string &direct) {
        libcore::QueryStatsReq request;
        request.set_tag(tag);
        request.set_direct(direct);

        libcore::QueryStatsResp reply;
        auto status = grpc_channel->Call("QueryStats", request, &reply, 500);

        if (status == QNetworkReply::NoError) {
            return reply.traffic();
        } else {
            return 0;
        }
    }

    std::string Client::ListConnections() {
        libcore::EmptyReq request;
        libcore::ListConnectionsResp reply;
        auto status = grpc_channel->Call("ListConnections", request, &reply, 500);

        if (status == QNetworkReply::NoError) {
            return reply.matsuri_connections_json();
        } else {
            return "";
        }
    }

    //

    libcore::TestResp Client::Test(bool *rpcOK, const libcore::TestReq &request) {
        libcore::TestResp reply;
        auto status = grpc_channel->Call("Test", request, &reply);

        if (status == QNetworkReply::NoError) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    libcore::UpdateResp Client::Update(bool *rpcOK, const libcore::UpdateReq &request) {
        libcore::UpdateResp reply;
        auto status = grpc_channel->Call("Update", request, &reply);

        if (status == QNetworkReply::NoError) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }
}

#endif
