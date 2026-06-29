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
void MainWindow::CalculateTransits() {

    // Only proceed if we have a calculated chart
    if (!m_chartCalculated) {
        QMessageBox::warning(this, "No Chart", "Please calculate a birth chart first.");
        return;
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


    // Gregorian calendar reform date
    QDate gregorianStart(1582, 10, 15);

    // Validate all dates are post-Gregorian
    if (birthDate < gregorianStart || fromDate < gregorianStart || toDate < gregorianStart) {
        QMessageBox::warning(
                    this,
                    tr("Unsupported Date"),
                    tr("All dates (birth, start, and end) must be after 15 October 1582 (Gregorian calendar reform) for transit calculations.\n"
                       "Please enter valid post-Gregorian dates.")
                    );
        return;
    }

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

    if (transitDays <= 0 || transitDays > 370) {
        QMessageBox::warning(this, "Input Error", "Prediction period must be between 1 and 370 days.");
        return;
    }

    if (transitDays > 60) {
        QMessageBox::information(
                    this,
                    "Please Be Patient",
                    "This operation may take some time.\n"

                    " When finished you will be notified."
                    );
    }


    // Update status
    statusBar()->showMessage(QString("Calculating transits for %1 to %2...")
                             .arg(fromDate.toString("yyyy-MM-dd"))
                             .arg(toDate.toString("yyyy-MM-dd")));

    // Calculate transits
    this->setEnabled(false); // Disable all widgets in the main window

    QJsonObject transitData = m_chartDataManager.calculateTransitsAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, fromDate, transitDays);

    this->setEnabled(true); // Enable all widgets in the main window


    if (m_chartDataManager.getLastError().isEmpty()) {
        displayRawTransitData(transitData);
        QMessageBox::information(this, "Transit Data", "Transit data has been generated successfully.\n"
                                                       "Please Navigate to the 'Raw Transit Data Table' to view the data.\n"
                                                       "You may use 'Tools->Transit Filter' for advanced filtering.");


    } else {
        handleError("Transit calculation error: " + m_chartDataManager.getLastError());

    }

}


void MainWindow::applyTransitFilter(const QString &datePattern,
                                    const QString &transitPattern,
                                    const QString &aspectPattern,
                                    const QString &natalPattern,
                                    const QString &maxOrbPattern,

                                    const QString &excludePattern)
{
    // Show "Applying filter..." before starting
    if (m_transitSearchDialog && m_transitSearchDialog->statusLabel) {
        m_transitSearchDialog->statusLabel->setText("Please wait...");
        qApp->processEvents();
    }
    // Save current state
    m_savedScrollPosition = rawTransitTable->verticalScrollBar()->value();
    m_savedSelection = rawTransitTable->selectionModel()->selection();

    int matchCount = 0;

    for(int row = 0; row < rawTransitTable->rowCount(); ++row) {
        bool match = true;

        // Apply include filters
        if(!datePattern.isEmpty()) {
            match &= rawTransitTable->item(row, 0)->text().contains(QRegularExpression(datePattern, QRegularExpression::CaseInsensitiveOption));
        }
        if(match && !transitPattern.isEmpty()) {
            match &= rawTransitTable->item(row, 1)->text().contains(QRegularExpression(transitPattern, QRegularExpression::CaseInsensitiveOption));
        }
        if(match && !aspectPattern.isEmpty()) {
            static const QString majorAspectRe = "Conjunction|Opposition|Trine|Square|Sextile";
            const QString &effectiveAspect = aspectPattern.trimmed().compare("MAJOR", Qt::CaseInsensitive) == 0
                                             ? majorAspectRe : aspectPattern;
            match &= rawTransitTable->item(row, 2)->text().contains(QRegularExpression(effectiveAspect, QRegularExpression::CaseInsensitiveOption));
        }
        if(match && !natalPattern.isEmpty()) {
            match &= rawTransitTable->item(row, 3)->text().contains(QRegularExpression(natalPattern, QRegularExpression::CaseInsensitiveOption));
        }
        // Orb Filter
        if (match && !maxOrbPattern.isEmpty()) {
            bool ok = false;
            double maxOrb = maxOrbPattern.toDouble(&ok);
            if (ok) {
                // Extract orb from natal planet column (column 3)
                QString natalText = rawTransitTable->item(row, 3)->text();
                QRegularExpression orbRegex("\\((\\d+(?:\\.\\d+)?)°\\)");
                QRegularExpressionMatch orbMatch = orbRegex.match(natalText);
                if (orbMatch.hasMatch()) {
                    double orbValue = orbMatch.captured(1).toDouble();
                    if (orbValue > maxOrb) {
                        match = false;
                    }
                }
                // If no orb found, you may want to hide or show by default:
                // else { match = false; } // Uncomment to hide rows without orb info
            }
        }


        //
        // Apply exclude filter
        if(match && !excludePattern.isEmpty()) {
            QStringList excludeTerms = excludePattern.split(',', Qt::SkipEmptyParts);
            for(const QString &term : excludeTerms) {
                QString trimmedTerm = term.trimmed();
                if(!trimmedTerm.isEmpty()) {
                    // Check if any column contains the exclude term
                    bool containsExcludeTerm =
                            rawTransitTable->item(row, 0)->text().contains(trimmedTerm, Qt::CaseInsensitive) ||
                            rawTransitTable->item(row, 1)->text().contains(trimmedTerm, Qt::CaseInsensitive) ||
                            rawTransitTable->item(row, 2)->text().contains(trimmedTerm, Qt::CaseInsensitive) ||
                            rawTransitTable->item(row, 3)->text().contains(trimmedTerm, Qt::CaseInsensitive);

                    if(containsExcludeTerm) {
                        match = false;
                        break; // No need to check other exclude terms
                    }
                }
            }
        }

        rawTransitTable->setRowHidden(row, !match);
        if(match) matchCount++;
    }
    // Show "Filter applied" after finishing
    if (m_transitSearchDialog && m_transitSearchDialog->statusLabel)
        m_transitSearchDialog->statusLabel->setText("Filter applied");
}


void MainWindow::openTransitFilter() {
    if (!m_transitSearchDialog) {
        m_transitSearchDialog = new TransitSearchDialog(this);
        connect(m_transitSearchDialog, &TransitSearchDialog::filterChanged,
                this, &MainWindow::applyTransitFilter);
    }
    m_transitSearchDialog->show();
}

void MainWindow::exportChartData(){
    // Check if we have chart data by looking for the details tab widget
    if (!m_chartDetailsWidget) {
        QMessageBox::warning(this, "No Chart Data", "Please generate a chart first.");
        return;
    }

    // Find the details tabs widget
    QTabWidget *detailsTabs = m_chartDetailsWidget->findChild<QTabWidget*>();
    if (!detailsTabs) {
        QMessageBox::warning(this, "No Chart Data", "Chart details not available.");
        return;
    }

    QString filePath = getFilepath("txt");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);

        // Write header
        stream << "ASTROLOGICAL CHART DATA\n";
        stream << "=======================\n\n";

        // Iterate through each tab
        for (int tabIndex = 0; tabIndex < detailsTabs->count(); ++tabIndex) {
            QWidget *tabWidget = detailsTabs->widget(tabIndex);
            QString tabName = detailsTabs->tabText(tabIndex).toUpper();
            QTableWidget *table = qobject_cast<QTableWidget*>(tabWidget);

            if (!table || table->rowCount() == 0) {
                continue; // Skip empty tables
            }

            // Write tab section header
            stream << tabName << "\n";
            stream << QString("-").repeated(tabName.length()) << "\n";

            // Write column headers
            QStringList headers;
            for (int col = 0; col < table->columnCount(); ++col) {
                headers << table->horizontalHeaderItem(col)->text();
            }

            // Calculate column widths for alignment
            QList<int> columnWidths;
            for (int col = 0; col < table->columnCount(); ++col) {
                int maxWidth = headers[col].length();
                for (int row = 0; row < table->rowCount(); ++row) {
                    QTableWidgetItem *item = table->item(row, col);
                    if (item) {
                        maxWidth = qMax(maxWidth, item->text().length());
                    }
                }
                columnWidths << qMax(maxWidth + 2, 8); // Minimum width of 8
            }

            // Write headers with proper spacing
            for (int col = 0; col < headers.size(); ++col) {
                stream << headers[col].leftJustified(columnWidths[col]);
            }
            stream << "\n";

            // Write header underlines
            for (int col = 0; col < headers.size(); ++col) {
                stream << QString("-").repeated(headers[col].length()).leftJustified(columnWidths[col]);
            }
            stream << "\n";

            // Write table data
            for (int row = 0; row < table->rowCount(); ++row) {
                for (int col = 0; col < table->columnCount(); ++col) {
                    QTableWidgetItem *item = table->item(row, col);
                    QString cellText = item ? item->text() : "";
                    stream << cellText.leftJustified(columnWidths[col]);
                }
                stream << "\n";
            }

            stream << "\n"; // Add spacing between sections
        }

        file.close();
        statusBar()->showMessage("Chart data exported to " + filePath, 3000);
    } else {
        QMessageBox::critical(this, "Export Error", "Could not save chart data to " + filePath);
    }
}

void MainWindow::CalculateEclipses()
{
    // Optional: Only proceed if a chart is calculated
    //  QMessageBox::warning(this, "No Chart", "Please calculate a birth chart first.");
    //}

    // Use the same date fields as for transits
    QDate fromDate = QDate::fromString(m_predictiveFromEdit->text(), "dd/MM/yyyy");
    QDate toDate = QDate::fromString(m_predictiveToEdit->text(), "dd/MM/yyyy");

    // Validate dates
    if (!fromDate.isValid() || !toDate.isValid()) {
        QMessageBox::warning(this, "Input Error", "Please enter valid dates for eclipse search range.");
        return;
    }
    if (fromDate > toDate) {
        QMessageBox::warning(this, "Input Error", "From date must be before To date.");
        return;
    }

    int days = fromDate.daysTo(toDate) + 1;
    if (days <= 0 || days > 365 * 100) {
        QMessageBox::warning(this, "Input Error", "Eclipse search period must be between 1 and 36500 days.");
        return;
    }

    // Example: Assume you have checkboxes for eclipse types

    if (!solarEclipses && !lunarEclipses) {
        QMessageBox::warning(this, "Input Error", "Please select at least one eclipse type (solar or lunar).");
        return;
    }

    statusBar()->showMessage(QString("Calculating eclipses for %1 to %2...")
                             .arg(fromDate.toString("yyyy-MM-dd"))
                             .arg(toDate.toString("yyyy-MM-dd")));

    QJsonArray eclipseData = m_chartDataManager.calculateEclipsesAsJson(
                fromDate, toDate, solarEclipses, lunarEclipses);

    if (m_chartDataManager.getLastError().isEmpty()) {
        displayRawEclipseData(eclipseData);
        QMessageBox::information(this, "Eclipse Data", "Eclipse data has been generated successfully.\n"
                                                       "Please navigate to 'Chart Details->Eclipses' tab tp view the data.");
    } else {
        handleError("Eclipse calculation error: " + m_chartDataManager.getLastError());
    }
}

void MainWindow::displayRawEclipseData(const QJsonArray &eclipseData)
{
    // Find the eclipse table by object name (set when creating the table)
    QTableWidget *eclipseTable = findChild<QTableWidget*>("Eclipses");
    if (!eclipseTable) return;

    eclipseTable->setRowCount(0); // Clear previous data

    for (const QJsonValue &val : eclipseData) {
        QJsonObject obj = val.toObject();
        int row = eclipseTable->rowCount();
        eclipseTable->insertRow(row);

        // Extract and set each field
        QString date = obj.value("date").toString();
        QString time = obj.value("time").toString();
        QString type = obj.value("type").toString();
        double magnitude = obj.value("magnitude").toDouble();
        double latitude = obj.value("latitude").toDouble();
        double longitude = obj.value("longitude").toDouble();

        eclipseTable->setItem(row, 0, new QTableWidgetItem(date));
        eclipseTable->setItem(row, 1, new QTableWidgetItem(time));
        eclipseTable->setItem(row, 2, new QTableWidgetItem(type));
        eclipseTable->setItem(row, 3, new QTableWidgetItem(QString::number(magnitude, 'f', 2)));
        eclipseTable->setItem(row, 4, new QTableWidgetItem(QString::number(latitude, 'f', 2)));
        eclipseTable->setItem(row, 5, new QTableWidgetItem(QString::number(longitude, 'f', 2)));
    }

    // Optional: sort by date column
    eclipseTable->sortItems(0);
}

