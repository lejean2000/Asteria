#ifndef ELEMENTMODALITYWIDGET_H
#define ELEMENTMODALITYWIDGET_H

#include <QWidget>
#include <QGridLayout>
#include <QTabWidget>
#include <QLabel>
#include <QMap>
#include "chartcalculator.h"

class ElementModalityWidget : public QWidget {
    Q_OBJECT
public:
    explicit ElementModalityWidget(QWidget *parent = nullptr);
    void updateData(const ChartData &chartData);
    void updateDualData(const ChartData &natal, const ChartData &progressed);

private:
    QTabWidget   *m_tabWidget;

    // Natal grid members
    QLabel       *m_titleLabel;
    QGridLayout  *m_natalGridLayout;
    QMap<QString, QLabel*> m_natalSignLabels;
    QLabel *m_natalFireTotal, *m_natalEarthTotal, *m_natalAirTotal, *m_natalWaterTotal;
    QLabel *m_natalCardinalTotal, *m_natalFixedTotal, *m_natalMutableTotal;
    QLabel *m_natalGrandTotal;

    // Progressed grid members
    QGridLayout  *m_progGridLayout;
    QMap<QString, QLabel*> m_progSignLabels;
    QLabel *m_progFireTotal, *m_progEarthTotal, *m_progAirTotal, *m_progWaterTotal;
    QLabel *m_progCardinalTotal, *m_progFixedTotal, *m_progMutableTotal;
    QLabel *m_progGrandTotal;

    void setupUi();
    QWidget* buildGridPanel(QGridLayout *&gridLayout,
                            QMap<QString, QLabel*> &signLabels,
                            QLabel *&fireT, QLabel *&earthT,
                            QLabel *&airT,  QLabel *&waterT,
                            QLabel *&cardT, QLabel *&fixedT,
                            QLabel *&mutT,  QLabel *&grandT);
    void fillGrid(QGridLayout *gridLayout,
                  QMap<QString, QLabel*> &signLabels,
                  QLabel *fireT, QLabel *earthT,
                  QLabel *airT,  QLabel *waterT,
                  QLabel *cardT, QLabel *fixedT,
                  QLabel *mutT,  QLabel *grandT,
                  const ChartData &chartData);

    QString getElement(const QString &sign);
    QString getModality(const QString &sign);
    QColor  getElementColor(const QString &element);
    QString getSignGlyph(const QString &sign);
    QString getPlanetGlyph(const QString &planetId);
};

#endif // ELEMENTMODALITYWIDGET_H
