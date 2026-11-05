/*******************************************************************************
 * discord/DiscordBridge.h                                                     *
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
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <retroshare/rschats.h>
#include <retroshare/rsidentity.h>

class QNetworkReply;
class DiscordBridge : public QObject
{
    Q_OBJECT
public:
    explicit DiscordBridge(QObject* parent = nullptr);
    ~DiscordBridge() override;
    bool start(RsChats* chats, RsIdentity* identities, const QString& token, const QString& channel, ChatLobbyId lobby);
    void stop();
    QString status() const { return mStatus; }
signals:
    void statusChanged(const QString& status);
private:
    friend class DiscordBridgeTest;
    void setStatus(const QString& status);
    void tick();
    void receive(const ChatMessage& message);
    void finish(QNetworkReply* reply, bool posting, bool baseline);
    QNetworkAccessManager mNetwork;
    QPointer<QNetworkReply> mReply;
    QTimer mTimer;
    RsChats* mChats = nullptr;
    RsIdentity* mIdentities = nullptr;
    RsEventsHandlerId_t mHandler = 0;
    QString mToken, mChannel, mCursor, mStatus = "Stopped";
    ChatLobbyId mLobby = 0;
    QQueue<QString> mOutgoing;
    QHash<QString, int> mEchoes;
    bool mRunning = false, mBaseline = false, mPostNext = false;
    int mFailures = 0;
    quint64 mGeneration = 0;
};
