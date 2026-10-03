#include "daemoncalculator.h"
#include <QJsonArray>
#include <QStringList>
#include <cmath>
#include <algorithm>
#include <tuple>

extern "C" {
#include "swephexp.h"
}

namespace {

// ── Essential dignity tables (signs indexed 0 = Aries) ──────────────────────
const Planet kSignRuler[12] = {
    Planet::Mars, Planet::Venus, Planet::Mercury, Planet::Moon, Planet::Sun, Planet::Mercury,
    Planet::Venus, Planet::Mars, Planet::Jupiter, Planet::Saturn, Planet::Saturn, Planet::Jupiter
};
const Planet kExaltation[12] = {
    Planet::Sun, Planet::Moon, Planet::Unknown, Planet::Jupiter, Planet::Unknown, Planet::Mercury,
    Planet::Saturn, Planet::Unknown, Planet::Unknown, Planet::Mars, Planet::Unknown, Planet::Venus
};
// Triplicity rulers by element (sign % 4: fire, earth, air, water)
const Planet kTriplicityDay[4]   = { Planet::Sun,     Planet::Venus, Planet::Saturn,  Planet::Venus };
const Planet kTriplicityNight[4] = { Planet::Jupiter, Planet::Moon,  Planet::Mercury, Planet::Mars };

struct Term { Planet planet; int end; }; // end degree, exclusive
const Term kTerms[12][5] = { // Egyptian terms
    {{Planet::Jupiter, 6}, {Planet::Venus, 12}, {Planet::Mercury, 20}, {Planet::Mars, 25}, {Planet::Saturn, 30}},   // Aries
    {{Planet::Venus, 8}, {Planet::Mercury, 14}, {Planet::Jupiter, 22}, {Planet::Saturn, 27}, {Planet::Mars, 30}},   // Taurus
    {{Planet::Mercury, 6}, {Planet::Jupiter, 12}, {Planet::Venus, 17}, {Planet::Mars, 24}, {Planet::Saturn, 30}},   // Gemini
    {{Planet::Mars, 7}, {Planet::Venus, 13}, {Planet::Mercury, 19}, {Planet::Jupiter, 26}, {Planet::Saturn, 30}},   // Cancer
    {{Planet::Jupiter, 6}, {Planet::Venus, 11}, {Planet::Saturn, 18}, {Planet::Mercury, 24}, {Planet::Mars, 30}},   // Leo
    {{Planet::Mercury, 7}, {Planet::Venus, 17}, {Planet::Jupiter, 21}, {Planet::Mars, 28}, {Planet::Saturn, 30}},   // Virgo
    {{Planet::Saturn, 6}, {Planet::Mercury, 14}, {Planet::Jupiter, 21}, {Planet::Venus, 28}, {Planet::Mars, 30}},   // Libra
    {{Planet::Mars, 7}, {Planet::Venus, 11}, {Planet::Mercury, 19}, {Planet::Jupiter, 24}, {Planet::Saturn, 30}},   // Scorpio
    {{Planet::Jupiter, 12}, {Planet::Venus, 17}, {Planet::Mercury, 21}, {Planet::Saturn, 26}, {Planet::Mars, 30}},  // Sagittarius
    {{Planet::Mercury, 7}, {Planet::Jupiter, 14}, {Planet::Venus, 22}, {Planet::Saturn, 26}, {Planet::Mars, 30}},   // Capricorn
    {{Planet::Mercury, 7}, {Planet::Venus, 13}, {Planet::Jupiter, 20}, {Planet::Mars, 25}, {Planet::Saturn, 30}},   // Aquarius
    {{Planet::Venus, 12}, {Planet::Jupiter, 16}, {Planet::Mercury, 19}, {Planet::Mars, 28}, {Planet::Saturn, 30}},  // Pisces
};
// Faces (decans) run in Chaldean order starting with Mars at 0° Aries.
const Planet kFaceSequence[7] = {
    Planet::Mars, Planet::Sun, Planet::Venus, Planet::Mercury, Planet::Moon, Planet::Saturn, Planet::Jupiter
};

// House strength, used by the compound chart almuten and the Sun-or-Moon tie-break.
int housePoints(int house)
{
    static const int points[13] = {0, 12, 6, 3, 9, 7, 1, 10, 4, 5, 11, 8, 2};
    return points[house];
}

const int kDayRulerPoints  = 7;
const int kHourRulerPoints = 6;

const QStringList kSignNames = {
    "Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
    "Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces"
};

// The 22 letters, in order. Non-final forms only: every name ends in "el",
// so no cast letter is ever word-final.
const char16_t kHebrew[22] = {
    u'א', u'ב', u'ג', u'ד', u'ה', u'ו', u'ז', u'ח',
    u'ט', u'י', u'כ', u'ל', u'מ', u'נ', u'ס', u'ע',
    u'פ', u'צ', u'ק', u'ר', u'ש', u'ת'
};
const char *const kHebrewNames[22] = {
    "Aleph", "Bet", "Gimel", "Dalet", "He", "Vav", "Zayin", "Het", "Tet", "Yod", "Kaf",
    "Lamed", "Mem", "Nun", "Samekh", "Ayin", "Pe", "Tsadi", "Qof", "Resh", "Shin", "Tav"
};
const char16_t kSuffixEl[2] = { u'א', u'ל' }; // אל

double norm(double lon)
{
    double r = std::fmod(lon, 360.0);
    return r < 0 ? r + 360.0 : r;
}

int signIndex(double lon) { return static_cast<int>(norm(lon) / 30.0) % 12; }
double degInSign(double lon) { return std::fmod(norm(lon), 30.0); }
int wholeDegree(double lon) { return static_cast<int>(std::floor(norm(lon))) % 360; }

// Zodiacal (direct) distance from -> to, 0..360.
double arc(double from, double to) { return norm(to - from); }

Planet termRuler(double lon)
{
    const double d = degInSign(lon);
    for (const Term &t : kTerms[signIndex(lon)])
        if (d < t.end)
            return t.planet;
    return kTerms[signIndex(lon)][4].planet;
}

Planet faceRuler(double lon)
{
    return kFaceSequence[static_cast<int>(norm(lon) / 10.0) % 7];
}

bool isDetriment(Planet p, double lon) { return kSignRuler[(signIndex(lon) + 6) % 12] == p; }
bool isFall(Planet p, double lon) { return kExaltation[(signIndex(lon) + 6) % 12] == p; }

QVector<int> zeroScores() { return QVector<int>(7, 0); }

void addScores(QVector<int> &total, const QVector<int> &row)
{
    for (int i = 0; i < 7; ++i)
        total[i] += row[i];
}

// All planets tied for the top score, in Chaldean order. The source gives no
// tie-break, so ties are reported rather than resolved.
QVector<Planet> argmax(const QVector<int> &scores)
{
    const int best = *std::max_element(scores.begin(), scores.end());
    QVector<Planet> out;
    for (int i = 0; i < 7; ++i)
        if (scores[i] == best)
            out << DaemonCalculator::chaldeanOrder()[i];
    return out;
}

QString planetList(const QVector<Planet> &planets)
{
    QStringList names;
    for (Planet p : planets)
        names << toString(p);
    return names.join(", ");
}

// "Saturn 41, Mercury 25, Jupiter 19" - the top `count` non-zero scores.
QString topScores(const QVector<int> &scores, int count = 3)
{
    QVector<int> order = {0, 1, 2, 3, 4, 5, 6};
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return scores[a] > scores[b]; });
    QStringList parts;
    for (int i : order) {
        if (parts.size() >= count || scores[i] <= 0)
            break;
        parts << QString("%1 %2").arg(toString(DaemonCalculator::chaldeanOrder()[i])).arg(scores[i]);
    }
    return parts.join(", ");
}

QString ordinal(int n)
{
    if (n % 100 >= 11 && n % 100 <= 13)
        return QString::number(n) + "th";
    switch (n % 10) {
    case 1: return QString::number(n) + "st";
    case 2: return QString::number(n) + "nd";
    case 3: return QString::number(n) + "rd";
    default: return QString::number(n) + "th";
    }
}

Planet simpleAlmutenFirst(double lon, bool isDay, QVector<Planet> *all = nullptr)
{
    QVector<Planet> winners = argmax(DaemonCalculator::scoreDegree(lon, isDay));
    if (all)
        *all = winners;
    return winners.first();
}

struct Place { QString name; double lon; };

QVector<Place> hylegicalPlaces(const DaemonInputs &in)
{
    // Fixed order: this is the letter order of the five-letter names.
    return {
        {"Ascendant", in.asc()},
        {"Sun", in.planets[DaemonCalculator::chaldeanIndex(Planet::Sun)]},
        {"Moon", in.planets[DaemonCalculator::chaldeanIndex(Planet::Moon)]},
        {"Part of Fortune", in.partOfFortune()},
        {"Pre-natal lunation", in.prenatalLunation},
    };
}

double planetLon(const DaemonInputs &in, Planet p)
{
    return in.planets[DaemonCalculator::chaldeanIndex(p)];
}

QJsonArray planetsToJson(const QVector<Planet> &planets)
{
    QJsonArray arr;
    for (Planet p : planets)
        arr.append(toString(p));
    return arr;
}

QVector<Planet> planetsFromJson(const QJsonArray &arr)
{
    QVector<Planet> out;
    for (const QJsonValue &v : arr) {
        Planet p = planetFromString(v.toString());
        if (p != Planet::Unknown)
            out << p;
    }
    return out;
}

QJsonObject nameToJson(const GeniusName &name)
{
    QJsonArray letters;
    for (int i : name.letters)
        letters.append(i);
    QJsonObject obj;
    obj["letters"] = letters;
    obj["hebrew"] = name.hebrew();
    obj["letterNames"] = name.letterNames();
    return obj;
}

GeniusName nameFromJson(const QJsonObject &obj)
{
    GeniusName name;
    for (const QJsonValue &v : obj["letters"].toArray()) {
        int i = v.toInt(-1);
        if (i >= 0 && i < 22)
            name.letters << i;
    }
    return name;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

double DaemonInputs::partOfFortune() const
{
    const double sun  = planets[DaemonCalculator::chaldeanIndex(Planet::Sun)];
    const double moon = planets[DaemonCalculator::chaldeanIndex(Planet::Moon)];
    return isDay ? norm(asc() + moon - sun) : norm(asc() + sun - moon);
}

QString GeniusName::hebrew() const
{
    QString s;
    for (int i : letters)
        s += QChar(kHebrew[i]);
    s += QChar(kSuffixEl[0]);
    s += QChar(kSuffixEl[1]);
    return s;
}

QString GeniusName::letterNames() const
{
    QStringList parts;
    for (int i : letters)
        parts << kHebrewNames[i];
    return parts.join(' ') + " + el";
}

int DaemonResults::methodCount() const
{
    int n = 0;
    for (const SignificatorResult &s : significators)
        if (!s.evil && !s.planets.isEmpty())
            ++n;
    return n;
}

int DaemonResults::votesFor(Planet p) const
{
    int n = 0;
    for (const SignificatorResult &s : significators)
        if (!s.evil && s.planets.contains(p))
            ++n;
    return n;
}

QVector<Planet> DaemonResults::consensus() const
{
    int best = 0;
    for (Planet p : DaemonCalculator::chaldeanOrder())
        best = (std::max)(best, votesFor(p)); // parenthesised: windows.h defines max()
    QVector<Planet> out;
    if (best == 0)
        return out;
    for (Planet p : DaemonCalculator::chaldeanOrder())
        if (votesFor(p) == best)
            out << p;
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────

const QVector<Planet> &DaemonCalculator::chaldeanOrder()
{
    static const QVector<Planet> order = {
        Planet::Saturn, Planet::Jupiter, Planet::Mars, Planet::Sun,
        Planet::Venus, Planet::Mercury, Planet::Moon
    };
    return order;
}

int DaemonCalculator::chaldeanIndex(Planet p)
{
    return chaldeanOrder().indexOf(p);
}

QVector<int> DaemonCalculator::scoreDegree(double lon, bool isDay)
{
    QVector<int> scores = zeroScores();
    const int s = signIndex(lon);
    scores[chaldeanIndex(kSignRuler[s])] += 5;
    if (kExaltation[s] != Planet::Unknown)
        scores[chaldeanIndex(kExaltation[s])] += 4;
    scores[chaldeanIndex(isDay ? kTriplicityDay[s % 4] : kTriplicityNight[s % 4])] += 3;
    scores[chaldeanIndex(termRuler(lon))] += 2;
    scores[chaldeanIndex(faceRuler(lon))] += 1;
    return scores;
}

int DaemonCalculator::houseOf(double lon, const QVector<double> &cusps, bool fiveDegreeRule)
{
    for (int i = 0; i < 12; ++i) {
        const double start = cusps[i];
        const double end = cusps[(i + 1) % 12];
        if (arc(start, lon) < arc(start, end)) {
            int house = i + 1;
            // A planet within 5° before the next cusp counts in that house.
            if (fiveDegreeRule && arc(lon, end) <= 5.0)
                house = house % 12 + 1;
            return house;
        }
    }
    return 1;
}

int DaemonCalculator::letterForward(double lon, double origin)
{
    return ((wholeDegree(lon) - wholeDegree(origin)) % 360 + 360) % 360 % 22;
}

int DaemonCalculator::letterBackward(double lon, double origin)
{
    return ((wholeDegree(origin) - wholeDegree(lon)) % 360 + 360) % 360 % 22;
}

QString DaemonCalculator::formatLongitude(double lon)
{
    const double d = degInSign(lon);
    int deg = static_cast<int>(d);
    int min = static_cast<int>(std::round((d - deg) * 60.0));
    if (min == 60) { // don't print 60'
        min = 0;
        ++deg;
    }
    return QString("%1°%2' %3").arg(deg).arg(min, 2, 10, QChar('0')).arg(kSignNames[signIndex(lon)]);
}

QString DaemonCalculator::planetNature(Planet p)
{
    switch (p) {
    case Planet::Saturn:  return "Cold and dry; greater malefic. Old age, wisdom, death, solitude; co-ruler of astrology and magic.";
    case Planet::Jupiter: return "Warm and moist; greater benefic. Generosity, justice, law, reason.";
    case Planet::Mars:    return "Hot and dry; lesser malefic. Wrath, soldiers, weapons, blood, fire.";
    case Planet::Sun:     return "Hot and dry; benefic or malefic by condition. Honour, kings, good judgment.";
    case Planet::Venus:   return "Cool and moist; lesser benefic. Love, music, art, jewels, games.";
    case Planet::Mercury: return "Variable; takes the nature of the planets it is with. Wit, learning, business; co-ruler of astrology and magic.";
    case Planet::Moon:    return "Cool and moist; benefic or malefic by condition. Messengers, sailors, the sea, a wavering disposition.";
    default:              return QString();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Ephemeris inputs
// ─────────────────────────────────────────────────────────────────────────────

DaemonInputs DaemonCalculator::inputsFromEphemeris(double jdUt, double latitude, double longitude,
                                                   QString *error)
{
    DaemonInputs in;
    char serr[256] = {0};
    const int flags = SEFLG_SWIEPH;
    auto fail = [&](const QString &msg) {
        if (error)
            *error = msg;
        return DaemonInputs();
    };

    static const int sweIds[7] = { SE_SATURN, SE_JUPITER, SE_MARS, SE_SUN, SE_VENUS, SE_MERCURY, SE_MOON };
    for (int i = 0; i < 7; ++i) {
        double xx[6];
        if (swe_calc_ut(jdUt, sweIds[i], flags, xx, serr) < 0)
            return fail(QString("Planet calculation failed: %1").arg(serr));
        in.planets << xx[0];
    }

    double cusps[13] = {0};
    double ascmc[10] = {0};
    if (swe_houses(jdUt, latitude, longitude, 'B', cusps, ascmc) < 0)
        return fail("Alcabitius house calculation failed");
    for (int i = 1; i <= 12; ++i)
        in.cusps << cusps[i];

    // Above the horizon = the half of the zodiac from the Descendant to the Ascendant.
    auto aboveHorizon = [&](double lon) { return arc(in.dsc(), lon) < 180.0; };
    in.isDay = aboveHorizon(in.planets[chaldeanIndex(Planet::Sun)]);

    // ── Pre-natal lunation ──────────────────────────────────────────────────
    // Sun-Moon elongation grows ~12°/day, so stepping back half a day at a time
    // it shrinks until it wraps through 0° (New Moon) or drops below 180° (Full Moon).
    auto elongation = [&](double t, bool &ok) {
        double sun[6], moon[6];
        ok = swe_calc_ut(t, SE_SUN, flags, sun, serr) >= 0 && swe_calc_ut(t, SE_MOON, flags, moon, serr) >= 0;
        return ok ? norm(moon[0] - sun[0]) : 0.0;
    };
    bool ok = true;
    double tAfter = jdUt;
    double eAfter = elongation(tAfter, ok);
    double tBefore = 0.0;
    bool isNew = true;
    for (double t = jdUt - 0.5; ok && t > jdUt - 32.0; t -= 0.5) {
        const double e = elongation(t, ok);
        if (!ok)
            break;
        if (e > eAfter) {                       // wrapped through 0°
            tBefore = t; isNew = true; break;
        }
        if (e < 180.0 && eAfter >= 180.0) {     // passed back through 180°
            tBefore = t; isNew = false; break;
        }
        tAfter = t;
        eAfter = e;
    }
    if (!ok || tBefore == 0.0)
        return fail("Could not find the pre-natal lunation");

    // Bisect on the signed distance from the target elongation.
    const double target = isNew ? 0.0 : 180.0;
    auto offset = [&](double t) { return norm(elongation(t, ok) - target + 180.0) - 180.0; };
    double lo = tBefore, hi = tAfter;
    for (int i = 0; i < 40 && ok; ++i) {
        const double mid = (lo + hi) / 2.0;
        (offset(mid) < 0.0 ? lo : hi) = mid;
    }
    double sunAt[6], moonAt[6];
    const double tSyzygy = (lo + hi) / 2.0;
    if (!ok || swe_calc_ut(tSyzygy, SE_SUN, flags, sunAt, serr) < 0
            || swe_calc_ut(tSyzygy, SE_MOON, flags, moonAt, serr) < 0)
        return fail("Could not find the pre-natal lunation");
    in.prenatalLunationIsNew = isNew;
    if (isNew) {
        in.prenatalLunation = sunAt[0];
    } else {
        // Ptolemy: the degree of whichever luminary is above the horizon at birth
        // (the Moon's when neither is).
        const bool sunUp = in.isDay;
        in.prenatalLunation = sunUp ? sunAt[0] : moonAt[0];
    }

    // ── Planetary day and hour ──────────────────────────────────────────────
    // Unequal hours: sunrise -> sunset and sunset -> next sunrise each split into
    // twelve. The planetary day begins at sunrise. When the Sun does not rise or
    // set (polar day/night) the rulers stay Unknown and are simply not scored.
    double geopos[3] = { longitude, latitude, 0.0 };
    auto nextEvent = [&](double from, int which, double &out) {
        return swe_rise_trans(from, SE_SUN, nullptr, flags, which, geopos, 0, 0, &out, serr) == 0;
    };
    double rise = 0.0, next = 0.0, set = 0.0, nextRise = 0.0;
    bool riseOk = nextEvent(jdUt - 2.0, SE_CALC_RISE, rise) && rise <= jdUt;
    while (riseOk && nextEvent(rise + 0.01, SE_CALC_RISE, next) && next <= jdUt)
        rise = next;
    if (riseOk && nextEvent(rise, SE_CALC_SET, set) && nextEvent(set, SE_CALC_RISE, nextRise)
            && nextRise > jdUt) {
        int hour;
        if (jdUt < set)
            hour = static_cast<int>((jdUt - rise) / ((set - rise) / 12.0));
        else
            hour = 12 + static_cast<int>((jdUt - set) / ((nextRise - set) / 12.0));
        hour = std::clamp(hour, 0, 23);

        // Weekday of the sunrise in local mean time (swe_day_of_week: 0 = Monday).
        static const Planet weekdayRuler[7] = {
            Planet::Moon, Planet::Mars, Planet::Mercury, Planet::Jupiter,
            Planet::Venus, Planet::Saturn, Planet::Sun
        };
        in.dayRuler = weekdayRuler[swe_day_of_week(rise + longitude / 360.0)];
        in.hourRuler = chaldeanOrder()[(chaldeanIndex(in.dayRuler) + hour) % 7];
    }

    if (error)
        error->clear();
    return in;
}

// ─────────────────────────────────────────────────────────────────────────────
// Stage A (significators) and Stage B (names)
// ─────────────────────────────────────────────────────────────────────────────

DaemonResults DaemonCalculator::compute(const DaemonInputs &in)
{
    DaemonResults r;
    if (!in.isValid())
        return r;

    const QVector<Planet> &order = chaldeanOrder();
    auto ownDignity = [&](Planet p) { return scoreDegree(planetLon(in, p), in.isDay)[chaldeanIndex(p)]; };

    // A1 - Lady of the nativity
    {
        const int s = signIndex(in.asc());
        r.significators << SignificatorResult{
            "ladyOfNativity", "Lady of the nativity",
            {kSignRuler[s]}, "Ruler of the Ascendant sign, " + kSignNames[s]};
    }

    // A2a - Most dignities, each planet scored in its own degree
    {
        QVector<int> scores = zeroScores();
        for (Planet p : order)
            scores[chaldeanIndex(p)] = ownDignity(p);
        QVector<Planet> winners = argmax(scores);
        QString detail = "Essential dignity in its own degree: " + topScores(scores);
        if (scores[chaldeanIndex(winners.first())] == 0) {
            winners.clear(); // every planet peregrine: no winner
            detail = "No planet has essential dignity in its own degree";
        }
        r.significators << SignificatorResult{"mostDignified", "Most dignified planet", winners, detail};
    }

    // A2b - Compound chart almuten (Ibn Ezra): dignities of the five hylegical
    // places, plus house strength of each planet, plus day and hour rulers.
    {
        QVector<int> total = zeroScores();
        for (const Place &place : hylegicalPlaces(in))
            addScores(total, scoreDegree(place.lon, in.isDay));
        for (Planet p : order)
            total[chaldeanIndex(p)] += housePoints(houseOf(planetLon(in, p), in.cusps, true));
        QString note;
        if (in.dayRuler != Planet::Unknown) {
            total[chaldeanIndex(in.dayRuler)] += kDayRulerPoints;
            total[chaldeanIndex(in.hourRuler)] += kHourRulerPoints;
        } else {
            note = " (no day/hour ruler: the Sun does not rise or set on this date)";
        }
        r.significators << SignificatorResult{
            "chartAlmuten", "Almuten of the chart", argmax(total), "Totals: " + topScores(total) + note};
    }

    // A3 - Ruler of the sign the Moon enters next
    {
        const int next = (signIndex(planetLon(in, Planet::Moon)) + 1) % 12;
        r.significators << SignificatorResult{
            "moonNextSign", "Moon's next sign", {kSignRuler[next]}, "The Moon enters " + kSignNames[next] + " next"};
    }

    // A4 - Sun or Moon, whichever is better placed: dignified > peregrine >
    // debilitated, then more dignity, then the stronger house (no 5° rule here).
    {
        struct Key { int tier, score, house; };
        auto key = [&](Planet p) {
            const double lon = planetLon(in, p);
            const int score = ownDignity(p);
            const bool debilitated = isDetriment(p, lon) || isFall(p, lon);
            const int tier = score > 0 ? 2 : (debilitated ? 0 : 1);
            return Key{tier, score, houseOf(lon, in.cusps, false)};
        };
        auto describe = [&](Planet p, const Key &k) {
            static const char *tiers[3] = {"debilitated", "peregrine", "dignified"};
            QString tier = QString::fromLatin1(tiers[k.tier]);
            if (k.score > 0)
                tier += QString(" (%1)").arg(k.score);
            return QString("%1 %2 in the %3 house").arg(toString(p), tier, ordinal(k.house));
        };
        const Key sun = key(Planet::Sun), moon = key(Planet::Moon);
        auto rank = [](const Key &k) { return std::make_tuple(k.tier, k.score, housePoints(k.house)); };
        QVector<Planet> winners;
        if (rank(sun) >= rank(moon)) winners << Planet::Sun;
        if (rank(moon) >= rank(sun)) winners << Planet::Moon;
        r.significators << SignificatorResult{
            "sunOrMoon", "Sun or Moon", winners,
            describe(Planet::Sun, sun) + "; " + describe(Planet::Moon, moon)};
    }

    // A5a - "A certain angle": the strongest simple almuten of any one angle
    const QVector<Place> angles = {
        {"Ascendant", in.asc()}, {"Midheaven", in.mc()}, {"Descendant", in.dsc()}, {"IC", in.ic()}
    };
    {
        int best = -1;
        QVector<Planet> winners;
        QStringList where;
        for (const Place &angle : angles) {
            const QVector<int> scores = scoreDegree(angle.lon, in.isDay);
            const QVector<Planet> top = argmax(scores);
            const int points = scores[chaldeanIndex(top.first())];
            if (points > best) {
                best = points;
                winners.clear();
                where.clear();
            }
            if (points == best) {
                for (Planet p : top)
                    if (!winners.contains(p))
                        winners << p;
                where << angle.name;
            }
        }
        std::sort(winners.begin(), winners.end(),
                  [](Planet a, Planet b) { return chaldeanIndex(a) < chaldeanIndex(b); });
        r.significators << SignificatorResult{
            "certainAngle", "Strongest angle",
            winners, QString("%1 points at the %2").arg(best).arg(where.join(" and "))};
    }

    // A5b - Compound almuten of all four angles
    QVector<int> angleTotals = zeroScores();
    for (const Place &angle : angles)
        addScores(angleTotals, scoreDegree(angle.lon, in.isDay));
    const QVector<Planet> anglesAlmuten = argmax(angleTotals);
    r.significators << SignificatorResult{
        "allAngles", "Almuten of the angles", anglesAlmuten, "Totals: " + topScores(angleTotals)};

    // A6 - Good genius from the 11th, evil genius from the 6th
    {
        const int s11 = signIndex(in.cusps[10]);
        const int s6 = signIndex(in.cusps[5]);
        r.significators << SignificatorResult{
            "goodGenius", "Good genius (11th house)", {kSignRuler[s11]}, "Ruler of the 11th cusp, " + kSignNames[s11]};
        r.significators << SignificatorResult{
            "evilGenius", "Evil genius (6th house)", {kSignRuler[s6]}, "Ruler of the 6th cusp, " + kSignNames[s6], true};
    }

    // ── Stage B: names ──────────────────────────────────────────────────────
    // B1/B2 cast from the Ascendant (good, in the order of the signs) and from
    // the Descendant (evil, against it). B3/B4 cast from 0° Aries / 29° Pisces.
    // In B1, B2 and B4 the letter is read where the almuten planet stands.
    for (Planet p : anglesAlmuten) {
        const double lon = planetLon(in, p);
        r.names << NameResult{
            "Almuten of the angles",
            toString(p) + " at " + formatLongitude(lon) + ", cast from the Ascendant / Descendant",
            GeniusName{{letterForward(lon, in.asc())}}, GeniusName{{letterBackward(lon, in.dsc())}}};
    }

    {
        QVector<Planet> goodAlmutens, evilAlmutens;
        simpleAlmutenFirst(in.cusps[10], in.isDay, &goodAlmutens);
        simpleAlmutenFirst(in.cusps[11], in.isDay, &evilAlmutens);
        for (Planet g : goodAlmutens) {
            for (Planet e : evilAlmutens) {
                const double gLon = planetLon(in, g), eLon = planetLon(in, e);
                r.names << NameResult{
                    "Almutens of the 11th and 12th",
                    QString("Good: %1 at %2; evil: %3 at %4")
                        .arg(toString(g), formatLongitude(gLon), toString(e), formatLongitude(eLon)),
                    GeniusName{{letterForward(gLon, in.asc())}}, GeniusName{{letterBackward(eLon, in.dsc())}}};
            }
        }
    }

    {
        // Good letters from the five places; evil letters from the points opposite them.
        GeniusName good, evil;
        QStringList basis;
        for (const Place &place : hylegicalPlaces(in)) {
            good.letters << letterForward(place.lon, 0.0);
            evil.letters << letterBackward(norm(place.lon + 180.0), 359.0);
            basis << place.name;
        }
        r.names << NameResult{"Five hylegical places", basis.join(", ") + " (evil: their opposite points)", good, evil};
    }

    {
        QVector<int> totals = zeroScores();
        for (const Place &place : hylegicalPlaces(in))
            addScores(totals, scoreDegree(place.lon, in.isDay));
        for (Planet p : argmax(totals)) {
            const double lon = planetLon(in, p);
            r.names << NameResult{
                "Almuten of the hylegical places",
                toString(p) + " at " + formatLongitude(lon) + ", cast from 0° Aries / 29° Pisces",
                GeniusName{{letterForward(lon, 0.0)}}, GeniusName{{letterBackward(lon, 359.0)}}};
        }
    }

    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON
// ─────────────────────────────────────────────────────────────────────────────

QJsonObject DaemonCalculator::significatorsToJson(const DaemonResults &results)
{
    QJsonArray significators;
    for (const SignificatorResult &s : results.significators) {
        QJsonObject obj;
        obj["key"] = s.key;
        obj["method"] = s.method;
        obj["planets"] = planetsToJson(s.planets);
        obj["detail"] = s.detail;
        if (s.evil)
            obj["evil"] = true;
        significators.append(obj);
    }

    QJsonObject consensus;
    const QVector<Planet> winners = results.consensus();
    consensus["planets"] = planetsToJson(winners);
    consensus["votes"] = winners.isEmpty() ? 0 : results.votesFor(winners.first());
    consensus["methods"] = results.methodCount();

    QJsonObject json;
    json["significators"] = significators;
    json["consensus"] = consensus;
    return json;
}

QJsonObject DaemonCalculator::toJson(const DaemonResults &results)
{
    QJsonObject json = significatorsToJson(results);
    QJsonArray names;
    for (const NameResult &n : results.names) {
        QJsonObject obj;
        obj["method"] = n.method;
        obj["basis"] = n.basis;
        obj["good"] = nameToJson(n.good);
        obj["evil"] = nameToJson(n.evil);
        names.append(obj);
    }
    json["names"] = names;
    return json;
}

DaemonResults DaemonCalculator::fromJson(const QJsonObject &json)
{
    DaemonResults r;
    for (const QJsonValue &v : json["significators"].toArray()) {
        const QJsonObject obj = v.toObject();
        r.significators << SignificatorResult{
            obj["key"].toString(), obj["method"].toString(),
            planetsFromJson(obj["planets"].toArray()), obj["detail"].toString(), obj["evil"].toBool()};
    }
    for (const QJsonValue &v : json["names"].toArray()) {
        const QJsonObject obj = v.toObject();
        r.names << NameResult{
            obj["method"].toString(), obj["basis"].toString(),
            nameFromJson(obj["good"].toObject()), nameFromJson(obj["evil"].toObject())};
    }
    return r;
}
