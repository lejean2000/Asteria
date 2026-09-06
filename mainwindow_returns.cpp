#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTableWidget>
#include <QHeaderView>
#include <QDir>
#include <QStandardPaths>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QPixmap>
#include <QTextStream>
#include <QDebug>
#if defined(FLATHUB_BUILD) || defined(GENTOO_BUILD)
// QPdfWriter is not available in Flathub
#else
#include <QPdfWriter>
#include <QPrinter>
#include <QPrintDialog>
#endif
#include <QTextDocument>
#include <QScrollBar>
#include "Globals.h"
#include "aspectsettingsdialog.h"
#include <QCheckBox>
#include <QRegularExpression>
#include <QClipboard>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QProcess>

extern QString g_astroFontFamily;
void MainWindow::calculateSolarReturn()
{
    QString dateText = m_birthDateEdit->text();

    if (!validateDateFormat(dateText, this)) {

        return;

    }
    // Get input values
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    // Prompt user for the solar return year
    bool ok = false;
    int year = QInputDialog::getInt(
                this,
                tr("Solar Return Year"),
                tr("Enter the year for the solar return:"),
                QDate::currentDate().year(), // default value
                1900, 2999, 1, &ok
                );
    if (!ok) {
        // User cancelled the dialog
        return;
    }

    // Create a non-modal dialog with a Continue button
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    // Connect the continue button
    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doSolarReturnCalculation(birthDate, birthTime, year);
    });

    dialog->show();
}

void MainWindow::doSolarReturnCalculation(const QDate& birthDate, const QTime& birthTime, int year)
{

    // Get the (possibly updated) input values
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    // Validate inputs
    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    // Reset chart state before new calculation
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject(); // Reset relationship info
    m_chartRenderer->scene()->clear();

    // Calculate solar return chart
    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());

    m_currentChartData = m_chartDataManager.calculateSolarReturnAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, houseSystem, year
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        // Display chart
        displayChart(m_currentChartData);
        m_chartCalculated = true;

        AsteriaGlobals::lastGeneratedChartType = "Solar Return";


        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);

        // Clear previous interpretation

        statusBar()->showMessage("Solar return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Solar Return Chart");


        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr   = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Solar Return Year: %1\n"
                    "Solar Return Moment\n"
                    "Date: %2\n"
                    "Time: %3\n"
                    "Julian Day: %4\n\n"
                    ).arg(QString::number(year), dateStr, timeStr, jdStr);

        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Solar return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Solar Return Moment", msg);

    } else {
        handleError("Solar return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        // Clear any partial chart data after error
        m_chartRenderer->scene()->clear();
    }
}

void MainWindow::calculateLunarReturn()
{
    QString dateText = m_birthDateEdit->text();

    if (!validateDateFormat(dateText, this)) {

        return;

    }
    // Get input values
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    // Prompt user for the lunar return year
    bool ok = false;
    int year = QInputDialog::getInt(
                this,
                tr("Lunar Return Year"),
                tr("Enter the year for the lunar return:"),
                QDate::currentDate().year(),
                1900, 2100, 1, &ok
                );
    if (!ok) {
        // User cancelled the dialog
        return;
    }

    // Prompt user for the lunar return month
    int month = QInputDialog::getInt(
                this,
                tr("Lunar Return Month"),
                tr("Enter the month for the lunar return (1-12):"),
                QDate::currentDate().month(),
                1, 12, 1, &ok
                );
    if (!ok) {
        // User cancelled the dialog
        return;
    }

    // Use the first day of the selected month as the target date
    QDate targetDate(year, month, 1);

    // Create a non-modal dialog with a Continue button
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    // Connect the continue button
    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doLunarReturnCalculation(birthDate, birthTime, targetDate);
    });

    dialog->show();
}

void MainWindow::doLunarReturnCalculation(const QDate& birthDate, const QTime& birthTime, const QDate& targetDate)
{
    // Get the (possibly updated) input values
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    // Validate inputs
    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    // Reset chart state before new calculation
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject(); // Reset relationship info
    m_chartRenderer->scene()->clear();

    // Calculate lunar return chart
    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());

    m_currentChartData = m_chartDataManager.calculateLunarReturnAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, houseSystem, targetDate
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        // Display chart
        displayChart(m_currentChartData);
        m_chartCalculated = true;
        // Set chart type for interpretation
        AsteriaGlobals::lastGeneratedChartType = "Lunar Return";

        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);

        // Clear previous interpretation

        statusBar()->showMessage("Lunar return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Lunar Return Chart");


        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr   = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Lunar Return Moment\n"
                    "Date: %1\n"
                    "Time: %2\n"
                    "Julian Day: %3\n\n"
                    ).arg(dateStr, timeStr, jdStr);

        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Lunar return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Lunar Return Moment", msg);

    } else {
        handleError("Lunar return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        // Clear any partial chart data after error
        m_chartRenderer->scene()->clear();
    }
}


void MainWindow::calculateSaturnReturn()
{
    QString dateText = m_birthDateEdit->text();

    if (!validateDateFormat(dateText, this)) {

        return;

    }
    // Get input values
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    // Calculate approximate current Saturn return
    double saturnPeriod = 29.4571; // Saturn orbital period in years
    int daysSinceBirth = birthDate.daysTo(QDate::currentDate());
    double yearsSinceBirth = daysSinceBirth / 365.25;
    int approxReturn = static_cast<int>(yearsSinceBirth / saturnPeriod) + 1;

    // Prompt user for which Saturn return
    bool ok = false;
    QString prompt = tr("Enter which Saturn return (1 = first, 2 = second, etc.):\n"
                        "Saturn orbital period: %1 years\n"
                        "You are currently in your approx. %2 Saturn return.")

            .arg(QString::number(saturnPeriod, 'f', 3))
            .arg(approxReturn);

    int returnNumber = QInputDialog::getInt(
                this,
                tr("Saturn Return Number"),
                prompt,
                approxReturn, 1, 200, 1, &ok // Allow up to 5 Saturn returns
                );

    if (!ok) return;

    // Create a non-modal dialog with a Continue button
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    // Connect the continue button
    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doSaturnReturnCalculation(birthDate, birthTime, returnNumber);
    });

    dialog->show();
}


void MainWindow::doSaturnReturnCalculation(const QDate& birthDate, const QTime& birthTime, int returnNumber)
{
    // Get the (possibly updated) input values
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    // Validate inputs
    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }


    // Reset chart state before new calculation
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();


    // Calculate Saturn return chart
    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());


    m_currentChartData = m_chartDataManager.calculateSaturnReturnAsJson(
                chartDate, birthTime, utcOffset, latitude, longitude, houseSystem, returnNumber
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        // Display chart
        displayChart(m_currentChartData);
        m_chartCalculated = true;
        AsteriaGlobals::lastGeneratedChartType = "Saturn Return";
        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);

        // Clear previous interpretation

        statusBar()->showMessage("Saturn return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Saturn Return Chart");

        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Saturn Return %1\n"
                    "Saturn Return Moment\n"
                    "Date: %2\n"
                    "Time: %3\n"
                    "Julian Day: %4\n\n"
                    ).arg(returnNumber).arg(dateStr).arg(timeStr).arg(jdStr);

        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Saturn return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Saturn Return Moment", msg);

    } else {
        handleError("Saturn return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        m_chartRenderer->scene()->clear();
    }
}

void MainWindow::calculateJupiterReturn()
{
    QString dateText = m_birthDateEdit->text();

    if (!validateDateFormat(dateText, this)) {

        return;

    }
    // Get input values
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    // Calculate approximate current Jupiter return
    double jupiterPeriod = 11.862; // Jupiter orbital period in years
    int daysSinceBirth = birthDate.daysTo(QDate::currentDate());
    double yearsSinceBirth = daysSinceBirth / 365.25;
    int approxReturn = static_cast<int>(yearsSinceBirth / jupiterPeriod) + 1;

    // Prompt user for which Jupiter return
    bool ok = false;
    QString prompt = tr("Enter which Jupiter return (1 = first, 2 = second, etc.):\n"
                        "Jupiter orbital period: %1 years\n"
                        "You are currently in your approx. %2 Jupiter return.")
            .arg(QString::number(jupiterPeriod, 'f', 3))
            .arg(approxReturn);

    int returnNumber = QInputDialog::getInt(
                this,
                tr("Jupiter Return Number"),
                prompt,
                approxReturn, 1, 300, 1, &ok // Allow up to 20 returns for flexibility
                );

    if (!ok) return;

    // Create a non-modal dialog with a Continue button
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    // Connect the continue button
    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doJupiterReturnCalculation(birthDate, birthTime, returnNumber);
    });

    dialog->show();
}


void MainWindow::doJupiterReturnCalculation(const QDate& birthDate, const QTime& birthTime, int returnNumber)
{
    // Get the (possibly updated) input values
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    // Validate inputs
    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    // Reset chart state before new calculation
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();

    // Calculate Jupiter return chart
    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());

    m_currentChartData = m_chartDataManager.calculateJupiterReturnAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, houseSystem, returnNumber
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        // Display chart
        displayChart(m_currentChartData);
        m_chartCalculated = true;
        AsteriaGlobals::lastGeneratedChartType = "Jupiter Return";
        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);

        statusBar()->showMessage("Jupiter return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Jupiter Return Chart");

        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Jupiter Return %1\n"
                    "Jupiter Return Moment\n"
                    "Date: %2\n"
                    "Time: %3\n"
                    "Julian Day: %4\n\n"

                    ).arg(returnNumber).arg(dateStr).arg(timeStr).arg(jdStr);
        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Jupiter return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Jupiter Return Moment", msg);

    } else {
        handleError("Jupiter return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        m_chartRenderer->scene()->clear();
    }
}

// more planet returns

void MainWindow::calculateVenusReturn()
{
    QString dateText = m_birthDateEdit->text();

    if (!validateDateFormat(dateText, this)) {

        return;

    }
    // Get input values
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    // Calculate approximate current Venus return
    double venusPeriod = 0.61519726; // Venus orbital period in years
    int daysSinceBirth = birthDate.daysTo(QDate::currentDate());
    double yearsSinceBirth = daysSinceBirth / 365.25;
    int approxReturn = static_cast<int>(yearsSinceBirth / venusPeriod) + 1;

    // Prompt user for which Venus return
    bool ok = false;
    QString prompt = tr("Enter which Venus return (1 = first, 2 = second, etc.):\n"
                        "Venus orbital period: %1 years\n"
                        "You are currently in your approx. %2 Venus return.")
            .arg(QString::number(venusPeriod, 'f', 3))
            .arg(approxReturn);

    int returnNumber = QInputDialog::getInt(
                this,
                tr("Venus Return Number"),
                prompt,
                approxReturn, 1, 1000, 1, &ok
                );
    if (!ok) return;

    // Create a non-modal dialog with a Continue button
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    // Connect the continue button
    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doVenusReturnCalculation(birthDate, birthTime, returnNumber);
    });

    dialog->show();
}

void MainWindow::doVenusReturnCalculation(const QDate& birthDate, const QTime& birthTime, int returnNumber)
{
    // Get the (possibly updated) input values
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    // Validate inputs
    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    // Reset chart state before new calculation
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();

    // Calculate Venus return chart
    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());

    m_currentChartData = m_chartDataManager.calculateVenusReturnAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, houseSystem, returnNumber
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        // Display chart
        displayChart(m_currentChartData);
        m_chartCalculated = true;
        AsteriaGlobals::lastGeneratedChartType = "Venus Return";
        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);
        statusBar()->showMessage("Venus return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Venus Return Chart");

        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Venus Return %1\n"
                    "Venus Return Moment\n"
                    "Date: %2\n"
                    "Time: %3\n"
                    "Julian Day: %4\n\n"
                    ).arg(returnNumber).arg(dateStr).arg(timeStr).arg(jdStr);

        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Venus return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Venus Return Moment", msg);

    } else {
        handleError("Venus return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        m_chartRenderer->scene()->clear();
    }
}

/* --- MARS RETURN --- */
void MainWindow::calculateMarsReturn()
{
    QString dateText = m_birthDateEdit->text();

    if (!validateDateFormat(dateText, this)) {

        return;

    }
    // Get input values
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    // Calculate approximate current Mars return
    double marsPeriod = 1.8808476; // Mars orbital period in years
    int daysSinceBirth = birthDate.daysTo(QDate::currentDate());
    double yearsSinceBirth = daysSinceBirth / 365.25;
    int approxReturn = static_cast<int>(yearsSinceBirth / marsPeriod) + 1;

    // Prompt user for which Mars return
    bool ok = false;
    QString prompt = tr("Enter which Mars return (1 = first, 2 = second, etc.):\n"
                        "Mars orbital period: %1 years\n"
                        "You are currently in your approx. %2 Mars return.")
            .arg(QString::number(marsPeriod, 'f', 3))
            .arg(approxReturn);

    int returnNumber = QInputDialog::getInt(
                this,
                tr("Mars Return Number"),
                prompt,
                approxReturn, 1, 1000, 1, &ok
                );
    if (!ok) return;

    // Create a non-modal dialog with a Continue button
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    // Connect the continue button
    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doMarsReturnCalculation(birthDate, birthTime, returnNumber);
    });

    dialog->show();
}

void MainWindow::doMarsReturnCalculation(const QDate& birthDate, const QTime& birthTime, int returnNumber)
{
    // Get the (possibly updated) input values
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    // Validate inputs
    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    // Reset chart state before new calculation
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();

    // Calculate Mars return chart
    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());

    m_currentChartData = m_chartDataManager.calculateMarsReturnAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, houseSystem, returnNumber
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        // Display chart
        displayChart(m_currentChartData);
        m_chartCalculated = true;
        AsteriaGlobals::lastGeneratedChartType = "Mars Return";
        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);
        statusBar()->showMessage("Mars return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Mars Return Chart");

        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Mars Return %1\n"
                    "Mars Return Moment\n"
                    "Date: %2\n"
                    "Time: %3\n"
                    "Julian Day: %4\n\n"
                    ).arg(returnNumber).arg(dateStr).arg(timeStr).arg(jdStr);

        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Mars return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Mars Return Moment", msg);

    } else {
        handleError("Mars return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        m_chartRenderer->scene()->clear();
    }
}

/* --- MERCURY RETURN --- */
void MainWindow::calculateMercuryReturn()
{
    QString dateText = m_birthDateEdit->text();

    if (!validateDateFormat(dateText, this)) {

        return;

    }
    // Get input values
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    // Calculate approximate current Mercury return
    double mercuryPeriod = 0.2408467; // Mercury orbital period in years
    int daysSinceBirth = birthDate.daysTo(QDate::currentDate());
    double yearsSinceBirth = daysSinceBirth / 365.25;
    int approxReturn = static_cast<int>(yearsSinceBirth / mercuryPeriod) + 1;

    // Prompt user for which Mercury return
    bool ok = false;
    QString prompt = tr("Enter which Mercury return (1 = first, 2 = second, etc.):\n"
                        "Mercury orbital period: %1 years\n"
                        "You are currently in your approx. %2 Mercury return.")
            .arg(QString::number(mercuryPeriod, 'f', 3))
            .arg(approxReturn);

    int returnNumber = QInputDialog::getInt(
                this,
                tr("Mercury Return Number"),
                prompt,
                approxReturn, 1, 1000, 1, &ok
                );
    if (!ok) return;

    // Create a non-modal dialog with a Continue button
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    // Connect the continue button
    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doMercuryReturnCalculation(birthDate, birthTime, returnNumber);
    });

    dialog->show();
}

void MainWindow::doMercuryReturnCalculation(const QDate& birthDate, const QTime& birthTime, int returnNumber)
{
    // Get the (possibly updated) input values
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    // Validate inputs
    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    // Reset chart state before new calculation
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();

    // Calculate Mercury return chart
    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());

    m_currentChartData = m_chartDataManager.calculateMercuryReturnAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, houseSystem, returnNumber
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        // Display chart
        displayChart(m_currentChartData);
        m_chartCalculated = true;
        AsteriaGlobals::lastGeneratedChartType = "Mercury Return";
        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);
        statusBar()->showMessage("Mercury return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Mercury Return Chart");

        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Mercury Return %1\n"
                    "Mercury Return Moment\n"
                    "Date: %2\n"
                    "Time: %3\n"
                    "Julian Day: %4\n\n"
                    ).arg(returnNumber).arg(dateStr).arg(timeStr).arg(jdStr);

        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Mercury return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Mercury Return Moment", msg);

    } else {
        handleError("Mercury return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        m_chartRenderer->scene()->clear();
    }
}


bool MainWindow::validateDateFormat(const QString& dateText, QWidget* parentWidget)
{
    QRegularExpressionMatch match = dateRegex.match(dateText);
    if (!match.hasMatch()) {
        QMessageBox::warning(parentWidget ? parentWidget : this, tr("Input Error"),
                             tr("Please enter the date in DD/MM/YYYY format with a four-digit year (0001–3000)."));
        return false;
    }

    // Now, check if the date is a real calendar date
    QDate date = QDate::fromString(dateText, "dd/MM/yyyy");
    if (!date.isValid()) {
        QMessageBox::warning(parentWidget ? parentWidget : this, tr("Input Error"),
                             tr("The date you entered does not exist in the calendar.\n\n"
                                "Examples of invalid dates:\n"
                                "  - 30/02/2023 (February never has 30 days)\n"
                                "  - 31/04/2022 (April has only 30 days)\n"
                                "  - 29/02/2023 (2023 is not a leap year)\n"
                                "  - 32/01/2020 (No month has more than 31 days)\n"
                                "  - 15/13/2020 (There is no 13th month)\n\n"
                                "Please check your entry and try again."));
        return false;
    }
    return true;
}

QDate MainWindow::julianToGregorian(int year, int month, int day) const
{
    // Calculate Julian Day Number for Julian calendar date
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    int julianDay = day + ((153 * m + 2) / 5) + 365 * y + y / 4 - 32083;

    // Now convert that JDN to Gregorian date using QDate
    QDate gregorianDate = QDate::fromJulianDay(julianDay);
    return gregorianDate;
}

QDate MainWindow::checkAndConvertJulian(const QDate& date, bool useJulian) const
{
    QDate gregorianStart(1582, 10, 15);
    if (useJulian && date.isValid() && date < gregorianStart) {
        QDate converted = julianToGregorian(date.year(), date.month(), date.day());
        qDebug() << "Julian input:" << date.toString(Qt::ISODate)
                 << "-> Gregorian:" << converted.toString(Qt::ISODate);
        return converted;
    }
    return date;
}

// Uranus Neptune Pluto Returns
void MainWindow::calculateUranusReturn()
{
    QString dateText = m_birthDateEdit->text();
    if (!validateDateFormat(dateText, this)) {
        return;
    }
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    double uranusPeriod = 84.016846; // Uranus orbital period in years
    int daysSinceBirth = birthDate.daysTo(QDate::currentDate());
    double yearsSinceBirth = daysSinceBirth / 365.25;
    int approxReturn = static_cast<int>(yearsSinceBirth / uranusPeriod) + 1;

    bool ok = false;
    QString prompt = tr("Enter which Uranus return (1 = first, 2 = second, etc.):\n"
                        "Uranus orbital period: %1 years\n"
                        "You are currently in your approx. %2 Uranus return.")
            .arg(QString::number(uranusPeriod, 'f', 3))
            .arg(approxReturn);

    int returnNumber = QInputDialog::getInt(
                this,
                tr("Uranus Return Number"),
                prompt,
                approxReturn, 1, 200, 1, &ok
                );
    if (!ok) return;

    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);
    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doUranusReturnCalculation(birthDate, birthTime, returnNumber);
    });
    dialog->show();
}

void MainWindow::doUranusReturnCalculation(const QDate& birthDate, const QTime& birthTime, int returnNumber)
{
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();

    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());
    m_currentChartData = m_chartDataManager.calculateUranusReturnAsJson(
                chartDate, birthTime, utcOffset, latitude, longitude, houseSystem, returnNumber
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        displayChart(m_currentChartData);
        m_chartCalculated = true;
        AsteriaGlobals::lastGeneratedChartType = "Uranus Return";
        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);

        statusBar()->showMessage("Uranus return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Uranus Return Chart");

        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Uranus Return %1\n"
                    "Uranus Return Moment\n"
                    "Date: %2\n"
                    "Time: %3\n"
                    "Julian Day: %4\n\n"
                    ).arg(returnNumber).arg(dateStr).arg(timeStr).arg(jdStr);

        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Uranus return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Uranus Return Moment", msg);

    } else {
        handleError("Uranus return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        m_chartRenderer->scene()->clear();
    }
}

// Repeat for Neptune

void MainWindow::calculateNeptuneReturn()
{
    QString dateText = m_birthDateEdit->text();
    if (!validateDateFormat(dateText, this)) {
        return;
    }
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    double neptunePeriod = 164.79132; // Neptune orbital period in years
    int daysSinceBirth = birthDate.daysTo(QDate::currentDate());
    double yearsSinceBirth = daysSinceBirth / 365.25;
    int approxReturn = static_cast<int>(yearsSinceBirth / neptunePeriod) + 1;

    bool ok = false;
    QString prompt = tr("Enter which Neptune return (1 = first, 2 = second, etc.):\n"
                        "Neptune orbital period: %1 years\n"
                        "You are currently in your approx. %2 Neptune return.")
            .arg(QString::number(neptunePeriod, 'f', 3))
            .arg(approxReturn);

    int returnNumber = QInputDialog::getInt(
                this,
                tr("Neptune Return Number"),
                prompt,
                approxReturn, 1, 200, 1, &ok
                );
    if (!ok) return;

    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);
    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doNeptuneReturnCalculation(birthDate, birthTime, returnNumber);
    });
    dialog->show();
}

void MainWindow::doNeptuneReturnCalculation(const QDate& birthDate, const QTime& birthTime, int returnNumber)
{
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();

    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());
    m_currentChartData = m_chartDataManager.calculateNeptuneReturnAsJson(
                chartDate, birthTime, utcOffset, latitude, longitude, houseSystem, returnNumber
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        displayChart(m_currentChartData);
        m_chartCalculated = true;
        AsteriaGlobals::lastGeneratedChartType = "Neptune Return";
        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);

        statusBar()->showMessage("Neptune return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Neptune Return Chart");

        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Neptune Return %1\n"
                    "Neptune Return Moment\n"
                    "Date: %2\n"
                    "Time: %3\n"
                    "Julian Day: %4\n\n"
                    ).arg(returnNumber).arg(dateStr).arg(timeStr).arg(jdStr);

        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Neptune return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Neptune Return Moment", msg);

    } else {
        handleError("Neptune return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        m_chartRenderer->scene()->clear();
    }
}

// Repeat for Pluto

void MainWindow::calculatePlutoReturn()
{
    QString dateText = m_birthDateEdit->text();
    if (!validateDateFormat(dateText, this)) {
        return;
    }
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    double plutoPeriod = 248.00; // Pluto orbital period in years (approximate)
    int daysSinceBirth = birthDate.daysTo(QDate::currentDate());
    double yearsSinceBirth = daysSinceBirth / 365.25;
    int approxReturn = static_cast<int>(yearsSinceBirth / plutoPeriod) + 1;

    bool ok = false;
    QString prompt = tr("Enter which Pluto return (1 = first, 2 = second, etc.):\n"
                        "Pluto orbital period: %1 years\n"
                        "You are currently in your approx. %2 Pluto return.")
            .arg(QString::number(plutoPeriod, 'f', 3))
            .arg(approxReturn);

    int returnNumber = QInputDialog::getInt(
                this,
                tr("Pluto Return Number"),
                prompt,
                approxReturn, 1, 200, 1, &ok
                );
    if (!ok) return;

    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);
    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doPlutoReturnCalculation(birthDate, birthTime, returnNumber);
    });
    dialog->show();
}

void MainWindow::doPlutoReturnCalculation(const QDate& birthDate, const QTime& birthTime, int returnNumber)
{
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();

    QDate chartDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());
    m_currentChartData = m_chartDataManager.calculatePlutoReturnAsJson(
                chartDate, birthTime, utcOffset, latitude, longitude, houseSystem, returnNumber
                );

    if (m_chartDataManager.getLastError().isEmpty()) {
        displayChart(m_currentChartData);
        m_chartCalculated = true;
        AsteriaGlobals::lastGeneratedChartType = "Pluto Return";
        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);

        statusBar()->showMessage("Pluto return chart calculated successfully", 3000);
        setWindowTitle("Asteria - Pluto Return Chart");

        QString dateStr = m_currentChartData.value("returnDate").toString();
        QString timeStr = m_currentChartData.value("returnTime").toString();
        QString jdStr = m_currentChartData.value("returnJulianDay").toString();

        QString infoText = QString(
                    "Pluto Return %1\n"
                    "Pluto Return Moment\n"
                    "Date: %2\n"
                    "Time: %3\n"
                    "Julian Day: %4\n\n"
                    ).arg(returnNumber).arg(dateStr).arg(timeStr).arg(jdStr);

        appendInterpretationEntry("chart_info", AsteriaGlobals::lastGeneratedChartType, infoText);

        QString msg = QString("Pluto return occurs on %1 at %2 (Julian Day: %3)")
                .arg(dateStr)
                .arg(timeStr)
                .arg(jdStr);

        QMessageBox::information(this, "Pluto Return Moment", msg);

    } else {
        handleError("Pluto return calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        m_chartRenderer->scene()->clear();
    }
}

// Secondary Progression Chart

void MainWindow::calculateSecondaryProgression()
{
    QString dateText = m_birthDateEdit->text();
    if (!validateDateFormat(dateText, this)) {
        return;
    }

    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");

    int daysSinceBirth = birthDate.daysTo(QDate::currentDate());
    int yearsSinceBirth = static_cast<int>(daysSinceBirth / 365.25);

    bool ok = false;
    QString prompt = tr("Enter the progression year (1 = first year after birth, etc.):\n"
                        "You are currently approximately %1 years old.")
            .arg(yearsSinceBirth);

    int progressionYear = QInputDialog::getInt(
                this,
                tr("Secondary Progression Year"),
                prompt,
                qMax(1, yearsSinceBirth), 1, 120, 1, &ok
                );
    if (!ok) return;

    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Location and UTC Offset"));
    dialog->setModal(false);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QLabel* label = new QLabel(tr(
                                   "If you want to set a different location, set it now with the proper UTC offset; "
                                   "otherwise, the birth location will be used.\n\n"
                                   "Click 'Continue' when ready."
                                   ), dialog);
    label->setWordWrap(true);
    layout->addWidget(label);
    QPushButton* continueBtn = new QPushButton(tr("Continue"), dialog);
    layout->addWidget(continueBtn);

    connect(continueBtn, &QPushButton::clicked, this, [=]() {
        dialog->close();
        doSecondaryProgressionCalculation(progressionYear);
    });
    dialog->show();
}

// Filter out additional bodies (Ceres, Pallas, etc.) when the checkbox is unchecked
ChartData MainWindow::filterAdditionalBodies(const ChartData &data) const
{
    if (m_additionalBodiesCB->isChecked()) return data;

    static const QSet<Planet> additionalSet = {
        Planet::Ceres, Planet::Pallas, Planet::Juno, Planet::Vesta,
        Planet::Lilith, Planet::Vertex, Planet::PartOfSpirit, Planet::EastPoint
    };

    ChartData result = data;
    result.planets.erase(std::remove_if(result.planets.begin(), result.planets.end(),
        [](const PlanetData &p){ return additionalSet.contains(p.id); }),
        result.planets.end());
    result.aspects.erase(std::remove_if(result.aspects.begin(), result.aspects.end(),
        [](const AspectData &a){
            return additionalSet.contains(a.planet1) || additionalSet.contains(a.planet2);
        }),
        result.aspects.end());
    return result;
}

// Helper: Calculate and display the secondary progression chart as a bi-wheel
void MainWindow::doSecondaryProgressionCalculation(int progressionYear)
{
    QString dateText = m_birthDateEdit->text();
    if (!validateDateFormat(dateText, this)) {
        return;
    }

    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");
    QString utcOffset  = m_utcOffsetCombo->currentText();
    QString latitude   = m_latitudeEdit->text();
    QString longitude  = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    // Secondary progressions: 1 day after birth = year 1, etc.
    QDate progressedDate = birthDate.addDays(progressionYear);
    progressedDate = checkAndConvertJulian(progressedDate, useJulianForPre1582Action->isChecked());

    // Reset chart state
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();
    aspectPatternsTable->setRowCount(0); // patterns are natal-only, for now

    // ── Calculate both charts ────────────────────────────────────────────────
    ChartData natalRaw = m_chartDataManager.calculateChart(
        birthDate, birthTime, utcOffset, latitude, longitude, houseSystem);
    if (!m_chartDataManager.getLastError().isEmpty()) {
        handleError("Natal chart error: " + m_chartDataManager.getLastError());
        return;
    }

    ChartData progressedRaw = m_chartDataManager.calculateChart(
        progressedDate, birthTime, utcOffset, latitude, longitude, houseSystem);
    if (!m_chartDataManager.getLastError().isEmpty()) {
        handleError("Secondary progression calculation error: " + m_chartDataManager.getLastError());
        return;
    }

    // Apply additional-bodies filter
    ChartData natal     = filterAdditionalBodies(natalRaw);
    ChartData progressed = filterAdditionalBodies(progressedRaw);

    // Progressed-to-natal interaspects
    QVector<AspectData> interAspects = m_chartDataManager.calculateInteraspects(progressed, natal);

    // ── Render bi-wheel ──────────────────────────────────────────────────────
    m_chartRenderer->setDualChartData(natal, progressed, interAspects);
    m_chartRenderer->renderChart();

    // ── Update side panels ───────────────────────────────────────────────────
    m_planetListWidget->updateDualData(natal, progressed);
    m_aspectarianWidget->updateDualData(natal, progressed, interAspects);
    m_modalityElementWidget->updateDualData(natal, progressed);

    // Store chart JSONs (progressed = AI/detail tables; natal = bi-wheel save/load)
    m_currentChartData      = m_chartDataManager.chartDataToJson(progressedRaw);
    m_currentNatalChartData = m_chartDataManager.chartDataToJson(natalRaw);
    m_progressionYear       = progressionYear;
    updateChartDetailsTables(m_currentChartData);

    m_chartCalculated = true;
    AsteriaGlobals::lastGeneratedChartType = "Secondary Progression";
    m_getInterpretationButton->setEnabled(true);
    getPredictionButton->setEnabled(true);
    getTransitsButton->setEnabled(true);

    statusBar()->showMessage("Secondary progression bi-wheel calculated successfully", 3000);

    QString infoText = QString(
        "Secondary Progression Chart (Bi-Wheel)\n"
        "Natal: %1\n"
        "Progressed Date: %2  (Year %3)\n\n")
        .arg(birthDate.toString("yyyy/MM/dd"))
        .arg(progressedDate.toString("yyyy/MM/dd"))
        .arg(progressionYear);
    appendInterpretationEntry("chart_info", "Secondary Progression", infoText);
}

// Draconic chart: a bi-wheel comparing the natal chart to a rigid rotation
// of it that places the natal North Node at 0° Aries - the "soul layer"
// distinct from the outward natal personality.
void MainWindow::calculateDraconicChart()
{
    QString dateText = m_birthDateEdit->text();
    if (!validateDateFormat(dateText, this)) {
        return;
    }

    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");
    QString utcOffset  = m_utcOffsetCombo->currentText();
    QString latitude   = m_latitudeEdit->text();
    QString longitude  = m_longitudeEdit->text();
    QString houseSystem = m_houseSystemCombo->currentText();

    if (latitude.isEmpty() || longitude.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please enter latitude and longitude.");
        return;
    }

    // Reset chart state
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();
    aspectPatternsTable->setRowCount(0); // patterns are natal-only, for now

    // ── Calculate natal chart, then derive the draconic rotation ───────────
    ChartData natalRaw = m_chartDataManager.calculateChart(
        birthDate, birthTime, utcOffset, latitude, longitude, houseSystem);
    if (!m_chartDataManager.getLastError().isEmpty()) {
        handleError("Natal chart error: " + m_chartDataManager.getLastError());
        return;
    }

    ChartData draconicRaw = m_chartDataManager.calculateDraconicChart(natalRaw);

    // Apply additional-bodies filter
    ChartData natal    = filterAdditionalBodies(natalRaw);
    ChartData draconic = filterAdditionalBodies(draconicRaw);

    // Draconic-to-natal interaspects
    QVector<AspectData> interAspects = m_chartDataManager.calculateInteraspects(draconic, natal);

    // ── Render bi-wheel ──────────────────────────────────────────────────────
    m_chartRenderer->setDualChartData(natal, draconic, interAspects);
    m_chartRenderer->renderChart();

    // ── Update side panels ───────────────────────────────────────────────────
    m_planetListWidget->updateDualData(natal, draconic, "Natal", "Draconic");
    m_aspectarianWidget->updateDualData(natal, draconic, interAspects,
                                        "Draconic → Draconic", "Draconic → Natal");
    m_modalityElementWidget->updateDualData(natal, draconic, "Natal", "Draconic");

    // Store chart JSONs (draconic = AI/detail tables; natal = bi-wheel save/load)
    m_currentChartData      = m_chartDataManager.chartDataToJson(draconicRaw);
    m_currentNatalChartData = m_chartDataManager.chartDataToJson(natalRaw);
    updateChartDetailsTables(m_currentChartData);

    m_chartCalculated = true;
    AsteriaGlobals::lastGeneratedChartType = "Draconic";
    m_getInterpretationButton->setEnabled(true);
    getPredictionButton->setEnabled(true);
    getTransitsButton->setEnabled(true);

    statusBar()->showMessage("Draconic bi-wheel calculated successfully", 3000);

    QString natalNodeSign;
    for (const PlanetData &p : natalRaw.planets) {
        if (p.id == Planet::NorthNode) {
            natalNodeSign = p.sign;
            break;
        }
    }

    QString infoText = QString(
        "Draconic Chart (Bi-Wheel)\n"
        "Natal: %1\n"
        "Natal North Node: %2  (rotated to 0° Aries in the draconic wheel)\n\n")
        .arg(birthDate.toString("yyyy/MM/dd"))
        .arg(natalNodeSign);
    appendInterpretationEntry("chart_info", "Draconic Chart", infoText);
}
