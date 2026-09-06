#include "aspectpatterndetector.h"

#include <QJsonArray>
#include <QMap>
#include <algorithm>

namespace {

bool isConjunctionBetween(const QVector<AspectData> &conjunctions, Planet a, Planet b)
{
    for (const AspectData &asp : conjunctions) {
        if ((asp.planet1 == a && asp.planet2 == b) ||
            (asp.planet1 == b && asp.planet2 == a))
            return true;
    }
    return false;
}

QVector<Planet> neighboursOf(const QVector<AspectData> &conjunctions, Planet p)
{
    QVector<Planet> result;
    for (const AspectData &asp : conjunctions) {
        if (asp.planet1 == p)
            result.append(asp.planet2);
        else if (asp.planet2 == p)
            result.append(asp.planet1);
    }
    return result;
}

bool hasAspect(const QVector<AspectData> &aspects, Planet a, Planet b, AspectType type)
{
    for (const AspectData &asp : aspects) {
        if (asp.aspectType != type)
            continue;
        if ((asp.planet1 == a && asp.planet2 == b) || (asp.planet1 == b && asp.planet2 == a))
            return true;
    }
    return false;
}

// The Node axis is always an exact (0° orb) opposition by construction
// (SouthNode is synthesized as NorthNode + 180°, see chartcalculator.cpp),
// not a meaningful configuration — excluded from opposition-based patterns.
bool isNodeAxis(Planet a, Planet b)
{
    return (a == Planet::NorthNode && b == Planet::SouthNode) ||
           (a == Planet::SouthNode && b == Planet::NorthNode);
}

QVector<Planet> allPlanetsIn(const QVector<AspectData> &aspects)
{
    QVector<Planet> result;
    for (const AspectData &asp : aspects) {
        if (!result.contains(asp.planet1))
            result.append(asp.planet1);
        if (!result.contains(asp.planet2))
            result.append(asp.planet2);
    }
    return result;
}

// Two base planets sharing `baseType`, both also `pointingType` to a third
// (apex) planet. Shared by all spike variants (Yod, Thor's Hammer, ...).
QVector<SpikePattern> detectSpikeVariant(const QVector<AspectData> &aspects, AspectType baseType,
                                          AspectType pointingType, SpikeType spikeType)
{
    QVector<SpikePattern> result;
    const QVector<Planet> allPlanets = allPlanetsIn(aspects);

    for (const AspectData &baseAsp : aspects) {
        if (baseAsp.aspectType != baseType)
            continue;

        const Planet p1 = baseAsp.planet1;
        const Planet p2 = baseAsp.planet2;

        for (Planet x : allPlanets) {
            if (x == p1 || x == p2)
                continue;

            if (hasAspect(aspects, p1, x, pointingType) && hasAspect(aspects, p2, x, pointingType)) {
                SpikePattern pattern;
                pattern.base = { p1, p2 };
                pattern.apex = x;
                pattern.spikeType = spikeType;
                result.append(pattern);
            }
        }
    }

    return result;
}

} // namespace

AspectPatternResults AspectPatternDetector::detect(const QVector<PlanetData> &planets,
                                                    const QVector<AspectData> &aspects)
{
    AspectPatternResults results;
    results.clusters = detectClusters(planets, aspects);
    results.easyOppositions = detectEasyOppositions(aspects);
    results.tSquares = detectTSquares(aspects);
    results.grandCrosses = detectGrandCrosses(aspects);
    results.grandTrines = detectGrandTrines(aspects);
    results.kites = detectKites(results.grandTrines, aspects);
    results.spikes = detectSpikes(aspects);
    return results;
}

QVector<ClusterPattern> AspectPatternDetector::detectClusters(const QVector<PlanetData> &planets,
                                                                const QVector<AspectData> &aspects)
{
    QVector<ClusterPattern> result;

    QMap<Planet, double> longitude;
    for (const PlanetData &p : planets)
        longitude.insert(p.id, p.longitude);

    // Restrict to Conjunction-only edges; the conjunction orb is uniform for
    // every pair, so a connected component of this subgraph automatically
    // satisfies "each longitude-consecutive pair is conjunct" — no separate
    // topology check is needed.
    QVector<AspectData> conjunctions;
    for (const AspectData &asp : aspects) {
        if (asp.aspectType == AspectType::Conjunction &&
            longitude.contains(asp.planet1) && longitude.contains(asp.planet2))
            conjunctions.append(asp);
    }

    QVector<Planet> visited;
    for (const AspectData &seedAsp : conjunctions) {
        for (Planet seed : { seedAsp.planet1, seedAsp.planet2 }) {
            if (visited.contains(seed))
                continue;

            // BFS over the conjunction-only graph to collect this component
            QVector<Planet> component;
            QVector<Planet> queue = { seed };
            visited.append(seed);
            while (!queue.isEmpty()) {
                Planet cur = queue.takeFirst();
                component.append(cur);
                for (Planet nb : neighboursOf(conjunctions, cur)) {
                    if (!visited.contains(nb)) {
                        visited.append(nb);
                        queue.append(nb);
                    }
                }
            }

            if (component.size() < 3)
                continue;

            std::sort(component.begin(), component.end(), [&](Planet a, Planet b) {
                return longitude.value(a) < longitude.value(b);
            });

            // Linearize the circular order: start right after the largest
            // angular gap so a 0°/360° wraparound never splits the cluster.
            const int n = component.size();
            int gapStart = 0;
            double largestGap = -1.0;
            for (int i = 0; i < n; ++i) {
                double here = longitude.value(component[i]);
                double next = longitude.value(component[(i + 1) % n]);
                double gap = next - here;
                if (gap < 0)
                    gap += 360.0;
                if (gap > largestGap) {
                    largestGap = gap;
                    gapStart = (i + 1) % n;
                }
            }

            QVector<Planet> ordered;
            ordered.reserve(n);
            for (int i = 0; i < n; ++i)
                ordered.append(component[(gapStart + i) % n]);

            bool tight = true;
            for (int i = 0; i < n && tight; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    if (!isConjunctionBetween(conjunctions, component[i], component[j])) {
                        tight = false;
                        break;
                    }
                }
            }

            ClusterPattern pattern;
            pattern.planets = ordered;
            pattern.subtype = tight ? ClusterSubtype::Tight : ClusterSubtype::Loose;
            result.append(pattern);
        }
    }

    return result;
}

QVector<EasyOppositionPattern> AspectPatternDetector::detectEasyOppositions(const QVector<AspectData> &aspects)
{
    QVector<EasyOppositionPattern> result;
    const QVector<Planet> allPlanets = allPlanetsIn(aspects);

    for (const AspectData &opp : aspects) {
        if (opp.aspectType != AspectType::Opposition)
            continue;
        if (isNodeAxis(opp.planet1, opp.planet2))
            continue;

        const Planet a = opp.planet1;
        const Planet b = opp.planet2;

        for (Planet c : allPlanets) {
            if (c == a || c == b)
                continue;

            const bool eases = (hasAspect(aspects, c, a, AspectType::Sextile) && hasAspect(aspects, c, b, AspectType::Trine)) ||
                                (hasAspect(aspects, c, a, AspectType::Trine) && hasAspect(aspects, c, b, AspectType::Sextile));

            if (eases) {
                EasyOppositionPattern pattern;
                pattern.pole1 = a;
                pattern.pole2 = b;
                pattern.easing = c;
                result.append(pattern);
            }
        }
    }

    return result;
}

QVector<TSquarePattern> AspectPatternDetector::detectTSquares(const QVector<AspectData> &aspects)
{
    QVector<TSquarePattern> result;
    const QVector<Planet> allPlanets = allPlanetsIn(aspects);

    for (const AspectData &opp : aspects) {
        if (opp.aspectType != AspectType::Opposition)
            continue;
        if (isNodeAxis(opp.planet1, opp.planet2))
            continue;

        const Planet a = opp.planet1;
        const Planet b = opp.planet2;

        for (Planet c : allPlanets) {
            if (c == a || c == b)
                continue;

            if (hasAspect(aspects, c, a, AspectType::Square) && hasAspect(aspects, c, b, AspectType::Square)) {
                TSquarePattern pattern;
                pattern.pole1 = a;
                pattern.pole2 = b;
                pattern.apex = c;
                result.append(pattern);
            }
        }
    }

    return result;
}

QVector<GrandCrossPattern> AspectPatternDetector::detectGrandCrosses(const QVector<AspectData> &aspects)
{
    QVector<GrandCrossPattern> result;

    QVector<AspectData> oppositions;
    for (const AspectData &asp : aspects) {
        if (asp.aspectType == AspectType::Opposition && !isNodeAxis(asp.planet1, asp.planet2))
            oppositions.append(asp);
    }

    for (int i = 0; i < oppositions.size(); ++i) {
        const Planet a = oppositions[i].planet1;
        const Planet b = oppositions[i].planet2;

        for (int j = i + 1; j < oppositions.size(); ++j) {
            const Planet c = oppositions[j].planet1;
            const Planet d = oppositions[j].planet2;

            // the two oppositions must involve four distinct planets
            if (c == a || c == b || d == a || d == b)
                continue;

            if (hasAspect(aspects, a, c, AspectType::Square) &&
                hasAspect(aspects, a, d, AspectType::Square) &&
                hasAspect(aspects, b, c, AspectType::Square) &&
                hasAspect(aspects, b, d, AspectType::Square)) {
                QVector<Planet> planets = { a, b, c, d };
                std::sort(planets.begin(), planets.end());

                GrandCrossPattern pattern;
                pattern.planets = planets;
                result.append(pattern);
            }
        }
    }

    return result;
}

QVector<GrandTrinePattern> AspectPatternDetector::detectGrandTrines(const QVector<AspectData> &aspects)
{
    QVector<GrandTrinePattern> result;

    QVector<AspectData> trines;
    for (const AspectData &asp : aspects) {
        if (asp.aspectType == AspectType::Trine)
            trines.append(asp);
    }

    const QVector<Planet> candidates = allPlanetsIn(trines);

    for (int i = 0; i < candidates.size(); ++i) {
        for (int j = i + 1; j < candidates.size(); ++j) {
            if (!hasAspect(aspects, candidates[i], candidates[j], AspectType::Trine))
                continue;

            for (int k = j + 1; k < candidates.size(); ++k) {
                if (hasAspect(aspects, candidates[i], candidates[k], AspectType::Trine) &&
                    hasAspect(aspects, candidates[j], candidates[k], AspectType::Trine)) {
                    QVector<Planet> planets = { candidates[i], candidates[j], candidates[k] };
                    std::sort(planets.begin(), planets.end());

                    GrandTrinePattern pattern;
                    pattern.planets = planets;
                    result.append(pattern);
                }
            }
        }
    }

    return result;
}

QVector<KitePattern> AspectPatternDetector::detectKites(const QVector<GrandTrinePattern> &grandTrines,
                                                          const QVector<AspectData> &aspects)
{
    QVector<KitePattern> result;
    const QVector<Planet> allPlanets = allPlanetsIn(aspects);

    for (const GrandTrinePattern &trine : grandTrines) {
        const QVector<Planet> &vertices = trine.planets; // 3 planets

        for (int apexIdx = 0; apexIdx < vertices.size(); ++apexIdx) {
            const Planet apex = vertices[apexIdx];
            const Planet other1 = vertices[(apexIdx + 1) % 3];
            const Planet other2 = vertices[(apexIdx + 2) % 3];

            for (Planet d : allPlanets) {
                if (d == vertices[0] || d == vertices[1] || d == vertices[2])
                    continue;

                if (hasAspect(aspects, d, apex, AspectType::Opposition) &&
                    hasAspect(aspects, d, other1, AspectType::Sextile) &&
                    hasAspect(aspects, d, other2, AspectType::Sextile)) {
                    KitePattern pattern;
                    pattern.grandTrinePlanets = vertices;
                    pattern.tail = d;
                    pattern.apex = apex;
                    result.append(pattern);
                }
            }
        }
    }

    return result;
}

QVector<SpikePattern> AspectPatternDetector::detectSpikes(const QVector<AspectData> &aspects)
{
    QVector<SpikePattern> result;
    result += detectSpikeVariant(aspects, AspectType::Sextile, AspectType::Quincunx, SpikeType::Yod);
    result += detectSpikeVariant(aspects, AspectType::Square, AspectType::Sesquiquadrate, SpikeType::ThorHammer);
    // BiquintileSpike intentionally omitted: Quintile/Biquintile aren't
    // AspectType values ChartCalculator computes.
    return result;
}

namespace {

QString clusterSubtypeToString(ClusterSubtype s) { return s == ClusterSubtype::Tight ? "Tight" : "Loose"; }
ClusterSubtype clusterSubtypeFromString(const QString &s) { return s == "Tight" ? ClusterSubtype::Tight : ClusterSubtype::Loose; }

QString spikeTypeToString(SpikeType s) { return s == SpikeType::Yod ? "Yod" : "ThorHammer"; }
SpikeType spikeTypeFromString(const QString &s) { return s == "Yod" ? SpikeType::Yod : SpikeType::ThorHammer; }

QJsonArray planetsToJsonArray(const QVector<Planet> &planets)
{
    QJsonArray arr;
    for (Planet p : planets)
        arr.append(toString(p));
    return arr;
}

QVector<Planet> planetsFromJsonArray(const QJsonArray &arr)
{
    QVector<Planet> result;
    for (const QJsonValue &v : arr)
        result.append(planetFromString(v.toString()));
    return result;
}

} // namespace

QJsonObject AspectPatternDetector::toJson(const AspectPatternResults &results)
{
    QJsonObject json;

    QJsonArray clusters;
    for (const ClusterPattern &p : results.clusters) {
        QJsonObject obj;
        obj["planets"] = planetsToJsonArray(p.planets);
        obj["subtype"] = clusterSubtypeToString(p.subtype);
        clusters.append(obj);
    }
    json["clusters"] = clusters;

    QJsonArray easyOppositions;
    for (const EasyOppositionPattern &p : results.easyOppositions) {
        QJsonObject obj;
        obj["pole1"] = toString(p.pole1);
        obj["pole2"] = toString(p.pole2);
        obj["easing"] = toString(p.easing);
        easyOppositions.append(obj);
    }
    json["easyOppositions"] = easyOppositions;

    QJsonArray tSquares;
    for (const TSquarePattern &p : results.tSquares) {
        QJsonObject obj;
        obj["pole1"] = toString(p.pole1);
        obj["pole2"] = toString(p.pole2);
        obj["apex"] = toString(p.apex);
        tSquares.append(obj);
    }
    json["tSquares"] = tSquares;

    QJsonArray grandCrosses;
    for (const GrandCrossPattern &p : results.grandCrosses) {
        QJsonObject obj;
        obj["planets"] = planetsToJsonArray(p.planets);
        grandCrosses.append(obj);
    }
    json["grandCrosses"] = grandCrosses;

    QJsonArray grandTrines;
    for (const GrandTrinePattern &p : results.grandTrines) {
        QJsonObject obj;
        obj["planets"] = planetsToJsonArray(p.planets);
        grandTrines.append(obj);
    }
    json["grandTrines"] = grandTrines;

    QJsonArray kites;
    for (const KitePattern &p : results.kites) {
        QJsonObject obj;
        obj["grandTrinePlanets"] = planetsToJsonArray(p.grandTrinePlanets);
        obj["tail"] = toString(p.tail);
        obj["apex"] = toString(p.apex);
        kites.append(obj);
    }
    json["kites"] = kites;

    QJsonArray spikes;
    for (const SpikePattern &p : results.spikes) {
        QJsonObject obj;
        obj["base"] = planetsToJsonArray(p.base);
        obj["apex"] = toString(p.apex);
        obj["spikeType"] = spikeTypeToString(p.spikeType);
        spikes.append(obj);
    }
    json["spikes"] = spikes;

    return json;
}

AspectPatternResults AspectPatternDetector::fromJson(const QJsonObject &json)
{
    AspectPatternResults results;

    for (const QJsonValue &v : json["clusters"].toArray()) {
        QJsonObject obj = v.toObject();
        ClusterPattern p;
        p.planets = planetsFromJsonArray(obj["planets"].toArray());
        p.subtype = clusterSubtypeFromString(obj["subtype"].toString());
        results.clusters.append(p);
    }

    for (const QJsonValue &v : json["easyOppositions"].toArray()) {
        QJsonObject obj = v.toObject();
        EasyOppositionPattern p;
        p.pole1 = planetFromString(obj["pole1"].toString());
        p.pole2 = planetFromString(obj["pole2"].toString());
        p.easing = planetFromString(obj["easing"].toString());
        results.easyOppositions.append(p);
    }

    for (const QJsonValue &v : json["tSquares"].toArray()) {
        QJsonObject obj = v.toObject();
        TSquarePattern p;
        p.pole1 = planetFromString(obj["pole1"].toString());
        p.pole2 = planetFromString(obj["pole2"].toString());
        p.apex = planetFromString(obj["apex"].toString());
        results.tSquares.append(p);
    }

    for (const QJsonValue &v : json["grandCrosses"].toArray()) {
        QJsonObject obj = v.toObject();
        GrandCrossPattern p;
        p.planets = planetsFromJsonArray(obj["planets"].toArray());
        results.grandCrosses.append(p);
    }

    for (const QJsonValue &v : json["grandTrines"].toArray()) {
        QJsonObject obj = v.toObject();
        GrandTrinePattern p;
        p.planets = planetsFromJsonArray(obj["planets"].toArray());
        results.grandTrines.append(p);
    }

    for (const QJsonValue &v : json["kites"].toArray()) {
        QJsonObject obj = v.toObject();
        KitePattern p;
        p.grandTrinePlanets = planetsFromJsonArray(obj["grandTrinePlanets"].toArray());
        p.tail = planetFromString(obj["tail"].toString());
        p.apex = planetFromString(obj["apex"].toString());
        results.kites.append(p);
    }

    for (const QJsonValue &v : json["spikes"].toArray()) {
        QJsonObject obj = v.toObject();
        SpikePattern p;
        p.base = planetsFromJsonArray(obj["base"].toArray());
        p.apex = planetFromString(obj["apex"].toString());
        p.spikeType = spikeTypeFromString(obj["spikeType"].toString());
        results.spikes.append(p);
    }

    return results;
}
