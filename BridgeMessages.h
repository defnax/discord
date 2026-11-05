/*******************************************************************************
 * discord/BridgeMessages.h                                                   *
 *                                                                             *
 * Copyright (C) 2026 RetroShare Team <retroshare.project@gmail.com>           *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Affero General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Affero General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Affero General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/
#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QTextDocument>
#include <algorithm>

namespace BridgeMessages {
inline bool validId(const QString& s)
{
    if(s.isEmpty() || s.size() > 20) return false;
    for(QChar c : s) if(c < QLatin1Char('0') || c > QLatin1Char('9')) return false;
    bool ok = false;
    return s.toULongLong(&ok) != 0 && ok;
}
inline QStringList split(QString text, int limit = 2000)
{
    QStringList result;
    while(!text.isEmpty()) {
        int n = qMin(limit, text.size());
        if(n < text.size() && text.at(n - 1).isHighSurrogate()) --n;
        result.append(text.left(n));
        text.remove(0, n);
    }
    return result;
}
inline QString plain(const QString& text)
{
    if(!Qt::mightBeRichText(text)) return text;
    QTextDocument doc;
    doc.setHtml(text);
    return doc.toPlainText();
}
inline QString fromDiscord(const QJsonObject& message)
{
    const auto author = message.value("author").toObject();
    if(author.value("bot").toBool() || message.contains("webhook_id")) return {};
    const int type = message.value("type").toInt();
    if(type != 0 && type != 19) return {};
    const QString content = message.value("content").toString();
    if(content.isEmpty()) return {};
    QString name = author.value("global_name").toString();
    if(name.isEmpty()) name = author.value("username").toString();
    name.replace('\n', ' '); name.replace('\r', ' ');
    return QString("[Discord / %1] %2").arg(name.left(80), content);
}
inline QList<QJsonObject> ordered(const QJsonArray& array)
{
    QList<QJsonObject> result;
    for(const auto& value : array) {
        auto message = value.toObject();
        if(validId(message.value("id").toString())) result.append(message);
    }
    std::sort(result.begin(), result.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a.value("id").toString().toULongLong() < b.value("id").toString().toULongLong();
    });
    return result;
}
}
