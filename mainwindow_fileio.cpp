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
void MainWindow::newChart() {
    // Clear input fields
    first_name->clear();  // Clear first name field
    last_name->clear();   // Clear last name field
    m_birthDateEdit->setText(QDate::currentDate().toString("dd/MM/yyyy"));
    m_birthTimeEdit->setText(QTime::currentTime().toString("HH:mm"));
    m_latitudeEdit->clear();
    m_longitudeEdit->clear();
    m_googleCoordsEdit->clear();
    m_utcOffsetCombo->setCurrentText("+00:00");
    m_houseSystemCombo->setCurrentIndex(0);
    m_nameLabel->clear();
    m_surnameLabel->clear();
    m_birthDateLabel->clear();
    m_birthTimeLabel->clear();
    m_locationLabel->clear();
    m_sunSignLabel->clear();
    m_ascendantLabel->clear();
    m_housesystemLabel->clear();
    m_predictiveFromEdit->setPlaceholderText("DD/MM/YYYY");
    // Set current date as default
    m_predictiveFromEdit->setText(QDate::currentDate().toString("dd/MM/yyyy"));
    // To date input
    m_predictiveToEdit->setPlaceholderText("DD/MM/YYYY");
    // Set default to current date + 1 days
    QDate defaultFutureDate = QDate::currentDate().addDays(1); // Just a default starting point
    m_predictiveToEdit->setText(defaultFutureDate.toString("dd/MM/yyyy"));

    // Clear chart and interpretation
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_interpretations = QJsonArray();
    m_currentRelationshipInfo = QJsonObject();  // Reset relationship info
    m_calculateButton->setEnabled(true); // Re-enable in case a relationship chart had disabled it

    // Clear chart renderer
    m_chartRenderer->scene()->clear();

    renderAllInterpretations();
    m_getInterpretationButton->setEnabled(false);

    // Clear sidebar widgets with empty data
    ChartData emptyData;
    if (m_planetListWidget) {
        m_planetListWidget->updateData(emptyData);
    }
    if (m_aspectarianWidget) {
        m_aspectarianWidget->updateData(emptyData);
    }
    if (m_modalityElementWidget) {
        m_modalityElementWidget->updateData(emptyData);
    }

    // Clear chart details tables
    QTabWidget *detailsTabs = m_chartDetailsWidget->findChild<QTabWidget*>();
    if (detailsTabs) {
        QTableWidget *planetsTable = detailsTabs->findChild<QTableWidget*>("Planets");
        QTableWidget *housesTable = detailsTabs->findChild<QTableWidget*>("Houses");
        QTableWidget *aspectsTable = detailsTabs->findChild<QTableWidget*>("Aspects");
        if (planetsTable) planetsTable->setRowCount(0);
        if (housesTable) housesTable->setRowCount(0);
        if (aspectsTable) aspectsTable->setRowCount(0);
    }

    statusBar()->showMessage("New chart", 3000);
    this->setWindowTitle("Asteria - Astrological Chart Analysis");
}


void MainWindow::saveChart() {
    if (!m_chartCalculated) {
        QMessageBox::warning(this, "No Chart", "Please calculate a chart first.");
        return;
    }

    QString filePath = getFilepath("astr");
    if (filePath.isEmpty())
        return;


    QString name = first_name->text().simplified();
    QString surname = last_name->text().simplified();

    // Create JSON document with chart data and interpretation
    QJsonObject saveData;
    saveData["chartData"]       = m_currentChartData;
    saveData["interpretations"] = m_interpretations;

    // Add birth information for reference
    QJsonObject birthInfo;
    birthInfo["firstName"] = name;
    birthInfo["lastName"] = surname;
    birthInfo["date"] = m_birthDateEdit->text();
    QTime time = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");
    birthInfo["time"] = time.toString(Qt::ISODate);
    birthInfo["latitude"] = m_latitudeEdit->text();
    birthInfo["longitude"] = m_longitudeEdit->text();
    birthInfo["utcOffset"] = m_utcOffsetCombo->currentText();
    birthInfo["houseSystem"] = m_houseSystemCombo->currentText();
    birthInfo["googleCoords"] = m_googleCoordsEdit->text();
    saveData["birthInfo"] = birthInfo;

    // Check if this is a relationship chart (Composite or Davison)
    // and add relationship info if it exists
    if (m_currentRelationshipInfo.isEmpty() == false) {
        saveData["relationshipInfo"] = m_currentRelationshipInfo;
    }

    saveData["chartType"] = AsteriaGlobals::lastGeneratedChartType;

    // Aspect patterns are natal-only, for now
    if (AsteriaGlobals::lastGeneratedChartType == "Natal Birth") {
        saveData["aspectPatterns"] = AspectPatternDetector::toJson(m_currentAspectPatterns);
    }

    // Check if this is a secondary progression bi-wheel
    if (!m_currentNatalChartData.isEmpty()) {
        saveData["natalChartData"]  = m_currentNatalChartData;
        saveData["progressionYear"] = m_progressionYear;
    }

    // Save to file
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(saveData);
        file.write(doc.toJson());
        file.close();
        statusBar()->showMessage("Chart saved to " + filePath, 3000);
    } else {
        QMessageBox::critical(this, "Save Error", "Could not save chart to " + filePath);
    }
}


void MainWindow::loadChart() {

    // Clear all previous chart data before loading a new one
    newChart();

    QString appName = QApplication::applicationName();
    QString appDir = AsteriaGlobals::appDir;
#ifdef FLATHUB_BUILD
    // In Flatpak, use the app-specific data directory
#else
    // For local builds, use a directory in home
#endif

    // Create directory if it doesn't exist
    QDir dir;
    if (!dir.exists(appDir))
        dir.mkpath(appDir);

    // Open file dialog starting in the app directory
    QString filePath = QFileDialog::getOpenFileName(this, "Load Chart",
                                                    appDir,
                                                    "Astrological Chart (*.astr)");
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject saveData = doc.object();

            // Load chart data
            if (saveData.contains("chartData") && saveData["chartData"].isObject()) {
                m_currentChartData = saveData["chartData"].toObject();

                if (saveData.contains("natalChartData") && saveData["natalChartData"].isObject()) {
                    // ── Bi-wheel chart (Secondary Progression or Synastry) ──
                    m_currentNatalChartData = saveData["natalChartData"].toObject();
                    m_progressionYear       = saveData.value("progressionYear").toInt();

                    AsteriaGlobals::lastGeneratedChartType =
                        saveData.value("chartType").toString("Secondary Progression");
                    const bool isSynastry = (AsteriaGlobals::lastGeneratedChartType == "Synastry");
                    const bool isDraconic = (AsteriaGlobals::lastGeneratedChartType == "Draconic");

                    ChartData natal      = filterAdditionalBodies(convertJsonToChartData(m_currentNatalChartData));
                    ChartData progressed = filterAdditionalBodies(convertJsonToChartData(m_currentChartData));
                    QVector<AspectData> interAspects =
                        m_chartDataManager.calculateInteraspects(progressed, natal);

                    m_chartRenderer->setDualChartData(natal, progressed, interAspects);
                    m_chartRenderer->renderChart();

                    if (isSynastry) {
                        QJsonObject relInfo = saveData["relationshipInfo"].toObject();
                        QString label1 = relInfo.value("person1").toString("Person A");
                        QString label2 = relInfo.value("person2").toString("Person B");
                        m_planetListWidget->updateDualData(natal, progressed, label1, label2);
                        m_aspectarianWidget->updateDualData(natal, progressed, interAspects,
                                                            label2 + " Aspects", label1 + " ↔ " + label2);
                        m_modalityElementWidget->updateDualData(natal, progressed, label1, label2);
                    } else if (isDraconic) {
                        m_planetListWidget->updateDualData(natal, progressed, "Natal", "Draconic");
                        m_aspectarianWidget->updateDualData(natal, progressed, interAspects,
                                                            "Draconic → Draconic", "Draconic → Natal");
                        m_modalityElementWidget->updateDualData(natal, progressed, "Natal", "Draconic");
                    } else {
                        m_planetListWidget->updateDualData(natal, progressed);
                        m_aspectarianWidget->updateDualData(natal, progressed, interAspects);
                        m_modalityElementWidget->updateDualData(natal, progressed);
                    }

                    updateChartDetailsTables(m_currentChartData);
                    aspectPatternsTable->setRowCount(0); // patterns are natal-only, for now
                } else {
                    // ── Regular single chart ────────────────────────────────
                    AsteriaGlobals::lastGeneratedChartType =
                        saveData.value("chartType").toString("Natal Birth").replace('-', ' ');
                    displayChart(m_currentChartData);

                    // Aspect patterns are natal-only, for now; older save files
                    // may not have this key.
                    if (AsteriaGlobals::lastGeneratedChartType == "Natal Birth" &&
                        saveData.contains("aspectPatterns")) {
                        m_currentAspectPatterns = AspectPatternDetector::fromJson(saveData["aspectPatterns"].toObject());
                        updateAspectPatternsTable(m_currentAspectPatterns);
                    }
                }

                m_chartCalculated = true;
                m_getInterpretationButton->setEnabled(true);
            }


            // Load birth information
            if (saveData.contains("birthInfo") && saveData["birthInfo"].isObject()) {
                QJsonObject birthInfo = saveData["birthInfo"].toObject();

                // Load first and last name
                if (birthInfo.contains("firstName")) {
                    first_name->setText(birthInfo["firstName"].toString());
                }
                if (birthInfo.contains("lastName")) {
                    last_name->setText(birthInfo["lastName"].toString());
                }
                if (birthInfo.contains("date")) {
                    m_birthDateEdit->setText(birthInfo["date"].toString());
                }
                if (birthInfo.contains("time")) {
                    QTime time = QTime::fromString(birthInfo["time"].toString(), Qt::ISODate);
                    m_birthTimeEdit->setText(time.toString("HH:mm"));
                }
                if (birthInfo.contains("latitude")) {
                    m_latitudeEdit->setText(birthInfo["latitude"].toString());
                }
                if (birthInfo.contains("longitude")) {
                    m_longitudeEdit->setText(birthInfo["longitude"].toString());
                }
                if (birthInfo.contains("utcOffset")) {
                    m_utcOffsetCombo->setCurrentText(birthInfo["utcOffset"].toString());
                }
                if (birthInfo.contains("houseSystem")) {
                    m_houseSystemCombo->setCurrentText(birthInfo["houseSystem"].toString());
                }
                if (birthInfo.contains("googleCoords")) {
                    m_googleCoordsEdit->setText(birthInfo["googleCoords"].toString());
                }
            }

            // Load interpretations (new format) or migrate legacy string
            if (saveData["interpretations"].isArray()) {
                m_interpretations = saveData["interpretations"].toArray();
                renderAllInterpretations();
            } else if (saveData["interpretation"].isString()) {
                QString legacyText = saveData["interpretation"].toString();
                if (!legacyText.isEmpty()) {
                    QJsonObject entry;
                    entry["type"]      = "legacy";
                    entry["chartType"] = saveData.value("chartType").toString();
                    entry["text"]      = legacyText;
                    m_interpretations.append(entry);
                    renderAllInterpretations();
                }
            }


            // Load relationship information if it exists
            if (saveData.contains("relationshipInfo") && saveData["relationshipInfo"].isObject()) {
                m_currentRelationshipInfo = saveData["relationshipInfo"].toObject();

                // Recalculating a relationship chart via the plain Calculate button would
                // discard its composite/Davison-specific data and replace it with an
                // unrelated calculation, so keep the button disabled while one is loaded.
                m_calculateButton->setEnabled(false);

                // Set window title based on relationship info
                if (m_currentRelationshipInfo.contains("displayName")) {
                    setWindowTitle("Asteria - Astrological Chart Analysis - " +
                                   m_currentRelationshipInfo["displayName"].toString());
                }
            } else {
                // Clear any existing relationship info.
                // Do NOT touch m_currentNatalChartData / m_progressionYear here —
                // they are managed by the chartData branch above. A secondary-
                // progression save has natalChartData but no relationshipInfo,
                // so clearing them here would wipe a just-loaded bi-wheel.
                m_currentRelationshipInfo = QJsonObject();
                m_calculateButton->setEnabled(true);

                // Set default window title for natal chart
                QString name = first_name->text();
                QString surname = last_name->text();
                if (!name.isEmpty() || !surname.isEmpty()) {
                    setWindowTitle("Asteria - Astrological Chart Analysis - " + name + " " + surname);
                } else {
                    setWindowTitle("Asteria - Astrological Chart Analysis - Birth Chart");
                }
            }

            populateInfoOverlay();
            statusBar()->showMessage("Chart loaded from " + filePath, 3000);
        } else {
            QMessageBox::critical(this, "Load Error", "Invalid chart file format");
        }
    } else {
        QMessageBox::critical(this, "Load Error", "Could not open chart file " + filePath);
    }
}


void MainWindow::exportInterpretation()
{
    if (m_interpretations.isEmpty()) {
        QMessageBox::warning(this, "No Interpretation", "Please get an interpretation first.");
        return;
    }

    QString filePath = getFilepath("txt");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Error", "Could not save interpretation to " + filePath);
        return;
    }

    QTextStream stream(&file);
    for (const QJsonValue &val : std::as_const(m_interpretations)) {
        QJsonObject entry   = val.toObject();
        QString type        = entry["type"].toString();
        QString text        = entry["text"].toString();
        QString genAt       = entry["generatedAt"].toString();
        QString genAtFormatted = genAt.isEmpty()
            ? QString()
            : QDateTime::fromString(genAt, Qt::ISODate).toString("dd MMM yyyy HH:mm");

        if (type == "chart_info") {
            stream << text << "\n";
        } else if (type == "ai_chart") {
            stream << "=== " << entry["chartType"].toString() << " Interpretation";
            if (!genAtFormatted.isEmpty())
                stream << " (" << genAtFormatted << ")";
            stream << " ===\n\n" << text << "\n\n";
        } else if (type == "ai_transit") {
            stream << "=== Transit Prediction";
            QString from = entry["periodFrom"].toString();
            QString to   = entry["periodTo"].toString();
            if (!from.isEmpty() && !to.isEmpty())
                stream << ": " << from << " to " << to;
            if (!genAtFormatted.isEmpty())
                stream << " (" << genAtFormatted << ")";
            stream << " ===\n\n" << text << "\n\n";
        } else {
            stream << text << "\n\n";
        }
    }

    file.close();
    statusBar()->showMessage("Interpretation exported to " + filePath, 3000);
}


void MainWindow::printChart() {
#if defined(FLATHUB_BUILD) || defined(GENTOO_BUILD)
    QMessageBox::information(
                this,
                tr("Functionality Unavailable"),
                tr("This functionality is not available in the Flathub or Gentoo versions of Asteria.")
                );
    return;
#else
    if (!m_chartCalculated) {
        QMessageBox::warning(this, "No Chart", "Please calculate a chart first.");
        return;
    }

    QList<QPrinterInfo> printers = QPrinterInfo::availablePrinters();
    if (printers.isEmpty()) {
        QMessageBox::warning(this, "No Printer Found",
                             "No active printer is available.\nPlease connect a printer or select 'Export as PDF' "
                             "from File menu and print the exported PDF manually at a later time");
        return;
    }

    // Connect signal to slot, single-shot so it disconnects after one call
    disconnect(this, &MainWindow::pdfExported, this, &MainWindow::printPdfFromPath); // prevent duplicates
    connect(this, &MainWindow::pdfExported, this, [this](const QString &path) {
        QTimer::singleShot(500, this, [this, path]() {
            printPdfFromPath(path);  // delayed to ensure PDF is ready
        });
    }, Qt::SingleShotConnection);

    exportAsPdf();
#endif
}

void MainWindow::showAboutDialog()
{
    QString version = QCoreApplication::applicationVersion();
    QMessageBox::about(
                this,
                "About Asteria L",
                QString("<h3>Asteria L - Astrological Chart Analysis</h3>"
                        "<p>Version %1</p>"
                        "<p>A fork of <a href=\"https://github.com/alamahant/Asteria\">Asteria</a> "
                        "by Alamahant, extended and maintained by lejean2000.</p>"
                        "<p>A tool for calculating and interpreting astrological charts "
                        "with AI-powered analysis.</p>"
                        "<p>Available for Windows and Linux (Flatpak).</p>"
                        "<p><a href=\"https://github.com/lejean2000/Asteria\">"
                        "https://github.com/lejean2000/Asteria</a></p>"
                        "<p>© 2025 Alamahant &nbsp;|&nbsp; © 2026 lejean2000</p>")
                .arg(version)
                );
}


void MainWindow::handleError(const QString &errorMessage)
{
    QMessageBox::critical(this, "Error", errorMessage);
    statusBar()->showMessage("Error: " + errorMessage, 5000);
    getPredictionButton->setEnabled(true);
    m_getInterpretationButton->setEnabled(true);
    m_aiWaitProgressBar->hide();
}

QString MainWindow::getChartFilePath(bool forSaving)
{
    // QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath;

    if (forSaving) {
        filePath = QFileDialog::getSaveFileName(this, "Save Chart",
                                                AsteriaGlobals::appDir, "Chart Files (*.chart)");
    } else {
        filePath = QFileDialog::getOpenFileName(this, "Open Chart",
                                                AsteriaGlobals::appDir, "Chart Files (*.chart)");
    }

    return filePath;
}

void MainWindow::saveSettings()
{
    QSettings settings;

    // Save window state
    settings.setValue("mainWindow/geometry", saveGeometry());
    settings.setValue("mainWindow/windowState", saveState());

    // Save last used house system
    settings.setValue("chart/houseSystem", m_houseSystemCombo->currentText());

    // Save UTC offset
    settings.setValue("chart/utcOffset", m_utcOffsetCombo->currentText());
    // add aditional bodies or not
    // Save aspect display settings

    // Save info overlay visibility setting
    settings.setValue("view/showInfoOverlay", m_showInfoOverlay);

    AspectSettings::instance().saveToSettings(settings);

}

void MainWindow::loadSettings()
{
    QSettings settings;

    // Restore last used house system
    if (settings.contains("chart/houseSystem")) {
        QString houseSystem = settings.value("chart/houseSystem").toString();
        int index = m_houseSystemCombo->findText(houseSystem);
        if (index >= 0) {
            m_houseSystemCombo->setCurrentIndex(index);
        }
    }

    // Restore UTC offset
    if (settings.contains("chart/utcOffset")) {
        QString utcOffset = settings.value("chart/utcOffset").toString();
        int index = m_utcOffsetCombo->findText(utcOffset);
        if (index >= 0) {
            m_utcOffsetCombo->setCurrentIndex(index);
        }
    }
    // Restore additional bodies setting

    // Load info overlay visibility setting
    if (settings.contains("view/showInfoOverlay")) {
        m_showInfoOverlay = settings.value("view/showInfoOverlay").toBool();
    } else {
        m_showInfoOverlay = false; // Default to false if setting doesn't exist
    }

    // Update the action checkbox state
    if (showOverlayAction) {
        showOverlayAction->setChecked(m_showInfoOverlay);
    }

    AspectSettings::instance().loadFromSettings(settings);

}

QDate MainWindow::getBirthDate() const {
    QString dateText = m_birthDateEdit->text();
    QRegularExpression dateRegex("(\\d{2})/(\\d{2})/(\\d{4})");
    QRegularExpressionMatch match = dateRegex.match(dateText);

    if (match.hasMatch()) {
        int day = match.captured(1).toInt();
        int month = match.captured(2).toInt();
        int year = match.captured(3).toInt();

        QDate date(year, month, day);
        if (date.isValid()) {
            return date;
        }
    }
    // Return current date as fallback
    return QDate::currentDate();
}


void MainWindow::resizeEvent(QResizeEvent *event)
{


    // If we have a chart, make sure it fits in the view
    //}
}

ChartData MainWindow::convertJsonToChartData(const QJsonObject &jsonData)
{
    ChartData chartData;

    // Convert planets
    if (jsonData.contains("planets") && jsonData["planets"].isArray()) {
        QJsonArray planetsArray = jsonData["planets"].toArray();
        for (const QJsonValue &value : planetsArray) {
            QJsonObject planetObj = value.toObject();
            PlanetData planet;
            planet.id = planetFromString(planetObj["id"].toString());
            planet.longitude = planetObj["longitude"].toDouble();
            planet.latitude = planetObj.contains("latitude") ? planetObj["latitude"].toDouble() : 0.0;
            // Remove the speed line since PlanetData doesn't have this member
            planet.house = planetObj.contains("house") ? planetObj["house"].toString() : "";
            planet.sign = planetObj.contains("sign") ? planetObj["sign"].toString() : "";
            if (planetObj.contains("isRetrograde")) {
                planet.isRetrograde = planetObj["isRetrograde"].toBool();
            } else {
                planet.isRetrograde = false;
            }

            chartData.planets.append(planet);
        }
    }

    // Convert houses
    if (jsonData.contains("houses") && jsonData["houses"].isArray()) {
        QJsonArray housesArray = jsonData["houses"].toArray();
        for (const QJsonValue &value : housesArray) {
            QJsonObject houseObj = value.toObject();
            HouseData house;
            house.id = houseObj["id"].toString();
            house.longitude = houseObj["longitude"].toDouble();
            house.sign = houseObj.contains("sign") ? houseObj["sign"].toString() : "";
            chartData.houses.append(house);
        }
    }

    // Convert angles (Asc, MC, etc.)
    if (jsonData.contains("angles") && jsonData["angles"].isArray()) {
        QJsonArray anglesArray = jsonData["angles"].toArray();
        for (const QJsonValue &value : anglesArray) {
            QJsonObject angleObj = value.toObject();
            AngleData angle;
            angle.id = angleObj["id"].toString();
            angle.longitude = angleObj["longitude"].toDouble();
            angle.sign = angleObj.contains("sign") ? angleObj["sign"].toString() : "";

            chartData.angles.append(angle);
        }
    }

    // Convert aspects
    if (jsonData.contains("aspects") && jsonData["aspects"].isArray()) {
        QJsonArray aspectsArray = jsonData["aspects"].toArray();
        for (const QJsonValue &value : aspectsArray) {
            QJsonObject aspectObj = value.toObject();
            AspectData aspect;
            aspect.planet1   = planetFromString(aspectObj["planet1"].toString());
            aspect.planet2   = planetFromString(aspectObj["planet2"].toString());
            aspect.aspectType = aspectTypeFromString(aspectObj["aspectType"].toString());
            aspect.orb = aspectObj.contains("orb") ? aspectObj["orb"].toDouble() : 0.0;
            chartData.aspects.append(aspect);
        }
    }
    return chartData;
}

//////////////////////Predictions
