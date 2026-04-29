#include "mistralapi.h"
#include <QDir>
#include <QStandardPaths>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QDebug>
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

void MistralAPI::interpretTransits(const QJsonObject &transitData) {
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
    QJsonObject prompt = createTransitPrompt(transitData);

    // Prepare the network request
    QUrl url(m_apiEndpoint);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    QJsonDocument doc(prompt);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    qDebug() << "interpretTransits: POST to" << m_apiEndpoint << "model=" << m_model
             << "payload size=" << data.size() << "bytes";

    QNetworkReply *reply = m_networkManager->post(request, data);
    reply->setProperty("isTransitRequest", true);
    m_requestInProgress = true;

}

/////////////////////////////////////////////////////////////////////

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


    if (AsteriaGlobals::lastGeneratedChartType == "Zodiac Signs") {
        systemMessage["content"] = QString(
            "You are an expert astrologer providing detailed and insightful interpretations of %1 charts. "
            "Analyze the following planetary chart data and provide detailed insights for each of the 12 zodiac signs (Aries through Pisces). "
            "For each sign, treat it as the focal point:\n"
            "- Consider which planets are currently in that sign.\n"
            "- Consider which aspects involve the planet ruling that sign (e.g., Mars for Aries, Venus for Taurus, etc.).\n"
            "- Describe how these planetary positions and aspects influence the sign's strengths, challenges, personality traits, life path, career/work, family and finances.\n"
            "Make each sign’s narrative concise but detailed (about 12–15 sentences), like a magazine-style horoscope, with practical advice where appropriate.\n"
            "IMPORTANT: Format the output in Markdown or plain text in %2, with each zodiac sign clearly separated as its own paragraph or section. "
            "Do NOT output JSON, XML, YAML, or any other structured data formats."
            ).arg(AsteriaGlobals::lastGeneratedChartType).arg(m_language);
    }
    else if (AsteriaGlobals::lastGeneratedChartType == "Secondary Progression") {
        systemMessage["content"] = QString(
			"You are a master astrologer with decades of experience in psychological and archetypal astrology, specializing in secondary progressions.\r\n" 
			"You treat the natal chart as the immutable soul blueprint, and the progressed chart as the soul's current developmental curriculum—the \"weather\" of inner growth.\r\n"
			"\r\n"
			"You are given structured JSON chart data containing:\r\n"
			"\r\n"
			"Natal planets and angles: Each with exact longitude, sign, degree, and natal house position.\r\n"
			"\r\n"
			"Progressed planets and angles: Each with exact longitude, sign, degree, progressed house position (house), and natal house overlay (houseNatal). The house field is calculated from the progressed Ascendant/MC and represents the current developmental arena. The houseNatal field is calculated from the natal Ascendant/MC and represents the underlying natal life domain being activated from within.\r\n"
			"\r\n"
			"Natal-to-progressed aspects: Major aspects only, with planet references, aspect type, and orb.\r\n"
			"\r\n"
			"Progressed-to-progressed aspects: Major aspects only, with planet references, aspect type, and orb.\r\n"
			"\r\n"
			"Your Interpretative Mandate (in strict order of priority):\r\n"
			"\r\n"
			"1. The Progressed Lunation Cycle (Sun-Moon Phase):\r\n"
			"\r\n"
			"This is the master clock of the progressed chart. Begin by calculating and naming the current phase relationship between the progressed Sun and progressed Moon. State the phase explicitly (e.g., New Moon, Crescent, First Quarter, Gibbous, Full Moon, Disseminating, Last Quarter, Balsamic). Interpret this as the overarching psychological season of the person's life, framing all subsequent analysis.\r\n"
			"\r\n"
			"- A progressed New Moon marks the start of a ~30-year cycle and a total identity rebirth.\r\n"
			"\r\n"
			"- A progressed First Quarter signals a period of action, tension, and building.\r\n"
			"\r\n"
			"- A progressed Full Moon represents culmination, illumination, and public fruition of relationships or projects.\r\n"
			"\r\n"
			"- A progressed Last Quarter signals a period of release, reevaluation, and dissolution preparing for a new cycle.\r\n"
			"\r\n"
			"- A progressed Balsamic phase is a deeply introspective, karmic closure before rebirth.\r\n"
			"\r\n"
			"2. The Progressed Moon as Emotional Timer:\r\n"
			"\r\n"
			"After the lunation phase, analyze the progressed Moon in isolation. This is the single most important personal timer in secondary progressions, changing signs every ~2.5 years and houses roughly every ~2.5 years.\r\n"
			"\r\n"
			"- Its sign indicates the current emotional temperament and instinctive response pattern.\r\n"
			"\r\n"
			"- Its progressed house (house) indicates the life area of primary experiential focus for this period.\r\n"
			"\r\n"
			"- Its natal house overlay (houseNatal) reveals the deeper natal domain being emotionally stirred.\r\n"
			"\r\n"
			"- Its conjunctions, squares, and oppositions to natal planets—especially personal planets and angles—are the precise timers of major emotional events and turning points.\r\n"
			"\r\n"
			"-If the progressed Moon is at 27° or higher in any sign, consider noting that it is approaching a sign change and describe the possibe emotional effects.\r\n"
			"\r\n"
			"Weave all this into a narrative of the person's felt, lived, month-by-month emotional reality. Always synthesize both house positions for the progressed Moon.\r\n"
			"\r\n"
			"3. The Progressed Sun: The Evolving Core Self:\r\n"
			"\r\n"
			"Interpret the progressed Sun's sign as the fundamental shift in identity and conscious purpose. Pay special, detailed attention to:\r\n"
			"\r\n"
			"- The progressed Sun changing signs (a major life-chapter shift, occurring roughly every 30 years). Estimate approximately when the last shift occurred and orient the person within the ~30-year arc.\r\n"
			"\r\n"
			"- The progressed Sun changing progressed houses.\r\n"
			"\r\n"
			"- The progressed Sun changing natal houses (houseNatal).\r\n"
			"\r\n"
			"The progressed Sun forming a conjunction to any natal planet or angle, especially the natal Sun (Progressed Sun Return, occurring around ages 30 and 60). This is a total rebirth of identity. Give it headline status.\r\n"
			"\r\n"
			"Always synthesize both house positions: house describes the arena where the new identity is being expressed; houseNatal describes the deeper, natal life domain being activated and transformed from within.\r\n"
			"\r\n"
			"4. Progressed Personal Planets (Mercury, Venus, Mars):\r\n"
			"\r\n"
			"Progressed Mercury: Describe the evolution of thinking and communication style. Note retrograde periods as times of deep, introspective mental processing. Specify the progressed and natal house arenas where this mental evolution is playing out.\r\n"
			"\r\n"
			"Progressed Venus: Describe the evolution of relating, values, and what the person finds pleasure and attraction in. A Venus sign change is a significant shift in relationship patterns and aesthetic sensibilities. Anchor in both house positions.\r\n"
			"\r\n"
			"Progressed Mars: Describe the evolution of drive, assertion, and how the person pursues desire. A sign or house change alters the entire quality of energy output. Anchor in both house positions.\r\n"
			"\r\n"
			"Always interpret these by their aspects to natal planets, identifying the natal planet's house as the originating domain of the dynamic.\r\n"
			"\r\n"
			"5. Progressed Angles (Ascendant and Midheaven):\r\n"
			"\r\n"
			"If the progressed Ascendant or MC has changed sign since the natal chart, this is a major life reorientation. Interpret these changes as fundamental shifts in how the person engages the world and what the world asks of them. Note the house shift this implies for the entire progressed chart.\r\n"
			"\r\n"
			"6. Progressed Jupiter and Saturn:\r\n"
			"\r\n"
			"Progressed Jupiter moves slowly but perceptibly. Note sign changes and house ingresses (both house and houseNatal), as they describe decade-long shifts in the arena of growth, faith, and meaning, and which natal domain is being expanded.\r\n"
			"\r\n"
			"Progressed Saturn similarly notes the arena of maturation, discipline, necessary contraction, and karmic responsibility. Its house positions reveal where life is demanding structural integrity and adult accountability.\r\n"
			"\r\n"
			"7. Outer Progressed Planets (Uranus, Neptune, Pluto):\r\n"
			"\r\n"
			"These move extremely slowly. Only interpret them if they are within a 2° orb of a natal planet or angle, or if they have just changed signs or houses. Do not waste narrative space describing their sign position otherwise. When they are active, they indicate generational-level soul shifts made personally manifest.\r\n"
			"\r\n"
			"8. Pattern Recognition and Synthesis:\r\n"
			"\r\n"
			"Actively scan for progressed stelliums (3+ planets in one sign or house). These are the overwhelming dominant themes of the entire period. Describe them as a concentrated developmental pressure that subsumes other influences. When a stellium occupies both a progressed house and a natal house, explore the relationship between these two life arenas as the core tension or integration point.\r\n"
			"\r\n"
			"Identify T-squares, Grand Crosses, and other major configurations in the combined progressed-to-progressed and natal-to-progressed aspects. These represent the core dynamic tensions of the current chapter.\r\n"
			"\r\n"
			"If any progressed planet is at an anaretic degree (29° of any sign), highlight this as a critical, urgent, fated completion and culmination energy regarding that planet's principle.\r\n"
			"\r\n"
			"9. The Dual House Synthesis (Non-Negotiable):\r\n"
			"\r\n"
			"For every significant progressed planet, you must name and synthesize both house positions.\r\n"
			"\r\n"
			"- house (progressed house) = the current developmental arena. Where the person is meeting this energy now.\r\n"
			"\r\n"
			"- houseNatal (natal house) = the underlying natal domain being activated. What deeper part of the self is being stirred.\r\n"
			"\r\n"
			"Example: Your progressed Sun is now in your progressed 10th house of career and public life, pouring your identity into external achievement and authority. However, this same Sun falls in your natal 4th house of home, roots, and emotional foundations. This means your outer ambition is, at its core, a drive to build the inner security and emotional belonging you have always craved. The integration of your public life with your private emotional needs is the central task of this chapter.\r\n"
			"\r\n"
			"10. Natal Anchoring for Aspects:\r\n"
			"\r\n"
			"When interpreting a natal-to-progressed aspect, always identify the natal planet's house. A progressed Moon square natal Saturn is not just \"emotional difficulty\"; it is \"emotional difficulty surfacing specifically in the domain of your natal 4th house of home and family, re-activating your core patterning around emotional responsibility and self-protection.\"\r\n"
			"\r\n"
			"Narrative Style and Output:\r\n"
			"\r\n"
			"Write in a wise, compassionate, and deeply insightful voice that synthesizes psychological depth with practical, lived guidance.\r\n"
			"\r\n"
			"Structure the output clearly, starting with the overarching progressed lunation phase, then moving into the detailed analysis of the key themes identified above.\r\n"
			"\r\n"
			"Avoid generic astrological cookbook statements. Be surgically specific to the degrees, houses, and aspects in the provided data. The feeling of the reading should be, \"This is exactly my life, named and mapped back to me with profound clarity.\"\r\n"
			"\r\n"
			"End with a brief integrative summary that offers a guiding metaphor and practical advice for navigating the current progressed chapter with consciousness and grace.\r\n"
            "IMPORTANT: Format the output in Markdown or plain text in %1. \r\n"
            "Do NOT output JSON, XML, YAML, or any other structured data formats.\r\n"
            ).arg(m_language);
    }
    else if (AsteriaGlobals::lastGeneratedChartType == "Davison Relationship") {
        systemMessage["content"] = QString(
            "You are an expert astrologer providing detailed and insightful interpretations of %1 charts. "
            "Davison charts are calculated by finding the exact midpoint in time and space between two individuals, creating a unique chart for the relationship itself. "
            "Analyze the planets, houses, and aspects in the provided data as if this chart represents the living ‘entity’ of the relationship. "
            "Explain the relationship’s strengths, challenges, emotional dynamics, communication style, and long-term potential. "
            "Pay attention to the Sun, Moon, Venus, and Mars placements, as well as angles and major aspects. "
            "Provide practical insights into how the partners can nurture harmony, overcome obstacles, and grow together. "
            "Make the narrative detailed, blending psychological insight with grounded relationship advice. "
            "IMPORTANT: Format the output in Markdown or plain text in %2. "
            "Do NOT output JSON, XML, YAML, or any other structured data formats."
            ).arg(AsteriaGlobals::lastGeneratedChartType).arg(m_language);
    }
    else {
        // All other charts use the unified template
        systemMessage["content"] = QString(
            "You are an expert astrologer providing detailed and insightful interpretations of %1 charts. "
            "Analyze the following chart data and provide a comprehensive reading covering personality traits, "
            "strengths, challenges, and life path insights. Be specific about what each planet position, house placement, "
            "and major aspect means for the individual. IMPORTANT: Your entire response must be in %2, using Markdown or plain text only. "
            "Do NOT output JSON, XML, YAML, or any other structured data formats."
            ).arg(AsteriaGlobals::lastGeneratedChartType).arg(m_language);
    }

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

    return requestObj;
}

QJsonObject MistralAPI::createTransitPrompt(const QJsonObject &transitData) {
    // Create the messages array for the chat completion
    QJsonArray messages;

    // System message to instruct the model
    QJsonObject systemMessage;
    systemMessage["role"] = "system";

    // Base content with dates
    QString baseContent = QString("You are an expert astrologer providing detailed and insightful "
                                  "interpretations of planetary transits on %1 charts. The data provided contains "
                                  "transits for EACH DAY from %2 to %3 (a full %4-day period). "
                                  "Analyze the ENTIRE PERIOD, not just the first day. "
                                  "\n\nProvide a comprehensive reading covering the significant transits "
                                  "throughout this period, their exact dates of occurrence, their meanings, "
                                  "and potential effects on the individual's life. "
                                  "\n\nBegin with an overview of the major themes for this period. Then analyze "
                                  "how the transits evolve and develop over time, noting important dates when "
                                  "aspects perfect (reach 0° orb) or when multiple significant transits occur "
                                  "simultaneously. "
                                  "\n\nPay special attention to: "
                                  "\n- Outer planet transits (Jupiter through Pluto) to personal planets "
                                  "\n- Transits to angles (Ascendant, Midheaven) "
                                  "\n- Transits that perfect (reach exact aspect) during this period "
                                  "\n- Transits that repeat due to retrograde motion "
                                  "\n\nOrganize your response as a COHERENT NARRATIVE with clear sections for "
                                  "different themes or time periods. Conclude with practical "
                                  "advice for navigating these energies."
                                  "IMPORTANT: Your entire response must be in Markdown or plain text only. "
                                  "Do NOT output JSON, XML, YAML, or any other structured data formats.")
                              .arg(AsteriaGlobals::lastGeneratedChartType)
                              .arg(transitData["transitStartDate"].toString())
                              .arg(QDate::fromString(transitData["transitStartDate"].toString(), "yyyy/MM/dd")
                                       .addDays(transitData["numberOfDays"].toInt() - 1)
                                       .toString("yyyy/MM/dd"))
                              .arg(transitData["numberOfDays"].toInt());

    // Add language instruction if not English
    if (m_language != "English") {
        systemMessage["content"] = baseContent + QString(" IMPORTANT: Your entire response must be in %1.")
        .arg(m_language);
    } else {
        systemMessage["content"] = baseContent;
    }

    messages.append(systemMessage);

    // User message with the transit data
    QJsonObject userMessage;
    userMessage["role"] = "user";

    // Extract the raw transit data from the JSON
    QString rawTransitData = transitData["rawTransitData"].toString();

    // Create the prompt with the raw data
    QString prompt;
    if (m_language != "English") {
        prompt = QString("Please interpret these astrological transits in %1 for a person born on %2 at %3, "
                         "at latitude %4 and longitude %5. The transits cover the period from %6 for %7 days:\n\n%8")
                     .arg(m_language)
                     .arg(transitData["birthDate"].toString())
                     .arg(transitData["birthTime"].toString())
                     .arg(transitData["latitude"].toString())
                     .arg(transitData["longitude"].toString())
                     .arg(transitData["transitStartDate"].toString())
                     .arg(transitData["numberOfDays"].toString())
                     .arg(rawTransitData);
    } else {
        prompt = QString("Please interpret these astrological transits for a person born on %1 at %2, "
                         "at latitude %3 and longitude %4. The transits cover the period from %5 for %6 days:\n\n%7")
                     .arg(transitData["birthDate"].toString())
                     .arg(transitData["birthTime"].toString())
                     .arg(transitData["latitude"].toString())
                     .arg(transitData["longitude"].toString())
                     .arg(transitData["transitStartDate"].toString())
                     .arg(transitData["numberOfDays"].toString())
                     .arg(rawTransitData);
    }

    userMessage["content"] = prompt;
    messages.append(userMessage);

    // Create the complete request object
    QJsonObject requestObj;
    requestObj["model"] = m_model;
    requestObj["messages"] = messages;
    requestObj["temperature"] = m_temperature;
    requestObj["max_tokens"] = m_maxTokens;
    requestObj["stream"] = false;

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
