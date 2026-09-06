#ifndef MODEL_H
#define MODEL_H

#include <QString>

struct Model {
    QString name;           // unique display name
    QString provider;       // e.g., "Mistral", "OpenAI", "Ollama"
    QString endpoint;       // full API URL
    QString apiKey;         // optional, may be empty
    QString modelName;      // model identifier used by the provider
    double temperature;     // default 1.0
    int maxTokens;          // default 32768

    // Equality operator for convenience
    bool operator==(const Model &other) const {
        return name == other.name;
    }
};

#endif // MODEL_H