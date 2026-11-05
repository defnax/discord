/*******************************************************************************
 * discord/DiscordBridge.cpp                                                   *
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
#include "DiscordBridge.h"
#include "BridgeMessages.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <cmath>

DiscordBridge::DiscordBridge(QObject* parent) : QObject(parent)
{
    mTimer.setSingleShot(true);
    connect(&mTimer, &QTimer::timeout, this, &DiscordBridge::tick);
}
DiscordBridge::~DiscordBridge() { stop(); }
void DiscordBridge::setStatus(const QString& value)
{
    mStatus = value;
    emit statusChanged(value);
}
void DiscordBridge::stop()
{
    mRunning = false;
    ++mGeneration;
    mTimer.stop();
    if(mHandler && rsEvents) rsEvents->unregisterEventsHandler(mHandler);
    mHandler = 0;
    if(mReply) {
        disconnect(mReply, nullptr, this, nullptr);
        mReply->abort();
        mReply->deleteLater();
        mReply = nullptr;
    }
    mOutgoing.clear(); mEchoes.clear(); mToken.clear();
    mChats = nullptr;
    setStatus("Stopped");
}
bool DiscordBridge::start(RsChats* chats, RsIdentity* identities, const QString& token, const QString& channel, ChatLobbyId lobby)
{
    stop();
    ChatLobbyInfo info;
    if(!chats || !rsEvents || token.trimmed().isEmpty() || token.contains('\n') || token.contains('\r') ||
       !BridgeMessages::validId(channel) || !lobby || !chats->getChatLobbyInfo(lobby, info)) {
        setStatus("Enter a bot token, numeric channel ID and a joined lobby.");
        return false;
    }
    mChats = chats; mIdentities = identities; mToken = token.trimmed(); mChannel = channel; mLobby = lobby;
    mCursor.clear(); mBaseline = false; mPostNext = false; mFailures = 0;
    const auto generation = mGeneration;
    auto error = rsEvents->registerEventsHandler([this, generation](std::shared_ptr<const RsEvent> event) {
        auto chat = std::dynamic_pointer_cast<const RsChatServiceEvent>(event);
        if(!chat || chat->mEventCode != RsChatServiceEventCode::CHAT_MESSAGE_RECEIVED) return;
        // Core callbacks run off the GUI thread. All bridge state stays on this object's thread.
        QMetaObject::invokeMethod(this, [this, chat, generation] {
            if(generation == mGeneration && mRunning) receive(chat->mMsg);
        }, Qt::QueuedConnection);
    }, mHandler, RsEventType::CHAT_SERVICE);
    if(error) { stop(); setStatus("Could not subscribe to RetroShare chat events."); return false; }
    mRunning = true;
    setStatus("Connecting; existing Discord history will be skipped.");
    mTimer.start(0);
    return true;
}
void DiscordBridge::receive(const ChatMessage& message)
{
    if(!message.chat_id.isLobbyId() || message.chat_id.toLobbyId() != mLobby) return;
    const QString raw = QString::fromUtf8(message.msg.data(), int(message.msg.size()));
    if(!message.incoming) {
        auto it = mEchoes.find(raw);
        if(it != mEchoes.end()) {
            if(--it.value() == 0) mEchoes.erase(it);
            return;
        }
    }
    const QString body = BridgeMessages::plain(raw);
    if(body.trimmed().isEmpty()) return;
    QString name;
    if(mIdentities) {
        RsIdentityDetails details;
        if(mIdentities->getIdDetails(message.lobby_peer_gxs_id, details))
            name = QString::fromStdString(details.mNickname);
    }
    if(name.isEmpty()) name = QString::fromStdString(message.peer_alternate_nickname);
    if(name.isEmpty()) name = QString::fromStdString(message.lobby_peer_gxs_id.toStdString());
    name.replace('\n', ' '); name.replace('\r', ' ');
    const auto parts = BridgeMessages::split(QString("[RetroShare / %1] %2").arg(name.left(80), body));
    if(mOutgoing.size() + parts.size() > 200) {
        setStatus("Outbound queue full; latest RetroShare message dropped."); return;
    }
    for(const auto& part : parts) mOutgoing.enqueue(part);
}
void DiscordBridge::tick()
{
    if(!mRunning || mReply) return;
    ChatLobbyInfo info;
    if(!mChats->getChatLobbyInfo(mLobby, info)) {
        stop(); setStatus("Bridge stopped: the selected lobby is no longer joined."); return;
    }
    const bool baseline = !mBaseline;
    const bool posting = !baseline && mPostNext && !mOutgoing.isEmpty();
    QUrl url("https://discord.com/api/v10/channels/" + mChannel + "/messages");
    if(!posting) {
        QUrlQuery query;
        query.addQueryItem("limit", baseline ? "1" : "100");
        if(!baseline) query.addQueryItem("after", mCursor);
        url.setQuery(query);
    }
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bot " + mToken.toUtf8());
    request.setRawHeader("User-Agent", "RetroShareDiscordBridge/0.1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    if(posting) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonObject payload{{"content", mOutgoing.head()},
                            {"allowed_mentions", QJsonObject{{"parse", QJsonArray{}}}}};
        mReply = mNetwork.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    } else mReply = mNetwork.get(request);
    auto reply = mReply.data();
    connect(reply, &QNetworkReply::finished, this, [this, reply, posting, baseline] {
        finish(reply, posting, baseline);
    });
}
void DiscordBridge::finish(QNetworkReply* reply, bool posting, bool baseline)
{
    mReply = nullptr;
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    int wait = 2000;
    const auto delay = [](double seconds) {
        if(!std::isfinite(seconds) || seconds <= 0) return 2000;
        return int(qMin(86400000.0, std::ceil(seconds * 1000.0) + 100.0));
    };
    if(reply->rawHeader("X-RateLimit-Remaining") == "0")
        wait = qMax(wait, delay(reply->rawHeader("X-RateLimit-Reset-After").toDouble()));
    reply->deleteLater();
    if(code == 401 || code == 403 || code == 404) {
        stop(); setStatus(QString("Bridge stopped: Discord HTTP %1. Check token, channel and permissions.").arg(code)); return;
    }
    if(code == 429) {
        wait = qMax(wait, delay(document.object().value("retry_after").toDouble()));
        wait = qMax(wait, delay(reply->rawHeader("Retry-After").toDouble()));
        setStatus("Discord rate limit; waiting before retry.");
    } else if(code < 200 || code >= 300 || reply->error() != QNetworkReply::NoError) {
        // A failed POST may already have reached Discord; do not blindly duplicate it.
        if(posting) mOutgoing.dequeue();
        wait = qMin(60000, 2000 * (1 << qMin(++mFailures, 5)));
        setStatus(posting ? "Send failed; message discarded to avoid duplicate delivery." : "Discord request failed; retrying.");
    } else if(parseError.error != QJsonParseError::NoError || (!posting && !document.isArray())) {
        stop(); setStatus("Bridge stopped: unexpected Discord response."); return;
    } else if(posting) {
        mOutgoing.dequeue(); mPostNext = false; mFailures = 0;
        setStatus("Running: text bridge active.");
    } else {
        const auto messages = BridgeMessages::ordered(document.array());
        if(baseline) {
            // An empty channel uses the start-time snowflake to avoid importing old history later.
            mCursor = messages.isEmpty()
                ? QString::number(quint64(QDateTime::currentMSecsSinceEpoch() - 1420070400000LL) << 22)
                : messages.last().value("id").toString();
            mBaseline = true;
        } else for(const auto& message : messages) {
            const QString id = message.value("id").toString();
            if(id.toULongLong() <= mCursor.toULongLong()) continue;
            const QString text = BridgeMessages::fromDiscord(message);
            if(!text.isEmpty()) {
                // Escape Discord text so it cannot inject RetroShare rich text.
                const QString html = "<span>" + text.toHtmlEscaped().replace("\n", "<br/>") + "</span>";
                ++mEchoes[html];
                if(!mChats->sendChat(ChatId(mLobby), html.toStdString())) {
                    if(--mEchoes[html] == 0) mEchoes.remove(html);
                    stop(); setStatus("Bridge stopped: RetroShare could not send to the lobby."); return;
                }
            }
            mCursor = id;
        }
        mPostNext = true; mFailures = 0;
        setStatus("Running: text bridge active.");
    }
    if(mRunning) mTimer.start(wait);
}
