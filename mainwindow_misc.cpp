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
void MainWindow::showNewFeaturesDialog() {
    // Create the dialog only if it doesn't exist yet
    if (!m_showNewFeaturesDialog) {
        m_showNewFeaturesDialog = new QDialog(this);
        m_showNewFeaturesDialog->setWindowTitle("What's New!");
        m_showNewFeaturesDialog->setMinimumSize(650, 600);

        // Create layout
        QVBoxLayout *layout = new QVBoxLayout(m_showNewFeaturesDialog);

        // Create a text browser for rich text display
        QTextBrowser *textBrowser = new QTextBrowser(m_showNewFeaturesDialog);
        textBrowser->setOpenExternalLinks(true);

        // Set the new features content
        QString featuresText = R"(

<h1 style="color:#27ae60;">What's New in Asteria L Version 0.11</h1>
<p><i>September 23, 2026</i></p>

<ul>
<li><b>Aspect pattern detection</b> for natal charts (T-Square, Grand Cross, Grand Trine, Kite, Yod, Thor's Hammer, clusters, easy oppositions) in a new Aspect Patterns tab</li>
<li><b>Reworked natal AI reading:</b> uses the detected patterns; an integrated, less technical essay</li>
<li><b>OpenRouter setup guide</b> (including free models) in Help → How to Use</li>
<li>New AI model configs default to temperature 1.0 and 32768 max tokens</li>
</ul>

<h1 style="color:#27ae60;">What's New in Asteria L Version 0.10</h1>
<p><i>August 23, 2026</i></p>

<ul>
<li><b>Synastry</b> bi-wheel chart with interaspects and a dedicated AI reading</li>
<li><b>Draconic</b> bi-wheel chart with its own AI reading</li>
<li><b>Copy HTML</b> option in the interpretation context menu</li>
<li>"Waiting for AI reply" progress indicator; interpretations are kept when a new chart is generated</li>
</ul>

<h1 style="color:#27ae60;">What's New in Asteria L Version 0.9</h1>
<p><i>April 30, 2026</i></p>

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

<h1 style="color:#27ae60;">What’s New in Version 2.1.3</h1>
<p>
This update introduces a streamlined data management system with a dedicated documents directory, making your astrological charts and materials more accessible and easier to manage.
</p>

<h2>Dedicated Documents Directory</h2>
<p>
All charts and user data are now saved and loaded from <code>~/Documents/Asteria</code>, providing improved organization and accessibility outside of hidden application directories.
</p>

<h2>Data Migration Support</h2>
<p>
Your existing data can be easily migrated to the new location. To manually transfer your charts and materials, open a terminal and run:
</p>
<pre style="background: #f5f5f5; padding: 10px; border-radius: 5px; overflow-x: auto; margin: 10px 20px;">
cp -a ~/.var/app/io.github.alamahant.Asteria/data/Asteria/* ~/Documents/Asteria/
</pre>

<h1 style="color:#27ae60;">What’s New in Version 2.1.2</h1>
<p>
Welcome to the 2.1.2 update! This release brings platform upgrades, improved compatibility, and astrological refinements that enhance chart accuracy and visual clarity.
</p>

<h2>Platform & Runtime Upgrade</h2>
<p>
Updated to KDE Platform 6.9 runtime for enhanced stability and performance, ensuring smoother operation across all supported systems.
</p>

<h2>Qt 6.9 Compatibility</h2>
<p>
Optimized QTableWidget rendering and font handling for full compatibility with Qt 6.9, providing better text clarity and interface responsiveness.
</p>

<h2>Western Chart Convention</h2>
<p>
Chart rendering now places the Ascendant at the 9 o'clock position to align with standard Western astrological practices, ensuring traditional chart layout accuracy.
</p>

<h2>Zodiac Sign Coloring</h2>
<p>
Planet list widget now displays zodiac signs in their traditional corresponding colors, making sign identification more intuitive and visually consistent.
</p>

<h2>Code Improvements</h2>
<ul>
  <li>Various code polish and optimization throughout the application</li>
  <li>Enhanced stability and performance for smoother user experience</li>
  <li>General bug fixes and maintenance improvements</li>
</ul>


<h1 style="color:#27ae60;">What’s New in Version 2.1.1</h1>
<p>
Welcome to the 2.1.1 update! This release refines AI interpretations, adds new chart tagging, and introduces the Zodiac Sign Chart, making your astrological analysis more powerful and visually appealing than ever.
</p>

<h2>Enhanced AI Interpretations</h2>
<p>
All AI-generated markdown responses are now transformed into rich HTML, producing beautifully formatted, clear, and engaging interpretations—like reading a professional digital publication.
</p>

<h2>Chart Type Tagging</h2>
<p>
All charts (Natal, Relationship, Progression, Return, Zodiac Signs) now carry unique tags, allowing AI to interpret them accurately. <b>Exception:</b> Composite charts are excluded for correctness.
</p>

<h2>Zodiac Sign Chart</h2>
<p>
Generates magazine-style horoscopes for all 12 signs for any chosen date/time, providing concise but detailed narratives with practical advice.
</p>

<h2>Multi-Window & Drag-and-Drop</h2>
<ul>
  <li>Open multiple Asteria windows from File → New Window, either in the same instance or as a new instance.</li>
  <li>Drag charts between windows using Ctrl + Left Mouse Button to load all data into the target window’s input or interpretation docks.</li>
</ul>

<h2>View & Dock Enhancements</h2>
<ul>
  <li>View menu checkboxes: toggle Chart-Only view or Info Overlay visibility (hidden by default).</li>
  <li>Clear Interpretation button: clears both displayed text and stored previous interpretations.</li>
  <li>Chart Zoom in/out functionality via CTRL+mouse wheel.</li>
</ul>

<h1 style="color:#27ae60;">What’s New in Version 2.1.0</h1>
<p>
Welcome to the major <b>2.1.0</b> release of our astrology application! This update brings a wealth of new features, enhancements, and refinements designed to make your astrological work more powerful, flexible, and insightful than ever before.
</p>

<h2>Chart Rendering Overhaul</h2>
<p>
One of the most significant changes is a complete overhaul of the chart rendering system. All chart elements are now drawn in an <b>anticlockwise</b> direction, aligning with traditional astrological conventions and providing a more intuitive visual experience. We’d love to hear your feedback on this new approach!
</p>

<h2>Expanded Chart Types</h2>
<ul>
  <li><b>Natal Charts:</b> Your foundational birth chart, calculated for your exact birth details.</li>
  <li><b>(NEW)Return Charts for All Major Planets:</b> Generate return charts for the Sun, Moon, Mercury, Venus, Mars, Jupiter, Saturn, Uranus, Neptune, and Pluto—offering deeper insights into planetary cycles.</li>
  <li><b>(NEW)Secondary Progression Charts:</b> Explore the symbolic evolution of your natal chart over time with secondary progression charts.</li>
  <li><b>Composite &amp; Davison Charts:</b> Analyze relationship dynamics from multiple perspectives with these advanced chart types.</li>
</ul>

<h2>Predictive Astrology Enhancements</h2>
<ul>
  <li><b>Eclipse Calculations:</b> Easily compute both lunar and solar eclipses. Set your desired period in the <i>Predictive Astrology</i> group box and navigate to the <b>Details</b> tab’s <b>Eclipses</b> section to view upcoming eclipse events relevant to your chart.</li>
  <li><b>Transits Calculation:</b> Calculate planetary transits for any chart over a period of up to one year using a dedicated UI button. With all planets and a wide orb, the system can calculate over 100,000 transits in a single run!</li>
  <li><b>Advanced Transit Search Dialog:</b> Efficiently filter and analyze transits by date, transiting planet, aspect, natal planet, orb, and exclusion filters. Access this powerful tool from the <b>Tools</b> menu.</li>
</ul>

<h2>Calendar &amp; Date Range Features</h2>
<ul>
  <li><b>Julian/Gregorian Calendar Toggle:</b> For dates prior to October 15, 1582, switch between calendar systems using a simple checkbox in the <b>Settings</b> menu for historical accuracy.</li>
  <li><b>Date Range Expansion:</b> Chart and transit calculations now support years from 0001 to 3000 CE (excluding BCE years due to technical limitations).</li>
</ul>

<h2>Data &amp; Table Improvements</h2>
<ul>
  <li><b>Export Chart Data:</b> Export all chart data as text, saving the contents of every table in the <b>Chart Details</b> tab for easy sharing and analysis.</li>
  <li><b>Angles Table:</b> View the values of the four primary chart angles: <b>Ascendant (ASC)</b>, <b>Descendant (DESC)</b>, <b>Imum Coeli (IC)</b>, and <b>Medium Coeli (MC)</b>—always at your fingertips.</li>
</ul>

<h2>Aspect Calculations</h2>
<p>
Our aspect calculations now include the following, each with its astrological significance:
</p>
<table border="1" cellpadding="4" cellspacing="0" style="border-collapse:collapse;">
  <tr><th>Code</th><th>Name</th><th>Angle</th><th>Significance</th></tr>
  <tr><td>CON</td><td>Conjunction</td><td>0°</td><td>Fusion, new beginnings, powerful blending of energies</td></tr>
  <tr><td>OPP</td><td>Opposition</td><td>180°</td><td>Tension, awareness, polarity, need for balance</td></tr>
  <tr><td>SQR</td><td>Square</td><td>90°</td><td>Challenge, action, friction, growth through conflict</td></tr>
  <tr><td>TRI</td><td>Trine</td><td>120°</td><td>Harmony, ease, natural talent, supportive flow</td></tr>
  <tr><td>SEX</td><td>Sextile</td><td>60°</td><td>Opportunity, cooperation, creative potential</td></tr>
  <tr><td>QUI</td><td>Quincunx</td><td>150°</td><td>Adjustment, awkwardness, need for adaptation</td></tr>
  <tr><td>SSQ</td><td>Semi-square</td><td>45°</td><td>Minor friction, irritation, motivation to resolve small issues</td></tr>
  <tr><td>SSX</td><td>Semi-sextile</td><td>30°</td><td>Subtle opportunity, minor growth, gentle nudges</td></tr>
  <tr><td>SQQ</td><td>Sesquiquadrate</td><td>135°</td><td>Dynamic tension, stress, need for adjustment <b>(new in 2.1.0)</b></td></tr>
</table>

<h2>Other Improvements</h2>
<ul>
  <li><b>Chart Type Flag for Transits:</b> Each calculated transit now includes a chart type flag, also passed to the AI interpretation prompt for more targeted and context-aware readings.</li>
  <li><b>General Improvements &amp; Bug Fixes:</b> Numerous minor UI polishes, bug fixes, and code optimizations for a smoother user experience.</li>
</ul>

<p>
We hope you enjoy these new features and improvements. As always, your feedback is invaluable—let us know how these changes enhance your astrological journey!
</p>
        )";

        textBrowser->setHtml(featuresText);

        layout->addWidget(textBrowser);

        // Add a close button at the bottom
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *closeButton = new QPushButton("Close", m_showNewFeaturesDialog);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        layout->addLayout(buttonLayout);

        // Connect the close button
        connect(closeButton, &QPushButton::clicked, m_showNewFeaturesDialog, &QDialog::close);

        // Connect the dialog's finished signal to handle cleanup
        connect(m_showNewFeaturesDialog, &QDialog::finished, this, [this]() {
            m_showNewFeaturesDialog->deleteLater();
            m_showNewFeaturesDialog = nullptr;
        });
    }

    // Show and raise the dialog to bring it to the front
    m_showNewFeaturesDialog->show();
    m_showNewFeaturesDialog->raise();
    m_showNewFeaturesDialog->activateWindow();
}

void MainWindow::toggleChartOnlyView(bool chartOnly)
{
    if (chartOnly) {
        // Hide docks, but respect the overlay setting
        m_inputDock->hide();
        m_interpretationDock->hide();
        if (chartInfoOverlay) {
            chartInfoOverlay->setVisible(false);
        }
    } else {
        // Show docks, and restore overlay based on its setting
        m_inputDock->show();
        m_interpretationDock->show();
        if (chartInfoOverlay && m_chartCalculated) {
            chartInfoOverlay->setVisible(m_showInfoOverlay);
        }
    }
}


bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_chartView->viewport()) {
        bool ctrlPressed = (QApplication::keyboardModifiers() & Qt::ControlModifier);

        // Handle Ctrl+Mouse Wheel for zooming
        if (event->type() == QEvent::Wheel && ctrlPressed) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            QPoint numDegrees = wheelEvent->angleDelta() / 8;

            if (!numDegrees.isNull()) {
                // Get current scale to apply limits
                qreal currentScale = m_chartView->transform().m11();
                qreal zoomFactor = numDegrees.y() > 0 ? 1.2 : 0.8;
                qreal newScale = currentScale * zoomFactor;

                // Apply zoom limits (0.1x to 10x)
                if (newScale >= 0.1 && newScale <= 10.0) {
                    m_chartView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
                    m_chartView->scale(zoomFactor, zoomFactor);
                }
                return true;
            }
        }
        // Handle Ctrl+Left Drag for chart transfer
        else if (ctrlPressed) {
            if (event->type() == QEvent::MouseButtonPress) {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                if ((mouseEvent->buttons() & Qt::LeftButton)) {
                    m_dragStartPosition = mouseEvent->pos();
                    return true;
                }
            }
            else if (event->type() == QEvent::MouseMove) {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                if ((mouseEvent->buttons() & Qt::LeftButton)) {
                    if ((mouseEvent->pos() - m_dragStartPosition).manhattanLength() >=
                            QApplication::startDragDistance()) {
                        startChartDrag();
                        return true;
                    }
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}


void MainWindow::startChartDrag()
{
    if (!m_chartCalculated) return;

    // Create a JSON representation of the input data only
    QJsonObject inputData;
    inputData["birthDate"] = m_birthDateEdit->text();
    inputData["birthTime"] = m_birthTimeEdit->text();
    inputData["utcOffset"] = m_utcOffsetCombo->currentText();
    inputData["latitude"] = m_latitudeEdit->text();
    inputData["longitude"] = m_longitudeEdit->text();
    inputData["houseSystem"] = m_houseSystemCombo->currentText();
    inputData["useJulian"] = useJulianForPre1582Action->isChecked();

    // Include the chart type
    inputData["chartType"] = AsteriaGlobals::lastGeneratedChartType;

    inputData["chartData"] = m_currentChartData;

    // Create MIME data for drag
    QMimeData *mimeData = new QMimeData();
    QByteArray jsonData = QJsonDocument(inputData).toJson();
    mimeData->setData("application/x-astrological-input", jsonData);
    mimeData->setText("Astrological Chart Input Data");

    // Create drag object
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    // Use a simple icon instead of chart screenshot
    drag->setPixmap(QIcon::fromTheme("x-office-calendar").pixmap(32, 32));

    // Start drag
    drag->exec(Qt::CopyAction);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-astrological-input")) {
        event->acceptProposedAction();
    }
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-astrological-input")) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-astrological-input")) {
        QByteArray jsonData = event->mimeData()->data("application/x-astrological-input");
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        QJsonObject inputData = doc.object();

        importChartInputData(inputData);
        event->acceptProposedAction();
    }
}

void MainWindow::importChartInputData(const QJsonObject &inputData)
{
    // Populate input fields from dragged data (optional, for UI consistency)
    m_birthDateEdit->setText(inputData["birthDate"].toString());
    m_birthTimeEdit->setText(inputData["birthTime"].toString());
    m_utcOffsetCombo->setCurrentText(inputData["utcOffset"].toString());
    m_latitudeEdit->setText(inputData["latitude"].toString());
    m_longitudeEdit->setText(inputData["longitude"].toString());
    m_houseSystemCombo->setCurrentText(inputData["houseSystem"].toString());
    useJulianForPre1582Action->setChecked(inputData["useJulian"].toBool());

    // Set the chart type from the dragged data
    if (inputData.contains("chartType")) {
        AsteriaGlobals::lastGeneratedChartType = inputData["chartType"].toString();
    }

    // Always use the pre-calculated chart data - call displayChart directly!
    QJsonObject chartData = inputData["chartData"].toObject();
    displayChart(chartData);

    m_chartCalculated = true;
    m_currentChartData = chartData;
    statusBar()->showMessage("Chart imported via drag & drop", 3000);
    setWindowTitle("Asteria - " + AsteriaGlobals::lastGeneratedChartType + " Chart");

}

// tranform markdown to html for ai response
QString MainWindow::markdownToHtml(const QString &markdown)
{
    QString html = markdown;

    // Convert headers
    html.replace(QRegularExpression("^###### (.*)$", QRegularExpression::MultilineOption), "<h6>\\1</h6>");
    html.replace(QRegularExpression("^##### (.*)$", QRegularExpression::MultilineOption), "<h5>\\1</h5>");
    html.replace(QRegularExpression("^#### (.*)$", QRegularExpression::MultilineOption), "<h4>\\1</h4>");
    html.replace(QRegularExpression("^### (.*)$", QRegularExpression::MultilineOption), "<h3>\\1</h3>");
    html.replace(QRegularExpression("^## (.*)$", QRegularExpression::MultilineOption), "<h2>\\1</h2>");
    html.replace(QRegularExpression("^# (.*)$", QRegularExpression::MultilineOption), "<h1>\\1</h1>");

    // Convert bold (**text**)
    html.replace(QRegularExpression("\\*\\*(.*?)\\*\\*"), "<b>\\1</b>");

    // Convert italic (*text*)
    html.replace(QRegularExpression("\\*(.*?)\\*"), "<i>\\1</i>");

    // Convert bullet points

    // Handle • bullets (with optional whitespace)
    html.replace(QRegularExpression("^[\\s]*\\•[\\s]+(.*)$", QRegularExpression::MultilineOption), "• \\1<br>");

    // Handle - bullets (with optional whitespace)
    html.replace(QRegularExpression("^[\\s]*\\-[\\s]+(.*)$", QRegularExpression::MultilineOption), "• \\1<br>");

    // Handle * bullets (with optional whitespace)
    html.replace(QRegularExpression("^[\\s]*\\*[\\s]+(.*)$", QRegularExpression::MultilineOption), "• \\1<br>");

    // Convert horizontal rules (---, ***, ___) with optional spaces
    html.replace(QRegularExpression("^\\s*(---|\\*\\*\\*|___)\\s*$", QRegularExpression::MultilineOption), "<hr>");

    html.replace("\n", "<br>");

    return html;
}


void MainWindow::calculateZodiacSignsChart()
{
    // Show info dialog before proceeding
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
                this,
                "Zodiac Signs Chart",
                "The Zodiac Signs Chart looks like a Birth chart but provides general astrological interpretations "
                "for all 12 zodiac signs based on planetary positions at a chosen date, time, and location.\n\n"
                "This chart's AI interpretations are not personal birth readings but give magazine-style forecasts. "
                "Please make sure to populate the relevant date, time, and location fields before proceeding.\n\n"
                "Do you want to continue?",
                QMessageBox::Ok | QMessageBox::Cancel
                );

    if (reply == QMessageBox::Cancel) {
        return;
    }

    // Proceed like in calculateChart()
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

    m_chartRenderer->scene()->clear();

    // Calculate chart
    birthDate = checkAndConvertJulian(birthDate, useJulianForPre1582Action->isChecked());

    m_currentChartData = m_chartDataManager.calculateChartAsJson(
                birthDate, birthTime, utcOffset, latitude, longitude, houseSystem);

    if (m_chartDataManager.getLastError().isEmpty()) {
        // Display chart
        displayChart(m_currentChartData);
        m_chartCalculated = true;

        // Set default name fields
        first_name->setText("no");
        last_name->setText("name");

        // Set chart type for interpretation
        AsteriaGlobals::lastGeneratedChartType = "Zodiac Signs";

        m_getInterpretationButton->setEnabled(true);
        getPredictionButton->setEnabled(true);
        getTransitsButton->setEnabled(true);

        statusBar()->showMessage("Zodiac Signs chart calculated successfully", 3000);
    } else {
        handleError("Chart calculation error: " + m_chartDataManager.getLastError());
        m_chartCalculated = false;
        m_getInterpretationButton->setEnabled(false);
        getPredictionButton->setEnabled(false);

        // Clear any partial chart data after error
        m_chartRenderer->scene()->clear();
    }
}

void MainWindow::copySavePath()
{
#ifdef FLATHUB_BUILD
    QString dataDirPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/" + QApplication::applicationName();
#else
    QString dataDirPath = QDir::homePath() + "/" + QApplication::applicationName();

#endif

    QApplication::clipboard()->setText(dataDirPath);
    // Optional: Show a confirmation message
    QMessageBox::information(this, "Path Copied",
                             QString("Save location copied to clipboard:\n%1").arg(dataDirPath));
}

void MainWindow::configureAIModels()
{
    ModelSelectorDialog dlg(this);

    // Connect to activeModelChanged signal
    connect(&dlg, &ModelSelectorDialog::activeModelChanged, this, [this](const QString &modelName) {
        // When active model changes, reload it in MistralAPI
        // This will update AsteriaGlobals::activeModelLoaded internally
        m_mistralApi.loadActiveModel();

        // Optional: Show status message
        statusBar()->showMessage(tr("Active model changed to: %1").arg(modelName), 3000);
    });

    dlg.exec();  // Just show the dialog, no need to process results

    m_mistralApi.loadActiveModel();

    // The dialog saves changes to QSettings automatically
    // The MistralAPI class will read the active model from QSettings when needed
}