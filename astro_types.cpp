#include "astro_types.h"

QString toString(Planet planet)
{
    switch (planet) {
    case Planet::Sun:          return QStringLiteral("Sun");
    case Planet::Moon:         return QStringLiteral("Moon");
    case Planet::Mercury:      return QStringLiteral("Mercury");
    case Planet::Venus:        return QStringLiteral("Venus");
    case Planet::Mars:         return QStringLiteral("Mars");
    case Planet::Jupiter:      return QStringLiteral("Jupiter");
    case Planet::Saturn:       return QStringLiteral("Saturn");
    case Planet::Uranus:       return QStringLiteral("Uranus");
    case Planet::Neptune:      return QStringLiteral("Neptune");
    case Planet::Pluto:        return QStringLiteral("Pluto");
    case Planet::NorthNode:    return QStringLiteral("North Node");
    case Planet::SouthNode:    return QStringLiteral("South Node");
    case Planet::Chiron:       return QStringLiteral("Chiron");
    case Planet::Ceres:        return QStringLiteral("Ceres");
    case Planet::Pallas:       return QStringLiteral("Pallas");
    case Planet::Juno:         return QStringLiteral("Juno");
    case Planet::Vesta:        return QStringLiteral("Vesta");
    case Planet::Lilith:       return QStringLiteral("Lilith");
    case Planet::Vertex:       return QStringLiteral("Vertex");
    case Planet::EastPoint:    return QStringLiteral("East Point");
    case Planet::Syzygy:       return QStringLiteral("Syzygy");
    case Planet::ParsFortuna:  return QStringLiteral("Pars Fortuna");
    case Planet::PartOfSpirit: return QStringLiteral("Part of Spirit");
    default:                   return QStringLiteral("Unknown");
    }
}

QString toString(AspectType aspectType)
{
    switch (aspectType) {
    case AspectType::Conjunction:    return QStringLiteral("Conjunction");
    case AspectType::Semisextile:    return QStringLiteral("Semisextile");
    case AspectType::Semisquare:     return QStringLiteral("Semisquare");
    case AspectType::Sextile:        return QStringLiteral("Sextile");
    case AspectType::Square:         return QStringLiteral("Square");
    case AspectType::Trine:          return QStringLiteral("Trine");
    case AspectType::Sesquiquadrate: return QStringLiteral("Sesquiquadrate");
    case AspectType::Quincunx:       return QStringLiteral("Quincunx");
    case AspectType::Opposition:     return QStringLiteral("Opposition");
    default:                         return QStringLiteral("Unknown");
    }
}

Planet planetFromString(const QString &s)
{
    if (s == QStringLiteral("Sun"))            return Planet::Sun;
    if (s == QStringLiteral("Moon"))           return Planet::Moon;
    if (s == QStringLiteral("Mercury"))        return Planet::Mercury;
    if (s == QStringLiteral("Venus"))          return Planet::Venus;
    if (s == QStringLiteral("Mars"))           return Planet::Mars;
    if (s == QStringLiteral("Jupiter"))        return Planet::Jupiter;
    if (s == QStringLiteral("Saturn"))         return Planet::Saturn;
    if (s == QStringLiteral("Uranus"))         return Planet::Uranus;
    if (s == QStringLiteral("Neptune"))        return Planet::Neptune;
    if (s == QStringLiteral("Pluto"))          return Planet::Pluto;
    if (s == QStringLiteral("North Node"))     return Planet::NorthNode;
    if (s == QStringLiteral("South Node"))     return Planet::SouthNode;
    if (s == QStringLiteral("Chiron"))         return Planet::Chiron;
    if (s == QStringLiteral("Ceres"))          return Planet::Ceres;
    if (s == QStringLiteral("Pallas"))         return Planet::Pallas;
    if (s == QStringLiteral("Juno"))           return Planet::Juno;
    if (s == QStringLiteral("Vesta"))          return Planet::Vesta;
    if (s == QStringLiteral("Lilith"))         return Planet::Lilith;
    if (s == QStringLiteral("Vertex"))         return Planet::Vertex;
    if (s == QStringLiteral("East Point"))     return Planet::EastPoint;
    if (s == QStringLiteral("Syzygy"))         return Planet::Syzygy;
    if (s == QStringLiteral("Pars Fortuna"))   return Planet::ParsFortuna;
    if (s == QStringLiteral("Part of Spirit")) return Planet::PartOfSpirit;
    return Planet::Unknown;
}

AspectType aspectTypeFromString(const QString &s)
{
    // Full names (current format)
    if (s == QStringLiteral("Conjunction"))    return AspectType::Conjunction;
    if (s == QStringLiteral("Semisextile"))    return AspectType::Semisextile;
    if (s == QStringLiteral("Semisquare"))     return AspectType::Semisquare;
    if (s == QStringLiteral("Sextile"))        return AspectType::Sextile;
    if (s == QStringLiteral("Square"))         return AspectType::Square;
    if (s == QStringLiteral("Trine"))          return AspectType::Trine;
    if (s == QStringLiteral("Sesquiquadrate")) return AspectType::Sesquiquadrate;
    if (s == QStringLiteral("Quincunx"))       return AspectType::Quincunx;
    if (s == QStringLiteral("Opposition"))     return AspectType::Opposition;
    // Legacy short codes (backward compatibility with old .astr files)
    if (s == QStringLiteral("CON"))            return AspectType::Conjunction;
    if (s == QStringLiteral("SSX"))            return AspectType::Semisextile;
    if (s == QStringLiteral("SSQ"))            return AspectType::Semisquare;
    if (s == QStringLiteral("SEX"))            return AspectType::Sextile;
    if (s == QStringLiteral("SQR"))            return AspectType::Square;
    if (s == QStringLiteral("TRI"))            return AspectType::Trine;
    if (s == QStringLiteral("SQQ"))            return AspectType::Sesquiquadrate;
    if (s == QStringLiteral("QUI"))            return AspectType::Quincunx;
    if (s == QStringLiteral("OPP"))            return AspectType::Opposition;
    return AspectType::Unknown;
}
