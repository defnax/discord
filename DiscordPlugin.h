/*******************************************************************************
 * discord/DiscordPlugin.h                                                      *
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
#include <retroshare/rsplugin.h>
#include <QPointer>
#include <QIcon>
class DiscordBridge;
class DiscordPlugin : public RsPlugin
{
public:
    ~DiscordPlugin() override;
    std::string getPluginName() const override { return "Discord Bridge"; }
    std::string getShortPluginDescription() const override { return "Native text bridge between a Discord channel and a joined RetroShare lobby."; }
    void getPluginVersion(int& major, int& minor, int& build, int& revision) const override;
    void setInterfaces(RsPlugInInterfaces& interfaces) override;
    void setPlugInHandler(RsPluginHandler*) override {}
    QWidget* qt_config_panel() const override;
    QIcon* qt_icon() const override;
    void stop() override;
private:
    RsChats* mChats = nullptr;
    RsIdentity* mIdentities = nullptr;
    mutable DiscordBridge* mBridge = nullptr;
    mutable QPointer<QWidget> mPanel;
    mutable QIcon* mIcon = nullptr;
};
