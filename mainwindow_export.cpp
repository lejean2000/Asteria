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
void MainWindow::getPrediction() {
    // Only proceed if we have a calculated chart
    if (!m_chartCalculated) {
        QMessageBox::warning(this, "No Chart", "Please calculate a birth chart first.");
        return;
    }

    if (!AsteriaGlobals::activeModelLoaded) {
           m_mistralApi.loadActiveModel();  // Try to reload once
           if (!AsteriaGlobals::activeModelLoaded) {
               QMessageBox::information(this, "AI Model Not Configured",
                   "No active AI model found. Please go to Settings → Configure AI Models to set up a model.");
               return;
           }
       }

    // Get birth details
    QDate birthDate = getBirthDate();
    QTime birthTime = QTime::fromString(m_birthTimeEdit->text(), "HH:mm");
    QString utcOffset = m_utcOffsetCombo->currentText();
    QString latitude = m_latitudeEdit->text();
    QString longitude = m_longitudeEdit->text();

    // Get transit date range
    QDate fromDate = QDate::fromString(m_predictiveFromEdit->text(), "dd/MM/yyyy");
    QDate toDate = QDate::fromString(m_predictiveToEdit->text(), "dd/MM/yyyy");

    // Validate dates
    if (!fromDate.isValid() || !toDate.isValid()) {
        QMessageBox::warning(this, "Input Error", "Please enter valid dates for prediction range.");
        return;
    }

    if (fromDate > toDate) {
        QMessageBox::warning(this, "Input Error", "From date must be before To date.");
        return;
    }

    // Calculate days between (inclusive)
    int transitDays = fromDate.daysTo(toDate) + 1;

    if (transitDays <= 0 || transitDays > 30) {
        QMessageBox::warning(this, "Input Error", "Prediction period must be between 1 and 30 days.");
        return;
    }

    getPredictionButton->setEnabled(false);

    // Update status
    statusBar()->showMessage(QString("Calculating transits for %1 to %2...")
                             .arg(fromDate.toString("yyyy-MM-dd"))
                             .arg(toDate.toString("yyyy-MM-dd")));

    // Calculate transits
    QJsonObject transitData = m_chartDataManager.calculateTransitsAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, fromDate, transitDays);

    if (m_chartDataManager.getLastError().isEmpty()) {
        displayRawTransitData(transitData);

        // Attach natal planets/angles so the AI has the birth chart context
        if (m_currentChartData.contains("planets"))
            transitData["natalPlanets"] = m_currentChartData["planets"];
        if (m_currentChartData.contains("angles"))
            transitData["natalAngles"] = m_currentChartData["angles"];

        AsteriaGlobals::lastGeneratedChartType = "Transits";

        // Send to API for interpretation
        m_mistralApi.interpretTransits(transitData);
    } else {
        handleError("Transit calculation error: " + m_chartDataManager.getLastError());
        getPredictionButton->setEnabled(true);

    }

}

void MainWindow::displayTransitInterpretation(const QString &interpretation) {
    appendInterpretationEntry("ai_transit", "Transits", interpretation,
                              m_predictiveFromEdit->text(),
                              m_predictiveToEdit->text());
    statusBar()->showMessage("Transit interpretation complete", 3000);
    getPredictionButton->setEnabled(true);
}

void MainWindow::populateInfoOverlay() {
    chartInfoOverlay->setVisible(m_showInfoOverlay);
    m_nameLabel->setText(first_name->text());
    m_surnameLabel->setText(last_name->text());
    m_birthDateLabel->setText(m_birthDateEdit->text());
    m_birthTimeLabel->setText(m_birthTimeEdit->text());
    m_locationLabel->setText(m_googleCoordsEdit->text());

    // Add Sun and Ascendant information from chart data
    if (!m_currentChartData.isEmpty()) {
        // Get Sun information
        if (m_currentChartData.contains("planets") && m_currentChartData["planets"].isArray()) {
            QJsonArray planets = m_currentChartData["planets"].toArray();

            for (const QJsonValue &planetValue : planets) {
                QJsonObject planet = planetValue.toObject();
                QString planetId = planet["id"].toString();

                if (planetId.toLower() == "sun") {
                    QString sunSign = planet["sign"].toString();
                    double sunDegree = planet["longitude"].toDouble();
                    m_sunSignLabel->setText(QString("Sun: %1").arg(sunSign));

                    break;
                }
            }
        }

        // Get Ascendant information
        if (m_currentChartData.contains("angles") && m_currentChartData["angles"].isArray()) {
            QJsonArray angles = m_currentChartData["angles"].toArray();

            for (const QJsonValue &angleValue : angles) {
                QJsonObject angle = angleValue.toObject();
                QString angleId = angle["id"].toString();

                if (angleId.toLower() == "asc") {
                    QString ascSign = angle["sign"].toString();
                    double ascDegree = angle["longitude"].toDouble();
                    m_ascendantLabel->setText(QString("Asc: %1").arg(ascSign));

                    break;
                }
            }
        }
    }
    m_housesystemLabel->setText(m_houseSystemCombo->currentText());
}

void MainWindow::displayRawTransitData(const QJsonObject &transitData) {
    QString rawData = transitData["rawTransitData"].toString();
    rawTransitTable->setRowCount(0);

    QStringList lines = rawData.split('\n', Qt::SkipEmptyParts);
    bool inTransitsSection = false;
    QDate currentDate;

    for (const QString &line : lines) {
        if (line.startsWith("---TRANSITS---")) {
            inTransitsSection = true;
            continue;
        }
        if (!inTransitsSection) {
            continue;
        }

        if (line.contains(QRegularExpression("^\\d{4}/\\d{2}/\\d{2}:"))) {
            QRegularExpression dateRe("^(\\d{4}/\\d{2}/\\d{2}):");
            QRegularExpressionMatch dateMatch = dateRe.match(line);
            if (dateMatch.hasMatch()) {
                currentDate = QDate::fromString(dateMatch.captured(1), "yyyy/MM/dd");
            }

            int startPos = line.indexOf(':') + 1;
            QString aspectsStr = line.mid(startPos).trimmed();
            QStringList aspects = aspectsStr.split(',', Qt::SkipEmptyParts);

            for (QString aspect : aspects) {
                aspect = aspect.trimmed();

                QRegularExpression aspectRe("((?:North|South) Node|Pars Fortuna|Part of Spirit|East Point|\\w+)(?:\\s+\\(R\\))? (\\w+) ((?:North|South) Node|Pars Fortuna|Part of Spirit|East Point|\\w+(?:\\s+\\(R\\))?) \\(([\\d.]+)°\\)");

                QRegularExpressionMatch aspectMatch = aspectRe.match(aspect);

                if (aspectMatch.hasMatch()) {
                    int row = rawTransitTable->rowCount();
                    rawTransitTable->insertRow(row);

                    QString transitPlanet = aspectMatch.captured(1);
                    bool isRetrograde = aspect.contains("(R)") &&
                            !transitPlanet.contains("Node");
                    QString natalPlanet = aspectMatch.captured(3);

                    if (transitPlanet == "North Node") transitPlanet = "NNode";
                    if (transitPlanet == "South Node") transitPlanet = "SNode";
                    if (transitPlanet == "Pars Fortuna") transitPlanet = "PFortuna";
                    if (transitPlanet == "Part of Spirit") transitPlanet = "PSpirit";
                    if (transitPlanet == "East Point") transitPlanet = "EPoint";

                    if (natalPlanet == "North Node") natalPlanet = "NNode";
                    if (natalPlanet == "South Node") natalPlanet = "SNode";
                    if (natalPlanet == "Pars Fortuna") natalPlanet = "PFortuna";
                    if (natalPlanet == "Part of Spirit") natalPlanet = "PSpirit";
                    if (natalPlanet == "East Point") natalPlanet = "EPoint";

                    rawTransitTable->setItem(row, 0, new QTableWidgetItem(currentDate.toString("yyyy-MM-dd")));
                    rawTransitTable->setItem(row, 1, new QTableWidgetItem(transitPlanet + (isRetrograde ? " (R)" : "")));
                    rawTransitTable->setItem(row, 2, new QTableWidgetItem(aspectMatch.captured(2)));
                    rawTransitTable->setItem(row, 3, new QTableWidgetItem(natalPlanet + " (" + aspectMatch.captured(4) + "°)"));
                }
            }
        }
    }
    rawTransitTable->sortItems(0);
}

void MainWindow::exportChartImage()
{
    if (!m_chartCalculated) {
        QMessageBox::warning(this, "No Chart", "Please calculate a chart first.");
        return;
    }

    QString filePath = getFilepath("png");
    if (filePath.isEmpty())
        return;

    // Ensure we're on the chart tab
    m_centralTabWidget->setCurrentIndex(0);

    // Create a pixmap to render the chart
    QPixmap pixmap(m_chartView->scene()->sceneRect().size().toSize());
    pixmap.fill(Qt::white);

    // Render the chart to the pixmap
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    m_chartView->scene()->render(&painter);
    painter.end();

    // Save image
    if (pixmap.save(filePath)) {
        statusBar()->showMessage("Chart image exported to " + filePath, 3000);
    } else {
        QMessageBox::critical(this, "Export Error", "Could not save image to " + filePath);
    }
}

void MainWindow::exportAsPdf() {
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
    
    QString filePath = getFilepath("pdf");
    if (filePath.isEmpty())
        return;
    
    // Render the chart view into a transparent pixmap
    QPixmap pixmap(m_chartView->size());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    m_chartView->render(&painter);
    painter.end();
    
    // Save debug image
    const QString debugImagePath = QDir::tempPath() + "/chart_debug_render.png";
    if (!pixmap.save(debugImagePath)) {
        QMessageBox::critical(this, "Error", "Failed to save debug image. Check rendering.");
        return;
    }
    
    // PDF setup
    QPdfWriter pdfWriter(filePath);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setResolution(300);
    pdfWriter.setTitle("Astrological Chart");
    QPainter pdfPainter(&pdfWriter);
    if (!pdfPainter.isActive()) {
        QMessageBox::critical(this, "Error", "PDF painter failed to initialize.");
        return;
    }
    
    drawPage0(pdfPainter, pdfWriter);
    pdfWriter.newPage();  // Proceed to rest of content
    
    const QRectF pageRect = pdfWriter.pageLayout().paintRectPixels(pdfWriter.resolution());
    const QPixmap scaledPixmap = pixmap.scaled(pageRect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const qreal x = (pageRect.width() - scaledPixmap.width()) / 2;
    const qreal y = (pageRect.height() - scaledPixmap.height()) / 2;
    pdfPainter.drawPixmap(QPointF(x, y), scaledPixmap);
    
    // ------- PAGE 2: PLANETS -------
    pdfWriter.newPage();
    QFont titleFont("Arial", 24, QFont::Bold);
    QFont headerFont("Arial", 18, QFont::Bold);
    QFont textFont("Arial", 16);
    const int margin = 30;
    const int pageWidth = pdfWriter.width();
    const int pageHeight = pdfWriter.height();
    const int rowHeight = 120;
    const int cellPadding = 15;
    const int tableX = margin;
    const int tableWidth = pageWidth - 2 * margin;
    const int colWidth = tableWidth / 4;
    int tableY = margin + 130;
    int currentY = tableY;
    
    auto drawTableText = [&](int col, int y, const QString &text, const QFont &font) {
        QRect cell(tableX + col * colWidth + cellPadding, y + cellPadding, colWidth - 2 * cellPadding, rowHeight - 2 * cellPadding);
        pdfPainter.setFont(font);
        pdfPainter.drawText(cell, Qt::AlignCenter, text);
    };
    
    // Title
    pdfPainter.setFont(titleFont);
    pdfPainter.drawText(QRect(0, margin, pageWidth, 70), Qt::AlignCenter, "Planets");
    
    // Table headers
    pdfPainter.setPen(QPen(Qt::black, 2.0));
    pdfPainter.drawLine(tableX, tableY, tableX + tableWidth, tableY);
    drawTableText(0, currentY, "Planet", headerFont);
    drawTableText(1, currentY, "Sign", headerFont);
    drawTableText(2, currentY, "Degree", headerFont);
    drawTableText(3, currentY, "House", headerFont);
    currentY += rowHeight;
    pdfPainter.drawLine(tableX, currentY, tableX + tableWidth, currentY);
    
    if (m_currentChartData.contains("planets") && m_currentChartData["planets"].isArray()) {
        const QJsonArray planets = m_currentChartData["planets"].toArray();
        for (const auto &planetValue : planets) {
            const QJsonObject planet = planetValue.toObject();
            QString planetName = planet["id"].toString();
            if (planet.contains("isRetrograde") && planet["isRetrograde"].toBool())
                planetName += " ℞";
            drawTableText(0, currentY, planetName, textFont);
            drawTableText(1, currentY, planet["sign"].toString(), textFont);
            drawTableText(2, currentY, QString::number(planet["longitude"].toDouble(), 'f', 2) + "°", textFont);
            drawTableText(3, currentY, planet["house"].toString(), textFont);
            currentY += rowHeight;
            pdfPainter.drawLine(tableX, currentY, tableX + tableWidth, currentY);
        }
    }
    
    for (int i = 0; i <= 4; ++i)
        pdfPainter.drawLine(tableX + i * colWidth, tableY, tableX + i * colWidth, currentY);
    
    // ------- PAGE 3: HOUSE CUSPS -------
    pdfWriter.newPage();
    pdfPainter.setFont(titleFont);
    pdfPainter.drawText(QRect(0, margin, pageWidth, 70), Qt::AlignCenter, "House Cusps");
    tableY = margin + 130;
    currentY = tableY;
    pdfPainter.setPen(QPen(Qt::black, 2.0));
    pdfPainter.drawLine(tableX, tableY, tableX + tableWidth, tableY);
    drawTableText(0, currentY, "House", headerFont);
    drawTableText(1, currentY, "Sign", headerFont);
    drawTableText(2, currentY, "Degree", headerFont);
    currentY += rowHeight;
    pdfPainter.drawLine(tableX, currentY, tableX + tableWidth, currentY);
    
    QMap<int, QJsonObject> houseMap;
    if (m_currentChartData.contains("houses") && m_currentChartData["houses"].isArray()) {
        for (const auto &houseValue : m_currentChartData["houses"].toArray()) {
            const QJsonObject house = houseValue.toObject();
            int id = house["id"].toString().mid(5).toInt();  // "house5" -> 5
            houseMap[id] = house;
        }
    }
    
    for (int i = 1; i <= 12; ++i) {
        const QJsonObject house = houseMap.value(i);
        drawTableText(0, currentY, QString::number(i), textFont);
        drawTableText(1, currentY, house["sign"].toString(), textFont);
        drawTableText(2, currentY, QString::number(house["longitude"].toDouble(), 'f', 2) + "°", textFont);
        currentY += rowHeight;
        pdfPainter.drawLine(tableX, currentY, tableX + tableWidth, currentY);
    }
    
    for (int i = 0; i <= 3; ++i)
        pdfPainter.drawLine(tableX + i * colWidth, tableY, tableX + i * colWidth, currentY);
    
    // ------- PAGE 4+: ASPECTS -------
    pdfWriter.newPage();
    pdfPainter.setFont(titleFont);
    pdfPainter.drawText(QRect(0, margin, pageWidth, 70), Qt::AlignCenter, "Aspects");
    tableY = margin + 130;
    currentY = tableY;
    pdfPainter.setFont(headerFont);
    drawTableText(0, currentY, "Planet 1", headerFont);
    drawTableText(1, currentY, "Aspect", headerFont);
    drawTableText(2, currentY, "Planet 2", headerFont);
    drawTableText(3, currentY, "Orb", headerFont);
    currentY += rowHeight;
    pdfPainter.drawLine(tableX, currentY, tableX + tableWidth, currentY);
    pdfPainter.setFont(textFont);
    
    const QJsonArray aspects = m_currentChartData["aspects"].toArray();
    for (const auto &aspectValue : aspects) {
        if (currentY + rowHeight > pageHeight - margin) {
            pdfWriter.newPage();
            currentY = tableY;
            pdfPainter.setFont(headerFont);
            drawTableText(0, currentY, "Planet 1", headerFont);
            drawTableText(1, currentY, "Aspect", headerFont);
            drawTableText(2, currentY, "Planet 2", headerFont);
            drawTableText(3, currentY, "Orb", headerFont);
            currentY += rowHeight;
            pdfPainter.drawLine(tableX, currentY, tableX + tableWidth, currentY);
            pdfPainter.setFont(textFont);
        }
        
        const QJsonObject aspect = aspectValue.toObject();
        drawTableText(0, currentY, aspect["planet1"].toString(), textFont);
        drawTableText(1, currentY, aspect["aspectType"].toString(), textFont);
        drawTableText(2, currentY, aspect["planet2"].toString(), textFont);
        drawTableText(3, currentY, QString::number(aspect["orb"].toDouble(), 'f', 2) + "°", textFont);
        currentY += rowHeight;
        pdfPainter.drawLine(tableX, currentY, tableX + tableWidth, currentY);
    }
    
    for (int i = 0; i <= 4; ++i)
        pdfPainter.drawLine(tableX + i * colWidth, tableY, tableX + i * colWidth, currentY);
    
    // ------- PAGE 5+: INTERPRETATION -------
    if (!m_interpretations.isEmpty()) {
        QString interpretationText;
        for (const QJsonValue &v : std::as_const(m_interpretations))
            interpretationText += v.toObject()["text"].toString() + "\n\n";

        pdfWriter.newPage();
        titleFont.setPointSize(22);
        pdfPainter.setFont(titleFont);
        pdfPainter.drawText(QRect(0, margin, pageWidth, 70), Qt::AlignCenter, "Interpretation");

        QTextDocument doc;
        textFont.setPointSize(16);
        doc.setDefaultFont(textFont);
        doc.setDocumentMargin(20);
        doc.setTextWidth(pageWidth - 2 * margin);
        doc.setPlainText(interpretationText.trimmed());
        
        QAbstractTextDocumentLayout* layout = doc.documentLayout();
        qreal totalHeight = layout->documentSize().height();
        qreal yOffset = 0;
        
        while (yOffset < totalHeight) {
            if (yOffset > 0) {
                pdfWriter.newPage();
                pdfPainter.setFont(titleFont);
                pdfPainter.drawText(QRect(0, margin, pageWidth, 70),
                                    Qt::AlignCenter, "Interpretation (cont.)");
            }
            
            qreal pageSpace = pageHeight - margin - (yOffset > 0 ? 100 : 200);
            QRectF clipRect(0, yOffset, pageWidth - 2 * margin, pageSpace);
            
            pdfPainter.save();
            pdfPainter.translate(margin, margin + (yOffset > 0 ? 100 : 200));
            QAbstractTextDocumentLayout::PaintContext ctx;
            ctx.clip = clipRect;
            layout->draw(&pdfPainter, ctx);
            pdfPainter.restore();
            
            yOffset += pageSpace;
            if (yOffset <= clipRect.top()) {
                qWarning() << "Pagination stuck at offset" << yOffset;
                break;
            }
        }
    } else {
        qWarning() << "No interpretation text to render";
    }
    
    pdfPainter.end();
    emit pdfExported(filePath);
#endif
}


void MainWindow::exportAsSvg() {

    if (!m_chartCalculated) {
        QMessageBox::warning(this, "No Chart", "Please calculate a chart first.");
        return;
    }

    QString filePath = getFilepath("svg");
    if (filePath.isEmpty())
        return;
    // Get the chart view and scene

    QGraphicsScene* scene = m_chartView->scene();

    // Save the current transform of the view
    QTransform originalTransform = m_chartView->transform();

    // Scale down the view temporarily (80% of original size)
    m_chartView->resetTransform();
    m_chartView->scale(0.8, 0.8);

    // Force update to ensure the scene reflects the new scale
    QApplication::processEvents();

    // Get the new scene rect after scaling
    QRectF sceneRect = scene->sceneRect();

    // Create SVG generator
    QSvgGenerator generator;
    generator.setFileName(filePath);
    generator.setSize(QSize(sceneRect.width(), sceneRect.height()));
    generator.setViewBox(sceneRect);
    generator.setTitle("Astrological Chart");
    generator.setDescription("Generated by Astrology Application");
    // Create painter
    QPainter painter;
    painter.begin(&generator);
    painter.setRenderHint(QPainter::Antialiasing);
    // Fill background with white
    painter.fillRect(sceneRect, Qt::white);
    // Render the scene
    scene->render(&painter, sceneRect, sceneRect);
    painter.end();
    // Restore the original transform
    m_chartView->setTransform(originalTransform);

    statusBar()->showMessage("Chart exported to " + filePath, 3000);
}

QString MainWindow::getFilepath(const QString &format)
{
    QString name = first_name->text().simplified();
    QString surname = last_name->text().simplified();

    if (name.isEmpty() || surname.isEmpty()) {
        QMessageBox::warning(this, "Missing Information", "Please enter both first name and last name to save the chart.");
        return QString();
    }

    QString appName = QApplication::applicationName();
    QString appDir = AsteriaGlobals::appDir;

#ifdef FLATHUB_BUILD
    // In Flatpak, use the app-specific data directory
#else
    // For local builds, use a directory in home

#endif

    QDir dir;
    if (!dir.exists(appDir))
        dir.mkpath(appDir);

    QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");
    QString currentTime = QTime::currentTime().toString("HHmm");
    QString chartTypeSanitized = QString(AsteriaGlobals::lastGeneratedChartType).replace(" ", "-");
    QString baseName = QString("%1-%2-%3-%4-%5-chart").arg(chartTypeSanitized, name, surname, currentDate, currentTime);
    QString defaultFilename = QString("%1.%2").arg(baseName, format);
    QString defaultPath = appDir + "/" + defaultFilename;

    QString filter;
    if (format == "svg") {
        filter = "SVG Files (*.svg)";
    } else if (format == "pdf") {
        filter = "PDF Files (*.pdf)";
    } else if (format == "png") {
        filter = "PNG Files (*.png)";
    } else if (format == "txt") {
        filter = "Text Files (*.txt)";
    } else if (format == "astr") {
        filter = "Asteria Files (*.astr)";
    } else {
        filter = "All Files (*)";
    }

    QString filePath = QFileDialog::getSaveFileName(this, "Export Chart", defaultPath, filter);

    if (filePath.isEmpty())
        return QString();

    // Force append extension if missing
    if (!filePath.endsWith("." + format, Qt::CaseInsensitive))
        filePath += "." + format;

    return filePath;
}


void MainWindow::printPdfFromPath(const QString& filePath) {
#if defined(FLATHUB_BUILD) || defined(GENTOO_BUILD)
    // Empty implementation for Flathub
    Q_UNUSED(filePath);
#else
    if (filePath.isEmpty()){
        return;
    }
    QPdfDocument pdf;
    pdf.load(filePath);
    if (pdf.status() != QPdfDocument::Status::Ready)
    {
        qWarning() << "PDF failed to load with status:" << pdf.status();
        qWarning() << "File path was:" << filePath;
        QMessageBox::critical(this, "Error", "Failed to load the exported PDF for printing.");
    }
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QPainter painter;
    if (!painter.begin(&printer)) {
        QMessageBox::critical(this, "Error", "Failed to start printing.");
        return;
    }
    for (int i = 0; i < pdf.pageCount(); ++i) {
        if (i > 0)
            printer.newPage();
        QSizeF pageSize = printer.pageRect(QPrinter::DevicePixel).size();
        QImage image = pdf.render(i, pageSize.toSize());
        painter.drawImage(QPoint(0, 0), image);
    }
    painter.end();
    statusBar()->showMessage("Chart printed successfully", 3000);
#endif
}

#if !defined(FLATHUB_BUILD) && !defined(GENTOO_BUILD)
void MainWindow::drawPage0(QPainter &painter, QPdfWriter &writer) {
    painter.save();
    const QRect pageRect = writer.pageLayout().paintRectPixels(writer.resolution());
    const int pageWidth = pageRect.width();
    const int margin = 50;
    // Title
    QFont titleFont("Times", 20, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(Qt::black);
    QString title = "Asteria - Astrological Chart Generation and Analysis Tool";
    QRect titleRect(margin, margin, pageWidth - 2 * margin, 60);
    painter.drawText(titleRect, Qt::AlignCenter, title);
    // Copyright (more vertical space and padding)
    QFont copyrightFont("Times", 10);
    painter.setFont(copyrightFont);
    int copyrightTop = margin + 100;
    QRect copyrightRect(margin, copyrightTop, pageWidth - 2 * margin, 30); // taller rect
    painter.drawText(copyrightRect, Qt::AlignCenter, "© 2025 Alamahant");
    // Star Banner
    int starTop = copyrightTop + 60;
    QRect starRect(pageWidth / 2 - 50, starTop, 100, 100);
    drawStarBanner(painter, starRect);
    // Labels
    QFont labelFont("Helvetica", 12);
    painter.setFont(labelFont);
    painter.setPen(Qt::darkBlue);
    QStringList labelTexts = {
        "Name: " + first_name->text(),
        "Surname: " + last_name->text(),
        "Birth Date: " + m_birthDateEdit->text(),
        "Birth Time: " + m_birthTimeEdit->text(),
        "Location: " + m_googleCoordsEdit->text(),
        "Sun Sign: " + m_sunSignLabel->text(),
        "Ascendant: " + m_ascendantLabel->text(),
        "House System: " + m_housesystemLabel->text()
    };
    // Start Y further down, aligned horizontally with star/copyright center
    int startY = starTop + 140;
    int spacing = 60;
    int extraSpacingAfterIndex = 3; // "Birth Time"
    int extraGap = 20;
    for (int i = 0; i < labelTexts.size(); ++i) {
        int yOffset = startY + i * spacing;
        if (i > extraSpacingAfterIndex) {
            yOffset += extraGap;
        }
        int boxHeight = spacing + 40;
        QRect textRect(margin, yOffset, pageWidth - 2 * margin, boxHeight);
        painter.drawText(textRect, Qt::AlignCenter, labelTexts[i]);
    }
    painter.restore();
}
#endif


void MainWindow::drawStarBanner(QPainter &painter, const QRect &rect) {
#if defined(FLATHUB_BUILD) || defined(GENTOO_BUILD)
    // Empty implementation for Flathub and Gentoo
    Q_UNUSED(painter);
    Q_UNUSED(rect);
#else
    painter.save();
    QPen pen(Qt::darkBlue);
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(QBrush(QColor(255, 215, 0))); // golden color
    QPoint center = rect.center();
    int radius = qMin(rect.width(), rect.height()) / 2 - 5;
    QPolygon star;
    for (int i = 0; i < 10; ++i) {
        double angle = i * M_PI / 5.0;
        int r = (i % 2 == 0) ? radius : radius / 2;
        int x = center.x() + r * std::sin(angle);
        int y = center.y() - r * std::cos(angle);
        star << QPoint(x, y);
    }
    painter.drawPolygon(star);
    painter.restore();
#endif
}
