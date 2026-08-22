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

QJsonValue roundJsonDoubles(const QJsonValue &val);

void MainWindow::getInterpretation() {
    if (!m_chartCalculated) {
        QMessageBox::warning(this, "No Chart", "Please calculate a chart first.");
        return;
    }

    const bool isSecondaryProgression =
        (AsteriaGlobals::lastGeneratedChartType == "Secondary Progression"
         && !m_currentNatalChartData.isEmpty());
    const bool isSynastry =
        (AsteriaGlobals::lastGeneratedChartType == "Synastry"
         && !m_currentNatalChartData.isEmpty());
    const bool isDraconic =
        (AsteriaGlobals::lastGeneratedChartType == "Draconic"
         && !m_currentNatalChartData.isEmpty());

    if (!AsteriaGlobals::activeModelLoaded) {
        m_mistralApi.loadActiveModel();
        if (!AsteriaGlobals::activeModelLoaded) {
            QMessageBox::information(this, "AI Model Not Configured",
                                     "No active AI model found. Please go to Settings → Configure AI Models to set up a model.");
            return;
        }
    }

    const QStringList additionalBodies = {
        "Ceres", "Pallas", "Juno", "Vesta", "Lilith",
        "Vertex", "Part of Spirit", "East Point"
    };
    const bool keepAdditional = m_additionalBodiesCB->isChecked();

    auto filterPlanets = [&](const QJsonArray &planets) -> QJsonArray {
        if (keepAdditional) return planets;
        QJsonArray out;
        for (const QJsonValue &v : planets) {
            if (!additionalBodies.contains(v.toObject()["id"].toString()))
                out.append(v);
        }
        return out;
    };

    auto filterAspectsForBodies = [&](const QJsonArray &aspects) -> QJsonArray {
        if (keepAdditional) return aspects;
        QJsonArray out;
        for (const QJsonValue &v : aspects) {
            QJsonObject a = v.toObject();
            // Support both the planet1/planet2 (secondary progression) and
            // personA/personB (synastry) key naming. Use .value() rather than
            // operator[] - the latter auto-inserts a null entry for missing
            // keys on a non-const QJsonObject, corrupting the output object.
            QString bodyA = a.contains("planet1") ? a.value("planet1").toString()
                                                   : a.value("personA").toString();
            QString bodyB = a.contains("planet2") ? a.value("planet2").toString()
                                                   : a.value("personB").toString();
            if (!additionalBodies.contains(bodyA) && !additionalBodies.contains(bodyB)) {
                out.append(a);
            }
        }
        return out;
    };

    QJsonObject dataToSend;

    if (isSecondaryProgression) {
        // Bi-wheel payload: split natal/progressed sides + both aspect sets.
        // Aspects restricted to the five major Ptolemaic types.
        static const QStringList majorAspects = {
            "Conjunction", "Opposition", "Square", "Trine", "Sextile"
        };
        auto keepOnlyMajors = [&](const QJsonArray &aspects) -> QJsonArray {
            QJsonArray out;
            for (const QJsonValue &v : aspects) {
                QJsonObject a = v.toObject();
                if (majorAspects.contains(a["aspectType"].toString()))
                    out.append(a);
            }
            return out;
        };

        // Recompute progressed×natal interaspects on demand.
        ChartData natal      = convertJsonToChartData(m_currentNatalChartData);
        ChartData progressed = convertJsonToChartData(m_currentChartData);
        QVector<AspectData> interAspects =
            m_chartDataManager.calculateInteraspects(progressed, natal);

        QJsonArray interAspectsJson;
        for (const AspectData &a : interAspects) {
            QJsonObject jo;
            jo["planet1"]    = toString(a.planet1);
            jo["planet2"]    = toString(a.planet2);
            jo["aspectType"] = toString(a.aspectType);
            jo["orb"]        = a.orb;
            interAspectsJson.append(jo);
        }

        // Determine which natal house a given ecliptic longitude falls in.
        // Mirrors ChartCalculator::findHouse (private, so replicated here).
        const QJsonArray natalHouses = m_currentNatalChartData["houses"].toArray();
        auto findNatalHouse = [&](double longitude) -> QString {
            longitude = fmod(longitude, 360.0);
            if (longitude < 0.0) longitude += 360.0;
            for (int i = 0; i < natalHouses.size(); ++i) {
                int j = (i + 1) % natalHouses.size();
                double start = natalHouses[i].toObject()["longitude"].toDouble();
                double end   = natalHouses[j].toObject()["longitude"].toDouble();
                if (end < start) {
                    if (longitude >= start || longitude < end)
                        return natalHouses[i].toObject()["id"].toString();
                } else {
                    if (longitude >= start && longitude < end)
                        return natalHouses[i].toObject()["id"].toString();
                }
            }
            return QStringLiteral("House1");
        };

        QJsonObject natalJson;
        natalJson["angles"]  = m_currentNatalChartData["angles"].toArray();
        natalJson["planets"] = filterPlanets(m_currentNatalChartData["planets"].toArray());

        // Build progressed planets with an extra houseNatal field.
        QJsonArray progressedPlanets;
        for (const QJsonValue &v : filterPlanets(m_currentChartData["planets"].toArray())) {
            QJsonObject p = v.toObject();
            p["houseNatal"] = findNatalHouse(p["longitude"].toDouble());
            progressedPlanets.append(p);
        }

        QJsonObject progressedJson;
        progressedJson["angles"]  = m_currentChartData["angles"].toArray();
        progressedJson["planets"] = progressedPlanets;

        dataToSend["natal"]      = natalJson;
        dataToSend["progressed"] = progressedJson;
        dataToSend["progressedToNatalAspects"] =
            keepOnlyMajors(filterAspectsForBodies(interAspectsJson));
        dataToSend["progressedToProgressedAspects"] =
            keepOnlyMajors(filterAspectsForBodies(m_currentChartData["aspects"].toArray()));
    }
    else if (isSynastry) {
        // Bi-wheel payload: Person A = m_currentNatalChartData (inner wheel),
        // Person B = m_currentChartData (outer wheel) - see createSynastryChart().
        // Aspects restricted to the five major Ptolemaic types.
        static const QStringList majorAspects = {
            "Conjunction", "Opposition", "Square", "Trine", "Sextile"
        };
        auto keepOnlyMajors = [&](const QJsonArray &aspects) -> QJsonArray {
            QJsonArray out;
            for (const QJsonValue &v : aspects) {
                QJsonObject a = v.toObject();
                if (majorAspects.contains(a["aspectType"].toString()))
                    out.append(a);
            }
            return out;
        };

        // Given a longitude, find which house of the OTHER person it falls into
        // (the synastry house overlay). Mirrors ChartCalculator::findHouse (private).
        auto makeFindHouse = [](const QJsonArray &houses) {
            return [houses](double longitude) -> QString {
                longitude = fmod(longitude, 360.0);
                if (longitude < 0.0) longitude += 360.0;
                for (int i = 0; i < houses.size(); ++i) {
                    int j = (i + 1) % houses.size();
                    double start = houses[i].toObject()["longitude"].toDouble();
                    double end   = houses[j].toObject()["longitude"].toDouble();
                    if (end < start) {
                        if (longitude >= start || longitude < end)
                            return houses[i].toObject()["id"].toString();
                    } else {
                        if (longitude >= start && longitude < end)
                            return houses[i].toObject()["id"].toString();
                    }
                }
                return QStringLiteral("House1");
            };
        };
        auto findHouseInB = makeFindHouse(m_currentChartData["houses"].toArray());
        auto findHouseInA = makeFindHouse(m_currentNatalChartData["houses"].toArray());

        QJsonArray personAPlanets;
        for (const QJsonValue &v : filterPlanets(m_currentNatalChartData["planets"].toArray())) {
            QJsonObject p = v.toObject();
            p["houseOverlay"] = findHouseInB(p["longitude"].toDouble());
            personAPlanets.append(p);
        }
        QJsonObject personAJson;
        personAJson["angles"]  = m_currentNatalChartData["angles"].toArray();
        personAJson["planets"] = personAPlanets;

        QJsonArray personBPlanets;
        for (const QJsonValue &v : filterPlanets(m_currentChartData["planets"].toArray())) {
            QJsonObject p = v.toObject();
            p["houseOverlay"] = findHouseInA(p["longitude"].toDouble());
            personBPlanets.append(p);
        }
        QJsonObject personBJson;
        personBJson["angles"]  = m_currentChartData["angles"].toArray();
        personBJson["planets"] = personBPlanets;

        ChartData personA = convertJsonToChartData(m_currentNatalChartData);
        ChartData personB = convertJsonToChartData(m_currentChartData);
        QVector<AspectData> synastryAspects =
            m_chartDataManager.calculateInteraspects(personB, personA);

        QJsonArray synastryAspectsJson;
        for (const AspectData &a : synastryAspects) {
            QJsonObject jo;
            jo["personB"]    = toString(a.planet1);
            jo["personA"]    = toString(a.planet2);
            jo["aspectType"] = toString(a.aspectType);
            jo["orb"]        = a.orb;
            synastryAspectsJson.append(jo);
        }

        dataToSend["personAName"] = m_currentRelationshipInfo.value("person1").toString("Person A");
        dataToSend["personBName"] = m_currentRelationshipInfo.value("person2").toString("Person B");
        dataToSend["personA"] = personAJson;
        dataToSend["personB"] = personBJson;
        dataToSend["synastryAspects"] = keepOnlyMajors(filterAspectsForBodies(synastryAspectsJson));
    }
    else if (isDraconic) {
        // Bi-wheel payload: natal (m_currentNatalChartData) vs. draconic
        // (m_currentChartData) - see calculateDraconicChart(). A draconic
        // planet's house is always identical to its natal house (rigid
        // rotation), so unlike Secondary Progression there is no separate
        // house-overlay field to compute.
        static const QStringList majorAspects = {
            "Conjunction", "Opposition", "Square", "Trine", "Sextile"
        };
        auto keepOnlyMajors = [&](const QJsonArray &aspects) -> QJsonArray {
            QJsonArray out;
            for (const QJsonValue &v : aspects) {
                QJsonObject a = v.toObject();
                if (majorAspects.contains(a["aspectType"].toString()))
                    out.append(a);
            }
            return out;
        };

        // Recompute draconic×natal interaspects on demand.
        ChartData natal    = convertJsonToChartData(m_currentNatalChartData);
        ChartData draconic = convertJsonToChartData(m_currentChartData);
        QVector<AspectData> interAspects =
            m_chartDataManager.calculateInteraspects(draconic, natal);

        QJsonArray interAspectsJson;
        for (const AspectData &a : interAspects) {
            QJsonObject jo;
            jo["planet1"]    = toString(a.planet1);
            jo["planet2"]    = toString(a.planet2);
            jo["aspectType"] = toString(a.aspectType);
            jo["orb"]        = a.orb;
            interAspectsJson.append(jo);
        }

        QJsonObject natalJson;
        natalJson["angles"]  = m_currentNatalChartData["angles"].toArray();
        natalJson["planets"] = filterPlanets(m_currentNatalChartData["planets"].toArray());

        QJsonObject draconicJson;
        draconicJson["angles"]  = m_currentChartData["angles"].toArray();
        draconicJson["planets"] = filterPlanets(m_currentChartData["planets"].toArray());

        dataToSend["natal"]    = natalJson;
        dataToSend["draconic"] = draconicJson;
        dataToSend["draconicToNatalAspects"] =
            keepOnlyMajors(filterAspectsForBodies(interAspectsJson));
        dataToSend["draconicToDraconicAspects"] =
            keepOnlyMajors(filterAspectsForBodies(m_currentChartData["aspects"].toArray()));
    }
    else {
        // Single-chart payload (original path).
        dataToSend = m_currentChartData;
        if (!keepAdditional) {
            dataToSend["planets"] = filterPlanets(m_currentChartData["planets"].toArray());
            dataToSend["aspects"] = filterAspectsForBodies(m_currentChartData["aspects"].toArray());
        }
    }

    dataToSend = roundJsonDoubles(dataToSend).toObject();

    qDebug() << "getInterpretation: lastGeneratedChartType=" << AsteriaGlobals::lastGeneratedChartType
             << "isSecondaryProgression=" << isSecondaryProgression
             << "natalEmpty=" << m_currentNatalChartData.isEmpty();
    qDebug() << "getInterpretation: dataToSend="
             << QString::fromUtf8(QJsonDocument(dataToSend).toJson(QJsonDocument::Compact));

    m_getInterpretationButton->setEnabled(false);
    statusBar()->showMessage("Requesting interpretation...");
    m_aiWaitProgressBar->show();

    m_mistralApi.interpretChart(dataToSend);
}

void MainWindow::displayInterpretation(const QString &interpretation)
{
    appendInterpretationEntry("ai_chart", AsteriaGlobals::lastGeneratedChartType, interpretation);
    m_getInterpretationButton->setEnabled(true);
    m_aiWaitProgressBar->hide();
    statusBar()->showMessage("Interpretation received", 3000);
}

// Helper function to convert plain text to basic HTML
QString MainWindow::plainTextToHtml(const QString &plainText)
{
    if (plainText.isEmpty()) return "";

    QString html = plainText;
    // Convert line breaks to HTML paragraphs
    html.replace("\n\n", "</p><p>");
    html.replace("\n", "<br>");
    return "<p>" + html + "</p>";
}


void MainWindow::appendInterpretationEntry(const QString &type, const QString &chartType,
                                            const QString &text,
                                            const QString &periodFrom,
                                            const QString &periodTo)
{
    QJsonObject entry;
    entry["type"]        = type;
    entry["chartType"]   = chartType;
    entry["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    entry["text"]        = text;
    QString model = m_mistralApi.modelName();
    if (!model.isEmpty()) entry["model"] = model;
    if (!periodFrom.isEmpty()) entry["periodFrom"] = periodFrom;
    if (!periodTo.isEmpty())   entry["periodTo"]   = periodTo;
    m_interpretations.append(entry);
    renderAllInterpretations();
}

void MainWindow::renderAllInterpretations()
{
    // Remove all existing cards from the layout
    while (QLayoutItem *item = m_interpretationLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }

    if (m_interpretations.isEmpty()) {
        QLabel *placeholder = new QLabel(
            "No interpretations yet.\nCalculate a chart and click\n'Get AI Interpretation'.",
            m_interpretationContainer);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setWordWrap(true);
        placeholder->setStyleSheet("color: palette(mid); padding: 32px;");
        m_interpretationLayout->addWidget(placeholder);
        return;
    }

    for (const QJsonValue &val : std::as_const(m_interpretations)) {
        QJsonObject entry      = val.toObject();
        QString type           = entry["type"].toString();
        QString chartType      = entry["chartType"].toString();
        QString text           = entry["text"].toString();
        QString genAt          = entry["generatedAt"].toString();
        QString genAtFormatted = genAt.isEmpty()
            ? QString()
            : QDateTime::fromString(genAt, Qt::ISODate).toString("dd MMM yyyy HH:mm");

        // Header label and accent colour per entry type
        QString headerLabel;
        QString bgColor;

        if (type == "chart_info") {
            headerLabel = chartType.isEmpty() ? "Chart Info" : chartType;
            bgColor = "#3a6186";
        } else if (type == "ai_chart") {
            headerLabel = (chartType.isEmpty() ? "Chart" : chartType) + " Interpretation";
            if (!genAtFormatted.isEmpty()) headerLabel += "  — " + genAtFormatted;
            QString model = entry["model"].toString();
            if (!model.isEmpty()) headerLabel += "  (" + model + ")";
            bgColor = "#1a6b4a";
        } else if (type == "ai_transit") {
            headerLabel = "Transit Prediction";
            QString from = entry["periodFrom"].toString();
            QString to   = entry["periodTo"].toString();
            if (!from.isEmpty() && !to.isEmpty())
                headerLabel += "  — " + from + " → " + to;
            if (!genAtFormatted.isEmpty()) headerLabel += "  (" + genAtFormatted + ")";
            QString model = entry["model"].toString();
            if (!model.isEmpty()) headerLabel += "  [" + model + "]";
            bgColor = "#7d4e1a";
        } else {
            headerLabel = "Imported Interpretation";
            if (!chartType.isEmpty()) headerLabel += " (" + chartType + ")";
            bgColor = "#4a4a4a";
        }

        // Card frame
        QFrame *card = new QFrame(m_interpretationContainer);
        card->setFrameShape(QFrame::StyledPanel);
        card->setFrameShadow(QFrame::Plain);
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(0);

        // Clickable header button
        QPushButton *hdr = new QPushButton(card);
        hdr->setCheckable(true);
        hdr->setChecked(true);
        hdr->setProperty("baseLabel", headerLabel);
        hdr->setText("▼  " + headerLabel);
        hdr->setStyleSheet(QString(
            "QPushButton {"
            "  background-color: %1;"
            "  color: #ffffff;"
            "  border: none;"
            "  padding: 7px 12px;"
            "  text-align: left;"
            "  font-weight: bold;"
            "  font-size: 11px;"
            "}").arg(bgColor));

        // Body browser — sized to content, outer scroll area handles navigation
        QTextBrowser *body = new QTextBrowser(card);
        body->setOpenExternalLinks(true);
        body->setFrameShape(QFrame::NoFrame);
        body->setStyleSheet("QTextBrowser { padding: 8px; }");
        body->setHtml(markdownToHtml(text));
        body->document()->setTextWidth(380);
        int docH = static_cast<int>(body->document()->size().height()) + 20;
        body->setFixedHeight(qMin(docH, 600));

        cardLayout->addWidget(hdr);
        cardLayout->addWidget(body);

        connect(hdr, &QPushButton::toggled, [hdr, body](bool expanded) {
            body->setVisible(expanded);
            hdr->setText(QString(expanded ? "▼  " : "▶  ")
                         + hdr->property("baseLabel").toString());
        });

        m_interpretationLayout->addWidget(card);
    }
}
