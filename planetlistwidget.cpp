#include "planetlistwidget.h"
#include <QHeaderView>
#include <QFont>
#include <QTabWidget>
#include <QTabBar>

extern QString g_astroFontFamily;


PlanetListWidget::PlanetListWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

static QTableWidget* makePlanetTable(QWidget *parent)
{
    QTableWidget *t = new QTableWidget(parent);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setShowGrid(true);
    t->setAlternatingRowColors(true);
    t->verticalHeader()->setVisible(false);
    t->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    t->setWordWrap(false);
    t->horizontalHeader()->setMinimumSectionSize(1);
    t->verticalHeader()->setMinimumSectionSize(1);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    t->setColumnCount(5);
    t->setHorizontalHeaderLabels({"Planet", "Sign", "Degree", "Minute", "House"});
    return t;
}

void PlanetListWidget::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_titleLabel = new QLabel("Planets", this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_natalTable      = makePlanetTable(this);
    m_progressedTable = makePlanetTable(this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(m_natalTable,      "Natal");
    m_tabWidget->addTab(m_progressedTable, "Progressed");
    m_tabWidget->tabBar()->setVisible(false);  // hidden in single-chart mode
    m_tabWidget->setTabVisible(1, false);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_tabWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);
}

void PlanetListWidget::updateData(const ChartData &chartData)
{
    // Revert to single-chart mode
    m_tabWidget->tabBar()->setVisible(false);
    m_tabWidget->setTabVisible(1, false);
    m_tabWidget->setCurrentIndex(0);
    m_progressedTable->setRowCount(0);

    populateTable(m_natalTable, chartData);
}

void PlanetListWidget::updateDualData(const ChartData &natal, const ChartData &progressed)
{
    m_tabWidget->setTabVisible(1, true);
    m_tabWidget->tabBar()->setVisible(true);
    m_tabWidget->setCurrentIndex(0);   // default: Natal

    populateTable(m_natalTable,      natal);
    populateTable(m_progressedTable, progressed);
}

void PlanetListWidget::populateTable(QTableWidget *table, const ChartData &chartData)
{
    // Clear the table
    table->setRowCount(0);

    // Sort planets in traditional order
    QStringList orderedPlanets = {
        "Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn",
        "Uranus", "Neptune", "Pluto", "Chiron", "North Node", "South Node",
        "Pars Fortuna", "Syzygy",
        // Additional bodies
        "Lilith", "Ceres", "Pallas", "Juno", "Vesta",
        "Vertex", "East Point", "Part of Spirit"
    };

    // Create a map for quick lookup
    QMap<QString, PlanetData> planetMap;
    for (const PlanetData &planet : chartData.planets) {
        planetMap[toString(planet.id)] = planet;
    }

    // Create font for symbols if Astromoony is available
    QFont symbolFont = table->font();
    bool useCustomFont = !g_astroFontFamily.isEmpty();
    if (useCustomFont) {
        symbolFont = QFont(g_astroFontFamily, symbolFont.pointSize());
    }

    // Add planets in order
    for (const QString &planetId : orderedPlanets) {
        if (planetMap.contains(planetId)) {
            const PlanetData &planet = planetMap[planetId];
            int row = table->rowCount();
            table->insertRow(row);

            // Planet symbol and name
            QString planetSymbol = getSymbolForPlanet(toString(planet.id));
            //QTableWidgetItem *planetItem = new QTableWidgetItem(planetSymbol + " " + toString(planet.id));
            QTableWidgetItem *planetItem = new QTableWidgetItem(planetSymbol + " " + toString(planet.id) + (planet.isRetrograde && planet.id != Planet::NorthNode && planet.id != Planet::SouthNode ? " ℞ " : ""));
            if (useCustomFont) {
                planetItem->setFont(symbolFont);
            }

            // Sign symbol and name
            //QString signSymbol = getSymbolForSign(planet.sign);
            //QTableWidgetItem *signItem = new QTableWidgetItem(signSymbol + " " + planet.sign);

            QString signName = planet.sign.split(' ').first();
            QString signSymbol = getSymbolForSign(signName);
            QTableWidgetItem *signItem = new QTableWidgetItem(signSymbol + " " + signName);
            //QTableWidgetItem *signItem = new QTableWidgetItem(signName);

            if (useCustomFont) {
                //QFont zodiacFont = table->font(); // ordinary UI font
                QFont zodiacFont("Dejavu Sans", 11);      // use a known system font
                zodiacFont.setStyleStrategy(QFont::NoFontMerging); // prevent emoji fallback

                signItem->setFont(zodiacFont);
            }
            signItem->setBackground(getColorForSign(signName));

            // Degree
            int degree = static_cast<int>(planet.longitude) % 30;
            QTableWidgetItem *degreeItem = new QTableWidgetItem(QString::number(degree) + "°");

            // Minute
            int minute = static_cast<int>((planet.longitude - static_cast<int>(planet.longitude)) * 60);
            QTableWidgetItem *minuteItem = new QTableWidgetItem(QString::number(minute) + "'");

            // House - use the house string directly
            QTableWidgetItem *houseItem = new QTableWidgetItem(planet.house);

            // Set items in the table
            table->setItem(row, 0, planetItem);
            table->setItem(row, 1, signItem);
            table->setItem(row, 2, degreeItem);
            table->setItem(row, 3, minuteItem);
            table->setItem(row, 4, houseItem);

            // Center align all items
            for (int col = 0; col < table->columnCount(); ++col) {
                table->item(row, col)->setTextAlignment(Qt::AlignCenter);
            }
        }
    }

    // Add any remaining planets not in the ordered list
    for (const PlanetData &planet : chartData.planets) {
        if (!orderedPlanets.contains(toString(planet.id))) {
            int row = table->rowCount();
            table->insertRow(row);

            // Planet name
            QTableWidgetItem *planetItem = new QTableWidgetItem(toString(planet.id));

            // Sign
            //QTableWidgetItem *signItem = new QTableWidgetItem(planet.sign);
            QString signName = planet.sign.split(' ').first();
            QTableWidgetItem *signItem = new QTableWidgetItem(signName);
            signItem->setBackground(getColorForSign(signName));

            // Degree
            int degree = static_cast<int>(planet.longitude) % 30;
            QTableWidgetItem *degreeItem = new QTableWidgetItem(QString::number(degree) + "°");

            // Minute
            int minute = static_cast<int>((planet.longitude - static_cast<int>(planet.longitude)) * 60);
            QTableWidgetItem *minuteItem = new QTableWidgetItem(QString::number(minute) + "'");

            // House - use the house string directly
            QTableWidgetItem *houseItem = new QTableWidgetItem(planet.house);

            // Set items in the table
            table->setItem(row, 0, planetItem);
            table->setItem(row, 1, signItem);
            table->setItem(row, 2, degreeItem);
            table->setItem(row, 3, minuteItem);
            table->setItem(row, 4, houseItem);

            // Center align all items
            for (int col = 0; col < table->columnCount(); ++col) {
                table->item(row, col)->setTextAlignment(Qt::AlignCenter);
            }
        }
    }
    // Re-apply header modes/minimums after model changes (Qt 6.9 may reset these)
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setMinimumSectionSize(1);
    table->verticalHeader()->setMinimumSectionSize(1);
}

QString PlanetListWidget::getSymbolForPlanet(const QString &planetId)
{
    // Unicode symbols for planets
    if (planetId == "Sun") return "☉";
    if (planetId == "Moon") return "☽";
    if (planetId == "Mercury") return "☿";
    if (planetId == "Venus") return "♀";
    if (planetId == "Mars") return "♂";
    if (planetId == "Jupiter") return "♃";
    if (planetId == "Saturn") return "♄";
    if (planetId == "Uranus") return "♅";
    if (planetId == "Neptune") return "♆";
    if (planetId == "Pluto") return "♇";
    if (planetId == "Chiron") return "⚷";
    if (planetId == "North Node") return "☊";
    if (planetId == "South Node") return "☋";
    if (planetId == "Pars Fortuna") return "⊕";
    if (planetId == "Syzygy") return "☍";
    // Additional bodies
    if (planetId == "Lilith") return "⚸";
    if (planetId == "Ceres") return "⚳";
    if (planetId == "Pallas") return "⚴";
    if (planetId == "Juno") return "⚵";
    if (planetId == "Vesta") return "⚶";
    if (planetId == "Vertex") return "⊗";
    if (planetId == "East Point") return "⊙";
    if (planetId == "Part of Spirit") return "⊖";

    return "";
}

QString PlanetListWidget::getSymbolForSign(const QString &sign)
{
    // Unicode symbols for zodiac signs
    if (sign == "Aries") return "♈";
    if (sign == "Taurus") return "♉";
    if (sign == "Gemini") return "♊";
    if (sign == "Cancer") return "♋";
    if (sign == "Leo") return "♌";
    if (sign == "Virgo") return "♍";
    if (sign == "Libra") return "♎";
    if (sign == "Scorpio") return "♏";
    if (sign == "Sagittarius") return "♐";
    if (sign == "Capricorn") return "♑";
    if (sign == "Aquarius") return "♒";
    if (sign == "Pisces") return "♓";

    return "";
}

QColor PlanetListWidget::getColorForSign(const QString &sign)
{
    // Colors based on elements
    if (sign == "Aries" || sign == "Leo" || sign == "Sagittarius") {
        return QColor(255, 200, 200);  // Light red for Fire
    } else if (sign == "Taurus" || sign == "Virgo" || sign == "Capricorn") {
        return QColor(255, 255, 200);  // Light yellow for earth 255, 255, 200

    } else if (sign == "Gemini" || sign == "Libra" || sign == "Aquarius") {
        return QColor(200, 255, 200);  // Light green for Air 200, 255, 200

    } else if (sign == "Cancer" || sign == "Scorpio" || sign == "Pisces") {
        return QColor(200, 200, 255);  // Light blue for Water
    }

    return QColor(240, 240, 240);  // Light gray default
}
