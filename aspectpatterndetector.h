#ifndef ASPECTPATTERNDETECTOR_H
#define ASPECTPATTERNDETECTOR_H

#include <QVector>
#include <QJsonObject>
#include "astro_types.h"
#include "chartcalculator.h" // PlanetData, AspectData

// ─────────────────────────────────────────────────────────────────────────────
// Composite aspect patterns detected from an already-computed aspect list.
// Detection is purely graph-based: it consumes AspectData (read-only) and
// never recomputes angles. Planet longitudes are only used for ordering.
// ─────────────────────────────────────────────────────────────────────────────
enum class ClusterSubtype {
    Tight, // every planet in the group is directly conjunct every other
    Loose  // only consecutive neighbours (by longitude) are conjunct
};

struct ClusterPattern {
    QVector<Planet> planets;   // ordered by ecliptic longitude
    ClusterSubtype subtype = ClusterSubtype::Loose;
};

// An opposition (pole1/pole2) softened by a third planet that is Sextile one
// pole and Trine the other.
struct EasyOppositionPattern {
    Planet pole1;
    Planet pole2;
    Planet easing;
};

// An opposition (pole1/pole2) with a third planet Square to both ends.
struct TSquarePattern {
    Planet pole1;
    Planet pole2;
    Planet apex;
};

// Two interlocking oppositions (A-B, C-D) with all four cross-connections
// (A-C, A-D, B-C, B-D) Square.
struct GrandCrossPattern {
    QVector<Planet> planets; // all four, sorted by Planet enum value (canonical order)
};

// Three planets each Trine the other two.
struct GrandTrinePattern {
    QVector<Planet> planets; // all three, sorted by Planet enum value (canonical order)
};

// A Grand Trine (grandTrinePlanets) with a fourth planet (tail) Opposition
// one trine vertex (apex) and Sextile the other two.
struct KitePattern {
    QVector<Planet> grandTrinePlanets; // the trine triplet, sorted canonical order
    Planet tail;
    Planet apex;
};

// Two base planets forming the same wide aspect to a shared apex planet.
// BiquintileSpike (Quintile base / Biquintile pointing) is not detected:
// Quintile/Biquintile aren't AspectType values ChartCalculator computes.
enum class SpikeType {
    Yod,        // Sextile base, Quincunx (150°) to apex
    ThorHammer  // Square base, Sesquiquadrate (135°) to apex
};

struct SpikePattern {
    QVector<Planet> base; // [P1, P2]
    Planet apex;
    SpikeType spikeType;
};

struct AspectPatternResults {
    QVector<ClusterPattern> clusters;
    QVector<EasyOppositionPattern> easyOppositions;
    QVector<TSquarePattern> tSquares;
    QVector<GrandCrossPattern> grandCrosses;
    QVector<GrandTrinePattern> grandTrines;
    QVector<KitePattern> kites;
    QVector<SpikePattern> spikes;
};

class AspectPatternDetector
{
public:
    // planets supplies longitude for ordering only; aspects is the sole source
    // of truth for which planets are in aspect with each other.
    static AspectPatternResults detect(const QVector<PlanetData> &planets,
                                        const QVector<AspectData> &aspects);

    // Serialize/deserialize detected patterns for chart save files.
    static QJsonObject toJson(const AspectPatternResults &results);
    static AspectPatternResults fromJson(const QJsonObject &json);

private:
    static QVector<ClusterPattern> detectClusters(const QVector<PlanetData> &planets,
                                                   const QVector<AspectData> &aspects);
    static QVector<EasyOppositionPattern> detectEasyOppositions(const QVector<AspectData> &aspects);
    static QVector<TSquarePattern> detectTSquares(const QVector<AspectData> &aspects);
    static QVector<GrandCrossPattern> detectGrandCrosses(const QVector<AspectData> &aspects);
    static QVector<GrandTrinePattern> detectGrandTrines(const QVector<AspectData> &aspects);
    static QVector<KitePattern> detectKites(const QVector<GrandTrinePattern> &grandTrines,
                                             const QVector<AspectData> &aspects);
    static QVector<SpikePattern> detectSpikes(const QVector<AspectData> &aspects);
};

#endif // ASPECTPATTERNDETECTOR_H
