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
void MainWindow::calculateChart()
{
    QString dateText = m_birthDateEdit->text();
    if (!validateDateFormat(dateText, this)) {
        return;
    }
    // Get input values
    QDate birthDate = getBirthDate();

    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");
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
    m_calculateButton->setEnabled(true); // Re-enable in case a relationship chart had disabled it

    m_chartRenderer->scene()->clear();

    // Calculate chart
    birthDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());

    m_currentChartData = m_chartDataManager.calculateChartAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, houseSystem);

    if (m_chartDataManager.getLastError().isEmpty()) {

        // Display chart (natal: also runs aspect pattern detection)
        displayChart(m_currentChartData, /*detectAspectPatterns=*/true);
        m_chartCalculated = true;
        // Set chart type for interpretation

        AsteriaGlobals::lastGeneratedChartType = "Natal Birth";

        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);
        statusBar()->showMessage("Chart calculated successfully", 3000);
    } else {
        handleError("Chart calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);
        // Clear any partial chart data after error
        m_chartRenderer->scene()->clear();
    }
}


void MainWindow::displayChart(const QJsonObject &chartData, bool detectAspectPatterns) {

    // Define which bodies are considered "additional"
    QStringList additionalBodies = {
        "Ceres", "Pallas", "Juno", "Vesta", "Lilith",
        "Vertex", "Part of Spirit", "East Point"
    };

    // Create a filtered copy of the chart data
    QJsonObject filteredChartData = chartData;

    // Filter out additional bodies if checkbox is not checked
    if (!m_additionalBodiesCB->isChecked()) {
        // Filter planets
        QJsonArray planets = filteredChartData["planets"].toArray();
        QJsonArray filteredPlanets;

        for (int i = 0; i < planets.size(); i++) {
            QJsonObject planet = planets[i].toObject();
            QString planetId = planet["id"].toString();

            // Keep the planet if it's not in the additional bodies list
            if (!additionalBodies.contains(planetId)) {
                filteredPlanets.append(planet);
            }
        }
        filteredChartData["planets"] = filteredPlanets;

        // Filter aspects
        QJsonArray aspects = filteredChartData["aspects"].toArray();
        QJsonArray filteredAspects;

        for (int i = 0; i < aspects.size(); i++) {
            QJsonObject aspect = aspects[i].toObject();
            QString planet1 = aspect["planet1"].toString();
            QString planet2 = aspect["planet2"].toString();

            // Keep the aspect if neither planet is an additional body
            if (!additionalBodies.contains(planet1) && !additionalBodies.contains(planet2)) {
                filteredAspects.append(aspect);
            }
        }
        filteredChartData["aspects"] = filteredAspects;
    }

    // Convert QJsonObject to ChartData
    ChartData data = convertJsonToChartData(filteredChartData);

    // Update chart renderer with new data
    m_chartRenderer->setChartData(data);
    m_chartRenderer->renderChart();

    // Update the sidebar widgets
    m_planetListWidget->updateData(data);
    m_aspectarianWidget->updateData(data);
    m_modalityElementWidget->updateData(data);

    // Update chart details tables
    updateChartDetailsTables(filteredChartData);

    // Aspect pattern detection - natal charts only, for now
    if (detectAspectPatterns) {
        m_currentAspectPatterns = AspectPatternDetector::detect(data.planets, data.aspects);
        updateAspectPatternsTable(m_currentAspectPatterns);
    } else {
        m_currentAspectPatterns = AspectPatternResults();
        aspectPatternsTable->setRowCount(0);
    }

    chartInfoOverlay->setVisible(m_showInfoOverlay);
    populateInfoOverlay();

    // Switch to chart tab
    m_centralTabWidget->setCurrentIndex(0);
}


void MainWindow::refreshDualWheelDisplay()
{
    ChartData natal     = filterAdditionalBodies(convertJsonToChartData(m_currentNatalChartData));
    ChartData secondary = filterAdditionalBodies(convertJsonToChartData(m_currentChartData));
    QVector<AspectData> interAspects = m_chartDataManager.calculateInteraspects(secondary, natal);

    m_chartRenderer->setDualChartData(natal, secondary, interAspects);
    m_chartRenderer->renderChart();

    const bool isSynastry = (AsteriaGlobals::lastGeneratedChartType == "Synastry");
    const bool isDraconic = (AsteriaGlobals::lastGeneratedChartType == "Draconic");

    if (isSynastry) {
        QString label1 = m_currentRelationshipInfo.value("person1").toString("Person A");
        QString label2 = m_currentRelationshipInfo.value("person2").toString("Person B");
        m_planetListWidget->updateDualData(natal, secondary, label1, label2);
        m_aspectarianWidget->updateDualData(natal, secondary, interAspects,
                                            label2 + " Aspects", label1 + " ↔ " + label2);
        m_modalityElementWidget->updateDualData(natal, secondary, label1, label2);
    } else if (isDraconic) {
        m_planetListWidget->updateDualData(natal, secondary, "Natal", "Draconic");
        m_aspectarianWidget->updateDualData(natal, secondary, interAspects,
                                            "Draconic → Draconic", "Draconic → Natal");
        m_modalityElementWidget->updateDualData(natal, secondary, "Natal", "Draconic");
    } else {
        m_planetListWidget->updateDualData(natal, secondary);
        m_aspectarianWidget->updateDualData(natal, secondary, interAspects);
        m_modalityElementWidget->updateDualData(natal, secondary);
    }

    updateChartDetailsTables(m_currentChartData);
}


void MainWindow::updateChartDetailsTables(const QJsonObject &chartData)
{
    // Get table widgets
    QTableWidget *planetsTable = m_chartDetailsWidget->findChild<QTabWidget*>()->findChild<QTableWidget*>("Planets");
    QTableWidget *anglesTable = m_chartDetailsWidget->findChild<QTabWidget*>()->findChild<QTableWidget*>("Angles");
    QTableWidget *housesTable = m_chartDetailsWidget->findChild<QTabWidget*>()->findChild<QTableWidget*>("Houses");
    QTableWidget *aspectsTable = m_chartDetailsWidget->findChild<QTabWidget*>()->findChild<QTableWidget*>("Aspects");

    if (!planetsTable || !housesTable || !aspectsTable || ! anglesTable) {
        return;
    }

    // Clear tables
    planetsTable->setRowCount(0);
    anglesTable->setRowCount(0);
    housesTable->setRowCount(0);
    aspectsTable->setRowCount(0);

    // Fill planets table
    if (chartData.contains("planets") && chartData["planets"].isArray()) {
        QJsonArray planets = chartData["planets"].toArray();
        planetsTable->setRowCount(planets.size());

        for (int i = 0; i < planets.size(); ++i) {
            QJsonObject planet = planets[i].toObject();

            QString planetName = planet["id"].toString();
            if (planet["isRetrograde"].toBool() && planetName != "North Node" && planetName != "South Node") {
                planetName += "   ℞"; // Using the official retrograde symbol (℞)
            }

            // Split the sign string to get just the sign name and degrees
            QString fullSign = planet["sign"].toString();
            QString signName = fullSign.split(' ').first();
            QStringList parts = fullSign.split(' ');
            QString degreesPart = parts.size() > 1 ? parts.mid(1).join(' ') : "";

            QTableWidgetItem *nameItem = new QTableWidgetItem(planetName);
            QTableWidgetItem *signItem = new QTableWidgetItem(signName);
            QTableWidgetItem *degreeItem = new QTableWidgetItem(degreesPart);
            QTableWidgetItem *houseItem = new QTableWidgetItem(planet["house"].toString());


            planetsTable->setItem(i, 0, nameItem);
            planetsTable->setItem(i, 1, signItem);
            planetsTable->setItem(i, 2, degreeItem);
            planetsTable->setItem(i, 3, houseItem);
        }
    }

    // Mapping from angle IDs to long names
    QMap<QString, QString> angleLongNames = {
        {"Asc", "Ascendant"},
        {"Desc", "Descendant"},
        {"MC", "Medium Coeli (Midheaven)"},
        {"IC", "Imum Coeli (Nadir)"}
    };
    // Fill angles table
    if (chartData.contains("angles") && chartData["angles"].isArray()) {
        QJsonArray angles = chartData["angles"].toArray();
        anglesTable->setRowCount(angles.size());
        for (int i = 0; i < angles.size(); ++i) {
            QJsonObject angle = angles[i].toObject();
            QString angleName = angle["id"].toString(); // e.g., ASC, DESC, IC, MC
            QString longName = angleLongNames.value(angleName, angleName); // fallback to id if not found

            QString signName = angle["sign"].toString();
            QString degreeStr = QString::number(angle["longitude"].toDouble(), 'f', 2) + "°";
            QTableWidgetItem *nameItem = new QTableWidgetItem(longName);


            QTableWidgetItem *signItem = new QTableWidgetItem(signName);
            QTableWidgetItem *degreeItem = new QTableWidgetItem(degreeStr);
            anglesTable->setItem(i, 0, nameItem);
            anglesTable->setItem(i, 1, signItem);
            anglesTable->setItem(i, 2, degreeItem);
        }
    }

    // Fill houses table

    if (chartData.contains("houses") && chartData["houses"].isArray()) {
        QJsonArray houses = chartData["houses"].toArray();
        housesTable->setRowCount(houses.size());

        for (int i = 0; i < houses.size(); ++i) {
            QJsonObject house = houses[i].toObject();

            QTableWidgetItem *nameItem = new QTableWidgetItem(house["id"].toString());
            QTableWidgetItem *signItem = new QTableWidgetItem(house["sign"].toString());
            QTableWidgetItem *degreeItem = new QTableWidgetItem(QString::number(house["longitude"].toDouble(), 'f', 2) + "°");

            housesTable->setItem(i, 0, nameItem);
            housesTable->setItem(i, 1, signItem);
            housesTable->setItem(i, 2, degreeItem);


        }
    }

    if (chartData.contains("aspects") && chartData["aspects"].isArray()) {
        QJsonArray aspects = chartData["aspects"].toArray();
        QJsonArray planets = chartData["planets"].toArray();
        aspectsTable->setRowCount(aspects.size());

        for (int i = 0; i < aspects.size(); ++i) {
            QJsonObject aspect = aspects[i].toObject();

            // Get planet names from the aspect
            QString planet1Name = aspect["planet1"].toString();
            QString planet2Name = aspect["planet2"].toString();

            // Check if planets are retrograde by looking them up in the planets array
            bool planet1Retrograde = false;
            bool planet2Retrograde = false;

            // Find retrograde status for both planets
            for (int j = 0; j < planets.size(); ++j) {
                QJsonObject planet = planets[j].toObject();
                if (planet["id"].toString() == planet1Name) {
                    planet1Retrograde = planet["isRetrograde"].toBool();
                }
                if (planet["id"].toString() == planet2Name) {
                    planet2Retrograde = planet["isRetrograde"].toBool();
                }
            }

            // Create display text with retrograde symbol if needed
            QString planet1Display = planet1Name;
            if (planet1Retrograde && planet1Name != "North Node" && planet1Name != "South Node") {
                planet1Display += " ℞";
            }

            QString planet2Display = planet2Name;
            if (planet2Retrograde && planet2Name != "North Node" && planet2Name != "South Node") {
                planet2Display += " ℞";
            }

            // Create table items
            QTableWidgetItem *planet1Item = new QTableWidgetItem(planet1Display);
            QTableWidgetItem *aspectTypeItem = new QTableWidgetItem(aspect["aspectType"].toString());
            QTableWidgetItem *planet2Item = new QTableWidgetItem(planet2Display);
            QTableWidgetItem *orbItem = new QTableWidgetItem(QString::number(aspect["orb"].toDouble(), 'f', 2) + "°");

            aspectsTable->setItem(i, 0, planet1Item);
            aspectsTable->setItem(i, 1, aspectTypeItem);
            aspectsTable->setItem(i, 2, planet2Item);
            aspectsTable->setItem(i, 3, orbItem);
        }
    }
}


namespace {
QString planetListString(const QVector<Planet> &planets)
{
    QStringList names;
    for (Planet p : planets)
        names << toString(p);
    return names.join(", ");
}
} // namespace

void MainWindow::updateAspectPatternsTable(const AspectPatternResults &results)
{
    aspectPatternsTable->setRowCount(0);

    auto addRow = [this](const QString &pattern, const QString &planets, const QString &details) {
        int row = aspectPatternsTable->rowCount();
        aspectPatternsTable->insertRow(row);
        aspectPatternsTable->setItem(row, 0, new QTableWidgetItem(pattern));
        aspectPatternsTable->setItem(row, 1, new QTableWidgetItem(planets));
        aspectPatternsTable->setItem(row, 2, new QTableWidgetItem(details));
    };

    for (const ClusterPattern &pattern : results.clusters) {
        QString label = (pattern.subtype == ClusterSubtype::Tight) ? "Cluster (Tight)" : "Cluster (Loose)";
        addRow(label, planetListString(pattern.planets), QString());
    }

    for (const EasyOppositionPattern &pattern : results.easyOppositions) {
        addRow("Easy Opposition",
               toString(pattern.pole1) + " – " + toString(pattern.pole2),
               "Eased by " + toString(pattern.easing));
    }

    for (const TSquarePattern &pattern : results.tSquares) {
        addRow("T-Square",
               toString(pattern.pole1) + " – " + toString(pattern.pole2),
               "Apex: " + toString(pattern.apex));
    }

    for (const GrandCrossPattern &pattern : results.grandCrosses) {
        addRow("Grand Cross", planetListString(pattern.planets), QString());
    }

    for (const GrandTrinePattern &pattern : results.grandTrines) {
        addRow("Grand Trine", planetListString(pattern.planets), QString());
    }

    for (const KitePattern &pattern : results.kites) {
        addRow("Kite", planetListString(pattern.grandTrinePlanets),
               "Tail: " + toString(pattern.tail) + " (opposite apex: " + toString(pattern.apex) + ")");
    }

    for (const SpikePattern &pattern : results.spikes) {
        QString label = (pattern.spikeType == SpikeType::Yod) ? "Yod" : "Thor's Hammer";
        addRow(label, planetListString(pattern.base), "Apex: " + toString(pattern.apex));
    }
}

QJsonValue roundJsonDoubles(const QJsonValue &val) {
    if (val.isDouble())
        return QJsonValue(qRound(val.toDouble() * 10.0) / 10.0);
    if (val.isObject()) {
        QJsonObject obj = val.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            obj[it.key()] = roundJsonDoubles(it.value());
        return obj;
    }
    if (val.isArray()) {
        QJsonArray arr = val.toArray();
        for (int i = 0; i < arr.size(); ++i)
            arr[i] = roundJsonDoubles(arr[i]);
        return arr;
    }
    return val;
}
