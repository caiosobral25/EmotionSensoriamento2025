#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>
#include <cwchar>
#include <limits>
#include <utility>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace Qt::StringLiterals;

namespace {
constexpr qint64 kChartWindowMs = 30'000;
constexpr qint64 kStatusTimeoutMs = 1'000;
constexpr qint32 kDefaultBaudRate = 9600;
constexpr qintptr kInvalidSerialHandle = static_cast<qintptr>(-1);

QString formatInteger(double value)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toString(qRound64(value));
}

QString formatDecimal(double value, int decimals = 1)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toString(value, 'f', decimals);
}

#ifdef Q_OS_WIN
HANDLE toHandle(qintptr rawHandle)
{
    return reinterpret_cast<HANDLE>(rawHandle);
}

QString portPath(const QString &portName)
{
    return uR"(\\.\)"_s + portName;
}
#endif
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_portRefreshTimer(new QTimer(this))
    , m_statusTimer(new QTimer(this))
    , m_serialPollTimer(new QTimer(this))
    , m_chart(nullptr)
    , m_rpmSeries(nullptr)
    , m_velSeries(nullptr)
    , m_tqSeries(nullptr)
    , m_axisX(nullptr)
    , m_axisYRpm(nullptr)
    , m_axisYVel(nullptr)
    , m_axisYTq(nullptr)
{
    ui->setupUi(this);

    configureUi();
    configureChart();
    configureConnections();

    refreshPortList();
    setConnectedState(false);

    m_portRefreshTimer->setInterval(2'000);
    m_portRefreshTimer->start();

    m_statusTimer->setInterval(250);
    m_statusTimer->start();

    m_serialPollTimer->setInterval(20);
}

MainWindow::~MainWindow()
{
    closeSerialPort();
    delete ui;
}

void MainWindow::configureUi()
{
    setMinimumSize(1100, 720);

    ui->titleLabel->setProperty("role", "title");
    ui->historyTitle->setProperty("role", "section");

    ui->metricTitleRPM->setProperty("role", "caption");
    ui->metricTitleVEL->setProperty("role", "caption");
    ui->metricTitleTQ->setProperty("role", "caption");

    ui->metricUnitRPM->setProperty("role", "unit");
    ui->metricUnitVEL->setProperty("role", "unit");
    ui->metricUnitTQ->setProperty("role", "unit");

    ui->labelRPM->setProperty("role", "rpmValue");
    ui->labelVEL->setProperty("role", "velValue");
    ui->labelTQ->setProperty("role", "tqValue");
    ui->comboPorta->setEditable(true);
    ui->comboPorta->setInsertPolicy(QComboBox::NoInsert);

    ui->btnDesconectar->setEnabled(false);
    ui->btnSalvarCsv->setEnabled(false);

    ui->chartView->setRenderHint(QPainter::Antialiasing, true);
    ui->chartView->setFrameShape(QFrame::NoFrame);

    setStyleSheet(
        R"(
        QMainWindow, QWidget#centralwidget {
            background-color: #0b0f1a;
            color: #e5e8f5;
            font-family: "Consolas", "Courier New", monospace;
        }
        QLabel[role="title"] {
            color: #dfe6ff;
            font-size: 22pt;
            letter-spacing: 2px;
            padding-bottom: 4px;
        }
        QLabel[role="section"] {
            color: #8f97b6;
            font-size: 12pt;
            letter-spacing: 4px;
            padding-bottom: 6px;
        }
        QFrame#cardRpm, QFrame#cardVel, QFrame#cardTq,
        QFrame#chartContainer, QFrame#controlsFrame {
            background-color: #101527;
            border: 1px solid #262d47;
            border-radius: 18px;
        }
        QFrame#cardRpm {
            border-top: 3px solid #ff5d67;
        }
        QFrame#cardVel {
            border-top: 3px solid #4b7dff;
        }
        QFrame#cardTq {
            border-top: 3px solid #42e38c;
        }
        QLabel[role="caption"] {
            color: #7f88a8;
            font-size: 11pt;
            letter-spacing: 3px;
        }
        QLabel[role="unit"] {
            color: #6f7693;
            font-size: 11pt;
        }
        QLabel[role="rpmValue"] {
            color: #ff5d67;
            font-size: 31pt;
            font-weight: 700;
        }
        QLabel[role="velValue"] {
            color: #5b86ff;
            font-size: 31pt;
            font-weight: 700;
        }
        QLabel[role="tqValue"] {
            color: #42e38c;
            font-size: 31pt;
            font-weight: 700;
        }
        QComboBox#comboPorta {
            min-width: 180px;
            min-height: 38px;
            border-radius: 10px;
            border: 1px solid #303756;
            padding: 6px 12px;
            background-color: #1a2033;
            color: #e5e8f5;
            selection-background-color: #2d3e78;
        }
        QComboBox#comboPorta::drop-down {
            border: none;
            width: 24px;
        }
        QPushButton {
            min-height: 40px;
            min-width: 150px;
            border-radius: 10px;
            border: 1px solid #2a3150;
            font-size: 12pt;
            letter-spacing: 1px;
        }
        QPushButton#btnConectar {
            background-color: #12341f;
            color: #5dff96;
            border-color: #1d6c3c;
        }
        QPushButton#btnConectar:hover {
            background-color: #17482a;
        }
        QPushButton#btnDesconectar {
            background-color: #34171b;
            color: #ff646d;
            border-color: #7e2c37;
        }
        QPushButton#btnDesconectar:hover {
            background-color: #492027;
        }
        QPushButton#btnSalvarCsv {
            background-color: #10253d;
            color: #5b9dff;
            border-color: #295d97;
        }
        QPushButton#btnSalvarCsv:hover {
            background-color: #143353;
        }
        QPushButton:disabled, QComboBox:disabled {
            color: #70789a;
            border-color: #2a2f41;
            background-color: #131827;
        }
        QFrame#statusLed {
            min-width: 14px;
            max-width: 14px;
            min-height: 14px;
            max-height: 14px;
            border-radius: 7px;
            background-color: #6b728a;
            border: none;
        }
        QLabel#labelStatus {
            color: #7f88a8;
            font-size: 11pt;
        }
        )");

    updateStatusText("Desconectado", "#6b728a");
}

void MainWindow::configureChart()
{
    m_chart = new QChart();
    m_chart->setBackgroundVisible(false);
    m_chart->setMargins(QMargins(0, 0, 0, 0));
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignTop);
    m_chart->legend()->setLabelColor(QColor("#95a0c5"));
    QFont legendFont("Consolas");
    legendFont.setPointSize(11);
    m_chart->legend()->setFont(legendFont);

    m_rpmSeries = new QLineSeries(this);
    m_velSeries = new QLineSeries(this);
    m_tqSeries = new QLineSeries(this);

    m_rpmSeries->setName("RPM");
    m_velSeries->setName("VEL");
    m_tqSeries->setName("TORQUE");

    QPen rpmPen(QColor("#ff5d67"));
    rpmPen.setWidth(3);
    QPen velPen(QColor("#4b7dff"));
    velPen.setWidth(3);
    QPen tqPen(QColor("#42e38c"));
    tqPen.setWidth(3);

    m_rpmSeries->setPen(rpmPen);
    m_velSeries->setPen(velPen);
    m_tqSeries->setPen(tqPen);

    m_chart->addSeries(m_rpmSeries);
    m_chart->addSeries(m_velSeries);
    m_chart->addSeries(m_tqSeries);

    m_axisX = new QValueAxis(this);
    m_axisX->setRange(-30.0, 0.0);
    m_axisX->setTickCount(7);
    m_axisX->setMinorTickCount(0);
    m_axisX->setLabelFormat("%.0fs");
    m_axisX->setLabelsColor(QColor("#647094"));
    m_axisX->setGridLineColor(QColor("#232a41"));
    m_axisX->setLinePenColor(QColor("#232a41"));

    m_axisYRpm = new QValueAxis(this);
    m_axisYRpm->setRange(0.0, 100.0);
    m_axisYRpm->setTickCount(4);
    m_axisYRpm->setLabelsVisible(false);
    m_axisYRpm->setGridLineColor(QColor("#232a41"));
    m_axisYRpm->setLinePenColor(Qt::transparent);

    m_axisYVel = new QValueAxis(this);
    m_axisYVel->setRange(0.0, 100.0);
    m_axisYVel->setTickCount(4);
    m_axisYVel->setLabelsVisible(false);
    m_axisYVel->setGridLineVisible(false);
    m_axisYVel->setLinePenColor(Qt::transparent);

    m_axisYTq = new QValueAxis(this);
    m_axisYTq->setRange(0.0, 100.0);
    m_axisYTq->setTickCount(4);
    m_axisYTq->setLabelsVisible(false);
    m_axisYTq->setGridLineVisible(false);
    m_axisYTq->setLinePenColor(Qt::transparent);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisYRpm, Qt::AlignLeft);
    m_chart->addAxis(m_axisYVel, Qt::AlignLeft);
    m_chart->addAxis(m_axisYTq, Qt::AlignLeft);

    m_rpmSeries->attachAxis(m_axisX);
    m_rpmSeries->attachAxis(m_axisYRpm);
    m_velSeries->attachAxis(m_axisX);
    m_velSeries->attachAxis(m_axisYVel);
    m_tqSeries->attachAxis(m_axisX);
    m_tqSeries->attachAxis(m_axisYTq);

    ui->chartView->setChart(m_chart);
}

void MainWindow::configureConnections()
{
    connect(ui->btnConectar, &QPushButton::clicked, this, &MainWindow::connectSelectedPort);
    connect(ui->btnDesconectar, &QPushButton::clicked, this, &MainWindow::disconnectPort);
    connect(ui->btnSalvarCsv, &QPushButton::clicked, this, &MainWindow::saveCsv);
    connect(m_portRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshPortList);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateConnectionIndicator);
    connect(m_serialPollTimer, &QTimer::timeout, this, &MainWindow::handleReadyRead);
    connect(ui->comboPorta, &QComboBox::currentTextChanged, this, [this](const QString &) {
        if (!serialIsOpen()) {
            ui->btnConectar->setEnabled(!selectedPortName().isEmpty());
        }
    });
}

bool MainWindow::openSerialPort(const QString &portName)
{
#ifdef Q_OS_WIN
    closeSerialPort();

    const std::wstring nativePortPath = portPath(portName).toStdWString();
    HANDLE handle = CreateFileW(
        nativePortPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        m_lastSerialError = windowsErrorString(GetLastError());
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(handle, &dcb)) {
        m_lastSerialError = windowsErrorString(GetLastError());
        CloseHandle(handle);
        return false;
    }

    dcb.BaudRate = kDefaultBaudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(handle, &dcb)) {
        m_lastSerialError = windowsErrorString(GetLastError());
        CloseHandle(handle);
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    if (!SetCommTimeouts(handle, &timeouts)) {
        m_lastSerialError = windowsErrorString(GetLastError());
        CloseHandle(handle);
        return false;
    }

    if (!SetupComm(handle, 4096, 4096)) {
        m_lastSerialError = windowsErrorString(GetLastError());
        CloseHandle(handle);
        return false;
    }

    if (!PurgeComm(handle, PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT)) {
        m_lastSerialError = windowsErrorString(GetLastError());
        CloseHandle(handle);
        return false;
    }

    m_serialHandle = reinterpret_cast<qintptr>(handle);
    m_lastSerialError.clear();
    m_serialPollTimer->start();
    return true;
#else
    Q_UNUSED(portName);
    m_lastSerialError = "Leitura serial nativa disponível apenas no Windows.";
    return false;
#endif
}

void MainWindow::closeSerialPort()
{
#ifdef Q_OS_WIN
    m_serialPollTimer->stop();
    if (serialIsOpen()) {
        CloseHandle(toHandle(m_serialHandle));
        m_serialHandle = kInvalidSerialHandle;
    }
#endif
}

bool MainWindow::serialIsOpen() const
{
    return m_serialHandle != kInvalidSerialHandle;
}

void MainWindow::setConnectedState(bool connected)
{
    ui->comboPorta->setEnabled(!connected);
    ui->btnConectar->setEnabled(!connected && !selectedPortName().isEmpty());
    ui->btnDesconectar->setEnabled(connected);
    ui->btnSalvarCsv->setEnabled(!m_samples.isEmpty());

    if (!connected) {
        updateStatusText("Desconectado", "#6b728a");
    } else {
        updateStatusText("Conectado - aguardando dados", "#42e38c");
    }
}

void MainWindow::connectSelectedPort()
{
    const QString portName = selectedPortName();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, "Porta serial", "Selecione ou digite uma porta, por exemplo COM3.");
        return;
    }

    m_serialBuffer.clear();
    m_lastIntervalMs = 100;
    m_elapsedBaseMs = m_samples.isEmpty() ? 0 : (m_samples.constLast().elapsedMs + m_lastIntervalMs);
    m_sessionTimer.start();

    if (!openSerialPort(portName)) {
        QMessageBox::critical(
            this,
            "Erro ao conectar",
            u"Não foi possível abrir a porta "_s + portName + u": "_s + m_lastSerialError);
        setConnectedState(false);
        return;
    }

    setConnectedState(true);
}

void MainWindow::disconnectPort()
{
    closeSerialPort();
    setConnectedState(false);
}

void MainWindow::saveCsv()
{
    if (m_samples.isEmpty()) {
        QMessageBox::information(this, "Salvar CSV", "Ainda não há dados para exportar.");
        return;
    }

    const QString defaultName =
        QDir::homePath() + u"/telemetria_"_s + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".csv";

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        "Salvar telemetria em CSV",
        defaultName,
        "Arquivo CSV (*.csv)");

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Salvar CSV", u"Não foi possível criar o arquivo: "_s + file.errorString());
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    const QLocale csvLocale(QLocale::Portuguese, QLocale::Brazil);
    out << "data_hora;tempo_ms;rpm;vel;tq\n";

    for (const Sample &sample : std::as_const(m_samples)) {
        out << QDateTime::fromMSecsSinceEpoch(sample.epochMs).toString("yyyy-MM-dd HH:mm:ss.zzz") << ';'
            << sample.elapsedMs << ';'
            << csvLocale.toString(sample.rpm, 'f', 2) << ';'
            << csvLocale.toString(sample.vel, 'f', 2) << ';'
            << csvLocale.toString(sample.tq, 'f', 2) << '\n';
    }

    QMessageBox::information(this, "Salvar CSV", "Arquivo CSV salvo com sucesso.");
}

void MainWindow::refreshPortList()
{
    if (serialIsOpen()) {
        return;
    }

    const QString currentPort = selectedPortName().isEmpty() ? "COM3" : selectedPortName();
    QStringList ports = availablePortNames();

    QCollator collator;
    collator.setNumericMode(true);
    std::sort(ports.begin(), ports.end(), [&collator](const QString &left, const QString &right) {
        return collator.compare(left, right) < 0;
    });

    ui->comboPorta->blockSignals(true);
    ui->comboPorta->clear();
    ui->comboPorta->addItems(ports);

    const int index = ui->comboPorta->findText(currentPort);
    if (index >= 0) {
        ui->comboPorta->setCurrentIndex(index);
    } else {
        ui->comboPorta->setEditText(currentPort);
    }
    ui->comboPorta->blockSignals(false);

    ui->btnConectar->setEnabled(!selectedPortName().isEmpty());
}

void MainWindow::handleReadyRead()
{
#ifdef Q_OS_WIN
    if (!serialIsOpen()) {
        return;
    }

    HANDLE handle = toHandle(m_serialHandle);
    DWORD errors = 0;
    COMSTAT status{};

    if (!ClearCommError(handle, &errors, &status)) {
        handleSerialError(u"Falha ao consultar a porta serial: "_s + windowsErrorString(GetLastError()));
        return;
    }

    if (errors != 0) {
        handleSerialError(u"Falha de comunicação na porta serial (código "_s + QString::number(errors) + ')');
        return;
    }

    if (status.cbInQue == 0) {
        return;
    }

    QByteArray chunk(static_cast<int>(status.cbInQue), Qt::Uninitialized);
    DWORD bytesRead = 0;
    if (!ReadFile(handle, chunk.data(), status.cbInQue, &bytesRead, nullptr)) {
        handleSerialError(u"Falha ao ler a porta serial: "_s + windowsErrorString(GetLastError()));
        return;
    }

    chunk.truncate(static_cast<int>(bytesRead));
    m_serialBuffer.append(chunk);
#endif

    while (true) {
        const int lineEnd = m_serialBuffer.indexOf('\n');
        if (lineEnd < 0) {
            break;
        }

        const QByteArray lineBytes = m_serialBuffer.left(lineEnd);
        m_serialBuffer.remove(0, lineEnd + 1);

        const QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        double rpm = 0.0;
        double vel = 0.0;
        double tq = 0.0;

        if (parseTelemetryLine(line, rpm, vel, tq)) {
            appendSample(rpm, vel, tq);
        }
    }
}

void MainWindow::updateConnectionIndicator()
{
    if (!serialIsOpen()) {
        updateStatusText("Desconectado", "#6b728a");
        return;
    }

    if (m_samples.isEmpty()) {
        updateStatusText("Conectado - aguardando dados", "#42e38c");
        return;
    }

    const qint64 ageMs = QDateTime::currentMSecsSinceEpoch() - m_samples.constLast().epochMs;
    if (ageMs > kStatusTimeoutMs) {
        updateStatusText(u"Conectado - sem dados recentes ("_s + QString::number(ageMs) + " ms)", "#ffb14a");
        return;
    }

    updateStatusText(u"Conectado - "_s + QString::number(m_lastIntervalMs) + " ms", "#42e38c");
}

void MainWindow::appendSample(double rpm, double vel, double tq)
{
    Sample sample;
    sample.epochMs = QDateTime::currentMSecsSinceEpoch();
    sample.elapsedMs = m_elapsedBaseMs + (m_sessionTimer.isValid() ? m_sessionTimer.elapsed() : 0);
    sample.rpm = rpm;
    sample.vel = vel;
    sample.tq = tq;

    if (!m_samples.isEmpty()) {
        m_lastIntervalMs = qMax<qint64>(1, sample.elapsedMs - m_samples.constLast().elapsedMs);
    }

    m_samples.append(sample);
    updateMetricLabels(sample);
    updateChart();
    ui->btnSalvarCsv->setEnabled(true);
}

void MainWindow::updateMetricLabels(const Sample &sample)
{
    ui->labelRPM->setText(formatInteger(sample.rpm));
    ui->labelVEL->setText(formatDecimal(sample.vel, 1));
    ui->labelTQ->setText(formatDecimal(sample.tq, 1));
}

void MainWindow::updateChart()
{
    if (m_samples.isEmpty()) {
        m_rpmSeries->clear();
        m_velSeries->clear();
        m_tqSeries->clear();
        m_axisX->setRange(-30.0, 0.0);
        m_axisYRpm->setRange(0.0, 100.0);
        m_axisYVel->setRange(0.0, 100.0);
        m_axisYTq->setRange(0.0, 100.0);
        return;
    }

    const qint64 latestElapsed = m_samples.constLast().elapsedMs;
    int startIndex = m_samples.size() - 1;
    while (startIndex > 0 && (latestElapsed - m_samples.at(startIndex - 1).elapsedMs) <= kChartWindowMs) {
        --startIndex;
    }

    QList<QPointF> rpmPoints;
    QList<QPointF> velPoints;
    QList<QPointF> tqPoints;
    rpmPoints.reserve(m_samples.size() - startIndex);
    velPoints.reserve(m_samples.size() - startIndex);
    tqPoints.reserve(m_samples.size() - startIndex);

    double minRpm = std::numeric_limits<double>::max();
    double maxRpm = std::numeric_limits<double>::lowest();
    double minVel = std::numeric_limits<double>::max();
    double maxVel = std::numeric_limits<double>::lowest();
    double minTq = std::numeric_limits<double>::max();
    double maxTq = std::numeric_limits<double>::lowest();

    for (int index = startIndex; index < m_samples.size(); ++index) {
        const Sample &sample = m_samples.at(index);
        const double seconds = static_cast<double>(sample.elapsedMs - latestElapsed) / 1000.0;

        rpmPoints.append(QPointF(seconds, sample.rpm));
        velPoints.append(QPointF(seconds, sample.vel));
        tqPoints.append(QPointF(seconds, sample.tq));

        minRpm = std::min(minRpm, sample.rpm);
        maxRpm = std::max(maxRpm, sample.rpm);
        minVel = std::min(minVel, sample.vel);
        maxVel = std::max(maxVel, sample.vel);
        minTq = std::min(minTq, sample.tq);
        maxTq = std::max(maxTq, sample.tq);
    }

    m_rpmSeries->replace(rpmPoints);
    m_velSeries->replace(velPoints);
    m_tqSeries->replace(tqPoints);

    auto applyAxisRange = [](QValueAxis *axis, double minValue, double maxValue) {
        double lower = minValue;
        double upper = maxValue;

        if (qFuzzyCompare(lower, upper)) {
            lower -= 1.0;
            upper += 1.0;
        }

        const double padding = qMax(1.0, (upper - lower) * 0.12);
        if (lower >= 0.0) {
            lower = qMax(0.0, lower - padding);
        } else {
            lower -= padding;
        }
        upper += padding;

        axis->setRange(lower, upper);
    };

    m_axisX->setRange(-30.0, 0.0);
    applyAxisRange(m_axisYRpm, minRpm, maxRpm);
    applyAxisRange(m_axisYVel, minVel, maxVel);
    applyAxisRange(m_axisYTq, minTq, maxTq);
}

void MainWindow::updateStatusText(const QString &text, const QString &color)
{
    ui->labelStatus->setText(text);
    ui->statusLed->setStyleSheet(u"#statusLed { background-color: "_s + color + "; border-radius: 7px; }");
}

void MainWindow::handleSerialError(const QString &message)
{
    closeSerialPort();
    setConnectedState(false);
    QMessageBox::critical(this, "Erro serial", message);
}

bool MainWindow::parseTelemetryLine(const QString &line, double &rpm, double &vel, double &tq) const
{
    static const QRegularExpression rpmRegex(R"((?i)\brpm\b\s*[:=]\s*([-+]?\d+(?:[.,]\d+)?))");
    static const QRegularExpression velRegex(
        R"((?i)\b(?:vel|velocidade|speed)\b\s*[:=]\s*([-+]?\d+(?:[.,]\d+)?))");
    static const QRegularExpression tqRegex(R"((?i)\b(?:tq|torque)\b\s*[:=]\s*([-+]?\d+(?:[.,]\d+)?))");

    const QRegularExpressionMatch rpmMatch = rpmRegex.match(line);
    const QRegularExpressionMatch velMatch = velRegex.match(line);
    const QRegularExpressionMatch tqMatch = tqRegex.match(line);

    if (rpmMatch.hasMatch() && velMatch.hasMatch() && tqMatch.hasMatch()) {
        return parseNumberToken(rpmMatch.captured(1), rpm)
            && parseNumberToken(velMatch.captured(1), vel)
            && parseNumberToken(tqMatch.captured(1), tq);
    }

    const QStringList semicolonTokens = line.split(QRegularExpression(R"([;\s]+)"), Qt::SkipEmptyParts);
    if (semicolonTokens.size() >= 3) {
        return parseNumberToken(semicolonTokens.at(0), rpm)
            && parseNumberToken(semicolonTokens.at(1), vel)
            && parseNumberToken(semicolonTokens.at(2), tq);
    }

    const QStringList commaTokens = line.split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts);
    if (commaTokens.size() >= 3) {
        return parseNumberToken(commaTokens.at(0), rpm)
            && parseNumberToken(commaTokens.at(1), vel)
            && parseNumberToken(commaTokens.at(2), tq);
    }

    return false;
}

bool MainWindow::parseNumberToken(QString token, double &value)
{
    token = token.trimmed();
    token.replace(',', '.');

    bool ok = false;
    value = QLocale::c().toDouble(token, &ok);
    return ok;
}

QStringList MainWindow::availablePortNames()
{
    QStringList ports;

#ifdef Q_OS_WIN
    std::vector<wchar_t> buffer(32'768, L'\0');
    const DWORD length = QueryDosDeviceW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length != 0) {
        const wchar_t *cursor = buffer.data();
        static const QRegularExpression portRegex(R"(^COM\d+$)", QRegularExpression::CaseInsensitiveOption);

        while (*cursor != L'\0') {
            const QString deviceName = QString::fromWCharArray(cursor).trimmed().toUpper();
            if (portRegex.match(deviceName).hasMatch()) {
                ports.append(deviceName);
            }
            cursor += std::wcslen(cursor) + 1;
        }
    }

    ports.removeDuplicates();
#endif

    return ports;
}

QString MainWindow::windowsErrorString(quint32 errorCode)
{
#ifdef Q_OS_WIN
    LPWSTR buffer = nullptr;
    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    QString message;
    if (size != 0 && buffer != nullptr) {
        message = QString::fromWCharArray(buffer, static_cast<int>(size)).trimmed();
        LocalFree(buffer);
    }

    if (message.isEmpty()) {
        message = u"erro "_s + QString::number(errorCode);
    }
    return message;
#else
    return u"erro "_s + QString::number(errorCode);
#endif
}

QString MainWindow::selectedPortName() const
{
    return ui->comboPorta->currentText().trimmed().toUpper();
}
