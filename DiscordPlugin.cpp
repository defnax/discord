/*******************************************************************************
 * discord/DiscordPlugin.cpp                                                    *
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
#include "DiscordPlugin.h"
#include "DiscordBridge.h"
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QThread>
#include <cstdlib>

extern "C" {
Q_DECL_EXPORT void* RETROSHARE_PLUGIN_provide() { return new DiscordPlugin; }
Q_DECL_EXPORT uint32_t RETROSHARE_PLUGIN_revision = std::abs(std::atoi(RS_EXTRA_VERSION));
Q_DECL_EXPORT uint32_t RETROSHARE_PLUGIN_api = RS_PLUGIN_API_VERSION;
}
DiscordPlugin::~DiscordPlugin() { stop(); delete mPanel.data(); delete mBridge; }
QIcon* DiscordPlugin::qt_icon() const
{
    if(!mIcon) mIcon = new QIcon(":/images/discord.png");
    return mIcon;
}
void DiscordPlugin::setInterfaces(RsPlugInInterfaces& interfaces)
{
    // Some hosts leave mChats unset when populating RsPlugInInterfaces.
    mChats = interfaces.mChats;
    if(!mChats) mChats = rsChats;
    mIdentities = interfaces.mIdentity;
}
void DiscordPlugin::getPluginVersion(int& major, int& minor, int& build, int& revision) const
{
    major = 0; minor = 1; build = 0; revision = std::abs(std::atoi(RS_EXTRA_VERSION));
}
void DiscordPlugin::stop()
{
    if(!mBridge) return;
    if(QThread::currentThread() == mBridge->thread()) mBridge->stop();
    else QMetaObject::invokeMethod(mBridge, [this] { mBridge->stop(); }, Qt::BlockingQueuedConnection);
}
QWidget* DiscordPlugin::qt_config_panel() const
{
    if(mPanel) return mPanel;
    if(!mBridge) mBridge = new DiscordBridge;
    auto panel = new QWidget;
    mPanel = panel;
    auto layout = new QFormLayout(panel);
    auto notice = new QLabel("Forward new text messages both ways. Room messages become visible on Discord.\n"
                             "Use one bridge per room/channel pair. Bot and webhook messages are ignored.\n"
                             "Token is kept in memory only; Start is required each session.", panel);
    notice->setWordWrap(true);
    layout->addRow(notice);
    auto token = new QLineEdit(panel);
    token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText("Discord bot token (never your user token)");
    auto channel = new QLineEdit(panel);
    channel->setPlaceholderText("Discord text channel ID");
    auto lobby = new QComboBox(panel);
    auto refresh = new QPushButton("Refresh joined lobbies", panel);
    const auto populate = [this, lobby] {
        const auto selected = lobby->currentData();
        lobby->clear();
        if(!mChats) {
            lobby->addItem("Chat service unavailable");
            return;
        }
        std::list<ChatLobbyId> ids;
        mChats->getChatLobbyList(ids);
        for(auto id : ids) {
            ChatLobbyInfo info;
            if(mChats->getChatLobbyInfo(id, info))
                lobby->addItem(QString::fromStdString(info.lobby_name) + " (" + QString::number(id) + ")",
                               QVariant::fromValue<qulonglong>(id));
        }
        const int index = lobby->findData(selected);
        if(index >= 0) lobby->setCurrentIndex(index);
        if(lobby->count() == 0) lobby->addItem("No joined lobbies — join a room, then refresh");
    };
    populate();
    QObject::connect(refresh, &QPushButton::clicked, panel, populate);
    layout->addRow("Bot token", token);
    layout->addRow("Discord channel", channel);
    layout->addRow("RetroShare lobby", lobby);
    layout->addRow(refresh);
    auto start = new QPushButton("Start bridge", panel);
    auto stopButton = new QPushButton("Stop bridge", panel);
    auto status = new QLabel(mBridge->status(), panel);
    status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText);
    layout->addRow(start, stopButton);
    layout->addRow("Status", status);
    QObject::connect(mBridge, &DiscordBridge::statusChanged, status, &QLabel::setText);
    QObject::connect(start, &QPushButton::clicked, panel, [this, token, channel, lobby] {
        if(mBridge->start(mChats, mIdentities, token->text(), channel->text().trimmed(), lobby->currentData().toULongLong()))
            token->clear();
    });
    QObject::connect(stopButton, &QPushButton::clicked, mBridge, &DiscordBridge::stop);
    return panel;
}
