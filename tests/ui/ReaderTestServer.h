#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

class ReaderTestServer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString url READ url CONSTANT)
    Q_PROPERTY(int hits READ hits NOTIFY hitsChanged)
    Q_PROPERTY(QString capturePath READ capturePath CONSTANT)
public:
    explicit ReaderTestServer(QObject *parent = nullptr) : QObject(parent) {
        m_server.listen(QHostAddress::LocalHost, 0);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = m_server.nextPendingConnection()) {
                ++m_hits; emit hitsChanged();
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    QString url() const { return m_server.isListening() ? QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()) : QString(); }
    QString capturePath() const { return qEnvironmentVariable("NKMAIL_READER_CAPTURE"); }
    int hits() const { return m_hits; }
signals:
    void hitsChanged();
private:
    QTcpServer m_server;
    int m_hits = 0;
};
