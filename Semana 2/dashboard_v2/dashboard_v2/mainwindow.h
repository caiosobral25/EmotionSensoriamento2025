#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

QT_FORWARD_DECLARE_CLASS(QTimer)
QT_FORWARD_DECLARE_CLASS(QChart)
QT_FORWARD_DECLARE_CLASS(QLineSeries)
QT_FORWARD_DECLARE_CLASS(QValueAxis)

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void connectSelectedPort();
    void disconnectPort();
    void saveCsv();
    void refreshPortList();
    void handleReadyRead();
    void updateConnectionIndicator();

private:
    struct Sample {
        qint64 epochMs = 0;
        qint64 elapsedMs = 0;
        double rpm = 0.0;
        double vel = 0.0;
        double tq = 0.0;
    };

    void configureUi();
    void configureChart();
    void configureConnections();
    bool openSerialPort(const QString &portName);
    void closeSerialPort();
    bool serialIsOpen() const;
    void setConnectedState(bool connected);
    void appendSample(double rpm, double vel, double tq);
    void updateMetricLabels(const Sample &sample);
    void updateChart();
    void updateStatusText(const QString &text, const QString &color);
    void handleSerialError(const QString &message);
    bool parseTelemetryLine(const QString &line, double &rpm, double &vel, double &tq) const;
    static bool parseNumberToken(QString token, double &value);
    static QStringList availablePortNames();
    static QString windowsErrorString(quint32 errorCode);
    QString selectedPortName() const;

    Ui::MainWindow *ui;
    QTimer *m_portRefreshTimer;
    QTimer *m_statusTimer;
    QTimer *m_serialPollTimer;
    QByteArray m_serialBuffer;
    QVector<Sample> m_samples;
    QElapsedTimer m_sessionTimer;
    qint64 m_lastIntervalMs = 100;
    qint64 m_elapsedBaseMs = 0;
    qintptr m_serialHandle = -1;
    QString m_lastSerialError;

    QChart *m_chart;
    QLineSeries *m_rpmSeries;
    QLineSeries *m_velSeries;
    QLineSeries *m_tqSeries;
    QValueAxis *m_axisX;
    QValueAxis *m_axisYRpm;
    QValueAxis *m_axisYVel;
    QValueAxis *m_axisYTq;
};

#endif // MAINWINDOW_H
