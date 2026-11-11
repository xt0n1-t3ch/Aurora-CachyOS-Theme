// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements format.h.

#include "core/format.h"

#include <QHash>

#include <array>

namespace fe {

namespace {

struct Modifier {
    QStringView prefix;
    QStringView label;
};

// In the order Windows writes them: "Windows key+Ctrl+D", "Ctrl+Alt+Del", "Alt+Shift+P".
constexpr std::array<Modifier, 4> MODIFIERS{{
    {u"Meta+", u"Win+"},
    {u"Ctrl+", u"Ctrl+"},
    {u"Alt+", u"Alt+"},
    {u"Shift+", u"Shift+"},
}};

// Microsoft's names where Qt's differ; every other key (letters, F2, Esc, Del, Tab, Home) reads the same.
QString keyName(const QString &key)
{
    static const QHash<QString, QString> NAMED{
        {QStringLiteral("Return"), QStringLiteral("Enter")},       {QStringLiteral("Left"), QStringLiteral("Left arrow")},
        {QStringLiteral("Right"), QStringLiteral("Right arrow")},  {QStringLiteral("Up"), QStringLiteral("Up arrow")},
        {QStringLiteral("Down"), QStringLiteral("Down arrow")},    {QStringLiteral("PgUp"), QStringLiteral("Page up")},
        {QStringLiteral("PgDown"), QStringLiteral("Page down")},   {QStringLiteral("Ins"), QStringLiteral("Insert")},
        {QStringLiteral("Space"), QStringLiteral("Spacebar")},     {QStringLiteral("+"), QStringLiteral("Plus")},
        {QStringLiteral("-"), QStringLiteral("Minus")},            {QStringLiteral(","), QStringLiteral("Comma")},
        {QStringLiteral("."), QStringLiteral("Period")},
    };
    return NAMED.value(key, key);
}

} // namespace

QString shortcutLabel(const QString &portableText)
{
    QStringView rest(portableText);
    std::array<bool, MODIFIERS.size()> held{};
    // The key itself may be "+", so modifiers are peeled off the front instead of splitting on '+'.
    for (bool peeled = true; peeled;) {
        peeled = false;
        for (size_t i = 0; i < MODIFIERS.size(); ++i) {
            if (rest.size() > MODIFIERS.at(i).prefix.size() && rest.startsWith(MODIFIERS.at(i).prefix)) {
                held.at(i) = true;
                rest = rest.mid(MODIFIERS.at(i).prefix.size());
                peeled = true;
            }
        }
    }
    QString label;
    for (size_t i = 0; i < MODIFIERS.size(); ++i) {
        if (held.at(i)) {
            label += MODIFIERS.at(i).label;
        }
    }
    return label + keyName(rest.toString());
}

QString statusLine(const FolderCounts &counts, const std::function<QString(qint64)> &formatSize)
{
    QString line;
    if (counts.selected > 0) {
        line = QStringLiteral("%1 of %2 selected").arg(counts.selected).arg(counts.items);
        if (counts.selectedFileBytes > 0) {
            line += QStringLiteral(", ") + formatSize(counts.selectedFileBytes);
        }
    } else {
        line = counts.items == 1 ? QStringLiteral("1 item") : QStringLiteral("%1 items").arg(counts.items);
    }
    if (counts.availableBytes) {
        line += QStringLiteral(", %1 available").arg(formatSize(*counts.availableBytes));
    }
    return line;
}

QString filterMatchText(qsizetype matches)
{
    if (matches == 0) {
        return QStringLiteral("No matches");
    }
    return matches == 1 ? QStringLiteral("1 match") : QStringLiteral("%1 matches").arg(matches);
}

} // namespace fe
