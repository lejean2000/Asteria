#ifndef DAEMONCALCULATOR_H
#define DAEMONCALCULATOR_H

#include <QVector>
#include <QString>
#include <QJsonObject>
#include "astro_types.h"

// ─────────────────────────────────────────────────────────────────────────────
// Presiding daemons / geniuses of a nativity, after Agrippa, Three Books of
// Occult Philosophy III.21 (significators) and III.26 (names), as reconstructed
// by Eric Purdue ("On Identifying Presiding Daemons and Geniuses from an
// Astrological Chart"). Traditional seven planets only, Egyptian terms,
// Alcabitius houses.
//
// Two independent stages:
//   A. significators - several methods each pick the planet of the daemon
//   B. names         - the Hebrew alphabet is cast over the zodiac, one letter
//                      per whole degree, and letters are read off, then "el"
//                      is appended
// ─────────────────────────────────────────────────────────────────────────────

// Everything the algorithms need from the ephemeris. Built by
// inputsFromEphemeris() for real charts, or by hand for tests.
struct DaemonInputs {
    QVector<double> planets;        // 7 longitudes, indexed by DaemonCalculator::chaldeanIndex()
    QVector<double> cusps;          // houses 1..12 (index 0..11), Alcabitius
    bool   isDay = true;            // Sun above the horizon
    double prenatalLunation = 0.0;  // degree of the last New/Full Moon before birth
    bool   prenatalLunationIsNew = true;
    Planet dayRuler  = Planet::Unknown; // Unknown when the Sun never rises/sets (polar latitudes)
    Planet hourRuler = Planet::Unknown;

    bool isValid() const { return planets.size() == 7 && cusps.size() == 12; }
    double asc() const { return cusps[0]; }
    double ic()  const { return cusps[3]; }
    double dsc() const { return cusps[6]; }
    double mc()  const { return cusps[9]; }
    double partOfFortune() const;   // sect-aware: reversed by night
};

struct SignificatorResult {
    QString key;             // stable id, e.g. "ladyOfNativity"
    QString method;          // display name
    QVector<Planet> planets; // all planets tied for the win
    QString detail;          // short human-readable justification
    bool evil = false;       // true only for the 6th-house evil genius
};

struct GeniusName {
    QVector<int> letters;    // indices into the 22-letter Hebrew alphabet, suffix excluded
    QString hebrew() const;  // letters + "אל", logical order (renders right-to-left)
    QString letterNames() const; // e.g. "Pe + el"
};

struct NameResult {
    QString method;
    QString basis;           // what the letters were read from, e.g. "Saturn at 11°14' Gemini"
    GeniusName good;
    GeniusName evil;
};

struct DaemonResults {
    QVector<SignificatorResult> significators;
    QVector<NameResult> names;

    bool isEmpty() const { return significators.isEmpty(); }

    // Votes per planet across the good-daemon methods (each tied winner gets a vote).
    int methodCount() const;
    int votesFor(Planet p) const;
    QVector<Planet> consensus() const; // planet(s) with the most votes
};

class DaemonCalculator
{
public:
    // Chaldean order: Saturn, Jupiter, Mars, Sun, Venus, Mercury, Moon.
    static const QVector<Planet> &chaldeanOrder();
    static int chaldeanIndex(Planet p); // -1 for non-traditional bodies

    // Query Swiss Ephemeris (must already be initialised) for the inputs.
    static DaemonInputs inputsFromEphemeris(double jdUt, double latitude, double longitude,
                                            QString *error = nullptr);

    static DaemonResults compute(const DaemonInputs &in);

    // Full results (for save files) / significators only (for the AI - the
    // names carry no interpretable meaning and would invite invention).
    static QJsonObject toJson(const DaemonResults &results);
    static QJsonObject significatorsToJson(const DaemonResults &results);
    static DaemonResults fromJson(const QJsonObject &json);

    // Short traditional description of a planet's nature (Purdue p. 184).
    static QString planetNature(Planet p);

    // Primitives, exposed for testing.
    static QVector<int> scoreDegree(double lon, bool isDay); // 7 scores, Chaldean index
    static int houseOf(double lon, const QVector<double> &cusps, bool fiveDegreeRule);
    static int letterForward(double lon, double origin);
    static int letterBackward(double lon, double origin);
    static QString formatLongitude(double lon);
};

#endif // DAEMONCALCULATOR_H
