#include "StatsStore.h"
#include <Preferences.h>
#include <Arduino.h>

static void nsFor(const char* algo, char* out, size_t outSz)
{
    // Preferences namespace limit is 15 chars. "stats_" + algo ≤ 15.
    snprintf(out, outSz, "stats_%s", (algo && *algo) ? algo : "X");
}

void StatsStore::load(const char* algo, MiningStats& stats)
{
    char ns[16];
    nsFor(algo, ns, sizeof(ns));

    Preferences p;
    if (!p.begin(ns, /*readOnly*/ true)) return;

    stats.sharesAccepted = p.getUInt("ok",   0);
    stats.sharesRejected = p.getUInt("rej",  0);
    stats.bestDifficulty = p.getFloat("best", 0.0f);
    stats.totalHashes    = p.getULong64("total", 0);
    stats.balance        = p.getFloat("bal",  0.0f);

    p.end();

    Serial.printf("[stats] restored %s: ok=%lu rej=%lu best=%.2f total=%llu bal=%.4f\n",
                  ns,
                  (unsigned long)stats.sharesAccepted,
                  (unsigned long)stats.sharesRejected,
                  (double)stats.bestDifficulty,
                  (unsigned long long)stats.totalHashes,
                  (double)stats.balance);
}

void StatsStore::save(const char* algo, const MiningStats& stats)
{
    char ns[16];
    nsFor(algo, ns, sizeof(ns));

    Preferences p;
    if (!p.begin(ns, /*readOnly*/ false)) return;

    p.putUInt("ok",       stats.sharesAccepted);
    p.putUInt("rej",      stats.sharesRejected);
    p.putFloat("best",    stats.bestDifficulty);
    p.putULong64("total", stats.totalHashes);
    p.putFloat("bal",     stats.balance);

    p.end();
}

void StatsStore::erase(const char* algo)
{
    char ns[16];
    nsFor(algo, ns, sizeof(ns));
    Preferences p;
    if (p.begin(ns, false)) {
        p.clear();
        p.end();
    }
}
