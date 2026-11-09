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
#include <QSettings>
#include <QThread>
#include <retroshare/rsinit.h>
#include <cstdlib>

extern "C" {
Q_DECL_EXPORT void* RETROSHARE_PLUGIN_provide() { return new DiscordPlugin; }
Q_DECL_EXPORT uint32_t RETROSHARE_PLUGIN_revision = std::abs(std::atoi(RS_EXTRA_VERSION));
Q_DECL_EXPORT uint32_t RETROSHARE_PLUGIN_api = RS_PLUGIN_API_VERSION;
}
static QSettings discordSettings()
{
    // Keep plugin settings with the active RetroShare account instead of the
    // Windows Registry or the host application's global Qt settings.
    const QString path = QString::fromStdString(
        RsAccounts::AccountDirectory() + "/discordbridge.conf");
    return QSettings(path, QSettings::IniFormat);
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
                             "Settings are remembered locally; the bridge can reconnect automatically after restart.", panel);
    notice->setWordWrap(true);
    layout->addRow(notice);
    auto token = new QLineEdit(panel);
    token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText("Discord bot token (never your user token)");
    auto channel = new QLineEdit(panel);
    channel->setPlaceholderText("Discord text channel ID");
    auto lobby = new QComboBox(panel);
    QSettings settings = discordSettings();
    token->setText(settings.value("DiscordBridge/token").toString());
    channel->setText(settings.value("DiscordBridge/channel").toString());
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
    status->setStyleSheet("color: #777;");
    stopButton->setEnabled(false);
    layout->addRow(start, stopButton);
    layout->addRow("Status", status);
    QObject::connect(mBridge, &DiscordBridge::statusChanged, panel, [status, start, stopButton](const QString& value) {
        status->setText(value.startsWith("Running") || value.startsWith("Connecting")
                            ? QString("● ") + value : value);
        const bool active = value.startsWith("Running") || value.startsWith("Connecting");
        status->setStyleSheet(active ? "color: #159447; font-weight: bold;" : "color: #777;");
        start->setEnabled(!active);
        stopButton->setEnabled(active);
    });
    QObject::connect(start, &QPushButton::clicked, panel, [this, token, channel, lobby] {
        QSettings settings = discordSettings();
        settings.setValue("DiscordBridge/token", token->text());
        settings.setValue("DiscordBridge/channel", channel->text().trimmed());
        settings.setValue("DiscordBridge/lobby", lobby->currentData().toULongLong());
        if(mBridge->start(mChats, mIdentities, token->text(), channel->text().trimmed(), lobby->currentData().toULongLong())) {
            settings.setValue("DiscordBridge/enabled", true);
            settings.sync();
        }
    });
    QObject::connect(stopButton, &QPushButton::clicked, panel, [this] {
        QSettings settings = discordSettings();
        settings.setValue("DiscordBridge/enabled", false);
        settings.sync();
        mBridge->stop();
    });
    const auto savedLobby = settings.value("DiscordBridge/lobby").toULongLong();
    const int savedIndex = lobby->findData(QVariant::fromValue<qulonglong>(savedLobby));
    if(savedIndex >= 0) lobby->setCurrentIndex(savedIndex);
    if(settings.value("DiscordBridge/enabled", false).toBool() && savedIndex >= 0) {
        QMetaObject::invokeMethod(panel, [this, token, channel, lobby] {
            QSettings settings = discordSettings();
            if(mBridge->start(mChats, mIdentities, token->text(), channel->text().trimmed(), lobby->currentData().toULongLong()))
                settings.sync();
        }, Qt::QueuedConnection);
    }
    return panel;
}
