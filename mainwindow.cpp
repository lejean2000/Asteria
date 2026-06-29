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
#include<QMessageBox>
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
#include <QCheckBox>  // Add this include if still needed
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

const QRegularExpression MainWindow::dateRegex(
        R"(^(0[1-9]|[12][0-9]|3[01])/(0[1-9]|1[0-2])/(000[1-9]|00[1-9][0-9]|0[1-9][0-9]{2}|[12][0-9]{3}|3000)$)"
);


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_chartCalculated(false)
    , m_transitDialog(nullptr)
    , m_dragStartPosition(0, 0) // Initialize drag start position
{
    setAcceptDrops(true);
    preloadMapResources();
    // Set window title and size
    setWindowTitle("Asteria L - Astrological Chart Analysis");
    setWindowIcon(QIcon(":/icons/asteria-icon-512.png"));
    // Setup UI components
    setupUi();

    // Load settings
    loadSettings();
    if (!m_chartDataManager.isCalculatorAvailable()) {
    }

    connect(languageComboBox, &QComboBox::currentTextChanged,
            &m_mistralApi, &MistralAPI::setLanguage);

    m_symbolsDialog = nullptr;
    m_howToUseDialog = nullptr;
    chartInfoOverlay->setVisible(false);

    // Set minimum size
    // Explicitly disable full screen

    // Force the window to be the size we want
    QTimer::singleShot(0, this, [this]() {
        this->resize(1200, 800);
    });
}

MainWindow::~MainWindow()
{
    // Save settings first
    saveSettings();

    // Delete dialogs that aren't part of the widget hierarchy
    if (m_symbolsDialog) {
        delete m_symbolsDialog;
        m_symbolsDialog = nullptr;
    }

    if (m_howToUseDialog) {
        delete m_howToUseDialog;
        m_howToUseDialog = nullptr;
    }
    if (m_relationshipChartsDialog) {
        delete m_relationshipChartsDialog;
        m_relationshipChartsDialog = nullptr;
    }
    m_aspectarianWidget = nullptr;
    m_modalityElementWidget = nullptr;
    m_planetListWidget = nullptr;
    m_centralTabWidget = nullptr;
    chartContainer = nullptr;
    m_chartView = nullptr;
    m_chartRenderer = nullptr;
    chartInfoOverlay = nullptr;
    m_inputDock = nullptr;
    m_interpretationDock = nullptr;
    m_chartDetailsWidget = nullptr;
    if (m_transitSearchDialog) {
        delete m_transitSearchDialog;
        m_transitSearchDialog = nullptr;
    }
}

