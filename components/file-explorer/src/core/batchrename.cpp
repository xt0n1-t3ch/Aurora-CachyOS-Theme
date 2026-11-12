// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements batchrename.h.

#include "core/batchrename.h"

#include <QMimeDatabase>
#include <QRegularExpression>

namespace fe {

namespace {

/// NAME_MAX of Linux filesystems, in bytes.
constexpr qsizetype NAME_MAX_BYTES = 255;
constexpr int COUNTER_DIGITS = 5;

QString titleCase(const QString &text)
{
    QString out = text.toLower();
    bool startOfWord = true;
    for (QChar &c : out) {
        if (c.isLetter()) {
            if (startOfWord) {
                c = c.toUpper();
            }
            startOfWord = false;
        } else {
            startOfWord = !c.isDigit();
        }
    }
    return out;
}

/// The new stem of one item, or nothing when the rule is wrong.
QString newStem(const QString &stem, const RenameItem &item, qsizetype position, const RenameRule &rule, const QRegularExpression &pattern)
{
    switch (rule.mode) {
    case RenameMode::Replace: {
        if (rule.find.isEmpty()) {
            return stem;
        }
        QString out = stem;
        if (rule.regularExpression) {
            return out.replace(pattern, rule.replaceWith);
        }
        return out.replace(rule.find, rule.replaceWith, rule.matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive);
    }
    case RenameMode::Add:
        return rule.position == RenamePosition::Before ? rule.text + stem : stem + rule.text;
    case RenameMode::Format: {
        QString mark;
        switch (rule.format) {
        case RenameFormat::Index:
            mark = QString::number(rule.startAt + position);
            break;
        case RenameFormat::Counter:
            mark = QStringLiteral("%1").arg(rule.startAt + position, COUNTER_DIGITS, 10, QLatin1Char('0'));
            break;
        case RenameFormat::Date:
            mark = item.modified.date().toString(Qt::ISODate);
            // A date is a word of its own: "Photo 2026-11-11", "2026-11-11 Photo".
            if (rule.customName.isEmpty()) {
                return mark;
            }
            return rule.numberPosition == RenamePosition::Before ? mark + QLatin1Char(' ') + rule.customName
                                                                 : rule.customName + QLatin1Char(' ') + mark;
        }
        return rule.numberPosition == RenamePosition::Before ? mark + rule.customName : rule.customName + mark;
    }
    case RenameMode::Case:
        switch (rule.letterCase) {
        case RenameCase::Lower:
            return stem.toLower();
        case RenameCase::Upper:
            return stem.toUpper();
        case RenameCase::Title:
            return titleCase(stem);
        }
    }
    return stem;
}

RenameProblem problemOf(const QString &name)
{
    if (name.isEmpty()) {
        return RenameProblem::Empty;
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QChar(0)) || name == u"." || name == u"..") {
        return RenameProblem::Invalid;
    }
    if (name.toUtf8().size() > NAME_MAX_BYTES) {
        return RenameProblem::TooLong;
    }
    return RenameProblem::None;
}

} // namespace

bool RenamePlan::canRun() const
{
    return ruleError.isEmpty() && changed > 0
        && std::none_of(rows.cbegin(), rows.cend(), [](const RenameRow &row) { return row.problem != RenameProblem::None; });
}

std::pair<QString, QString> splitExtension(const QString &name)
{
    // The rule of KIO's batch rename, which adds the old suffix back to any new name that does not end with it
    // (BatchRenameJobPrivate::slotStart, batchrenamejob.cpp, KIO 6.31.0, lines 104-114): with the same split, every new
    // name ends with its suffix and KIO adds nothing.
    static const QMimeDatabase mimeTypes;
    const QString suffix = mimeTypes.suffixForFileName(name);
    if (suffix.isEmpty() || !name.endsWith(QLatin1Char('.') + suffix)) {
        return {name, QString()};
    }
    const qsizetype dot = name.size() - suffix.size() - 1;
    return {name.left(dot), name.mid(dot)};
}

RenamePlan planRenames(const QList<RenameItem> &items, const RenameRule &rule, const QHash<QString, QSet<QString>> &namesInFolders)
{
    RenamePlan plan;
    QRegularExpression pattern;
    if (rule.mode == RenameMode::Replace && rule.regularExpression && !rule.find.isEmpty()) {
        pattern.setPattern(rule.find);
        pattern.setPatternOptions(rule.matchCase ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
        if (!pattern.isValid()) {
            plan.ruleError = pattern.errorString();
        }
    }
    for (qsizetype i = 0; i < items.size(); ++i) {
        const auto [stem, extension] = splitExtension(items.at(i).name);
        const QString name = plan.ruleError.isEmpty() ? newStem(stem, items.at(i), i, rule, pattern) + extension : items.at(i).name;
        plan.rows.append({name, problemOf(name)});
    }
    if (rule.mode == RenameMode::Format && rule.format == RenameFormat::Date) {
        // Items of one day in one folder would share a name: the second and later get " 2", " 3" before the extension.
        QHash<QString, int> seen;
        for (qsizetype i = 0; i < items.size(); ++i) {
            QString &name = plan.rows[i].newName;
            const int count = ++seen[items.at(i).folder + QLatin1Char('/') + name];
            if (count > 1) {
                const auto [stem, extension] = splitExtension(name);
                name = stem + QLatin1Char(' ') + QString::number(count) + extension;
                plan.rows[i].problem = problemOf(name);
            }
        }
    }
    // New names two changing items share; an item that keeps its name is "already there" instead.
    QHash<QString, int> taken;
    for (qsizetype i = 0; i < items.size(); ++i) {
        if (plan.rows.at(i).newName != items.at(i).name) {
            ++taken[items.at(i).folder + QLatin1Char('/') + plan.rows.at(i).newName];
        }
    }
    for (qsizetype i = 0; i < items.size(); ++i) {
        RenameRow &row = plan.rows[i];
        const RenameItem &item = items.at(i);
        if (row.newName == item.name) {
            continue;
        }
        ++plan.changed;
        if (row.problem != RenameProblem::None) {
            continue;
        }
        const QSet<QString> here = namesInFolders.value(item.folder);
        const bool anotherItemHasIt = std::any_of(items.cbegin(), items.cend(), [&](const RenameItem &other) {
            return other.folder == item.folder && other.name == row.newName;
        });
        if (taken.value(item.folder + QLatin1Char('/') + row.newName) > 1) {
            row.problem = RenameProblem::Duplicate;
        } else if (here.contains(row.newName) || anotherItemHasIt) {
            row.problem = RenameProblem::Exists;
        }
    }
    return plan;
}

QString renameProblemText(RenameProblem problem)
{
    switch (problem) {
    case RenameProblem::None:
        break;
    case RenameProblem::Empty:
        return QStringLiteral("The name is empty");
    case RenameProblem::Invalid:
        return QStringLiteral("A name cannot hold \"/\" or be \".\" or \"..\"");
    case RenameProblem::TooLong:
        return QStringLiteral("The name is longer than 255 bytes");
    case RenameProblem::Duplicate:
        return QStringLiteral("Another item gets this name");
    case RenameProblem::Exists:
        return QStringLiteral("An item with this name is already there");
    }
    return {};
}

} // namespace fe
