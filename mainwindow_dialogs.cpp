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
void MainWindow::searchLocationCoordinates(const QString& location) {
    if (location.isEmpty()) {
        return;
    }

#ifdef FLATHUB_BUILD
    QMessageBox::information(this, tr("Feature Unavailable"),
                             tr("This feature is not available in the Flathub version of Asteria.\n"
                                "Please manually search for the location coordinates."));
#else
    // Create the search URL
    QString searchQuery = QString("coordinates of %1").arg(location);
    QString encodedQuery = QUrl::toPercentEncoding(searchQuery);
    QUrl url(QString("https://www.google.com/search?q=%1").arg(QString(encodedQuery)));

    // Open the URL in the default browser
    QDesktopServices::openUrl(url);
    locationSearchEdit->clear();
#endif
}


void MainWindow::showSymbolsDialog()
{
    // Create the dialog only if it doesn't exist yet
    if (!m_symbolsDialog) {
        m_symbolsDialog = new SymbolsDialog(this, g_astroFontFamily);

        // Connect the dialog's finished signal to handle cleanup
        connect(m_symbolsDialog, &QDialog::finished, this, [this]() {
            // This ensures the dialog is properly deleted when closed
            m_symbolsDialog->deleteLater();
            m_symbolsDialog = nullptr;
        });
    }

    // Show and raise the dialog to bring it to the front
    m_symbolsDialog->show();
    m_symbolsDialog->raise();
    m_symbolsDialog->activateWindow();
}


void MainWindow::showHowToUseDialog() {
    // Create the dialog only if it doesn't exist yet
    if (!m_howToUseDialog) {
        m_howToUseDialog = new QDialog(this);
        m_howToUseDialog->setWindowTitle("How to Use Asteria");
        m_howToUseDialog->setMinimumSize(500, 400);
        // Create layout
        QVBoxLayout *layout = new QVBoxLayout(m_howToUseDialog);
        // Create a text browser for rich text display
        QTextBrowser *textBrowser = new QTextBrowser(m_howToUseDialog);
        textBrowser->setOpenExternalLinks(true);
        // Set the help content
        QString helpText = R"(
        <h2>How to Use Asteria</h2>
        <h3>Getting Started</h3>
        <p>Asteria allows you to create and analyze astrological birth charts. Follow these steps to get started:</p>
        <ol>
            <li><b>Enter Birth Information:</b> Fill in the name, date, time, and location of birth in the input fields.</li>
            <li><b>Generate Chart:</b> Click the "Calculate Chart" button to create the astrological chart.</li>
            <li><b>View Chart:</b> The chart will appear in the main display area.</li>
            <li><b>Analyze Aspects:</b> The aspect grid shows relationships between planets.</li>
            <li><b>Get AI Interpretation:</b> Click "Get Birth Chart From AI" or "Get AI Prediction" to receive an interpretation of the chart or a prediction.</li>
        </ol>
        <h3>Chart Features</h3>
        <ul>
            <li><b>Planets:</b> The chart displays the positions of celestial bodies at the time of birth.</li>
            <li><b>Houses:</b> The twelve houses represent different areas of life.</li>
            <li><b>Aspects:</b> Lines connecting planets show their relationships (conjunctions, oppositions, etc.).</li>
            <li><b>Zodiac Signs:</b> The twelve signs of the zodiac form the outer wheel of the chart.</li>
        </ul>
        <h3>Getting AI Interpretations</h3>
        <p>Asteria L works with any AI provider that uses the OpenAI-compatible API format. This requires an API key from a provider, which can seem intimidating if you've never done it before — the walkthrough below covers the easiest option step by step.</p>
        <ol>
            <li><b>Choose an AI provider:</b>
                <ul>
                    <li><b>Easiest for beginners: OpenRouter.</b> One account and one API key give you access to dozens of models (DeepSeek, Kimi, Llama, Gemma, Mistral, and more) — including several completely <b>free</b> models, so you can try Asteria's AI features without paying anything or providing billing details.</li>
                    <li>Recommended paid models (via OpenRouter or directly): <b>DeepSeek v4 Pro</b> or <b>Kimi K2.6</b> — both deliver excellent astrological interpretations at low cost.</li>
                    <li>Also compatible with OpenAI, Groq, Mistral, Ollama (local), and any OpenAI-compatible endpoint.</li>
                    <li><b>Not compatible:</b> Anthropic Claude and Google Gemini use different API formats and will not work.</li>
                </ul>
            </li>
            <li><b>Get a free API key from OpenRouter (recommended path):</b>
                <ul>
                    <li>Go to <a href="https://openrouter.ai/">openrouter.ai</a> and sign up (email, Google, or GitHub — no credit card required).</li>
                    <li>Once logged in, click your account icon (top right) and go to <b>Keys</b>, or go directly to <a href="https://openrouter.ai/settings/keys">openrouter.ai/settings/keys</a>.</li>
                    <li>Click <b>Create Key</b>, give it any name (e.g. "Asteria"), and click Create.</li>
                    <li>Copy the key immediately — it starts with <code>sk-or-</code> and is only shown once. Store it somewhere safe.</li>
                    <li>To find a free model, browse <a href="https://openrouter.ai/models?max_price=0">openrouter.ai/models (filtered to free)</a>. Free model IDs end in <code>:free</code>, for example <code>deepseek/deepseek-chat-v3.1:free</code> or <code>meta-llama/llama-3.3-70b-instruct:free</code>. Availability changes over time, so check the current list.</li>
                    <li>Free models may be slower, rate-limited, or occasionally unavailable — if you outgrow them, the same OpenRouter key works with any paid model on the site, you just change the Model Name.</li>
                </ul>
            </li>
            <li><b>If you'd rather use a different provider:</b> get your API key from that provider's own developer console/dashboard instead (e.g. Mistral, OpenAI, Groq) and keep it secure.</li>
            <li><b>Configure a model in Asteria L:</b>
                <ul>
                    <li>Go to <b>Settings → Configure AI Models</b></li>
                    <li>Click <b>Add</b> and fill in:
                        <ul>
                            <li>Provider: <code>OpenRouter</code> (or your chosen provider's name)</li>
                            <li>Endpoint URL: <code>https://openrouter.ai/api/v1/chat/completions</code> (for OpenRouter)</li>
                            <li>API Key: paste the key you copied</li>
                            <li>Model Name: the model ID, e.g. <code>deepseek/deepseek-chat-v3.1:free</code></li>
                        </ul>
                    </li>
                    <li>Set it as the active model and click Save</li>
                </ul>
            </li>
            <li><b>Using AI features:</b>
                <ul>
                    <li>After calculating a birth chart, click "Get Birth Chart From AI" to receive a detailed interpretation</li>
                    <li>For future predictions, set your desired date range and click "Get AI Prediction"</li>
                    <li>The AI will analyze the astrological data and provide personalized insights</li>
                </ul>
            </li>
        </ol>
        <p><b>Note:</b> Cloud providers require an internet connection; paid models charge per use, but OpenRouter's free (<code>:free</code>) models cost nothing. For a fully offline, free alternative, install <a href="https://ollama.ai/">Ollama</a> and configure it with endpoint <code>http://localhost:11434/v1</code>.</p>
        <h3>Tips & Features</h3>
        <ul>
            <li>Hover your mouse over planets, signs, and houses on the chart to see detailed tooltips containing valuable information.</li>
            <li>You can also hover at the edges of the different panels of the app (such as the Planets-Aspectarian-Elements panel, the AI interpretation panel etc. Just hover you mouse and find out). When you see the mouse cursor change to a resize cursor, you can click and drag to resize these panels for better viewing and a more comfortable layout.</li>
            <li>Asteria can generate AI-powered future predictions for a period of up to 30 days. Play with it! You can set the starting date anytime in the future. Asteria can give insight into how future transits may affect your chart.</li>
            <li>You can select the language used by the AI from the dropdown menu at the bottom right corner of the UI. This feature is still experimental—feel free to explore, but English is recommended for best results.</li>
            <li>For the most immersive and clear view of the chart, it is best to use Asteria in full screen mode.</li>
        </ul>
        <p>For accurate charts, ensure the birth time and location are as precise as possible. Use the "Astrological Symbols" reference to understand the chart's symbols. You can also save or print charts from the File menu.</p>
        <p>For more information about astrology and chart interpretation, consult astrological resources or books.</p>
        )";
        textBrowser->setHtml(helpText);
        layout->addWidget(textBrowser);
        // Add a close button at the bottom
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *closeButton = new QPushButton("Close", m_howToUseDialog);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        layout->addLayout(buttonLayout);
        // Connect the close button
        connect(closeButton, &QPushButton::clicked, m_howToUseDialog, &QDialog::close);
        // Connect the dialog's finished signal to handle cleanup
        connect(m_howToUseDialog, &QDialog::finished, this, [this]() {
            m_howToUseDialog->deleteLater();
            m_howToUseDialog = nullptr;
        });
    }
    // Show and raise the dialog to bring it to the front
    m_howToUseDialog->show();
    m_howToUseDialog->raise();
    m_howToUseDialog->activateWindow();
}


void MainWindow::onOpenMapClicked()
{
    OSMMapDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QGeoCoordinate coords = dialog.selectedCoordinates();

        // Determine direction
        QString latDir = (coords.latitude() >= 0) ? "N" : "S";
        QString longDir = (coords.longitude() >= 0) ? "E" : "W";

        // Update only the Google coordinates field
        m_googleCoordsEdit->setText(QString("%1° %2, %3° %4")
                                    .arg(qAbs(coords.latitude()), 0, 'f', 4)
                                    .arg(latDir)
                                    .arg(qAbs(coords.longitude()), 0, 'f', 4)
                                    .arg(longDir));

        // The lat/long edits will be automatically updated by your existing onTextChanged handler
    }
}

QString MainWindow::getOrbDescription(double orb) {
    if (orb <= 7.0)
        return "Conservative/Tight";
    else if (orb <= 9.0)
        return "Moderate";
    else if (orb <= 10.5)
        return "Standard";
    else
        return "Liberal/Wide";
}


void MainWindow::preloadMapResources() {
    // Create a hidden instance of the map dialog to preload QML
    OSMMapDialog *preloadDialog = new OSMMapDialog(this);
    preloadDialog->hide();  // Make sure it's hidden

    // Schedule deletion after a short delay to ensure QML is fully loaded
    QTimer::singleShot(1000, [preloadDialog]() {
        preloadDialog->deleteLater();
    });
}

void MainWindow::showAspectSettings()
{
    AspectSettingsDialog dialog(this);

    // If the user accepts the dialog (clicks Save)
    if (dialog.exec() == QDialog::Accepted) {
        if (m_chartCalculated) {
            displayChart(m_currentChartData);
        }
    }
}


/////////////////////////////////////Relationship charts

