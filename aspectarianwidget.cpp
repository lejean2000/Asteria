#include "aspectarianwidget.h"
#include <QHeaderView>
#include <QDebug>
#include <QTabBar>

extern QString g_astroFontFamily;


AspectarianWidget::AspectarianWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

static QTableWidget* makeAspectTable(QWidget *parent)
{
    QTableWidget *t = new QTableWidget(parent);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setSelectionBehavior(QAbstractItemView::SelectItems);
    t->setShowGrid(true);
    t->setGridStyle(Qt::SolidLine);
    t->horizontalHeader()->setVisible(true);
    t->verticalHeader()->setVisible(true);
    t->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    t->horizontalHeader()->setMinimumSectionSize(1);
    t->verticalHeader()->setMinimumSectionSize(1);
    t->setWordWrap(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    return t;
}

void AspectarianWidget::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_titleLabel = new QLabel("Aspectarian", this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_table      = makeAspectTable(this);
    m_interTable = makeAspectTable(this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(m_table,      "Aspects");
    m_tabWidget->addTab(m_interTable, "Prog → Natal");
    m_tabWidget->tabBar()->setVisible(false);
    m_tabWidget->setTabVisible(1, false);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_tabWidget);
    setLayout(layout);
}

void AspectarianWidget::updateData(const ChartData &chartData)
{
    // Revert to single-chart mode
    m_tabWidget->setTabText(0, "Aspects");
    m_tabWidget->tabBar()->setVisible(false);
    m_tabWidget->setTabVisible(1, false);
    m_tabWidget->setCurrentIndex(0);
    m_interTable->clear();
    m_interTable->setRowCount(0);
    m_interTable->setColumnCount(0);

    fillAspectTable(m_table, chartData);
}

void AspectarianWidget::updateDualData(const ChartData &natal,
                                       const ChartData &progressed,
                                       const QVector<AspectData> &interAspects)
{
    m_tabWidget->setTabText(0, "Prog → Prog");
    m_tabWidget->setTabVisible(1, true);
    m_tabWidget->tabBar()->setVisible(true);
    m_tabWidget->setCurrentIndex(1);   // default: Prog → Natal

    fillAspectTable(m_table, progressed);           // Tab 0: within-progressed aspects
    fillInterTable(m_interTable, natal, progressed, interAspects);  // Tab 1: interaspects
}

void AspectarianWidget::fillAspectTable(QTableWidget *table, const ChartData &chartData)
{
    // Clear the table
    table->clear();

    // Get list of planets to display
    QStringList planets;
    for (const auto &planet : chartData.planets) {
        planets << toString(planet.id);
    }

    // Sort planets in traditional order
    /*
    QStringList orderedPlanets = {
        "Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn",
        "Uranus", "Neptune", "Pluto", "Chiron", "North Node", "South Node",
        "Pars Fortuna", "Syzygy"
    };
    */

    QStringList orderedPlanets = {
        "Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn",
        "Uranus", "Neptune", "Pluto", "Chiron", "North Node", "South Node",
        "Part of Fortune", "Syzygy",
        // Additional bodies
        "Lilith", "Ceres", "Pallas", "Juno", "Vesta",
        "Vertex", "East Point", "Part of Spirit"
    };

    // Filter and sort planets
    QStringList displayPlanets;
    for (const QString &planet : orderedPlanets) {
        if (planets.contains(planet)) {
            displayPlanets << planet;
        }
    }

    // Add any remaining planets not in the ordered list
    for (const QString &planet : planets) {
        if (!displayPlanets.contains(planet)) {
            displayPlanets << planet;
        }
    }

    // Set up table dimensions
    int numPlanets = displayPlanets.size();
    table->setRowCount(numPlanets);
    table->setColumnCount(numPlanets);

    // Re-apply header modes/minimums after model geometry changes (Qt 6.9 may reset these)
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setMinimumSectionSize(1);
    table->verticalHeader()->setMinimumSectionSize(1);

    // Create symbol headers
    QStringList symbolHeaders;
    for (const QString &planet : displayPlanets) {
        symbolHeaders << planetSymbol(planet);
    }

    // Set headers with symbols
    table->setHorizontalHeaderLabels(symbolHeaders);
    table->setVerticalHeaderLabels(symbolHeaders);

    // Set font for headers
    QFont headerFont = table->font();
    headerFont.setPointSize(14);
    if (!g_astroFontFamily.isEmpty()) {
        headerFont = QFont(g_astroFontFamily, 14);
    }
    table->horizontalHeader()->setFont(headerFont);
    table->verticalHeader()->setFont(headerFont);

    for (int i = 0; i < numPlanets; i++) {
        QTableWidgetItem* hItem = table->horizontalHeaderItem(i);
        QTableWidgetItem* vItem = table->verticalHeaderItem(i);
        if (hItem) { hItem->setFont(headerFont); hItem->setToolTip(displayPlanets[i]); }
        if (vItem) { vItem->setFont(headerFont); vItem->setToolTip(displayPlanets[i]); }
    }

    // Create a map for quick planet lookup
    QMap<QString, int> planetIndices;
    for (int i = 0; i < displayPlanets.size(); ++i) {
        planetIndices[displayPlanets[i]] = i;
    }

    // Fill the table with aspects
    for (const AspectData &aspect : chartData.aspects) {
        if (!planetIndices.contains(toString(aspect.planet1)) || !planetIndices.contains(toString(aspect.planet2))) {
            continue;
        }
        int row = planetIndices[toString(aspect.planet1)];
        int col = planetIndices[toString(aspect.planet2)];
        if (row > col) { std::swap(row, col); }

        QFont symbolFont = table->font();
        symbolFont.setPointSize(14);

        QTableWidgetItem *item = new QTableWidgetItem(aspectSymbol(aspect.aspectType));
        item->setTextAlignment(Qt::AlignCenter);
        item->setFont(symbolFont);
        item->setBackground(aspectColor(aspect.aspectType));
        item->setToolTip(QString("%1 %2 %3 (Orb: %4°)")
                             .arg(toString(aspect.planet1))
                             .arg(toString(aspect.aspectType))
                             .arg(toString(aspect.planet2))
                             .arg(aspect.orb, 0, 'f', 2));
        table->setItem(row, col, item);
    }
}

void AspectarianWidget::fillInterTable(QTableWidget *table,
                                        const ChartData &natal,
                                        const ChartData &progressed,
                                        const QVector<AspectData> &interAspects)
{
    table->clear();

    static const QStringList orderedPlanets = {
        "Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn",
        "Uranus", "Neptune", "Pluto", "Chiron", "North Node", "South Node",
        "Pars Fortuna", "Syzygy",
        "Lilith", "Ceres", "Pallas", "Juno", "Vesta",
        "Vertex", "East Point", "Part of Spirit"
    };

    // Build display lists in order
    auto buildList = [&](const ChartData &cd) {
        QStringList have;
        for (const PlanetData &p : cd.planets) have << toString(p.id);
        QStringList result;
        for (const QString &n : orderedPlanets) if (have.contains(n)) result << n;
        for (const QString &n : have) if (!result.contains(n)) result << n;
        return result;
    };
    QStringList progPlanets  = buildList(progressed);
    QStringList natalPlanets = buildList(natal);

    // Rows = progressed, Columns = natal
    table->setRowCount(progPlanets.size());
    table->setColumnCount(natalPlanets.size());
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setMinimumSectionSize(1);
    table->verticalHeader()->setMinimumSectionSize(1);

    QStringList colHeaders, rowHeaders;
    for (const QString &p : natalPlanets)  colHeaders << planetSymbol(p);
    for (const QString &p : progPlanets)   rowHeaders << planetSymbol(p);
    table->setHorizontalHeaderLabels(colHeaders);
    table->setVerticalHeaderLabels(rowHeaders);

    QFont headerFont = table->font();
    headerFont.setPointSize(14);
    if (!g_astroFontFamily.isEmpty()) headerFont = QFont(g_astroFontFamily, 14);
    table->horizontalHeader()->setFont(headerFont);
    table->verticalHeader()->setFont(headerFont);
    for (int i = 0; i < natalPlanets.size(); i++) {
        auto *h = table->horizontalHeaderItem(i);
        if (h) { h->setFont(headerFont); h->setToolTip(natalPlanets[i] + " (Natal)"); }
    }
    for (int i = 0; i < progPlanets.size(); i++) {
        auto *v = table->verticalHeaderItem(i);
        if (v) { v->setFont(headerFont); v->setToolTip(progPlanets[i] + " (Prog)"); }
    }

    // Build index maps
    QMap<QString,int> progIdx, natalIdx;
    for (int i = 0; i < progPlanets.size();  ++i) progIdx[progPlanets[i]]   = i;
    for (int i = 0; i < natalPlanets.size(); ++i) natalIdx[natalPlanets[i]] = i;

    // Fill interaspects (all combos, no triangle — rows and cols are different sets)
    QFont symFont = table->font();
    symFont.setPointSize(14);
    for (const AspectData &aspect : interAspects) {
        QString p1 = toString(aspect.planet1);  // progressed
        QString p2 = toString(aspect.planet2);  // natal
        if (!progIdx.contains(p1) || !natalIdx.contains(p2)) continue;

        QTableWidgetItem *item = new QTableWidgetItem(aspectSymbol(aspect.aspectType));
        item->setTextAlignment(Qt::AlignCenter);
        item->setFont(symFont);
        item->setBackground(aspectColor(aspect.aspectType));
        item->setToolTip(QString("%1 (P) %2 %3 (N)  Orb: %4°")
                             .arg(p1).arg(toString(aspect.aspectType)).arg(p2)
                             .arg(aspect.orb, 0, 'f', 2));
        table->setItem(progIdx[p1], natalIdx[p2], item);
    }
}



QString AspectarianWidget::planetSymbol(const QString &planetName) {
    if (planetName == "Sun") return "☉";
    if (planetName == "Moon") return "☽";
    if (planetName == "Mercury") return "☿";
    if (planetName == "Venus") return "♀";
    if (planetName == "Mars") return "♂";
    if (planetName == "Jupiter") return "♃";
    if (planetName == "Saturn") return "♄";
    if (planetName == "Uranus") return "♅";
    if (planetName == "Neptune") return "♆";
    if (planetName == "Pluto") return "♇";
    if (planetName == "Chiron") return "⚷";
    if (planetName == "North Node") return "☊";
    if (planetName == "South Node") return "☋";
    if (planetName == "Pars Fortuna" || planetName == "Part of Fortune") return "⊕";
    if (planetName == "Syzygy") return "☍";

    // Additional bodies
    if (planetName == "Lilith") return "⚸";
    if (planetName == "Ceres") return "⚳";
    if (planetName == "Pallas") return "⚴";
    if (planetName == "Juno") return "⚵";
    if (planetName == "Vesta") return "⚶";
    if (planetName == "Vertex") return "⊗";
    if (planetName == "East Point") return "⊙";
    if (planetName == "Part of Spirit") return "⊖";

    // Return first letter for any other planet
    return planetName.left(1);
}




QColor AspectarianWidget::aspectColor(AspectType aspectType)
{
    switch (aspectType) {
    case AspectType::Conjunction:    return QColor(128, 128, 128, 50); // Gray
    case AspectType::Opposition:     return QColor(255,   0,   0, 50); // Red
    case AspectType::Square:         return QColor(255,   0,   0, 50); // Red
    case AspectType::Trine:          return QColor(  0,   0, 255, 50); // Blue
    case AspectType::Sextile:        return QColor(  0, 255, 255, 50); // Cyan
    case AspectType::Quincunx:       return QColor(  0, 255,   0, 50); // Bright Green
    case AspectType::Semisquare:     return QColor(255, 140,   0, 50); // Dark Orange
    case AspectType::Semisextile:    return QColor(186,  85, 211, 50); // Medium Orchid
    case AspectType::Sesquiquadrate: return QColor(255, 105, 180, 50); // Hot Pink
    default:                         return QColor(128, 128, 128, 50); // Default gray
    }
}




QString AspectarianWidget::aspectSymbol(AspectType aspectType)
{
    switch (aspectType) {
    case AspectType::Conjunction:    return QStringLiteral("☌");  // Conjunction
    case AspectType::Opposition:     return QStringLiteral("☍");  // Opposition
    case AspectType::Square:         return QStringLiteral("□");  // Square
    case AspectType::Trine:          return QStringLiteral("△");  // Trine
    case AspectType::Sextile:        return QStringLiteral("✶");  // Sextile
    case AspectType::Quincunx:       return QStringLiteral("⦻");  // Quincunx
    case AspectType::Semisquare:     return QStringLiteral("∟");  // Semi-square
    case AspectType::Semisextile:    return QStringLiteral("⧫");  // Semi-sextile
    case AspectType::Sesquiquadrate: return QStringLiteral("⋔");  // Sesquiquadrate
    default:                         return toString(aspectType);  // Fallback to name
    }
}
