#ifndef ASPECTARIANWIDGET_H
#define ASPECTARIANWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "chartcalculator.h"

class AspectarianWidget : public QWidget {
    Q_OBJECT
public:
    explicit AspectarianWidget(QWidget *parent = nullptr);
    void updateData(const ChartData &chartData);
    void updateDualData(const ChartData &natal,
                        const ChartData &progressed,
                        const QVector<AspectData> &interAspects);
    QString planetSymbol(const QString &planetName);

private:
    QTabWidget   *m_tabWidget;
    QTableWidget *m_table;       // single-chart view / "Prog→Prog" tab in dual mode
    QTableWidget *m_interTable;  // "Prog→Natal" interaspects tab in dual mode
    QLabel       *m_titleLabel;

    void setupUi();
    void fillAspectTable(QTableWidget *table, const ChartData &chartData);
    void fillInterTable(QTableWidget *table,
                        const ChartData &natal,
                        const ChartData &progressed,
                        const QVector<AspectData> &interAspects);
    QColor  aspectColor(AspectType aspectType);
    QString aspectSymbol(AspectType aspectType);
};

#endif // ASPECTARIANWIDGET_H
