#include "elementmodalitywidget.h"
#include <QFont>
#include <QFrame>
#include <QVBoxLayout>
#include <QTabBar>

extern QString g_astroFontFamily;

ElementModalityWidget::ElementModalityWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

// ─────────────────────────────────────────────────────────────────────────────
// Build one grid panel (natal or progressed) and populate output label pointers
// ─────────────────────────────────────────────────────────────────────────────
QWidget* ElementModalityWidget::buildGridPanel(
    QGridLayout *&gridLayout,
    QMap<QString, QLabel*> &signLabels,
    QLabel *&fireT,  QLabel *&earthT,
    QLabel *&airT,   QLabel *&waterT,
    QLabel *&cardT,  QLabel *&fixedT,
    QLabel *&mutT,   QLabel *&grandT)
{
    QWidget *panel = new QWidget(this);
    gridLayout = new QGridLayout(panel);
    gridLayout->setSpacing(2);

    // Header labels
    QLabel *headerElement  = new QLabel("",        panel);
    QLabel *headerFire     = new QLabel("Fire",    panel);
    QLabel *headerEarth    = new QLabel("Earth",   panel);
    QLabel *headerAir      = new QLabel("Air",     panel);
    QLabel *headerWater    = new QLabel("Water",   panel);
    QLabel *headerTotal    = new QLabel("Total",   panel);
    QLabel *headerCardinal = new QLabel("Cardinal",panel);
    QLabel *headerFixed    = new QLabel("Fixed",   panel);
    QLabel *headerMutable  = new QLabel("Mutable", panel);

    QFont headerFont;
    headerFont.setBold(true);
    for (QLabel *l : {headerElement, headerFire, headerEarth, headerAir,
                      headerWater,   headerTotal, headerCardinal,
                      headerFixed,   headerMutable}) {
        l->setFont(headerFont);
    }
    headerFire ->setStyleSheet("background-color: rgba(255, 100, 100, 100);");
    headerEarth->setStyleSheet("background-color: rgba(255, 255, 100, 100);");
    headerAir  ->setStyleSheet("background-color: rgba(100, 200, 100, 100);");
    headerWater->setStyleSheet("background-color: rgba(100, 100, 255, 100);");

    gridLayout->addWidget(headerElement,  0, 0);
    gridLayout->addWidget(headerFire,     0, 1);
    gridLayout->addWidget(headerEarth,    0, 2);
    gridLayout->addWidget(headerAir,      0, 3);
    gridLayout->addWidget(headerWater,    0, 4);
    gridLayout->addWidget(headerTotal,    0, 5);
    gridLayout->addWidget(headerCardinal, 1, 0);
    gridLayout->addWidget(headerFixed,    2, 0);
    gridLayout->addWidget(headerMutable,  3, 0);

    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    for (int c = 1; c <= 5; ++c) gridLayout->setColumnStretch(c, 1);
    for (int r = 1; r <= 3; ++r) gridLayout->setRowStretch(r, 1);
    gridLayout->setRowStretch(4, 0);

    // Sign cells
    QStringList signs = {
        "Aries", "Taurus", "Gemini", "Cancer",
        "Leo", "Virgo", "Libra", "Scorpio",
        "Sagittarius", "Capricorn", "Aquarius", "Pisces"
    };
    QMap<QString, QPair<int,int>> signPositions = {
        {"Aries",{1,1}},{"Taurus",{2,2}},{"Gemini",{3,3}},{"Cancer",{1,4}},
        {"Leo",{2,1}},{"Virgo",{3,2}},{"Libra",{1,3}},{"Scorpio",{2,4}},
        {"Sagittarius",{3,1}},{"Capricorn",{1,2}},{"Aquarius",{2,3}},{"Pisces",{3,4}}
    };

    for (const QString &sign : signs) {
        QLabel *label = new QLabel(getSignGlyph(sign), panel);
        label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        label->setFrameShape(QFrame::Box);
        label->setMinimumSize(1, 1);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        label->setWordWrap(false);
        QFont gf("DejaVu Sans", 14);
        gf.setStyleStrategy(QFont::NoFontMerging);
        label->setFont(gf);
        QString element = getElement(sign);
        QColor color = getElementColor(element);
        label->setStyleSheet(QString("background-color: rgba(%1, %2, %3, 50);")
                                 .arg(color.red()).arg(color.green()).arg(color.blue()));
        QPair<int,int> pos = signPositions[sign];
        gridLayout->addWidget(label, pos.first, pos.second);
        signLabels[sign] = label;
    }

    // Total labels
    auto makeTotalLabel = [&](const QString &text) {
        QLabel *l = new QLabel(text, panel);
        QFont tf = l->font(); tf.setBold(true); l->setFont(tf);
        l->setAlignment(Qt::AlignCenter);
        return l;
    };
    fireT  = makeTotalLabel("0"); earthT = makeTotalLabel("0");
    airT   = makeTotalLabel("0"); waterT = makeTotalLabel("0");
    cardT  = makeTotalLabel("0"); fixedT = makeTotalLabel("0");
    mutT   = makeTotalLabel("0");

    gridLayout->addWidget(fireT,  4, 1);
    gridLayout->addWidget(earthT, 4, 2);
    gridLayout->addWidget(airT,   4, 3);
    gridLayout->addWidget(waterT, 4, 4);
    gridLayout->addWidget(cardT,  1, 5);
    gridLayout->addWidget(fixedT, 2, 5);
    gridLayout->addWidget(mutT,   3, 5);

    QLabel *grandLabel = new QLabel("Total", panel);
    grandLabel->setFont(headerFont);
    grandT = makeTotalLabel("0");
    gridLayout->addWidget(grandLabel, 4, 0);
    gridLayout->addWidget(grandT,     4, 5);

    return panel;
}

void ElementModalityWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_titleLabel = new QLabel("Elements & Modalities", this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);

    QWidget *natalPanel = buildGridPanel(
        m_natalGridLayout, m_natalSignLabels,
        m_natalFireTotal,  m_natalEarthTotal,
        m_natalAirTotal,   m_natalWaterTotal,
        m_natalCardinalTotal, m_natalFixedTotal,
        m_natalMutableTotal,  m_natalGrandTotal);

    QWidget *progPanel = buildGridPanel(
        m_progGridLayout, m_progSignLabels,
        m_progFireTotal,  m_progEarthTotal,
        m_progAirTotal,   m_progWaterTotal,
        m_progCardinalTotal, m_progFixedTotal,
        m_progMutableTotal,  m_progGrandTotal);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(natalPanel, "Natal");
    m_tabWidget->addTab(progPanel,  "Progressed");
    m_tabWidget->tabBar()->setVisible(false);
    m_tabWidget->setTabVisible(1, false);

    mainLayout->addWidget(m_titleLabel);
    mainLayout->addWidget(m_tabWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);
}

// ─────────────────────────────────────────────────────────────────────────────
// Fill one grid with chart data
// ─────────────────────────────────────────────────────────────────────────────
void ElementModalityWidget::fillGrid(
    QGridLayout */*gridLayout*/,
    QMap<QString, QLabel*> &signLabels,
    QLabel *fireT,  QLabel *earthT,
    QLabel *airT,   QLabel *waterT,
    QLabel *cardT,  QLabel *fixedT,
    QLabel *mutT,   QLabel *grandT,
    const ChartData &chartData)
{
    QStringList signs = {
        "Aries", "Taurus", "Gemini", "Cancer",
        "Leo", "Virgo", "Libra", "Scorpio",
        "Sagittarius", "Capricorn", "Aquarius", "Pisces"
    };

    QMap<QString, QStringList> signPlanets;
    for (const QString &sign : signs) signPlanets[sign] = {};

    QMap<QString,int> elementCounts  = {{"Fire",0},{"Earth",0},{"Air",0},{"Water",0}};
    QMap<QString,int> modalityCounts = {{"Cardinal",0},{"Fixed",0},{"Mutable",0}};

    for (const PlanetData &planet : chartData.planets) {
        QString signName = planet.sign.split(' ').first();
        if (signPlanets.contains(signName)) {
            signPlanets[signName].append(toString(planet.id));
            elementCounts[getElement(signName)]++;
            modalityCounts[getModality(signName)]++;
        }
    }

    for (const QString &sign : signs) {
        if (!signLabels.contains(sign)) continue;
        QString signGlyph   = getSignGlyph(sign);
        QString planetGlyphs;
        for (const QString &pid : signPlanets[sign])
            planetGlyphs += getPlanetGlyph(pid) + " ";
        QString labelText = signGlyph + "\n" + planetGlyphs.trimmed();
        signLabels[sign]->setText(labelText);

        if (!g_astroFontFamily.isEmpty()) {
            QFont af("DejaVu Sans", 14);
            af.setStyleStrategy(QFont::NoFontMerging);
            af.setBold(true);
            signLabels[sign]->setFont(af);
        }
    }

    fireT ->setText(QString::number(elementCounts["Fire"]));
    earthT->setText(QString::number(elementCounts["Earth"]));
    airT  ->setText(QString::number(elementCounts["Air"]));
    waterT->setText(QString::number(elementCounts["Water"]));
    cardT ->setText(QString::number(modalityCounts["Cardinal"]));
    fixedT->setText(QString::number(modalityCounts["Fixed"]));
    mutT  ->setText(QString::number(modalityCounts["Mutable"]));
    grandT->setText(QString::number(
        elementCounts["Fire"] + elementCounts["Earth"] +
        elementCounts["Air"]  + elementCounts["Water"]));
}

void ElementModalityWidget::updateData(const ChartData &chartData)
{
    // Single-chart mode: show only natal tab, hide tab bar
    m_tabWidget->tabBar()->setVisible(false);
    m_tabWidget->setTabVisible(1, false);
    m_tabWidget->setCurrentIndex(0);

    fillGrid(m_natalGridLayout, m_natalSignLabels,
             m_natalFireTotal,  m_natalEarthTotal,
             m_natalAirTotal,   m_natalWaterTotal,
             m_natalCardinalTotal, m_natalFixedTotal,
             m_natalMutableTotal,  m_natalGrandTotal,
             chartData);
}

void ElementModalityWidget::updateDualData(const ChartData &natal, const ChartData &progressed)
{
    m_tabWidget->setTabVisible(1, true);
    m_tabWidget->tabBar()->setVisible(true);
    m_tabWidget->setCurrentIndex(0);  // default: Natal

    fillGrid(m_natalGridLayout, m_natalSignLabels,
             m_natalFireTotal,  m_natalEarthTotal,
             m_natalAirTotal,   m_natalWaterTotal,
             m_natalCardinalTotal, m_natalFixedTotal,
             m_natalMutableTotal,  m_natalGrandTotal,
             natal);

    fillGrid(m_progGridLayout, m_progSignLabels,
             m_progFireTotal,  m_progEarthTotal,
             m_progAirTotal,   m_progWaterTotal,
             m_progCardinalTotal, m_progFixedTotal,
             m_progMutableTotal,  m_progGrandTotal,
             progressed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper lookups (unchanged)
// ─────────────────────────────────────────────────────────────────────────────
QString ElementModalityWidget::getElement(const QString &sign)
{
    if (sign == "Aries" || sign == "Leo" || sign == "Sagittarius") return "Fire";
    if (sign == "Taurus" || sign == "Virgo" || sign == "Capricorn") return "Earth";
    if (sign == "Gemini" || sign == "Libra" || sign == "Aquarius") return "Air";
    if (sign == "Cancer" || sign == "Scorpio" || sign == "Pisces") return "Water";
    return "";
}

QString ElementModalityWidget::getModality(const QString &sign)
{
    if (sign == "Aries" || sign == "Cancer" || sign == "Libra" || sign == "Capricorn") return "Cardinal";
    if (sign == "Taurus" || sign == "Leo" || sign == "Scorpio" || sign == "Aquarius") return "Fixed";
    if (sign == "Gemini" || sign == "Virgo" || sign == "Sagittarius" || sign == "Pisces") return "Mutable";
    return "";
}

QColor ElementModalityWidget::getElementColor(const QString &element)
{
    if (element == "Fire")  return QColor(255, 100, 100);
    if (element == "Earth") return QColor(255, 255, 100);
    if (element == "Air")   return QColor(100, 200, 100);
    if (element == "Water") return QColor(100, 100, 255);
    return QColor(200, 200, 200);
}

QString ElementModalityWidget::getSignGlyph(const QString &sign) {
    if (sign == "Aries")       return "♈";
    if (sign == "Taurus")      return "♉";
    if (sign == "Gemini")      return "♊";
    if (sign == "Cancer")      return "♋";
    if (sign == "Leo")         return "♌";
    if (sign == "Virgo")       return "♍";
    if (sign == "Libra")       return "♎";
    if (sign == "Scorpio")     return "♏";
    if (sign == "Sagittarius") return "♐";
    if (sign == "Capricorn")   return "♑";
    if (sign == "Aquarius")    return "♒";
    if (sign == "Pisces")      return "♓";
    return sign;
}

QString ElementModalityWidget::getPlanetGlyph(const QString &planetId) {
    if (planetId == "Sun")          return "☉";
    if (planetId == "Moon")         return "☽";
    if (planetId == "Mercury")      return "☿";
    if (planetId == "Venus")        return "♀";
    if (planetId == "Mars")         return "♂";
    if (planetId == "Jupiter")      return "♃";
    if (planetId == "Saturn")       return "♄";
    if (planetId == "Uranus")       return "♅";
    if (planetId == "Neptune")      return "♆";
    if (planetId == "Pluto")        return "♇";
    if (planetId == "Chiron")       return "⚷";
    if (planetId == "North Node")   return "☊";
    if (planetId == "South Node")   return "☋";
    if (planetId == "Pars Fortuna") return "⊗";
    if (planetId == "Syzygy")       return "☍";
    if (planetId == "Lilith")       return "⚸";
    if (planetId == "Ceres")        return "⚳";
    if (planetId == "Pallas")       return "⚴";
    if (planetId == "Juno")         return "⚵";
    if (planetId == "Vesta")        return "⚶";
    if (planetId == "Vertex")       return "⊗";
    if (planetId == "East Point")   return "⊙";
    if (planetId == "Part of Spirit") return "⊖";
    return planetId.left(1);
}
