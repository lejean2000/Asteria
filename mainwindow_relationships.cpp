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
void MainWindow::createCompositeChart() {
    // Show info message
    QMessageBox::information(this, "Select Charts",
                             "Please select two natal charts to create a composite chart.");
    // Get app directory for file dialog
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

    // Open file dialog for selecting two charts
    QStringList filePaths = QFileDialog::getOpenFileNames(
                this, "Select Two Charts", appDir, "Astrological Chart (*.astr)");

    // Validate selection
    if (filePaths.size() != 2) {
        QMessageBox::warning(this, "Invalid Selection",
                             "You must select exactly two charts.");
        return;
    }

    // Load the charts
    QJsonObject saveData1;
    QJsonObject saveData2;
    // Load first chart
    QFile file1(filePaths[0]);
    if (file1.open(QIODevice::ReadOnly)) {
        QByteArray data = file1.readAll();
        file1.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            saveData1 = doc.object();
        } else {
            QMessageBox::critical(this, "Load Error", "Invalid chart file format: " + filePaths[0]);
            return;
        }
    } else {
        QMessageBox::critical(this, "Load Error", "Could not open chart file " + filePaths[0]);
        return;
    }

    // Load second chart
    QFile file2(filePaths[1]);
    if (file2.open(QIODevice::ReadOnly)) {
        QByteArray data = file2.readAll();
        file2.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            saveData2 = doc.object();
        } else {
            QMessageBox::critical(this, "Load Error", "Invalid chart file format: " + filePaths[1]);
            return;
        }
    } else {
        QMessageBox::critical(this, "Load Error", "Could not open chart file " + filePaths[1]);
        return;
    }

    // Extract chart data
    QJsonObject chartData1 = saveData1["chartData"].toObject();
    QJsonObject chartData2 = saveData2["chartData"].toObject();

    // Extract birth info
    QJsonObject birthInfo1 = saveData1["birthInfo"].toObject();
    QJsonObject birthInfo2 = saveData2["birthInfo"].toObject();

    // Get names for display
    QString name1 = birthInfo1["firstName"].toString();
    QString name2 = birthInfo2["firstName"].toString();
    QString surname1 = birthInfo1["lastName"].toString();
    QString surname2 = birthInfo2["lastName"].toString();

    // Create composite chart data
    QJsonObject compositeChartData;

    // Calculate composite houses first (equal-house system from the composite Ascendant)
    // so that composite planets can be placed into them below.
    QJsonArray houses1 = chartData1["houses"].toArray();
    QJsonArray houses2 = chartData2["houses"].toArray();

    // Create maps for quick lookup
    QMap<int, QJsonObject> houseMap1;
    QMap<int, QJsonObject> houseMap2;
    for (const QJsonValue &houseValue : houses1) {
        QJsonObject house = houseValue.toObject();
        QString id = house["id"].toString();
        // Extract house number from id (e.g., "House1" -> 1)
        int houseNumber = id.mid(5).toInt();
        if (houseNumber > 0 && houseNumber <= 12) {
            houseMap1[houseNumber] = house;
        }
    }
    for (const QJsonValue &houseValue : houses2) {
        QJsonObject house = houseValue.toObject();
        QString id = house["id"].toString();
        // Extract house number from id (e.g., "House1" -> 1)
        int houseNumber = id.mid(5).toInt();
        if (houseNumber > 0 && houseNumber <= 12) {
            houseMap2[houseNumber] = house;
        }
    }

    // Calculate the composite Ascendant (House 1)
    QJsonObject house1_1 = houseMap1[1];
    QJsonObject house1_2 = houseMap2[1];
    double longitude1_1 = house1_1["longitude"].toDouble();
    double longitude1_2 = house1_2["longitude"].toDouble();
    double diff1 = fmod(longitude1_2 - longitude1_1 + 540.0, 360.0) - 180.0;
    double compositeAsc = fmod(longitude1_1 + diff1/2.0 + 360.0, 360.0);

    // Now calculate equal houses from the composite Ascendant
    QJsonArray compositeHouses;
    for (int i = 1; i <= 12; i++) {
        // Each house is 30 degrees from the previous one
        double houseLongitude = fmod(compositeAsc + (i-1) * 30.0, 360.0);

        // Determine the sign
        int signIndex = static_cast<int>(houseLongitude) / 30;
        QStringList signs = {"Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
                             "Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces"};
        QString sign = signs[signIndex];

        // Create house object
        QJsonObject compositeHouse;
        compositeHouse["id"] = QString("House%1").arg(i);
        compositeHouse["longitude"] = houseLongitude;
        compositeHouse["sign"] = sign;
        compositeHouses.append(compositeHouse);
    }

    // Given a longitude, find which composite house it falls into (mirrors
    // ChartCalculator::findHouse, operating on the composite's own JSON houses).
    auto findCompositeHouse = [](double longitude, const QJsonArray &houses) -> QString {
        longitude = fmod(longitude, 360.0);
        if (longitude < 0) longitude += 360.0;
        for (int i = 0; i < houses.size(); i++) {
            QJsonObject house = houses[i].toObject();
            QJsonObject nextHouse = houses[(i + 1) % houses.size()].toObject();
            double start = house["longitude"].toDouble();
            double end = nextHouse["longitude"].toDouble();
            if (end < start) {
                if (longitude >= start || longitude < end)
                    return house["id"].toString();
            } else {
                if (longitude >= start && longitude < end)
                    return house["id"].toString();
            }
        }
        return "House1";
    };

    // Calculate midpoint planets
    QJsonArray planets1 = chartData1["planets"].toArray();
    QJsonArray planets2 = chartData2["planets"].toArray();
    QJsonArray compositePlanets;

    // Create a map for quick lookup of planets in chart2
    QMap<QString, QJsonObject> planetMap2;
    for (const QJsonValue &planetValue : planets2) {
        QJsonObject planet = planetValue.toObject();
        planetMap2[planet["id"].toString()] = planet;
    }

    // Calculate midpoints for planets
    for (const QJsonValue &planetValue1 : planets1) {
        QJsonObject planet1 = planetValue1.toObject();
        QString planetId = planet1["id"].toString();
        // Find matching planet in chart2
        if (planetMap2.contains(planetId)) {
            QJsonObject planet2 = planetMap2[planetId];
            // Create composite planet
            QJsonObject compositePlanet;
            compositePlanet["id"] = planetId;
            // Calculate midpoint longitude
            double long1 = planet1["longitude"].toDouble();
            double long2 = planet2["longitude"].toDouble();
            // Handle the case where angles cross 0°/360° boundary
            double diff = fmod(long2 - long1 + 540.0, 360.0) - 180.0;
            double midpoint = fmod(long1 + diff/2.0 + 360.0, 360.0);
            compositePlanet["longitude"] = midpoint;
            // Determine the sign for the midpoint
            int signIndex = static_cast<int>(midpoint) / 30;
            QStringList signs = {"Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
                                 "Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces"};
            compositePlanet["sign"] = signs[signIndex];
            // Retrograde: mark the composite point retrograde only if the planet was
            // retrograde in both source charts. (The correct JSON key is "isRetrograde" -
            // the old code checked a nonexistent "retrograde" key, so this never fired.)
            bool retro1 = planet1.value("isRetrograde").toBool();
            bool retro2 = planet2.value("isRetrograde").toBool();
            compositePlanet["isRetrograde"] = retro1 && retro2;
            // House: place the composite (midpoint) planet into the composite's own
            // houses, instead of copying person 1's unrelated natal house placement.
            compositePlanet["house"] = findCompositeHouse(midpoint, compositeHouses);
            compositePlanets.append(compositePlanet);
        }
    }

    // Calculate midpoints for angles
    QJsonArray angles1 = chartData1["angles"].toArray();
    QJsonArray angles2 = chartData2["angles"].toArray();
    QJsonArray compositeAngles;

    // Create maps for quick lookup
    QMap<QString, QJsonObject> angleMap1;
    QMap<QString, QJsonObject> angleMap2;
    for (const QJsonValue &angleValue : angles1) {
        QJsonObject angle = angleValue.toObject();
        if (angle.contains("id")) {
            angleMap1[angle["id"].toString()] = angle;
        }
    }
    for (const QJsonValue &angleValue : angles2) {
        QJsonObject angle = angleValue.toObject();
        if (angle.contains("id")) {
            angleMap2[angle["id"].toString()] = angle;
        }
    }

    // Calculate midpoints for common angles
    QStringList angleIds = {"Asc", "MC", "Desc", "IC"};
    for (const QString &id : angleIds) {
        if (angleMap1.contains(id) && angleMap2.contains(id)) {
            QJsonObject angle1 = angleMap1[id];
            QJsonObject angle2 = angleMap2[id];
            double longitude1 = angle1["longitude"].toDouble();
            double longitude2 = angle2["longitude"].toDouble();
            // Handle the case where angles cross 0°/360° boundary
            double diff = fmod(longitude2 - longitude1 + 540.0, 360.0) - 180.0;
            double midpoint = fmod(longitude1 + diff/2.0 + 360.0, 360.0);
            // Determine the sign for the midpoint
            int signIndex = static_cast<int>(midpoint) / 30;
            QStringList signs = {"Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
                                 "Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces"};
            QString sign = signs[signIndex];
            // Create a new angle object for the composite chart
            QJsonObject compositeAngle;
            compositeAngle["id"] = id;
            compositeAngle["longitude"] = midpoint;
            compositeAngle["sign"] = sign;
            compositeAngles.append(compositeAngle);
        }
    }

    // Assemble the composite chart data
    compositeChartData["planets"] = compositePlanets;
    compositeChartData["angles"] = compositeAngles;
    compositeChartData["houses"] = compositeHouses;

    // Copy aspects from first chart (this is a simplification)
    // In a real implementation, you would recalculate aspects between composite planets
    QJsonArray compositeAspects;
    // Define aspect types and their orbs
    struct AspectType {
        QString name;
        double angle;
        double orb;
    };
    double orbMax = getOrbMax();
    QVector<AspectType> aspectTypes = {
        {"CON", 0.0, orbMax},      // Conjunction
        {"OPP", 180.0, orbMax},    // Opposition
        {"TRI", 120.0, orbMax},    // Trine
        {"SQR", 90.0, orbMax},     // Square
        {"SEX", 60.0, orbMax},     // Sextile
        {"QUI", 150.0, orbMax * 0.75},  // Quintile
        {"SSQ", 45.0, orbMax * 0.75},  // Semi-square
        {"SQQ", 135.0, orbMax * 0.75}, // Sesquiquadrate


        {"SSX", 30.0, orbMax * 0.75}   // Semi-sextile
    };

    // Check for aspects between each pair of planets
    for (int i = 0; i < compositePlanets.size(); i++) {
        QJsonObject planet1 = compositePlanets[i].toObject();
        for (int j = i + 1; j < compositePlanets.size(); j++) {
            QJsonObject planet2 = compositePlanets[j].toObject();
            double long1 = planet1["longitude"].toDouble();
            double long2 = planet2["longitude"].toDouble();
            // Calculate the angular distance between planets
            double distance = fabs(long1 - long2);
            if (distance > 180.0) distance = 360.0 - distance;
            // Check if this distance matches any aspect type
            for (const AspectType &aspectType : aspectTypes) {
                double orb = fabs(distance - aspectType.angle);
                if (orb <= aspectType.orb) {
                    // Create aspect object
                    QJsonObject aspect;
                    aspect["planet1"] = planet1["id"].toString();
                    aspect["planet2"] = planet2["id"].toString();
                    aspect["aspectType"] = aspectType.name;  // Use "aspectType" not "type"
                    aspect["orb"] = orb;
                    compositeAspects.append(aspect);
                    break; // Only record the closest matching aspect
                }
            }
        }
    }

    // Update the composite chart data with the calculated aspects
    compositeChartData["aspects"] = compositeAspects;

    // Create the full save data structure
    QJsonObject compositeSaveData;
    compositeSaveData["chartData"] = compositeChartData;

    // Add relationship info
    QJsonObject relationshipInfo;
    QString compositeFirstName = name1 + " " + surname1;
    QString compositeLastName = name2 + " " + surname2;
    relationshipInfo["type"] = "Composite";
    relationshipInfo["person1"] = name1 + " " + birthInfo1["lastName"].toString();
    relationshipInfo["person2"] = name2 + " " + birthInfo2["lastName"].toString();
    relationshipInfo["displayName"] = "Composite Chart: " + compositeFirstName + " & " + compositeLastName;
    m_currentRelationshipInfo = relationshipInfo;
    // Recalculating would discard the composite (midpoint-averaged) data and replace it
    // with an unrelated Swiss Ephemeris calculation for the midpoint date/time - disable
    // the button so that can't happen by accident.
    m_calculateButton->setEnabled(false);

    // Calculate midpoint date and time using QDateTime
    QDateTime dateTime1, dateTime2;
    QString dateStr1 = birthInfo1["date"].toString();
    QString timeStr1 = birthInfo1["time"].toString();
    QString dateStr2 = birthInfo2["date"].toString();
    QString timeStr2 = birthInfo2["time"].toString();

    // Handle different date formats
    if (dateStr1.contains("/")) {
        // Format is dd/MM/yyyy
        QDate date1 = QDate::fromString(dateStr1, "dd/MM/yyyy");
        QTime time1 = QTime::fromString(timeStr1, "HH:mm");
        dateTime1 = QDateTime(date1, time1);
    } else {
        // Format is yyyy-MM-dd
        dateTime1 = QDateTime::fromString(dateStr1 + " " + timeStr1, "yyyy-MM-dd HH:mm");
    }
    if (dateStr2.contains("/")) {
        // Format is dd/MM/yyyy
        QDate date2 = QDate::fromString(dateStr2, "dd/MM/yyyy");
        QTime time2 = QTime::fromString(timeStr2, "HH:mm");
        dateTime2 = QDateTime(date2, time2);
    } else {
        // Format is yyyy-MM-dd
        dateTime2 = QDateTime::fromString(dateStr2 + " " + timeStr2, "yyyy-MM-dd HH:mm");
    }

    // Calculate midpoint timestamp (average of Unix timestamps)
    qint64 timestamp1 = dateTime1.toSecsSinceEpoch();
    qint64 timestamp2 = dateTime2.toSecsSinceEpoch();
    qint64 midpointTimestamp = (timestamp1 + timestamp2) / 2;

    // Convert back to QDateTime
    QDateTime midpointDateTime = QDateTime::fromSecsSinceEpoch(midpointTimestamp);

    // Format midpoint date and time
    QString compositeDateStr = midpointDateTime.toString("dd/MM/yyyy");
    QString compositeTimeStr = midpointDateTime.toString("HH:mm");

    // Add to relationshipInfo
    relationshipInfo["date"] = compositeDateStr;
    relationshipInfo["time"] = compositeTimeStr;
    compositeSaveData["relationshipInfo"] = relationshipInfo;

    // Calculate midpoint location
    double lat1 = birthInfo1["latitude"].toString().toDouble();
    double lon1 = birthInfo1["longitude"].toString().toDouble();
    double lat2 = birthInfo2["latitude"].toString().toDouble();
    double lon2 = birthInfo2["longitude"].toString().toDouble();
    double midLat = (lat1 + lat2) / 2.0;
    double midLon = (lon1 + lon2) / 2.0;
    QString compositeLatStr = QString::number(midLat, 'f', 6);
    QString compositeLonStr = QString::number(midLon, 'f', 6);

    // Format Google coordinates string in the proper format
    QString latDirection = midLat >= 0 ? "N" : "S";
    QString lonDirection = midLon >= 0 ? "E" : "W";
    QString compositeGoogleCoords = QString("%1° %2, %3° %4")
            .arg(fabs(midLat), 0, 'f', 4)
            .arg(latDirection)
            .arg(fabs(midLon), 0, 'f', 4)
            .arg(lonDirection);

    // Create the composite birth info object
    QJsonObject compositeBirthInfo;
    compositeBirthInfo["firstName"] = compositeFirstName;
    compositeBirthInfo["lastName"] = compositeLastName;
    compositeBirthInfo["date"] = compositeDateStr;  // Midpoint date
    compositeBirthInfo["time"] = compositeTimeStr;  // Midpoint time
    compositeBirthInfo["latitude"] = compositeLatStr;  // Midpoint latitude
    compositeBirthInfo["longitude"] = compositeLonStr;  // Midpoint longitude
    compositeBirthInfo["googleCoords"] = compositeGoogleCoords;  // Formatted Google coordinates

    // Average the two source charts' own UTC offsets (mirrors createDavisonChart()) -
    // this used to be hardcoded to "+0:00" regardless of the actual source offsets.
    QString utcOffsetStr1 = birthInfo1["utcOffset"].toString();
    QString utcOffsetStr2 = birthInfo2["utcOffset"].toString();
    bool utcNeg1 = utcOffsetStr1.startsWith("-");
    bool utcNeg2 = utcOffsetStr2.startsWith("-");
    QStringList utcParts1 = utcOffsetStr1.mid(1).split(":");
    QStringList utcParts2 = utcOffsetStr2.mid(1).split(":");
    double utcHours1 = utcParts1.value(0).toDouble() + utcParts1.value(1).toDouble() / 60.0;
    double utcHours2 = utcParts2.value(0).toDouble() + utcParts2.value(1).toDouble() / 60.0;
    if (utcNeg1) utcHours1 = -utcHours1;
    if (utcNeg2) utcHours2 = -utcHours2;
    double compositeUtcHours = (utcHours1 + utcHours2) / 2.0;
    bool utcNeg = compositeUtcHours < 0;
    double utcAbsHours = std::abs(compositeUtcHours);
    int utcH = static_cast<int>(utcAbsHours);
    int utcM = static_cast<int>((utcAbsHours - utcH) * 60);
    QString compositeUtcOffsetStr = QString("%1%2:%3")
            .arg(utcNeg ? "-" : "+")
            .arg(utcH, 1, 10, QChar('0'))
            .arg(utcM, 2, 10, QChar('0'));

    // Set in birth info
    compositeBirthInfo["utcOffset"] = compositeUtcOffsetStr;

    // Set in UI combobox, trying progressively looser matches before adding a new entry
    int index = m_utcOffsetCombo->findText(compositeUtcOffsetStr);
    if (index < 0) {
        index = m_utcOffsetCombo->findText("UTC" + compositeUtcOffsetStr);
    }
    if (index < 0) {
        QString numericPart = compositeUtcOffsetStr.mid(1);
        for (int i = 0; i < m_utcOffsetCombo->count(); i++) {
            if (m_utcOffsetCombo->itemText(i).contains(numericPart)) {
                index = i;
                break;
            }
        }
    }
    if (index >= 0) {
        m_utcOffsetCombo->setCurrentIndex(index);
    } else {
        // If all else fails, add the calculated offset to the combobox
        m_utcOffsetCombo->addItem(compositeUtcOffsetStr);
        m_utcOffsetCombo->setCurrentIndex(m_utcOffsetCombo->count() - 1);
    }

    compositeBirthInfo["houseSystem"] = birthInfo1["houseSystem"].toString();  // Keep first person's house system
    compositeSaveData["birthInfo"] = compositeBirthInfo;

    // Update UI fields for saving
    first_name->setText(compositeFirstName);
    last_name->setText(compositeLastName);
    m_birthDateEdit->setText(compositeDateStr);
    m_birthTimeEdit->setText(compositeTimeStr);
    m_googleCoordsEdit->setText(compositeGoogleCoords);  // Use the formatted Google coordinates

    // Display the chart
    m_currentChartData = compositeChartData;
    m_interpretations = QJsonArray(); // Discard interpretations from any previously displayed chart
    renderAllInterpretations();
    displayChart(compositeChartData);
    m_chartCalculated = true;
    AsteriaGlobals::lastGeneratedChartType = "Composite Relationship";
    populateInfoOverlay();

    // Save the composite chart
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmm");
    QString outputFileName = QString("Composite_%1_%2_%3.astr")
            .arg(name1 + surname1)
            .arg(name2 + surname2)
            .arg(timestamp);
    QDir relationshipDir(appDir + "/RelationshipCharts");
    if (!relationshipDir.exists()) {
        relationshipDir.mkpath(".");
    }
    QString outputFilePath = appDir + "/RelationshipCharts/" + outputFileName;
    QFile outputFile(outputFilePath);
    if (outputFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(compositeSaveData);
        outputFile.write(doc.toJson(QJsonDocument::Indented));
        outputFile.close();
        QMessageBox::information(this, "Chart Saved", "Composite chart saved to:\n" + outputFilePath);
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not save Composite chart to:\n" + outputFilePath);
    }

    // Update window title
    setWindowTitle("Asteria - Astrological Chart Analysis - " + relationshipInfo["displayName"].toString());
}


void MainWindow::createDavisonChart() {
    QMessageBox::information(this, "Select Charts",
                             "Please select two natal charts to create a Davison chart.");
    QString appName = QApplication::applicationName();
    QString appDir = AsteriaGlobals::appDir;
#ifdef FLATHUB_BUILD
#else
#endif
    QDir dir;
    if (!dir.exists(appDir))
        dir.mkpath(appDir);

    QStringList filePaths = QFileDialog::getOpenFileNames(
                this, "Select Two Charts", appDir, "Astrological Chart (*.astr)");

    if (filePaths.size() != 2) {
        QMessageBox::warning(this, "Invalid Selection",
                             "You must select exactly two charts.");
        return;
    }

    QJsonObject saveData1, saveData2;
    QFile file1(filePaths[0]);
    if (file1.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file1.readAll());
        file1.close();
        if (doc.isObject()) {
            saveData1 = doc.object();
        } else {
            QMessageBox::critical(this, "Load Error", "Invalid chart file format: " + filePaths[0]);
            return;
        }
    } else {
        QMessageBox::critical(this, "Load Error", "Could not open chart file " + filePaths[0]);
        return;
    }

    QFile file2(filePaths[1]);
    if (file2.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file2.readAll());
        file2.close();
        if (doc.isObject()) {
            saveData2 = doc.object();
        } else {
            QMessageBox::critical(this, "Load Error", "Invalid chart file format: " + filePaths[1]);
            return;
        }
    } else {
        QMessageBox::critical(this, "Load Error", "Could not open chart file " + filePaths[1]);
        return;
    }

    QJsonObject birthInfo1 = saveData1["birthInfo"].toObject();
    QJsonObject birthInfo2 = saveData2["birthInfo"].toObject();
    QString name1 = birthInfo1["firstName"].toString();
    QString surname1 = birthInfo1["lastName"].toString();
    QString name2 = birthInfo2["firstName"].toString();
    QString surname2 = birthInfo2["lastName"].toString();

    // Fixed date/time parsing
    QString dateStr1 = birthInfo1["date"].toString();
    QString timeStr1 = birthInfo1["time"].toString();
    QString dateStr2 = birthInfo2["date"].toString();
    QString timeStr2 = birthInfo2["time"].toString();

    // Try to parse with seconds first
    QDateTime dateTime1 = QDateTime::fromString(dateStr1 + " " + timeStr1, "dd/MM/yyyy HH:mm:ss");
    QDateTime dateTime2 = QDateTime::fromString(dateStr2 + " " + timeStr2, "dd/MM/yyyy HH:mm:ss");

    // If parsing failed, try without seconds
    if (!dateTime1.isValid()) {
        dateTime1 = QDateTime::fromString(dateStr1 + " " + timeStr1, "dd/MM/yyyy HH:mm");
    }
    if (!dateTime2.isValid()) {
        dateTime2 = QDateTime::fromString(dateStr2 + " " + timeStr2, "dd/MM/yyyy HH:mm");
    }

    // Calculate midpoint date
    qint64 midpointTimestamp = (dateTime1.toSecsSinceEpoch() + dateTime2.toSecsSinceEpoch()) / 2;
    QDateTime midpointDateTime = QDateTime::fromSecsSinceEpoch(midpointTimestamp);

    // Calculate the average time separately
    int hour1 = dateTime1.time().hour();
    int minute1 = dateTime1.time().minute();
    int hour2 = dateTime2.time().hour();
    int minute2 = dateTime2.time().minute();
    int totalMinutes1 = hour1 * 60 + minute1;
    int totalMinutes2 = hour2 * 60 + minute2;
    int midpointTotalMinutes = (totalMinutes1 + totalMinutes2) / 2;
    int midpointHour = midpointTotalMinutes / 60;
    int midpointMinute = midpointTotalMinutes % 60;

    // Set the correct time on the midpoint date
    midpointDateTime.setTime(QTime(midpointHour, midpointMinute));

    // Format the date and time for display
    QString midpointDate = midpointDateTime.toString("dd/MM/yyyy");
    QString midpointTime = midpointDateTime.toString("HH:mm");

    double lat1 = birthInfo1["latitude"].toString().toDouble();
    double lon1 = birthInfo1["longitude"].toString().toDouble();
    double lat2 = birthInfo2["latitude"].toString().toDouble();
    double lon2 = birthInfo2["longitude"].toString().toDouble();
    double midpointLat = (lat1 + lat2) / 2.0;
    double lonDiff = fmod(lon2 - lon1 + 540.0, 360.0) - 180.0;
    double midpointLon = fmod(lon1 + lonDiff / 2.0 + 360.0, 360.0);
    if (midpointLon > 180.0) midpointLon -= 360.0;
    QString midpointLatStr = QString::number(midpointLat, 'f', 6);
    QString midpointLonStr = QString::number(midpointLon, 'f', 6);

    QString utcOffsetStr1 = birthInfo1["utcOffset"].toString();
    QString utcOffsetStr2 = birthInfo2["utcOffset"].toString();
    bool neg1 = utcOffsetStr1.startsWith("-");
    bool neg2 = utcOffsetStr2.startsWith("-");
    QStringList parts1 = utcOffsetStr1.mid(1).split(":");
    QStringList parts2 = utcOffsetStr2.mid(1).split(":");
    double hours1 = parts1[0].toDouble() + parts1[1].toDouble() / 60.0;
    double hours2 = parts2[0].toDouble() + parts2[1].toDouble() / 60.0;
    if (neg1) hours1 = -hours1;
    if (neg2) hours2 = -hours2;
    double midpointHours = (hours1 + hours2) / 2.0;
    bool neg = midpointHours < 0;
    double absHours = std::abs(midpointHours);
    int h = static_cast<int>(absHours);
    int m = static_cast<int>((absHours - h) * 60);

    // With this corrected version:
    QString midpointUtcOffsetStr = QString("%1%2:%3")
            .arg(neg ? "-" : "+")
            .arg(h, 1, 10, QChar('0'))  // Use width 1 to avoid unnecessary padding
            .arg(m, 2, 10, QChar('0'));

    QString houseSystem = birthInfo1["houseSystem"].toString();
    QString davisonFirstName = name1 + " " + surname1;
    QString davisonLastName = name2 + " " + surname2;

    first_name->setText(davisonFirstName);
    last_name->setText(davisonLastName);
    m_birthDateEdit->setText(midpointDate);
    m_birthTimeEdit->setText(midpointTime);

    QString latDirection = (midpointLat >= 0) ? "N" : "S";
    QString lonDirection = (midpointLon >= 0) ? "E" : "W";
    QString googleCoords = QString("%1° %2, %3° %4")
            .arg(qAbs(midpointLat), 0, 'f', 4).arg(latDirection)
            .arg(qAbs(midpointLon), 0, 'f', 4).arg(lonDirection);
    m_googleCoordsEdit->setText(googleCoords);

    int utcIndex = -1;
    // First try exact match
    utcIndex = m_utcOffsetCombo->findText(midpointUtcOffsetStr);
    // If not found, try with "UTC" prefix
    if (utcIndex < 0) {
        utcIndex = m_utcOffsetCombo->findText("UTC" + midpointUtcOffsetStr);
    }
    // If still not found, try partial match
    if (utcIndex < 0) {
        // Extract the numeric part (e.g., "+1:30" -> "1:30")
        QString numericPart = midpointUtcOffsetStr.mid(1);
        for (int i = 0; i < m_utcOffsetCombo->count(); i++) {
            QString itemText = m_utcOffsetCombo->itemText(i);
            if (itemText.contains(numericPart)) {
                utcIndex = i;
                break;
            }
        }
    }
    // If still not found, try matching just the hour part
    if (utcIndex < 0) {
        QString hourPart = QString::number(h);
        for (int i = 0; i < m_utcOffsetCombo->count(); i++) {
            QString itemText = m_utcOffsetCombo->itemText(i);
            if ((itemText.contains("+" + hourPart) || itemText.contains("-" + hourPart)) &&
                    ((neg && itemText.contains("-")) || (!neg && !itemText.contains("-")))) {
                utcIndex = i;
                break;
            }
        }
    }
    // If we found a match, set it
    if (utcIndex >= 0) {
        m_utcOffsetCombo->setCurrentIndex(utcIndex);
    } else {
        // If all else fails, add the calculated offset to the combobox
        m_utcOffsetCombo->addItem("UTC" + midpointUtcOffsetStr);
        m_utcOffsetCombo->setCurrentIndex(m_utcOffsetCombo->count() - 1);
    }

    int houseSystemIndex = m_houseSystemCombo->findText(houseSystem);
    if (houseSystemIndex >= 0)
        m_houseSystemCombo->setCurrentIndex(houseSystemIndex);

    calculateChart();

    QJsonObject relationshipInfo;
    relationshipInfo["type"] = "Davison";
    relationshipInfo["person1"] = name1 + " " + surname1;
    relationshipInfo["person2"] = name2 + " " + surname2;
    relationshipInfo["displayName"] = "Davison Chart: " + davisonFirstName + " & " + davisonLastName;
    m_currentRelationshipInfo = relationshipInfo;
    // Recalculating afterward would silently redo this same midpoint calculation, which
    // is harmless but pointless and easy to mistake for verifying the chart - disable it.
    m_calculateButton->setEnabled(false);

    QJsonObject davisonBirthInfo;
    davisonBirthInfo["firstName"] = davisonFirstName;
    davisonBirthInfo["lastName"] = davisonLastName;
    davisonBirthInfo["date"] = midpointDate;
    davisonBirthInfo["time"] = midpointTime;
    davisonBirthInfo["latitude"] = midpointLatStr;
    davisonBirthInfo["longitude"] = midpointLonStr;
    davisonBirthInfo["utcOffset"] = midpointUtcOffsetStr;
    davisonBirthInfo["houseSystem"] = houseSystem;
    davisonBirthInfo["googleCoords"] = googleCoords;
    QJsonObject saveData;
    saveData["chartData"] = m_currentChartData;
    saveData["birthInfo"] = davisonBirthInfo;
    saveData["relationshipInfo"] = relationshipInfo;  // ← THIS is the only required addition

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmm");
    QString outputFileName = QString("Davison_%1_%2_%3.astr")
            .arg(name1 + surname1)
            .arg(name2 + surname2)
            .arg(timestamp);
    QDir relationshipDir(appDir + "/RelationshipCharts");
    if (!relationshipDir.exists()) {
        relationshipDir.mkpath(".");
    }
    QString outputFilePath = appDir + "/RelationshipCharts/" + outputFileName;

    QFile outputFile(outputFilePath);
    if (outputFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(saveData);
        outputFile.write(doc.toJson(QJsonDocument::Indented));
        outputFile.close();
        QMessageBox::information(this, "Chart Saved", "Davison chart saved to:\n" + outputFilePath);
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not save Davison chart to:\n" + outputFilePath);
    }

    displayChart(m_currentChartData);
    m_chartCalculated = true;
    AsteriaGlobals::lastGeneratedChartType = "Davison Relationship";

    populateInfoOverlay();
    setWindowTitle("Asteria - Astrological Chart Analysis - " + relationshipInfo["displayName"].toString());
}


void MainWindow::createSynastryChart()
{
    QMessageBox::information(this, "Select Charts",
                             "Please select two natal charts to create a Synastry chart.");
    QString appDir = AsteriaGlobals::appDir;
    QDir dir;
    if (!dir.exists(appDir))
        dir.mkpath(appDir);

    QStringList filePaths = QFileDialog::getOpenFileNames(
                this, "Select Two Charts", appDir, "Astrological Chart (*.astr)");

    if (filePaths.size() != 2) {
        QMessageBox::warning(this, "Invalid Selection",
                             "You must select exactly two charts.");
        return;
    }

    QJsonObject saveData1, saveData2;
    QFile file1(filePaths[0]);
    if (file1.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file1.readAll());
        file1.close();
        if (doc.isObject()) {
            saveData1 = doc.object();
        } else {
            QMessageBox::critical(this, "Load Error", "Invalid chart file format: " + filePaths[0]);
            return;
        }
    } else {
        QMessageBox::critical(this, "Load Error", "Could not open chart file " + filePaths[0]);
        return;
    }

    QFile file2(filePaths[1]);
    if (file2.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file2.readAll());
        file2.close();
        if (doc.isObject()) {
            saveData2 = doc.object();
        } else {
            QMessageBox::critical(this, "Load Error", "Invalid chart file format: " + filePaths[1]);
            return;
        }
    } else {
        QMessageBox::critical(this, "Load Error", "Could not open chart file " + filePaths[1]);
        return;
    }

    QJsonObject birthInfo1 = saveData1["birthInfo"].toObject();
    QJsonObject birthInfo2 = saveData2["birthInfo"].toObject();
    QString name1 = birthInfo1["firstName"].toString();
    QString surname1 = birthInfo1["lastName"].toString();
    QString name2 = birthInfo2["firstName"].toString();
    QString surname2 = birthInfo2["lastName"].toString();
    QString displayName1 = (name1 + " " + surname1).trimmed();
    QString displayName2 = (name2 + " " + surname2).trimmed();
    if (displayName1.isEmpty()) displayName1 = "Person A";
    if (displayName2.isEmpty()) displayName2 = "Person B";
    QString label1 = name1.isEmpty() ? displayName1 : name1;
    QString label2 = name2.isEmpty() ? displayName2 : name2;

    // Reset chart state
    m_chartCalculated = false;
    m_currentChartData = QJsonObject();
    m_currentNatalChartData = QJsonObject();
    m_progressionYear = 0;
    m_currentRelationshipInfo = QJsonObject();
    m_chartRenderer->scene()->clear();

    // Person A plays the "natal" (outer wheel) role, Person B the "progressed"
    // (inner wheel) role in the generic bi-wheel infrastructure shared with
    // Secondary Progression - see setDualChartData()/calculateInteraspects().
    ChartData personA = filterAdditionalBodies(convertJsonToChartData(saveData1["chartData"].toObject()));
    ChartData personB = filterAdditionalBodies(convertJsonToChartData(saveData2["chartData"].toObject()));

    // Interaspects: planet1 = Person B, planet2 = Person A (matches the
    // (progressed, natal) argument convention so a reload via loadChart()
    // recomputes the identical table - see CLAUDE.md's bi-wheel section).
    QVector<AspectData> synastryAspects = m_chartDataManager.calculateInteraspects(personB, personA);

    // ── Render bi-wheel ──────────────────────────────────────────────────────
    m_chartRenderer->setDualChartData(personA, personB, synastryAspects);
    m_chartRenderer->renderChart();

    // ── Update side panels ───────────────────────────────────────────────────
    m_planetListWidget->updateDualData(personA, personB, label1, label2);
    m_aspectarianWidget->updateDualData(personA, personB, synastryAspects,
                                        label2 + " Aspects", label1 + " ↔ " + label2);
    m_modalityElementWidget->updateDualData(personA, personB, label1, label2);

    // Store chart JSONs (Person B = "chartData"/detail slot, Person A = "natalChartData"/base slot)
    m_currentChartData      = saveData2["chartData"].toObject();
    m_currentNatalChartData = saveData1["chartData"].toObject();
    updateChartDetailsTables(m_currentChartData);

    QJsonObject relationshipInfo;
    relationshipInfo["type"] = "Synastry";
    relationshipInfo["person1"] = displayName1;
    relationshipInfo["person2"] = displayName2;
    relationshipInfo["displayName"] = "Synastry: " + displayName1 + " & " + displayName2;
    m_currentRelationshipInfo = relationshipInfo;
    // Recalculating via the plain Calculate button would discard this two-chart
    // comparison and replace it with an unrelated single calculation.
    m_calculateButton->setEnabled(false);

    first_name->setText(displayName1);
    last_name->setText("& " + displayName2);

    m_chartCalculated = true;
    AsteriaGlobals::lastGeneratedChartType = "Synastry";
    m_getInterpretationButton->setEnabled(true);
    // Transit/return predictions don't apply to a static two-person overlay.
    getPredictionButton->setEnabled(false);
    getTransitsButton->setEnabled(false);

    m_interpretations = QJsonArray();
    renderAllInterpretations();
    statusBar()->showMessage("Synastry chart calculated successfully", 3000);

    QString infoText = QString(
        "Synastry Chart (Bi-Wheel)\n"
        "%1 (outer wheel) & %2 (inner wheel)\n\n")
        .arg(displayName1)
        .arg(displayName2);
    appendInterpretationEntry("chart_info", "Synastry", infoText);

    // Save the synastry chart
    QJsonObject synastrySaveData;
    synastrySaveData["chartData"] = m_currentChartData;
    synastrySaveData["natalChartData"] = m_currentNatalChartData;
    synastrySaveData["chartType"] = AsteriaGlobals::lastGeneratedChartType;
    synastrySaveData["relationshipInfo"] = relationshipInfo;

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmm");
    QString outputFileName = QString("Synastry_%1_%2_%3.astr")
            .arg(name1 + surname1)
            .arg(name2 + surname2)
            .arg(timestamp);
    QDir relationshipDir(appDir + "/RelationshipCharts");
    if (!relationshipDir.exists()) {
        relationshipDir.mkpath(".");
    }
    QString outputFilePath = appDir + "/RelationshipCharts/" + outputFileName;
    QFile outputFile(outputFilePath);
    if (outputFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(synastrySaveData);
        outputFile.write(doc.toJson(QJsonDocument::Indented));
        outputFile.close();
        QMessageBox::information(this, "Chart Saved", "Synastry chart saved to:\n" + outputFilePath);
    } else {
        QMessageBox::warning(this, "Save Failed", "Could not save Synastry chart to:\n" + outputFilePath);
    }

    populateInfoOverlay();
    setWindowTitle("Asteria - Astrological Chart Analysis - " + relationshipInfo["displayName"].toString());
}

void MainWindow::showRelationshipChartsDialog()
{
    // Create the dialog only if it doesn't exist yet
    if (!m_relationshipChartsDialog) {
        m_relationshipChartsDialog = new QDialog(this);
        m_relationshipChartsDialog->setWindowTitle("About Relationship Charts");
        m_relationshipChartsDialog->setMinimumSize(500, 400);

        // Create layout
        QVBoxLayout *layout = new QVBoxLayout(m_relationshipChartsDialog);

        // Create a text browser for rich text display
        QTextBrowser *textBrowser = new QTextBrowser(m_relationshipChartsDialog);
        textBrowser->setOpenExternalLinks(true);

        // Set the help content
        QString helpText = R"(
        <h2>Understanding Relationship Charts</h2>

        <h3>Composite Charts</h3>
        <p>A Composite Chart represents the midpoints between two people's natal charts and shows the energy of the relationship itself as a separate entity.</p>

        <p><b>How it's calculated:</b> For each planet and point, the Composite Chart takes the midpoint between the same planets in both individuals' charts. For example, if Person A's Sun is at 15° Aries and Person B's Sun is at 15° Libra, the Composite Sun would be at 15° Cancer (the midpoint).</p>

        <p><b>Important Note:</b> When viewing a Composite Chart in Asteria, the birth information fields are populated with placeholder values only. These values are not used in the actual calculation of the chart. <span style="color:red;font-weight:bold;">Do not attempt to recalculate the chart</span> using these placeholder values, as this will produce an incorrect chart.</p>

        <p><b>Purpose:</b> The Composite Chart reveals the purpose and potential of the relationship. It shows how two people function as a unit and the shared destiny or path of the relationship.</p>

        <p><b>Insights offered:</b></p>
        <ul>
            <li>The relationship's inherent strengths and challenges</li>
            <li>The purpose or mission of the relationship</li>
            <li>How others perceive you as a couple</li>
            <li>The natural dynamics that emerge when you're together</li>
        </ul>

        <h3>Davison Relationship Charts</h3>
        <p>A Davison Chart creates a hypothetical birth chart for the relationship by finding the midpoint between two people's birth times and locations.</p>

        <p><b>How it's calculated:</b> The Davison Chart uses the average of:</p>
        <ul>
            <li>The two birth dates (midpoint in time)</li>
            <li>The two birth times (midpoint in time)</li>
            <li>The two birth locations (midpoint in space - latitude and longitude)</li>
        </ul>

        <p><b>Purpose:</b> The Davison Chart treats the relationship as if it were a person with its own birth chart. It shows the relationship's inherent nature and potential evolution over time.</p>

        <p><b>Insights offered:</b></p>
        <ul>
            <li>The relationship's innate character and development potential</li>
            <li>How the relationship responds to transits and progressions</li>
            <li>The relationship's timing and life cycles</li>
            <li>A more dynamic view of the relationship as an evolving entity</li>
        </ul>

        <h3>Synastry Charts</h3>
        <p>A Synastry Chart overlays two people's real natal charts as a bi-wheel and shows the aspects each person's planets make to the other person's planets - no midpoint or hypothetical entity is created.</p>

        <p><b>How it's calculated:</b> The two natal charts are drawn as a bi-wheel (one on the outer wheel, one on the inner wheel), and interaspects are calculated between every planet pair across the two charts. Each planet's house overlay - which house it falls into in the other person's chart - is also considered.</p>

        <p><b>Purpose:</b> Synastry reveals how two people's individual natal energies interact directly with one another - attractions, frictions, and areas of natural support or challenge.</p>

        <p><b>Insights offered:</b></p>
        <ul>
            <li>Where the two people's planets support or challenge each other</li>
            <li>Which areas of life (houses) each person activates in the other</li>
            <li>Core attractions and points of friction in the relationship</li>
        </ul>

        <h3>Which Chart to Use?</h3>
        <p>All three techniques offer valuable, complementary insights:</p>
        <ul>
            <li><b>Composite:</b> Better for understanding the relationship's purpose and inherent dynamics</li>
            <li><b>Davison:</b> Better for timing events in the relationship and understanding its evolution</li>
            <li><b>Synastry:</b> Better for understanding how the two individuals' own natal charts interact</li>
        </ul>

        <p>For a complete relationship analysis, it's beneficial to examine all three techniques together.</p>
        )";

        textBrowser->setHtml(helpText);
        layout->addWidget(textBrowser);

        // Add a close button at the bottom
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *closeButton = new QPushButton("Close", m_relationshipChartsDialog);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        layout->addLayout(buttonLayout);

        // Connect the close button
        connect(closeButton, &QPushButton::clicked, m_relationshipChartsDialog, &QDialog::close);

        // Connect the dialog's finished signal to handle cleanup
        connect(m_relationshipChartsDialog, &QDialog::finished, this, [this]() {
            m_relationshipChartsDialog->deleteLater();
            m_relationshipChartsDialog = nullptr;
        });
    }

    // Show and raise the dialog to bring it to the front
    m_relationshipChartsDialog->show();
    m_relationshipChartsDialog->raise();
    m_relationshipChartsDialog->activateWindow();

}

QJsonObject MainWindow::loadChartForRelationships(const QString &filePath) {
    QJsonObject chartData;

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject saveData = doc.object();
            // Load chart data
            if (saveData.contains("chartData") && saveData["chartData"].isObject()) {
                chartData = saveData["chartData"].toObject();
            }
            // Load birth information
            if (saveData.contains("birthInfo") && saveData["birthInfo"].isObject()) {
                chartData["birthInfo"] = saveData["birthInfo"].toObject();
            }
        }
    }

    return chartData;
}


void MainWindow::showChangelog(){

    // Create the dialog only if it doesn't exist yet
    if (!m_showChangelogDialog) {
        m_showChangelogDialog = new QDialog(this);
        m_showChangelogDialog->setWindowTitle("Changelog");
        m_showChangelogDialog->setMinimumSize(600, 500);

        // Create layout
        QVBoxLayout *layout = new QVBoxLayout(m_showChangelogDialog);

        // Create a text browser for rich text display
        QTextBrowser *textBrowser = new QTextBrowser(m_showChangelogDialog);
        textBrowser->setOpenExternalLinks(true);

        // Set the changelog content
        QString changelogText = R"(

<h1>Changelog</h1>

<h2>Version 0.9 (2026-04-30) <span style='color:#27ae60;'>— Asteria L fork by lejean2000</span></h2>
<ul>
  <li><b>Dual progressed/natal bi-wheel chart</b> with full save and reload support</li>
  <li><b>Extended AI prompts:</b> richer natal chart narrative, dedicated progressed chart prompt, improved transit interpretations</li>
  <li><b>AI prompts moved to Qt resource files</b> for easier maintenance</li>
  <li><b>Clone button</b> in the AI Model Selector dialog</li>
  <li><b>Planet icons</b> now shown in the modalities panel</li>
  <li><b>Editable latitude/longitude fields</b> — coordinates can be typed directly</li>
  <li><b>Windows installer</b> built with Inno Setup</li>
  <li><b>Fixes:</b> chart type persistence, JSON data precision, higher default API token limits</li>
</ul>

)";

        textBrowser->setHtml(changelogText);

        layout->addWidget(textBrowser);

        // Add a close button at the bottom
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *closeButton = new QPushButton("Close", m_showChangelogDialog);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        layout->addLayout(buttonLayout);

        // Connect the close button
        connect(closeButton, &QPushButton::clicked, m_showChangelogDialog, &QDialog::close);

        // Connect the dialog's finished signal to handle cleanup
        connect(m_showChangelogDialog, &QDialog::finished, this, [this]() {
            m_showChangelogDialog->deleteLater();
            m_showChangelogDialog = nullptr;
        });
    }

    // Show and raise the dialog to bring it to the front
    m_showChangelogDialog->show();
    m_showChangelogDialog->raise();
    m_showChangelogDialog->activateWindow();
}

