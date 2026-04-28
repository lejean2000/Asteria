#ifndef ASTRO_TYPES_H
#define ASTRO_TYPES_H

#include <QString>
#include <QVector>

// ─────────────────────────────────────────────────────────────────────────────
// All celestial bodies used in the application
// ─────────────────────────────────────────────────────────────────────────────
enum class Planet {
    // Classical planets
    Sun = 0,
    Moon,
    Mercury,
    Venus,
    Mars,
    Jupiter,
    Saturn,
    Uranus,
    Neptune,
    Pluto,
    // Nodes and minor bodies
    NorthNode,
    SouthNode,
    Chiron,
    // Major asteroids
    Ceres,
    Pallas,
    Juno,
    Vesta,
    // Sensitive points
    Lilith,
    Vertex,
    EastPoint,
    // Derived points
    Syzygy,
    ParsFortuna,
    PartOfSpirit,

    Unknown = -1
};

// ─────────────────────────────────────────────────────────────────────────────
// Aspect types (enum value = exact angle in degrees)
// ─────────────────────────────────────────────────────────────────────────────
enum class AspectType {
    Conjunction    =   0,
    Semisextile    =  30,
    Semisquare     =  45,
    Sextile        =  60,
    Square         =  90,
    Trine          = 120,
    Sesquiquadrate = 135,
    Quincunx       = 150,
    Opposition     = 180,
    Unknown        =  -1
};

// ─────────────────────────────────────────────────────────────────────────────
// Strongly-typed pairwise aspect
// Planets are stored in canonical order (a < b by enum int value)
// ─────────────────────────────────────────────────────────────────────────────
struct Aspect {
    Planet     a;
    Planet     b;
    AspectType type;
    double     orb;       // |deviation from exact|, degrees, always >= 0
    bool       applying;  // true = planets moving toward exact
};

using AspectList = QVector<Aspect>;

// ─────────────────────────────────────────────────────────────────────────────
// Comparison operator so Planet can be used as a QMap key
// ─────────────────────────────────────────────────────────────────────────────
inline bool operator<(Planet a, Planet b)
{
    return static_cast<int>(a) < static_cast<int>(b);
}

// ─────────────────────────────────────────────────────────────────────────────
// Conversion helpers
// ─────────────────────────────────────────────────────────────────────────────
QString    toString(Planet planet);
QString    toString(AspectType aspectType);

Planet     planetFromString(const QString &s);
// Handles both legacy short codes ("CON") and full names ("Conjunction")
AspectType aspectTypeFromString(const QString &s);

#endif // ASTRO_TYPES_H
