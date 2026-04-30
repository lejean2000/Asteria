#include "mistralapi.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QDebug>
#include <QDate>
#include <QRegularExpression>
#include <cmath>
#include"Globals.h"

//#include<QNetworkRequest>
//#include<QByteArray>
MistralAPI::MistralAPI(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_requestInProgress(false)
    , m_language("English")
{
    // Connect network reply signal
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &MistralAPI::handleNetworkReply);

    // Try to load API key from settings
    AsteriaGlobals::activeModelLoaded = loadActiveModel();
}

MistralAPI::~MistralAPI()
{
    // QObject parent-child relationship will handle deletion
}

void MistralAPI::interpretChart(const QJsonObject &chartData)
{
    if (m_requestInProgress) {
        m_lastError = "A request is already in progress";
        emit error(m_lastError);
        return;
    }

    if (!AsteriaGlobals::activeModelLoaded) {
        m_lastError = "No active AI model configured. Please configure one in Settings → Configure AI Models.";;
        emit error(m_lastError);
        return;
    }


    // Create the prompt for Mistral
    QJsonObject prompt = createPrompt(chartData);

    QUrl url(m_apiEndpoint);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    QJsonDocument doc(prompt);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    qDebug() << "interpretChart: POST to" << m_apiEndpoint << "model=" << m_model
             << "payload size=" << data.size() << "bytes";

    m_networkManager->post(request, data);
    m_requestInProgress = true;

}


void MistralAPI::handleNetworkReply(QNetworkReply *reply) {
    m_requestInProgress = false;

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();

    qDebug() << "handleNetworkReply: HTTP status=" << httpStatus
             << "Qt error=" << reply->error()
             << "body (" << responseData.size() << "bytes)=" << QString::fromUtf8(responseData);

    // Check for network errors (includes HTTP 4xx/5xx on Qt side)
    if (reply->error() != QNetworkReply::NoError) {
        QString body = QString::fromUtf8(responseData).left(500);
        m_lastError = QString("HTTP %1 — %2\nResponse body: %3")
                          .arg(httpStatus)
                          .arg(reply->errorString())
                          .arg(body.isEmpty() ? "(empty)" : body);
        emit error(m_lastError);
        reply->deleteLater();
        return;
    }

    // Parse the response
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        m_lastError = QString("Invalid JSON response (HTTP %1). Raw body: %2")
                          .arg(httpStatus)
                          .arg(QString::fromUtf8(responseData).left(500));
        emit error(m_lastError);
        reply->deleteLater();
        return;
    }

    QJsonObject responseObj = doc.object();

    // Format the response
    QString formattedResponse = formatInterpretation(responseObj);
    if (formattedResponse.isEmpty()) {
        m_lastError = QString("Failed to extract response from API (HTTP %1). Raw JSON: %2")
                          .arg(httpStatus)
                          .arg(QString::fromUtf8(responseData).left(500));
        emit error(m_lastError);
    } else {
        // Determine which signal to emit based on the request type
        if (reply->property("isTransitRequest").toBool()) {
            emit transitInterpretationReady(formattedResponse);
        } else {
            emit interpretationReady(formattedResponse);
        }
    }

    reply->deleteLater();
}

QString MistralAPI::formatInterpretation(const QJsonObject &response)
{
    // Extract the interpretation from the Mistral API response
    if (!response.contains("choices") || !response["choices"].isArray()) {
        return QString();
    }

    QJsonArray choices = response["choices"].toArray();
    if (choices.isEmpty() || !choices[0].isObject()) {
        return QString();
    }

    QJsonObject choice = choices[0].toObject();
    if (!choice.contains("message") || !choice["message"].isObject()) {
        return QString();
    }

    QJsonObject message = choice["message"].toObject();
    if (!message.contains("content") || !message["content"].isString()) {
        return QString();
    }

    return message["content"].toString();
}

QString MistralAPI::getSettingsPath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appDataPath);

    // Create directory if it doesn't exist
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return appDataPath + "/settings.ini";
}

QString MistralAPI::getLastError() const
{
    return m_lastError;
}

///////////////////////Predictions

QJsonArray MistralAPI::buildGroupedTransitJson(const QString &rawTransitData) const
{
    static const QMap<QString, QString> majorAspects = {
        {"Conjunction", "CON"}, {"Opposition", "OPP"},
        {"Trine", "TRI"}, {"Square", "SQR"}, {"Sextile", "SEX"}
    };
    static const QRegularExpression dateLineRe(R"(^(\d{4}/\d{2}/\d{2}): (.*)$)");
    static const QRegularExpression aspectRe(
        "((?:North|South) Node|Pars Fortuna|Part of Spirit|East Point|\\w+)"
        "(?:\\s+\\(R\\))? (\\w+) "
        "((?:North|South) Node|Pars Fortuna|Part of Spirit|East Point|\\w+(?:\\s+\\(R\\))?)"
        " \\(([\\d.]+)°\\)");

    struct GroupMeta { QString transitPlanet, natalPlanet, aspect; };
    QMap<QString, GroupMeta> groupMeta;
    QMap<QString, QMap<QString, QString>> groupDates;
    QList<QString> keyOrder;

    bool inSection = false;
    for (const QString &line : rawTransitData.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith("---TRANSITS---")) { inSection = true; continue; }
        if (!inSection) continue;

        auto dateMatch = dateLineRe.match(line);
        if (!dateMatch.hasMatch()) continue;

        QString date = QDate::fromString(dateMatch.captured(1), "yyyy/MM/dd").toString("yyyy-MM-dd");

        for (const QString &token : dateMatch.captured(2).split(',', Qt::SkipEmptyParts)) {
            auto m = aspectRe.match(token.trimmed());
            if (!m.hasMatch()) continue;

            QString transitPlanet = m.captured(1);
            QString aspectName    = m.captured(2);
            QString natalPlanet   = m.captured(3);
            if (natalPlanet.endsWith(" (R)")) natalPlanet.chop(4);
            double orb = m.captured(4).toDouble();

            if (!majorAspects.contains(aspectName) || orb > 5.0) continue;

            QString abbrev = majorAspects[aspectName];
            QString key = transitPlanet + "|" + natalPlanet + "|" + abbrev;

            if (!groupDates.contains(key)) {
                keyOrder.append(key);
                groupMeta[key] = {transitPlanet, natalPlanet, abbrev};
            }
            groupDates[key][date] = QString::number(orb, 'f', 2);
        }
    }

    QJsonArray result;
    for (const QString &key : keyOrder) {
        const auto &meta = groupMeta[key];
        QJsonObject obj;
        obj["transit_planet"] = meta.transitPlanet;
        obj["natal_planet"]   = meta.natalPlanet;
        obj["aspect"]         = meta.aspect;
        QJsonObject dateOrb;
        for (auto it = groupDates[key].constBegin(); it != groupDates[key].constEnd(); ++it)
            dateOrb[it.key()] = it.value();
        obj["date_orb"] = dateOrb;
        result.append(obj);
    }
    return result;
}

void MistralAPI::interpretTransits(const QJsonObject &transitData) {
    if (m_requestInProgress) {
        m_lastError = "A request is already in progress";
        emit error(m_lastError);
        return;
    }

    if (!AsteriaGlobals::activeModelLoaded) {
        m_lastError = "No active AI model configured. Please configure one in Settings → Configure AI Models.";
        emit error(m_lastError);
        return;
    }

    QJsonObject prompt = createTransitPrompt(transitData);
    // createTransitPrompt already logs the system prompt (first 120 chars)
    QString userContent = prompt["messages"].toArray()[1].toObject()["content"].toString();
    qDebug() << "interpretTransits: user message:" << userContent;

    QJsonDocument doc(prompt);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    QUrl url(m_apiEndpoint);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    qDebug() << "interpretTransits: POST to" << m_apiEndpoint << "model=" << m_model
             << "payload size=" << data.size() << "bytes";

    QNetworkReply *reply = m_networkManager->post(request, data);
    reply->setProperty("isTransitRequest", true);
    m_requestInProgress = true;
}

/////////////////////////////////////////////////////////////////////

// Load a system prompt from :/resources/<Chart_Type_With_Underscores>.md,
// falling back to :/resources/Default.md when no specific file exists.
static QString loadSystemPromptFromResource(const QString &chartType)
{
    auto tryLoad = [](const QString &path) -> QString {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        QString content = QString::fromUtf8(f.readAll());
        f.close();
        return content;
    };
    QString name = QString(chartType).replace(' ', '_');
    QString specificPath = QString(":/resources/%1.md").arg(name);
    QString content = tryLoad(specificPath);
    qDebug() << "loadSystemPromptFromResource: tried" << specificPath << "found:" << !content.isEmpty();
    if (content.isEmpty())
        content = tryLoad(":/resources/Default.md");
    return content;
}

// Recursively round all JSON doubles to the given number of decimal places.
static QJsonValue roundJsonDoubles(const QJsonValue &val, int decimals = 1)
{
    if (val.isDouble()) {
        double factor = std::pow(10.0, decimals);
        return QJsonValue(std::round(val.toDouble() * factor) / factor);
    }
    if (val.isArray()) {
        QJsonArray arr;
        for (const QJsonValue &v : val.toArray())
            arr.append(roundJsonDoubles(v, decimals));
        return arr;
    }
    if (val.isObject()) {
        QJsonObject obj = val.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            obj[it.key()] = roundJsonDoubles(it.value(), decimals);
        return obj;
    }
    return val;
}

QJsonObject MistralAPI::createPrompt(const QJsonObject &chartData) {
    // Create the messages array for the chat completion
    QJsonArray messages;

    // System message to instruct the model
    QJsonObject systemMessage;
    systemMessage["role"] = "system";

    systemMessage["content"] = loadSystemPromptFromResource(AsteriaGlobals::lastGeneratedChartType)
        + QString("\n\nIMPORTANT: Format the output in Markdown or plain text in %1. "
                  "Do NOT output JSON, XML, YAML, or any other structured data formats.")
              .arg(m_language);

    qDebug() << "createPrompt: chartType=" << AsteriaGlobals::lastGeneratedChartType
             << "systemPrompt(first 120 chars)=" << systemMessage["content"].toString().left(120);

    messages.append(systemMessage);

    // User message with the chart data
    QJsonObject userMessage;
    userMessage["role"] = "user";

    QJsonObject roundedData = roundJsonDoubles(chartData).toObject();
    QString chartJson = QString::fromUtf8(
        QJsonDocument(roundedData).toJson(QJsonDocument::Compact));

    if (m_language != "English") {
        userMessage["content"] = QString("Please interpret this astrological chart in %1: %2")
            .arg(m_language)
            .arg(chartJson);
    } else {
        userMessage["content"] = QString("Please interpret this astrological chart: %1")
            .arg(chartJson);
    }

    messages.append(userMessage);

    // Create the complete request object
    QJsonObject requestObj;
    requestObj["model"] = m_model;
    requestObj["messages"] = messages;
    requestObj["temperature"] = m_temperature;
    requestObj["max_tokens"] = m_maxTokens;
    requestObj["stream"] = false;

    if (AsteriaGlobals::lastGeneratedChartType == "Secondary Progression") {
        requestObj["reasoning_effort"] = QString("high");
    }

    return requestObj;
}

QJsonObject MistralAPI::createTransitPrompt(const QJsonObject &transitData) {
    // System prompt from resource file (Transits.md → Default.md fallback)
    QString systemContent = loadSystemPromptFromResource(AsteriaGlobals::lastGeneratedChartType)
        + QString("\n\nIMPORTANT: Format the output in Markdown or plain text in %1. "
                  "Do NOT output JSON, XML, YAML, or any other structured data formats.")
              .arg(m_language);
    if (m_language != "English")
        systemContent += QString(" Your entire response must be in %1.").arg(m_language);

    qDebug() << "createTransitPrompt: chartType=" << AsteriaGlobals::lastGeneratedChartType
             << "systemPrompt(first 120 chars)=" << systemContent.left(120);

    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = systemContent;

    // Build compact combined JSON (major aspects ≤5° grouped by transit, natal context)
    QJsonArray grouped = buildGroupedTransitJson(transitData["rawTransitData"].toString());
    QJsonObject natalObj;
    if (transitData.contains("natalPlanets")) natalObj["planets"] = transitData["natalPlanets"];
    if (transitData.contains("natalAngles"))  natalObj["angles"]  = transitData["natalAngles"];
    QJsonObject combined;
    combined["transits"] = grouped;
    combined["natal"]    = natalObj;
    QString compactJson = QString::fromUtf8(QJsonDocument(combined).toJson(QJsonDocument::Compact));

    // Compute period string
    QString startDate = transitData["transitStartDate"].toString();
    QString endDate   = QDate::fromString(startDate, "yyyy-MM-dd")
                            .addDays(transitData["numberOfDays"].toString().toInt() - 1)
                            .toString("yyyy-MM-dd");

    // User message: short prefix + compact JSON payload
    QString userText = m_language != "English"
        ? QString("Prepare transit interpretation in %1 based on the following natal and transit data "
                  "for period %2 to %3: ").arg(m_language, startDate, endDate)
        : QString("Prepare transit interpretation based on the following natal and transit data "
                  "for period %1 to %2: ").arg(startDate, endDate);
    userText += compactJson;

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = userText;

    QJsonArray messages;
    messages.append(systemMessage);
    messages.append(userMessage);

    QJsonObject requestObj;
    requestObj["model"]       = m_model;
    requestObj["messages"]    = messages;
    requestObj["temperature"] = m_temperature;
    requestObj["max_tokens"]  = m_maxTokens;
    requestObj["stream"]      = false;

    return requestObj;
}

bool MistralAPI::loadActiveModel()
{
    QSettings settings;
    settings.beginGroup("Models");

    // Get the active model name
    QString activeModelName = settings.value("ActiveModel").toString();
    if (activeModelName.isEmpty()) {
        m_lastError = "No active model selected";
        AsteriaGlobals::activeModelLoaded = false;
        settings.endGroup();
        return false;
    }

    // Load only the active model's settings
    settings.beginGroup(activeModelName);
    m_apiEndpoint = settings.value("endpoint").toString();
    m_apiKey = settings.value("apiKey").toString();
    m_model = settings.value("modelName").toString();
    m_temperature = settings.value("temperature", 0.8).toDouble();
    m_maxTokens = settings.value("maxTokens", 65536).toInt();

    settings.endGroup(); // closes "Models/SomeModelName"
    settings.endGroup(); // closes "Models"

    bool result = !m_apiEndpoint.isEmpty() && !m_model.isEmpty();
    AsteriaGlobals::activeModelLoaded = result;
    return result;
}
