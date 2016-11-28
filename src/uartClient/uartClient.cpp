#include <QtSerialPort/QSerialPort>
#include <QTime>

#include "uartClient.h"

QT_USE_NAMESPACE

UartClient::UartClient(QObject *parent) : QThread(parent), waitTimeout(0), quit(false) {

}

UartClient::~UartClient() {
    mutex.lock();
    quit = true;
    cond.wakeOne();
    mutex.unlock();
    wait();
}

void UartClient::writeRead(const QString &portName, int waitTimeout, const QByteArray &dataToSend) {
    QMutexLocker locker(&mutex);
    this->portName = portName;
    this->waitTimeout = waitTimeout;
    this->dataToSend = dataToSend;
    if (!isRunning()) {
        start();
    } else {
        cond.wakeOne();
    }
}

void UartClient::run()
{
    bool currentPortNameChanged = false;

    mutex.lock();
    QString currentPortName;
    if(currentPortName != portName) {
        currentPortName = portName;
        currentPortNameChanged = true;
    }

    int currentWaitTimeout = waitTimeout;
    QByteArray _dataToSend = dataToSend;
    mutex.unlock();
    QSerialPort serial;
    serial.setBaudRate(1250000);

    while(!quit) {
        if(currentPortNameChanged) {
            serial.close();
            serial.setPortName(currentPortName);

            if(!serial.open(QIODevice::ReadWrite)) {
                emit error(tr("Can't open %1, error code %2").arg(portName).arg(serial.error()).toUtf8());
                return;
            }
        }
        // write request
        QByteArray bytesToSend = dataToSend;
        serial.write(bytesToSend);
        // Wait for bytes to be written or timeout
        if(serial.waitForBytesWritten(currentWaitTimeout)) {
            //qDebug("UART Data Written");
            // Wait for the first byte to come in
            if(serial.waitForReadyRead(currentWaitTimeout)) {
                QByteArray bytesRead = serial.readAll();
                // Then continue reading until no bytes come in for at least 20 ms
                while(serial.waitForReadyRead(50))
                {
                    bytesRead += serial.readAll();
                }

                int size = bytesRead.size();
                qDebug("UART Data Read: " + bytesRead);

                QByteArray dataRead(bytesRead);
                emit this->response(dataRead);
            } else {
                //qDebug("UART timed out trying to read data.");
                emit timeout(tr("Wait read response timeout %1").arg(QTime::currentTime().toString()).toUtf8());
            }
        } else {
            emit timeout(tr("Wait write request timeout %1").arg(QTime::currentTime().toString()).toUtf8());
        }
        mutex.lock();
        cond.wait(&mutex);
        if(currentPortName != portName) {
            currentPortName = portName;
            currentPortNameChanged = true;
        } else {
            currentPortNameChanged = false;
        }
        currentWaitTimeout = waitTimeout;
        _dataToSend = dataToSend;
        mutex.unlock();
    }
}


