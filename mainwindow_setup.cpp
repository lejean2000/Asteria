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
void MainWindow::setupUi()
{
    // Setup central widget with tabs
    setupCentralWidget();
    // Setup dock widgets
    setupInputDock();
    setupInterpretationDock();
    // Setup menus
    setupMenus();
    // Setup signal/slot connections
    setupConnections();
    // Set dock widget sizes
    resizeDocks({m_inputDock, m_interpretationDock}, {250, 350}, Qt::Horizontal);

    this->setStyleSheet("QScrollBar:horizontal { height: 0px; background: transparent; }");
}

void MainWindow::setupCentralWidget() {
    // Create tab widget for central area
    m_centralTabWidget = new QTabWidget(this);

    // Create chart container widget with layout
    chartContainer = new QWidget(this);
    chartLayout = new QHBoxLayout(chartContainer);
    chartLayout->setContentsMargins(0, 0, 0, 0);  // Remove margins for better splitter experience

    // Create main horizontal splitter
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, chartContainer);
    mainSplitter->setObjectName("mainSplitter");
    mainSplitter->setHandleWidth(10);
    // Create chart view and renderer
    m_chartView = new QGraphicsView(mainSplitter);

    // drag drop
    m_chartView->setAcceptDrops(true);
    m_chartView->viewport()->installEventFilter(this);

    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_chartView->setOptimizationFlags(QGraphicsView::DontSavePainterState);
    m_chartView->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    m_chartView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // Create chart renderer
    m_chartRenderer = new ChartRenderer(this);
    m_chartRenderer->hide(); // scene-only use; m_chartView is the actual display widget
    m_chartView->setScene(m_chartRenderer->scene());


    // Create chart info overlay widget
    chartInfoOverlay = new QWidget(m_chartView);
    chartInfoOverlay->setGeometry(10, 10, 250, 160); // Adjusted position and height
    chartInfoOverlay->setStyleSheet("background-color: rgba(235, 225, 200, 0);"); // Completely transparent background
    QVBoxLayout *infoLayout = new QVBoxLayout(chartInfoOverlay);
    infoLayout->setContentsMargins(0, 2, 0, 2); // Set both left and right margins to 0
    infoLayout->setSpacing(4); // Keep original spacing


    // Create labels for chart information
    m_nameLabel = new QLabel("Name",chartInfoOverlay);
    m_surnameLabel = new QLabel("Surname",chartInfoOverlay);
    m_birthDateLabel = new QLabel("Birth Date",chartInfoOverlay);
    m_birthTimeLabel = new QLabel("Birth Time",chartInfoOverlay);
    m_locationLabel = new QLabel("Birth Place",chartInfoOverlay);
    m_sunSignLabel = new QLabel(chartInfoOverlay);
    m_ascendantLabel = new QLabel(chartInfoOverlay);
    m_housesystemLabel = new QLabel(chartInfoOverlay);
    // Add labels to layout
    infoLayout->addWidget(m_nameLabel);
    infoLayout->addWidget(m_surnameLabel);
    infoLayout->addWidget(m_birthDateLabel);
    infoLayout->addWidget(m_birthTimeLabel);
    infoLayout->addWidget(m_locationLabel);
    infoLayout->addWidget(m_sunSignLabel);
    infoLayout->addWidget(m_ascendantLabel);
    infoLayout->addWidget(m_housesystemLabel);

    // Add chart view to main splitter
    mainSplitter->addWidget(m_chartView);

    // Create right sidebar with vertical splitter
    QSplitter *sidebarSplitter = new QSplitter(Qt::Vertical, mainSplitter);

    //
    sidebarSplitter->setObjectName("sidebarSplitter");
    sidebarSplitter->setHandleWidth(10);
    sidebarSplitter->setStyleSheet( "QSplitter#sidebarSplitter::handle:vertical {" " background: rgba(120,120,120,0.5);" " margin: 0;" "}" "QSplitter#sidebarSplitter::handle:vertical:hover {" " background: rgba(90,90,90,0.8);" "}" "QSplitter#sidebarSplitter::handle:vertical:pressed {" " background: rgba(70,70,70,0.9);" "}" "QSplitter#sidebarSplitter::handle:vertical > * {" " background: transparent;" "}" );
    //

    // Create PlanetListWidget
    m_planetListWidget = new PlanetListWidget(sidebarSplitter);
    m_planetListWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    sidebarSplitter->addWidget(m_planetListWidget);

    // Create AspectarianWidget
    m_aspectarianWidget = new AspectarianWidget(sidebarSplitter);
    m_aspectarianWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    sidebarSplitter->addWidget(m_aspectarianWidget);

    // Create ModalityElementWidget
    m_modalityElementWidget = new ElementModalityWidget(sidebarSplitter);
    m_modalityElementWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);


    sidebarSplitter->addWidget(m_modalityElementWidget);

    // Set initial sizes for the sidebar splitter
    QList<int> sidebarSizes;
    sidebarSizes << 200 << 300 << 150;  // PlanetList: 200px, Aspectarian: 300px, ModalityElement: 150px
    sidebarSplitter->setSizes(sidebarSizes);


    // Add sidebar splitter to main splitter
    mainSplitter->addWidget(sidebarSplitter);

    // Set initial sizes for the main splitter (75% chart, 25% sidebar)
    QList<int> mainSizes;
    mainSizes << 75 << 25; // 75 << 25
    mainSplitter->setSizes(mainSizes);


    // Add main splitter to chart layout
    chartLayout->addWidget(mainSplitter);

    // Create chart details widget (table view of chart data)
    m_chartDetailsWidget = new QWidget(this);
    QVBoxLayout *detailsLayout = new QVBoxLayout(m_chartDetailsWidget);

    // Create tables for planets, houses, and aspects
    QTabWidget *detailsTabs = new QTabWidget(m_chartDetailsWidget);

    // Planets table
    QTableWidget *planetsTable = new QTableWidget(0, 4, detailsTabs);
    planetsTable->setObjectName("Planets");
    planetsTable->setHorizontalHeaderLabels({"Planet", "Sign", "Degree", "House"});
    planetsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Angles table
    QTableWidget *anglesTable = new QTableWidget(0, 3, detailsTabs);
    anglesTable->setObjectName("Angles");
    anglesTable->setHorizontalHeaderLabels({"Angle", "Sign", "Raw Degrees in Dec"});
    anglesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);


    // Houses table
    QTableWidget *housesTable = new QTableWidget(0, 3, detailsTabs);
    housesTable->setObjectName("Houses");
    housesTable->setHorizontalHeaderLabels({"House", "Sign", "Raw Degrees in Dec"});
    housesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Aspects table
    QTableWidget *aspectsTable = new QTableWidget(0, 4, detailsTabs);
    aspectsTable->setObjectName("Aspects");
    aspectsTable->setHorizontalHeaderLabels({"Planet 1", "Aspect", "Planet 2", "Orb"});
    aspectsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    //////////Prediction Data
    // Create a new tab for raw prediction data
    rawTransitTable = new QTableWidget(0, 4, detailsTabs);
    rawTransitTable->setObjectName("RawTransits");
    rawTransitTable->setHorizontalHeaderLabels({"Date", "Transit Planet", "Aspect", "Natal Planet (Orb)"});
    rawTransitTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);


    // Eclipse Data table
    QTableWidget *eclipseTable = new QTableWidget(0, 6, detailsTabs);
    eclipseTable->setObjectName("Eclipses");
    eclipseTable->setHorizontalHeaderLabels({"Date", "Time", "Type", "Magnitude", "Latitude", "Longitude"});
    eclipseTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    // make all tables copiable
    QList<QTableWidget*> tables = {planetsTable, anglesTable, housesTable, aspectsTable, rawTransitTable, eclipseTable};

    for (QTableWidget *table : tables) {
        table->setSelectionBehavior(QAbstractItemView::SelectItems);
        table->setSelectionMode(QAbstractItemView::ExtendedSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setContextMenuPolicy(Qt::ActionsContextMenu);

        QAction *copyAction = new QAction("Copy", table);
        copyAction->setShortcut(QKeySequence::Copy);
        table->addAction(copyAction);

        connect(copyAction, &QAction::triggered, [table]() {
            QItemSelectionModel *selection = table->selectionModel();
            QModelIndexList indexes = selection->selectedIndexes();

            if (indexes.isEmpty())
                return;

            // Sort indexes by row and column
            std::sort(indexes.begin(), indexes.end(), [](const QModelIndex &a, const QModelIndex &b) {
                if (a.row() == b.row())
                    return a.column() < b.column();
                return a.row() < b.row();
            });

            QString copiedText;
            int prevRow = indexes.first().row();
            for (int i = 0; i < indexes.size(); ++i) {
                const QModelIndex &index = indexes.at(i);
                if (i > 0) {
                    if (index.row() != prevRow) {
                        copiedText += '\n';
                        prevRow = index.row();
                    } else {
                        copiedText += '\t';
                    }
                }
                QTableWidgetItem *item = table->item(index.row(), index.column());
                copiedText += item ? item->text() : "";
            }

            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(copiedText);
        });
    }

    // Add tables to tabs
    detailsTabs->addTab(planetsTable, "Planets");
    detailsTabs->addTab(anglesTable, "Angles");
    detailsTabs->addTab(housesTable, "Houses");
    detailsTabs->addTab(aspectsTable, "Aspects");
    detailsTabs->addTab(rawTransitTable, "Raw Transit Data");
    detailsTabs->addTab(eclipseTable, "Eclipses");

    detailsLayout->addWidget(detailsTabs);

    // Add widgets to central tab widget
    m_centralTabWidget->addTab(chartContainer, "Chart Wheel");
    m_centralTabWidget->addTab(m_chartDetailsWidget, "Chart Details");

    setCentralWidget(m_centralTabWidget);
}

void MainWindow::setupInputDock() {
    // Create input dock widget
    QLabel* titleLabel = new QLabel("☉☽☿♀♂♃♄⛢♆♇");
    titleLabel->setFont(QFont(g_astroFontFamily, 16));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("background-color: palette(window); padding: 2px;");

    m_inputDock = new QDockWidget(this);
    m_inputDock->setObjectName("Birth Chart Input");
    m_inputDock->setWindowTitle("Birth Chart Input");
    m_inputDock->setTitleBarWidget(titleLabel);
    m_inputDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_inputDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

    QWidget *inputWidget = new QWidget(m_inputDock);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputWidget);

    // Birth information group
    QGroupBox *birthGroup = new QGroupBox("Birth Details", inputWidget);

    QFormLayout *birthLayout = new QFormLayout(birthGroup);

    // Date input as QLineEdit with regex validation
    m_birthDateEdit = new QLineEdit(birthGroup);
    m_birthDateEdit->setToolTip("To set new date, highlight and delete the existing date and set desired with proper format.\n"
                                "Allowed range 0001-3000. Allowed format 'dd/MM/yyyy' Year MUST be in four digit yyyy format.");

    m_birthDateEdit->setPlaceholderText("DD/MM/YYYY");
    // Create a validator for the date format. dateRegex is defined at the top of the class
    QValidator *dateValidator = new QRegularExpressionValidator(dateRegex, this);
    m_birthDateEdit->setValidator(dateValidator);

    // Set current date as default
    QDate currentDate = QDate::currentDate();
    m_birthDateEdit->setText(currentDate.toString("dd/MM/yyyy"));

    // Time input as QLineEdit with regex validation
    m_birthTimeEdit = new QLineEdit(birthGroup);
    m_birthTimeEdit->setToolTip("To set new time, highlight and delete the existing time and set desired with proper format.\n"
                                "Allowed format 'HH:mm'");

    m_birthTimeEdit->setPlaceholderText("HH:MM (24-hour format)");
    // Create a validator for the time format
    QRegularExpression timeRegex("^([01]\\d|2[0-3]):([0-5]\\d)$");
    QValidator *timeValidator = new QRegularExpressionValidator(timeRegex, this);
    m_birthTimeEdit->setValidator(timeValidator);
    // Set current time as default
    QTime currentTime = QTime::currentTime();
    m_birthTimeEdit->setText(currentTime.toString("HH:mm"));

    // Latitude input with regex validation
    m_latitudeEdit = new QLineEdit(birthGroup);
    m_latitudeEdit->setReadOnly(false);
    m_latitudeEdit->setToolTip("Please prefer the 'From Google' field or the 'Select on Map' button.");

    // Create a validator for latitude format: degrees(0-90) + N/S + minutes(0-59)

    // Longitude input with regex validation
    m_longitudeEdit = new QLineEdit(birthGroup);
    m_longitudeEdit->setReadOnly(false);
    m_longitudeEdit->setToolTip("Please prefer the 'From Google' field or the 'Select on Map' button.");

    // Create a validator for longitude format: degrees(0-180) + E/W + minutes(0-59)

    // Google coordinates input
    m_googleCoordsEdit = new QLineEdit(birthGroup);
    m_googleCoordsEdit->setPlaceholderText("e.g: 51.5072° N, 0.1276° W");
    m_googleCoordsEdit->setToolTip("Search for a location on Google, copy the coordinates, and paste them here");

    m_googleCoordsEdit->setStyleSheet(
                "QLineEdit {"
                "  background-color: #fff9c4;"  // soft yellow
                "  border: 2px solid #f9a825;"  // amber border
                "  border-radius: 5px;"
                "  padding: 4px;"
                "  font-weight: bold;"
                "}"
                );


    connect(m_googleCoordsEdit, &QLineEdit::textChanged, this, [=](const QString &text) {
        // Only try to parse if the text looks like it might be complete coordinates
        if (text.contains(',') &&
                (text.contains('N') || text.contains('n') || text.contains('S') || text.contains('s')) &&
                (text.contains('E') || text.contains('e') || text.contains('W') || text.contains('w'))) {

            // Remove all spaces to simplify parsing
            QString input = text;
            input.remove(' ');

            // Split into latitude and longitude parts
            int commaPos = input.indexOf(',');
            QString latPart = input.left(commaPos);
            QString longPart = input.mid(commaPos + 1);

            // Find the position of N/S in latitude
            int latDirPos = latPart.indexOf('N');
            if (latDirPos == -1) latDirPos = latPart.indexOf('n');
            if (latDirPos == -1) latDirPos = latPart.indexOf('S');
            if (latDirPos == -1) latDirPos = latPart.indexOf('s');

            // Find the position of E/W in longitude
            int longDirPos = longPart.indexOf('E');
            if (longDirPos == -1) longDirPos = longPart.indexOf('e');
            if (longDirPos == -1) longDirPos = longPart.indexOf('W');
            if (longDirPos == -1) longDirPos = longPart.indexOf('w');

            if (latDirPos != -1 && longDirPos != -1) {
                // Extract the numeric parts and direction indicators
                QString latNumStr = latPart.left(latDirPos).remove(QString::fromUtf8("°"));
                QString latDir = latPart.mid(latDirPos, 1).toUpper();
                QString longNumStr = longPart.left(longDirPos).remove(QString::fromUtf8("°"));
                QString longDir = longPart.mid(longDirPos, 1).toUpper();

                // Convert to double
                bool latOk, longOk;
                double latDegrees = latNumStr.toDouble(&latOk);
                double longDegrees = longNumStr.toDouble(&longOk);

                if (latOk && longOk) {
                    // For Swiss Ephemeris, we need decimal degrees with sign
                    // Negative for South latitude and West longitude
                    double latDecimal = latDegrees;
                    if (latDir == "S") latDecimal = -latDecimal;

                    double longDecimal = longDegrees;
                    if (longDir == "W") longDecimal = -longDecimal;

                    // Set the decimal coordinates directly
                    m_latitudeEdit->setText(QString::number(latDecimal, 'f', 6));
                    m_longitudeEdit->setText(QString::number(longDecimal, 'f', 6));

                    // Show a status message
                    statusBar()->showMessage("Coordinates converted successfully", 3000);
                }
            }
        }
    });

    // Google search Location coordinates
    locationSearchEdit = new QLineEdit(this);
    locationSearchEdit->setPlaceholderText("Enter location and press Enter to search coordinates");
    locationSearchEdit->setToolTip("Enter location, for example 'Athens Greece', and press Enter to search coordinates");

    // Connect Enter key press to the search function
    connect(locationSearchEdit, &QLineEdit::returnPressed, this, [this]() {
        searchLocationCoordinates(locationSearchEdit->text());
    });

    m_utcOffsetCombo = new QComboBox(birthGroup);

    // Create a list to hold all offsets
    QList<QPair<QString, double>> offsetsWithValues;

    // Add whole hour offsets
    for (int i = -12; i <= 14; i++) {
        QString offset = (i >= 0) ? QString("+%1:00").arg(i) : QString("%1:00").arg(i);
        double value = i;
        offsetsWithValues.append(qMakePair(offset, value));
    }

    // Add common half-hour and 45-minute offsets
    QMap<QString, double> specialOffsets = {
        {"-9:30", -9.5},   // Marquesas Islands
        {"-3:30", -3.5},   // Newfoundland, Canada
        {"+3:30", 3.5},    // Iran
        {"+4:30", 4.5},    // Afghanistan
        {"+5:30", 5.5},    // India, Sri Lanka
        {"+5:45", 5.75},   // Nepal
        {"+6:30", 6.5},    // Myanmar, Cocos Islands
        {"+8:45", 8.75},   // Western Australia (Eucla)
        {"+9:30", 9.5},    // South Australia, Northern Territory (Australia)
        {"+10:30", 10.5},  // Lord Howe Island (Australia)
        {"+12:45", 12.75}  // Chatham Islands (New Zealand)
    };

    // Add special offsets to the list
    for (auto it = specialOffsets.begin(); it != specialOffsets.end(); ++it) {
        offsetsWithValues.append(qMakePair(it.key(), it.value()));
    }

    // Sort by numeric value
    std::sort(offsetsWithValues.begin(), offsetsWithValues.end(),
              [](const QPair<QString, double>& a, const QPair<QString, double>& b) {
        return a.second < b.second;
    });

    // Add sorted items to combo box
    for (const auto& pair : offsetsWithValues) {
        m_utcOffsetCombo->addItem(pair.first);
    }

    m_utcOffsetCombo->setCurrentText("+00:00");
    m_utcOffsetCombo->setToolTip("Select the UTC offset for the birth location.\n"
                                 "Remember to account for Daylight Saving Time if applicable.\n"
                                 "For accurate charts, you need to determine if DST was in effect\n"
                                 "at the time and location of birth.");


    // House system combo
    m_houseSystemCombo = new QComboBox(birthGroup);
    m_houseSystemCombo->addItems({"Placidus", "Koch", "Porphyrius", "Regiomontanus", "Campanus", "Equal", "Whole Sign"});

    // Add widgets to form layout
    first_name = new QLineEdit(birthGroup);
    first_name->setPlaceholderText("optional");
    last_name = new QLineEdit(birthGroup);
    last_name->setPlaceholderText("optional");
    birthLayout->addRow("First Name:", first_name);
    birthLayout->addRow("Last Name:", last_name);
    birthLayout->addRow("Birth Date:", m_birthDateEdit);
    birthLayout->addRow("Birth Time:", m_birthTimeEdit);
    birthLayout->addRow("Latitude:", m_latitudeEdit);
    birthLayout->addRow("Longitude:", m_longitudeEdit);
    birthLayout->addRow("Paste from Google:", m_googleCoordsEdit);
    birthLayout->addRow("Search Google",locationSearchEdit);


    m_selectLocationButton = new QPushButton("Select on Map", birthGroup);
    m_selectLocationButton->setIcon(QIcon::fromTheme("view-refresh"));
    connect(m_selectLocationButton, &QPushButton::clicked, this, &MainWindow::onOpenMapClicked);

    // Set fixed size policy to match QLineEdit width
    m_selectLocationButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Force the button to take the full available width
    m_selectLocationButton->setMinimumWidth(200);  // Set a reasonable minimum width

    // Add to form layout
    birthLayout->addRow("Location:", m_selectLocationButton);

    // After all widgets are added to the layout and the form is shown,
    // you might need to call this in the showEvent or after setup:
    m_selectLocationButton->setMinimumWidth(locationSearchEdit->width());


    birthLayout->addRow("UTC Offset:", m_utcOffsetCombo);
    birthLayout->addRow("House System:", m_houseSystemCombo);
    QWidget *orbContainer = new QWidget(inputWidget);
    QVBoxLayout *orbLayout = new QVBoxLayout(orbContainer);
    orbLayout->setContentsMargins(0, 0, 0, 0);

    // Create a horizontal layout for the slider and value label
    QHBoxLayout *sliderLayout = new QHBoxLayout();

    // Create the slider
    QSlider *orbSlider = new QSlider(Qt::Horizontal, orbContainer);
    orbSlider->setRange(0, 24);  // 0 to 12 in 0.5° increments (multiply by 2)
    orbSlider->setValue(static_cast<int>(getOrbMax() * 2)); // Convert current value to slider range
    orbSlider->setTickInterval(4); // Tick marks every 2° (multiply by 2)
    orbSlider->setTickPosition(QSlider::TicksBelow);
    orbSlider->setMinimumWidth(150);

    // Create value label
    QLabel *orbValueLabel = new QLabel(QString::number(getOrbMax(), 'f', 1) + "°", orbContainer);
    orbValueLabel->setMinimumWidth(40);
    orbValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Add slider and value label to the horizontal layout
    sliderLayout->addWidget(orbSlider);
    sliderLayout->addWidget(orbValueLabel);

    // Create a label for the description
    QLabel *orbDescriptionLabel = new QLabel(getOrbDescription(getOrbMax()), orbContainer);
    orbDescriptionLabel->setAlignment(Qt::AlignCenter);

    // Add both layouts to the container
    orbLayout->addLayout(sliderLayout);
    orbLayout->addWidget(orbDescriptionLabel);

    // Connect slider value changes
    connect(orbSlider, &QSlider::valueChanged, [=](int value) {
        double orb = value / 2.0;
        orbValueLabel->setText(QString::number(orb, 'f', 1) + "°");
        orbDescriptionLabel->setText(getOrbDescription(orb));
        setOrbMax(orb); // Update the global setting
    });

    // Add the container to the form layout
    birthLayout->addRow("Aspect Orbs:", orbContainer);


    m_additionalBodiesCB = new QCheckBox("Include Additional Bodies", this);
    m_additionalBodiesCB->setToolTip("Include Lilith, Ceres, Pallas, Juno, Vesta, Vertex, East Point and Part of Spirit");
    connect(m_additionalBodiesCB, &QCheckBox::toggled, this, [this](bool checked) {

        AsteriaGlobals::additionalBodiesEnabled = checked;

        if (m_chartCalculated) {
            if (!m_currentNatalChartData.isEmpty()) {
                // Bi-wheel chart (Secondary Progression / Draconic / Synastry) -
                // displayChart() only knows how to render a single wheel.
                refreshDualWheelDisplay();
            } else {
                displayChart(m_currentChartData);
            }
        }

    });
    birthLayout->addRow(m_additionalBodiesCB);

    // Calculate button
    m_calculateButton = new QPushButton("Calculate Natal Chart", inputWidget);
    m_calculateButton->setIcon(QIcon::fromTheme("view-refresh"));


    // Add Predictive Astrology section
    QGroupBox *predictiveGroup = new QGroupBox("Predictive Astrology", inputWidget);
    QFormLayout *predictiveLayout = new QFormLayout(predictiveGroup);

    // From date input
    m_predictiveFromEdit = new QLineEdit(predictiveGroup);
    m_predictiveFromEdit->setPlaceholderText("DD/MM/YYYY");
    m_predictiveFromEdit->setToolTip("To set new date, highlight and delete the existing date and set desired with proper format");

    m_predictiveFromEdit->setValidator(dateValidator); // Reuse the same validator
    // Set current date as default
    m_predictiveFromEdit->setText(currentDate.toString("dd/MM/yyyy"));

    // To date input
    m_predictiveToEdit = new QLineEdit(predictiveGroup);
    m_predictiveToEdit->setPlaceholderText("DD/MM/YYYY");
    m_predictiveToEdit->setToolTip("To set new date, highlight and delete the existing date and set desired with proper format");

    m_predictiveToEdit->setValidator(dateValidator); // Reuse the same validator
    // Set default to current date + 30 days
    QDate defaultFutureDate = currentDate.addDays(1); // Just a default starting point
    m_predictiveFromEdit->setText(currentDate.toString("dd/MM/yyyy"));
    m_predictiveToEdit->setText(defaultFutureDate.toString("dd/MM/yyyy"));

    // Add to form layout
    predictiveLayout->addRow("From:", m_predictiveFromEdit);
    predictiveLayout->addRow("Up to:", m_predictiveToEdit);
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    // Prediction Button
    getPredictionButton = new QPushButton("Get AI Prediction", predictiveGroup);
    getPredictionButton->setToolTip("This operation generates a huge ammount of data that is sent to AI for interpretation.\n"
                                    "Therefore it may be costly tokenwise.\n To mitigate this please reduce the number of days and/or use smaller orb.");
    getPredictionButton->setEnabled(false);
    getPredictionButton->setIcon(QIcon::fromTheme("view-refresh"));
    getPredictionButton->setStatusTip("The AI prediction will be appended at the end of any existing text. Scroll down and be patient!");

    // Transit Chart Button
    getTransitsButton = new QPushButton("Calculate Transits", predictiveGroup);
    getTransitsButton->setToolTip("Calculate transits for the selected period");
    getTransitsButton->setEnabled(false);
    getTransitsButton->setIcon(QIcon::fromTheme("view-chart"));
    getTransitsButton->setStatusTip("Calculate transits for the selected period");

    // Add buttons to horizontal layout
    buttonLayout->addWidget(getPredictionButton);
    buttonLayout->addWidget(getTransitsButton);

    // Add the button layout to the form
    predictiveLayout->addRow(buttonLayout);

    // Connect the new button
    connect(getTransitsButton, &QPushButton::clicked, this, &MainWindow::CalculateTransits);


    // Indeterminate progress bar shown while waiting on a (potentially slow) AI
    // reply - chart interpretation or AI prediction. Hidden the rest of the time.
    m_aiWaitProgressBar = new QProgressBar(inputWidget);
    m_aiWaitProgressBar->setRange(0, 0); // indeterminate/busy
    m_aiWaitProgressBar->setTextVisible(true);
    m_aiWaitProgressBar->setFormat("Waiting for AI reply...");
    m_aiWaitProgressBar->setToolTip("Waiting for AI reply...");
    m_aiWaitProgressBar->hide();

    // Add widgets to main layout
    inputLayout->addWidget(birthGroup);
    inputLayout->addWidget(m_calculateButton);
    inputLayout->addWidget(predictiveGroup);
    inputLayout->addWidget(m_aiWaitProgressBar);
    inputLayout->addStretch();

    // Set widget as dock content
    m_inputDock->setWidget(inputWidget);
    addDockWidget(Qt::LeftDockWidgetArea, m_inputDock);
}


void MainWindow::setupInterpretationDock() {
    // Create interpretation dock widget
    m_interpretationDock = new QDockWidget("Chart Interpretation", this);
    m_interpretationDock->setObjectName("Chart Interpretation");
    m_interpretationDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_interpretationDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

    QWidget *interpretationWidget = new QWidget(m_interpretationDock);
    QVBoxLayout *interpretationLayout = new QVBoxLayout(interpretationWidget);

    // Get interpretation button
    m_getInterpretationButton = new QPushButton("Get Chart Interpretation From AI", interpretationWidget);
    m_getInterpretationButton->setIcon(QIcon::fromTheme("system-search"));
    m_getInterpretationButton->setEnabled(false);

    // Interpretation scroll area with collapsible cards
    m_interpretationScrollArea = new QScrollArea(interpretationWidget);
    m_interpretationScrollArea->setWidgetResizable(true);
    m_interpretationScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_interpretationContainer = new QWidget();
    m_interpretationLayout = new QVBoxLayout(m_interpretationContainer);
    m_interpretationLayout->setAlignment(Qt::AlignTop);
    m_interpretationLayout->setContentsMargins(4, 4, 4, 4);
    m_interpretationLayout->setSpacing(6);
    m_interpretationScrollArea->setWidget(m_interpretationContainer);

    // Add Language Button
    QHBoxLayout* languageLayout = new QHBoxLayout();
    languageComboBox = new QComboBox(interpretationWidget);
    languageComboBox->setToolTip("Select AI Response Language");
    languageComboBox->addItem("English");
    languageComboBox->addItem("Spanish");
    languageComboBox->addItem("French");
    languageComboBox->addItem("German");
    languageComboBox->addItem("Italian");
    languageComboBox->addItem("Russian");
    languageComboBox->addItem("Greek");
    languageComboBox->addItem("Portuguese");
    languageComboBox->addItem("Hindi");
    languageComboBox->addItem("Chinese (Simplified)");
    languageComboBox->addItem("Bulgarian");
    languageComboBox->addItem("Arabic");
    languageComboBox->setCurrentIndex(0);
    languageComboBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    QPushButton *clearTextButton = new QPushButton("ClearText", this);
    clearTextButton->setToolTip("Clear AI Interpretation Text Area");
    clearTextButton->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    // Connect using a lambda
    connect(clearTextButton, &QPushButton::clicked, this, [this]() {
        m_interpretations = QJsonArray();
        renderAllInterpretations();
    });

    languageLayout->addWidget(languageComboBox);

    // Add widgets to layout
    interpretationLayout->addWidget(m_getInterpretationButton);
    interpretationLayout->addWidget(m_interpretationScrollArea);
    interpretationLayout->addLayout(languageLayout);
    interpretationLayout->addWidget(clearTextButton);

    // Set widget as dock content
    m_interpretationDock->setWidget(interpretationWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_interpretationDock);
}


void MainWindow::setupMenus()
{

    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    // Open new app window

    // New Window action
    QAction *newWindowAction = fileMenu->addAction("New &Window in New Process");
    newWindowAction->setShortcut(QKeySequence("Ctrl+Shift+N"));
    newWindowAction->setIcon(QIcon::fromTheme("window-new"));
    newWindowAction->setStatusTip("Open a new application window");

    // Connect using lambda
    connect(newWindowAction, &QAction::triggered, this, [this]() {
        // Create and show a new MainWindow instance
        QProcess::startDetached(QApplication::applicationFilePath(), QStringList());
    });

    fileMenu->addSeparator();


    // open window in current process
    QAction *newSameProcessAction = fileMenu->addAction("New &Window");
    newSameProcessAction->setShortcut(QKeySequence("Ctrl+Shift+W"));
    newSameProcessAction->setIcon(QIcon::fromTheme("window-new"));
    newSameProcessAction->setStatusTip("Open a new window within current application");

    connect(newSameProcessAction, &QAction::triggered, this, [this]() {
        MainWindow *newWindow = new MainWindow();
        newWindow->show();
    });

    fileMenu->addSeparator();

    QAction *openChartInNewWindowAction = fileMenu->addAction("Open Chart in New &Window");
    openChartInNewWindowAction->setShortcut(QKeySequence("Ctrl+Alt+O"));
    openChartInNewWindowAction->setIcon(QIcon::fromTheme("window-duplicate"));
    openChartInNewWindowAction->setStatusTip("Open current chart with all data in a new window");

    connect(openChartInNewWindowAction, &QAction::triggered, this, [this]() {
        if (!m_chartCalculated) {
            QMessageBox::information(this, "No Chart", "Please calculate a chart first.");
            return;
        }

        // Create the same data structure as drag operation
        QJsonObject chartData;
        chartData["birthDate"] = m_birthDateEdit->text();
        chartData["birthTime"] = m_birthTimeEdit->text();
        chartData["utcOffset"] = m_utcOffsetCombo->currentText();
        chartData["latitude"] = m_latitudeEdit->text();
        chartData["longitude"] = m_longitudeEdit->text();
        chartData["houseSystem"] = m_houseSystemCombo->currentText();
        chartData["useJulian"] = useJulianForPre1582Action->isChecked();
        chartData["chartType"] = AsteriaGlobals::lastGeneratedChartType;

        chartData["chartData"] = m_currentChartData;

        // Create new window and import the data
        MainWindow *newWindow = new MainWindow();
        newWindow->importChartInputData(chartData);
        newWindow->show();

        statusBar()->showMessage("Chart opened in new window", 3000);
    });
    fileMenu->addSeparator();

    // New/Open/Save group
    QAction *newAction = fileMenu->addAction("&New Chart", this, &MainWindow::newChart);
    newAction->setShortcut(QKeySequence::New);
    newAction->setIcon(QIcon::fromTheme("document-new"));

    QAction *openAction = fileMenu->addAction("&Open Chart...", this, &MainWindow::loadChart);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setIcon(QIcon::fromTheme("document-open"));

    QAction *saveAction = fileMenu->addAction("&Save Chart...", this, &MainWindow::saveChart);
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setIcon(QIcon::fromTheme("document-save"));

    fileMenu->addSeparator();

    // Export group
    QAction *exportChartAction = fileMenu->addAction("Export Chart as &Image...", this, &MainWindow::exportChartImage);
    exportChartAction->setIcon(QIcon::fromTheme("image-x-generic"));

    QAction *exportSvgAction = fileMenu->addAction("Export as &SVG...", this, &MainWindow::exportAsSvg);
    exportSvgAction->setIcon(QIcon::fromTheme("image-svg+xml"));

    QAction *exportPdfAction = fileMenu->addAction("Export as &PDF...", this, &MainWindow::exportAsPdf);
    exportPdfAction->setIcon(QIcon::fromTheme("application-pdf"));

    QAction *exportTextAction = fileMenu->addAction("Export &Interpretation as Text...", this, &MainWindow::exportInterpretation);
    exportTextAction->setIcon(QIcon::fromTheme("text-x-generic"));

    QAction *exportDataAction = fileMenu->addAction("Export Chart &Data as Text...", this, &MainWindow::exportChartData);
    exportDataAction->setIcon(QIcon::fromTheme("text-x-generic"));
    fileMenu->addSeparator();

    QAction *getDataDirAction = fileMenu->addAction("Copy Save Location...", this, &MainWindow::copySavePath);
    getDataDirAction->setShortcut(QKeySequence("Ctrl+C"));
    getDataDirAction->setIcon(QIcon::fromTheme("edit-copy"));

    fileMenu->addSeparator();


    // Print group
    QAction *printAction = fileMenu->addAction("&Print...", this, &MainWindow::printChart);
    printAction->setShortcut(QKeySequence::Print);
    printAction->setIcon(QIcon::fromTheme("document-print"));

    fileMenu->addSeparator();

    // Exit
    QAction *exitAction = fileMenu->addAction("E&xit", this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setIcon(QIcon::fromTheme("application-exit"));

    // View menu
    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_inputDock->toggleViewAction());
    viewMenu->addAction(m_interpretationDock->toggleViewAction());

    // Add "Chart Only" toggle action
    m_chartOnlyAction = new QAction("&View Chart Only", this);
    m_chartOnlyAction->setCheckable(true);
    m_chartOnlyAction->setChecked(false); // Default to not checked
    connect(m_chartOnlyAction, &QAction::toggled, this, &MainWindow::toggleChartOnlyView);
    // show infooverlay action
    viewMenu->addAction(m_chartOnlyAction);

    showOverlayAction = new QAction("Show &Info Overlay", this);
    showOverlayAction->setCheckable(true);
    showOverlayAction->setChecked(false); // Default to not checked (hidden)

    // Connect with lambda that handles both UI and settings
    connect(showOverlayAction, &QAction::toggled, this, [this](bool checked) {
        m_showInfoOverlay = checked;
        if(chartInfoOverlay && m_chartCalculated)
            chartInfoOverlay->setVisible(checked);
    });

    viewMenu->addAction(showOverlayAction);


    // Settings menu
    QMenu *settingsMenu = menuBar()->addMenu("&Settings");


    QAction *aiModelsAction = settingsMenu->addAction("Configure AI &Models...", this, &MainWindow::configureAIModels);


    QAction *checkModelAction = settingsMenu->addAction("Check AI Model &Status", this, [this]() {
        if (!AsteriaGlobals::activeModelLoaded) {
            m_mistralApi.loadActiveModel();
            if (!AsteriaGlobals::activeModelLoaded) {

                QMessageBox msgBox(this);
                msgBox.setWindowTitle("AI Model Not Configured");
                msgBox.setText("No active AI model found. You need to configure a model to get chart interpretations.");
                msgBox.setInformativeText("Would you like to configure one now?\n\n"
                                          "Note: If you've been using Mistral, you can add it as a provider with your API key.");

                QPushButton *configureButton = msgBox.addButton("Configure Models", QMessageBox::ActionRole);
                QPushButton *closeButton = msgBox.addButton(QMessageBox::Close);

                msgBox.exec();

                if (msgBox.clickedButton() == configureButton) {
                    configureAIModels();
                }
            }
        } else {
            // Load the active model settings
            QSettings settings;
            settings.beginGroup("Models");
            QString activeModelName = settings.value("ActiveModel").toString();
            settings.beginGroup(activeModelName);

            QString provider = settings.value("provider").toString();
            QString endpoint = settings.value("endpoint").toString();
            QString modelName = settings.value("modelName").toString();
            QString apiKey = settings.value("apiKey").toString();
            double temperature = settings.value("temperature", 0.7).toDouble();
            int maxTokens = settings.value("maxTokens", 8192).toInt();

            settings.endGroup();
            settings.endGroup();

            // Build status message
            QString statusMessage = QString(
                        "<b>Active Model:</b> %1<br><br>"
                        "<b>Provider:</b> %2<br>"
                        "<b>Model:</b> %3<br>"
                        "<b>Endpoint:</b> %4<br>"
                        "<b>Temperature:</b> %5<br>"
                        "<b>Max Tokens:</b> %6<br>"
                        "<b>API Key:</b> %7"
                        ).arg(activeModelName)
                    .arg(provider)
                    .arg(modelName)
                    .arg(endpoint)
                    .arg(temperature)
                    .arg(maxTokens)
                    .arg(apiKey.isEmpty() ? "<font color='red'><b>MISSING</b></font>" : "<font color='green'><b>Configured</b></font>");

            // Check if API key is missing for cloud providers (not local)
            if (apiKey.isEmpty() && !endpoint.contains("localhost") && !endpoint.contains("127.0.0.1")) {
                statusMessage += "<br><br><font color='red'><b>WARNING:</b> This appears to be a cloud provider but no API key is set. Interpretations will fail.</font>";
            }

            QMessageBox::information(this, "AI Model Status", statusMessage);
        }
    });
    checkModelAction->setIcon(QIcon::fromTheme("dialog-information"));

    // Create an action for aspect settings
    QAction *aspectSettingsAction = new QAction("&Aspect Display Settings...", this);
    // Connect the action to a slot that will open the dialog
    connect(aspectSettingsAction, &QAction::triggered, this, &MainWindow::showAspectSettings);
    // Add the action to the settings menu
    settingsMenu->addAction(aspectSettingsAction);
    //

    // Add the Julian/Gregorian checkbox
    useJulianForPre1582Action = new QAction(tr("Use Julian calendar for dates before 15 October 1582"), this);
    useJulianForPre1582Action->setCheckable(true);
    QSettings settings;
    if (settings.contains("useJulianForPre1582")) {
        useJulianForPre1582Action->setChecked(settings.value("useJulianForPre1582").toBool());
    } else {
        useJulianForPre1582Action->setChecked(true); // Default: checked (use Julian for pre-1582)
    }
    settingsMenu->addAction(useJulianForPre1582Action);

    // Persist state changes to QSettings
    connect(useJulianForPre1582Action, &QAction::toggled, this, [this](bool checked) {
        qDebug() << "checked";
        QSettings settings;
        settings.setValue("useJulianForPre1582", checked);
    });
    // Create Tools menu
    QMenu *toolsMenu = menuBar()->addMenu("Tools");

    QAction *natalChartAction = new QAction("Calculate Natal Chart", this);
    natalChartAction->setToolTip("Calculate the natal chart for the current birth data");
    natalChartAction->setStatusTip("Calculate the natal chart for the current birth data");
    connect(natalChartAction, &QAction::triggered, this, &MainWindow::calculateChart);
    toolsMenu->addAction(natalChartAction);
    toolsMenu->addSeparator();

    // Create Relationship Charts submenu
    QMenu *relationshipMenu = toolsMenu->addMenu("Relationship Charts");

    // Create actions for relationship chart types
    QAction *compositeAction = new QAction("Composite Chart (exp)", this);
    QAction *davisonAction = new QAction("Davison Relationship Chart", this);
    QAction *synastryAction = new QAction("Synastry Chart", this);

    // Add actions to the relationship menu
    relationshipMenu->addAction(compositeAction);
    relationshipMenu->addAction(davisonAction);
    relationshipMenu->addAction(synastryAction);

    // Connect actions to slots
    connect(compositeAction, &QAction::triggered, this, &MainWindow::createCompositeChart);
    connect(davisonAction, &QAction::triggered, this, &MainWindow::createDavisonChart);
    connect(synastryAction, &QAction::triggered, this, &MainWindow::createSynastryChart);


    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *aboutAction = helpMenu->addAction("&About...", this, &MainWindow::showAboutDialog);
    aboutAction->setIcon(QIcon::fromTheme("help-about"));

    QAction *symbolsAction = helpMenu->addAction(tr("Astrological &Symbols"));
    connect(symbolsAction, &QAction::triggered, this, &MainWindow::showSymbolsDialog);

    QAction *howToUseAction = helpMenu->addAction(tr("&How to Use"));
    connect(howToUseAction, &QAction::triggered, this, &MainWindow::showHowToUseDialog);

    QAction *relationshipChartsAction = helpMenu->addAction(tr("About &Relationship Charts"));
    connect(relationshipChartsAction, &QAction::triggered, this, &MainWindow::showRelationshipChartsDialog);

    QAction *changelogAction = helpMenu->addAction(tr("Changelog"));
    connect(changelogAction, &QAction::triggered, this, &MainWindow::showChangelog);


    QAction *newFeaturesAction = helpMenu->addAction(tr("What's New!"));
    connect(newFeaturesAction, &QAction::triggered, this, &MainWindow::showNewFeaturesDialog);

    QAction *supportAction = helpMenu->addAction(tr("Support Us"));
    connect(supportAction, &QAction::triggered, this, [this]() {
        DonationDialog dialog(this);
        dialog.exec();
    });

    QAction *transitFilterAction = new QAction("Transit Filter", this);
    transitFilterAction->setToolTip("Filter transit data by date, planets and aspects");
    transitFilterAction->setStatusTip("Open transit filter dialog");
    transitFilterAction->setShortcut(QKeySequence("Ctrl+T"));
    connect(transitFilterAction, &QAction::triggered, this, &MainWindow::openTransitFilter);
    toolsMenu->addAction(transitFilterAction);

    QAction *eclipseCalcAction = new QAction("Calculate Eclipses", this);
    eclipseCalcAction->setToolTip("Calculate solar and lunar eclipses in the selected date range");
    eclipseCalcAction->setStatusTip("Calculate eclipses for the current chart and date range");
    eclipseCalcAction->setShortcut(QKeySequence("Ctrl+Shift+E"));

    connect(eclipseCalcAction, &QAction::triggered, this, &MainWindow::CalculateEclipses);

    toolsMenu->addAction(eclipseCalcAction);

    // Add Return Charts submenu
    QMenu *returnChartsMenu = toolsMenu->addMenu(tr("Return Charts"));

    QAction *solarReturnCalcAction = new QAction("Calculate Solar Return", this);
    solarReturnCalcAction->setToolTip("Calculate the solar return chart for a selected year");
    solarReturnCalcAction->setStatusTip("Calculate the solar return chart for the current birth data and chosen year");
    solarReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+H"));

    connect(solarReturnCalcAction, &QAction::triggered, this, &MainWindow::calculateSolarReturn);

    returnChartsMenu->addAction(solarReturnCalcAction);

    QAction *lunarReturnCalcAction = new QAction("Calculate Lunar Return", this);
    lunarReturnCalcAction->setToolTip("Calculate the lunar return chart for a selected month and year");
    lunarReturnCalcAction->setStatusTip("Calculate the lunar return chart for the current birth data and chosen month/year");
    lunarReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+L"));

    connect(lunarReturnCalcAction, &QAction::triggered, this, &MainWindow::calculateLunarReturn);

    returnChartsMenu->addAction(lunarReturnCalcAction);

    QAction *saturnReturnCalcAction = new QAction("Calculate Saturn Return", this);
    saturnReturnCalcAction->setToolTip("Calculate the Saturn return chart for a selected return number");
    saturnReturnCalcAction->setStatusTip("Calculate the Saturn return chart for the current birth data and chosen return number");
    saturnReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+S")); // Choose a shortcut that doesn't conflict
    connect(saturnReturnCalcAction, &QAction::triggered, this, &MainWindow::calculateSaturnReturn);

    returnChartsMenu->addAction(saturnReturnCalcAction);

    QAction *jupiterReturnCalcAction = new QAction("Calculate Jupiter Return", this);
    jupiterReturnCalcAction->setToolTip("Calculate the Jupiter return chart for a selected return number");
    jupiterReturnCalcAction->setStatusTip("Calculate the Jupiter return chart for the current birth data and chosen return number");
    jupiterReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+J"));
    connect(jupiterReturnCalcAction, &QAction::triggered, this, &MainWindow::calculateJupiterReturn);
    returnChartsMenu->addAction(jupiterReturnCalcAction);

    // Venus Return
    QAction *venusReturnCalcAction = new QAction("Calculate Venus Return", this);
    venusReturnCalcAction->setToolTip("Calculate the Venus return chart for a selected return number");
    venusReturnCalcAction->setStatusTip("Calculate the Venus return chart for the current birth data and chosen return number");
    venusReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+V"));
    connect(venusReturnCalcAction, &QAction::triggered, this, &MainWindow::calculateVenusReturn);
    returnChartsMenu->addAction(venusReturnCalcAction);

    // Mars Return
    QAction *marsReturnCalcAction = new QAction("Calculate Mars Return", this);
    marsReturnCalcAction->setToolTip("Calculate the Mars return chart for a selected return number");
    marsReturnCalcAction->setStatusTip("Calculate the Mars return chart for the current birth data and chosen return number");
    marsReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+R"));
    connect(marsReturnCalcAction, &QAction::triggered, this, &MainWindow::calculateMarsReturn);
    returnChartsMenu->addAction(marsReturnCalcAction);

    // Mercury Return
    QAction *mercuryReturnCalcAction = new QAction("Calculate Mercury Return", this);
    mercuryReturnCalcAction->setToolTip("Calculate the Mercury return chart for a selected return number");
    mercuryReturnCalcAction->setStatusTip("Calculate the Mercury return chart for the current birth data and chosen return number");
    mercuryReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+M"));
    connect(mercuryReturnCalcAction, &QAction::triggered, this, &MainWindow::calculateMercuryReturn);
    returnChartsMenu->addAction(mercuryReturnCalcAction);

    QAction *uranusReturnCalcAction = new QAction("Calculate Uranus Return", this);
    uranusReturnCalcAction->setToolTip("Calculate the Uranus return chart for a selected return number");
    uranusReturnCalcAction->setStatusTip("Calculate the Uranus return chart for the current birth data and chosen return number");
    uranusReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+U"));
    connect(uranusReturnCalcAction, &QAction::triggered, this, &MainWindow::calculateUranusReturn);
    returnChartsMenu->addAction(uranusReturnCalcAction);

    QAction *neptuneReturnCalcAction = new QAction("Calculate Neptune Return", this);
    neptuneReturnCalcAction->setToolTip("Calculate the Neptune return chart for a selected return number");
    neptuneReturnCalcAction->setStatusTip("Calculate the Neptune return chart for the current birth data and chosen return number");
    neptuneReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+N"));
    connect(neptuneReturnCalcAction, &QAction::triggered, this, &MainWindow::calculateNeptuneReturn);
    returnChartsMenu->addAction(neptuneReturnCalcAction);

    QAction *plutoReturnCalcAction = new QAction("Calculate Pluto Return", this);
    plutoReturnCalcAction->setToolTip("Calculate the Pluto return chart for a selected return number");
    plutoReturnCalcAction->setStatusTip("Calculate the Pluto return chart for the current birth data and chosen return number");
    plutoReturnCalcAction->setShortcut(QKeySequence("Ctrl+Alt+P"));
    connect(plutoReturnCalcAction, &QAction::triggered, this, &MainWindow::calculatePlutoReturn);
    returnChartsMenu->addAction(plutoReturnCalcAction);

    QAction *secondaryProgressionAction = new QAction("Calculate Secondary Progression Chart", this);
    secondaryProgressionAction->setToolTip("Calculate a secondary progression chart for a selected year of life");
    secondaryProgressionAction->setStatusTip("Calculate the secondary progression chart for the current birth data and chosen progression year");
    secondaryProgressionAction->setShortcut(QKeySequence("Ctrl+G")); // Choose a shortcut that doesn't conflict
    connect(secondaryProgressionAction, &QAction::triggered, this, &MainWindow::calculateSecondaryProgression);
    toolsMenu->insertAction(nullptr, secondaryProgressionAction); // Add at the top of Tools menu

    QAction *draconicChartAction = new QAction("Calculate Draconic Chart", this);
    draconicChartAction->setToolTip("Calculate the draconic chart (natal rotated to the North Node) as a bi-wheel");
    draconicChartAction->setStatusTip("Calculate the draconic bi-wheel for the current birth data");
    connect(draconicChartAction, &QAction::triggered, this, &MainWindow::calculateDraconicChart);
    toolsMenu->insertAction(nullptr, draconicChartAction); // Add at the top of Tools menu

    // Current Chart
    QAction *zodiacChartAction = new QAction("Calculate Zodiac Chart", this);
    zodiacChartAction->setToolTip("Calculate a chart for all Zodiac Signs");
    zodiacChartAction->setShortcut(QKeySequence("Ctrl+H")); // Choose a shortcut that doesn't conflict
    connect(zodiacChartAction, &QAction::triggered, this, &MainWindow::calculateZodiacSignsChart);
    toolsMenu->insertAction(nullptr, zodiacChartAction); // Add at the top of Tools menu

}

void MainWindow::setupConnections()
{
    // Chart calculation
    connect(m_calculateButton, &QPushButton::clicked, this, &MainWindow::calculateChart);

    // Interpretation
    connect(m_getInterpretationButton, &QPushButton::clicked, this, &MainWindow::getInterpretation);

    // Connect to MistralAPI signals
    connect(&m_mistralApi, &MistralAPI::interpretationReady, this, &MainWindow::displayInterpretation);
    connect(&m_mistralApi, &MistralAPI::error, this, &MainWindow::handleError);

    // Connect to ChartDataManager signals
    connect(&m_chartDataManager, &ChartDataManager::error, this, &MainWindow::handleError);

    /////////////predictive
    connect(getPredictionButton, &QPushButton::clicked, this, &MainWindow::getPrediction);
    connect(&m_mistralApi, &MistralAPI::transitInterpretationReady,
            this, &MainWindow::displayTransitInterpretation);
}
