// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements palette.h.

#include "core/palette.h"

#include <algorithm>

namespace fe {

namespace {

constexpr int MATCH = 1;
constexpr int FOLLOWS = 5;
constexpr int WORD_START = 8;
constexpr int TEXT_START = 10;
/// At most this much is taken off for the letters skipped before one match.
constexpr int GAP_LIMIT = 3;

bool startsWord(QStringView text, qsizetype at)
{
    if (at == 0) {
        return true;
    }
    const QChar before = text.at(at - 1);
    return !before.isLetterOrNumber() || (before.isLower() && text.at(at).isUpper());
}

} // namespace

std::optional<int> fuzzyScore(QStringView query, QStringView text)
{
    int score = 0;
    qsizetype from = 0;
    qsizetype last = -2;
    for (const QChar wanted : query) {
        if (wanted.isSpace()) {
            continue;
        }
        // The first place the letter starts a word wins over an earlier place inside a word: "nf" finds the F of
        // "New Folder", not the f inside a word before it.
        qsizetype found = -1;
        qsizetype firstAnywhere = -1;
        for (qsizetype i = from; i < text.size(); ++i) {
            if (text.at(i).toCaseFolded() != wanted.toCaseFolded()) {
                continue;
            }
            if (firstAnywhere < 0) {
                firstAnywhere = i;
            }
            if (i == last + 1 || startsWord(text, i)) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            found = firstAnywhere;
        }
        if (found < 0) {
            return std::nullopt;
        }
        score += MATCH;
        if (found == last + 1) {
            score += FOLLOWS;
        }
        if (found == 0) {
            score += TEXT_START;
        } else if (startsWord(text, found)) {
            score += WORD_START;
        }
        score -= static_cast<int>(std::min<qsizetype>(found - from, GAP_LIMIT));
        last = found;
        from = found + 1;
    }
    return score;
}

QList<qsizetype> rankPalette(const QString &query, const QList<PaletteEntry> &entries)
{
    struct Hit {
        int score;
        qsizetype index;
    };
    QList<Hit> hits;
    for (qsizetype i = 0; i < entries.size(); ++i) {
        const PaletteEntry &entry = entries.at(i);
        // The text alone, or the text and its place ("view list" finds View's "as List"); the place helps less.
        std::optional<int> score = fuzzyScore(query, entry.text);
        if (const auto withPlace = fuzzyScore(query, entry.place + QLatin1Char(' ') + entry.text)) {
            score = std::max(score.value_or(*withPlace - 1), *withPlace - 1);
        }
        if (score) {
            hits.append({*score, i});
        }
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) { return a.score > b.score; });
    QList<qsizetype> order;
    order.reserve(hits.size());
    for (const Hit &hit : hits) {
        order.append(hit.index);
    }
    return order;
}

} // namespace fe
