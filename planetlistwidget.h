#ifndef PLANETLISTWIDGET_H
#define PLANETLISTWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QLabel>
#include <QVBoxLayout>
#include "chartcalculator.h"

class PlanetListWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlanetListWidget(QWidget *parent = nullptr);
    void updateData(const ChartData &chartData);
    void updateDualData(const ChartData &natal, const ChartData &progressed);

private:
    QTabWidget    *m_tabWidget;
    QTableWidget  *m_natalTable;
    QTableWidget  *m_progressedTable;
    QLabel        *m_titleLabel;

    void setupUi();
    void populateTable(QTableWidget *table, const ChartData &chartData);
    QString getSymbolForPlanet(const QString &planetId);
    QString getSymbolForSign(const QString &sign);
    QColor  getColorForSign(const QString &sign);
};

#endif // PLANETLISTWIDGET_H
