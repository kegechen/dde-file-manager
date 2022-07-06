/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     liuyangming<liuyangming@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             yanghao<yanghao@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "tag.h"
#include "files/tagfileinfo.h"
#include "files/tagfilewatcher.h"
#include "files/tagdiriterator.h"
#include "utils/taghelper.h"
#include "utils/tagfilehelper.h"
#include "utils/tagmanager.h"
#include "widgets/tagwidget.h"
#include "menu/tagmenuscene.h"
#include "menu/tagdirmenuscene.h"
#include "events/tageventreceiver.h"

#include "plugins/common/dfmplugin-menu/menu_eventinterface_helper.h"

#include "services/common/propertydialog/propertydialogservice.h"
#include "services/filemanager/detailspace/detailspaceservice.h"

#include "dfm-base/base/urlroute.h"
#include "dfm-base/base/schemefactory.h"
#include "dfm-base/base/application/application.h"
#include "dfm-base/base/application/settings.h"
#include "dfm-base/dfm_event_defines.h"
#include "dfm-base/widgets/dfmwindow/filemanagerwindowsmanager.h"

#include <QRectF>

Q_DECLARE_METATYPE(QRectF *)
Q_DECLARE_METATYPE(QList<QVariantMap> *)
Q_DECLARE_METATYPE(QList<QUrl> *)

DSC_USE_NAMESPACE
DFMBASE_USE_NAMESPACE
DSB_FM_USE_NAMESPACE
using namespace dfmplugin_tag;

void Tag::initialize()
{
    UrlRoute::regScheme(TagManager::scheme(), "/", {}, true, tr("Tag"));

    InfoFactory::regClass<TagFileInfo>(TagManager::scheme());
    WatcherFactory::regClass<TagFileWatcher>(TagManager::scheme());
    DirIteratorFactory::regClass<TagDirIterator>(TagManager::scheme());

    connect(&FMWindowsIns, &FileManagerWindowsManager::windowOpened, this, &Tag::onWindowOpened, Qt::DirectConnection);
    connect(dpfListener, &dpf::Listener::pluginsInitialized, this, &Tag::onAllPluginsInitialized, Qt::DirectConnection);

    TagManager::instance();

    bindEvents();
}

bool Tag::start()
{
    PropertyDialogService::service()->registerControlExpand(Tag::createTagWidget, 0);
    DetailSpaceService::serviceInstance()->registerControlExpand(Tag::createTagWidget);

    DetailFilterTypes filter = DetailFilterType::kFileSizeField;
    filter |= DetailFilterType::kFileChangeTImeField;
    filter |= DetailFilterType::kFileInterviewTimeField;
    DetailSpaceService::serviceInstance()->registerFilterControlField(TagManager::scheme(), filter);

    TagEventReceiver::instance()->initConnect();

    dfmplugin_menu_util::menuSceneRegisterScene(TagMenuCreator::name(), new TagMenuCreator);
    bindScene("FileOperatorMenu");

    WorkspaceService::service()->setWorkspaceMenuScene(TagManager::scheme(), TagDirMenuCreator::name());
    dfmplugin_menu_util::menuSceneRegisterScene(TagDirMenuCreator::name(), new TagDirMenuCreator);

    followEvents();

    return true;
}

void Tag::onWindowOpened(quint64 windId)
{
    auto window = FMWindowsIns.findWindowById(windId);
    Q_ASSERT_X(window, "Tag", "Cannot find window by id");

    if (window->titleBar())
        regTagCrumbToTitleBar();
    else
        connect(window, &FileManagerWindow::titleBarInstallFinished, this, &Tag::regTagCrumbToTitleBar, Qt::DirectConnection);

    if (window->sideBar())
        installToSideBar();
    else
        connect(window, &FileManagerWindow::sideBarInstallFinished, this, &Tag::installToSideBar, Qt::DirectConnection);
}

void Tag::regTagCrumbToTitleBar()
{
    dpfSlotChannel->push("dfmplugin_titlebar", "slot_Custom_Register", TagManager::scheme(), QVariantMap {});
}

void Tag::onAllPluginsInitialized()
{
    TagHelper::workspaceServIns()->addScheme(TagManager::scheme());
}

QWidget *Tag::createTagWidget(const QUrl &url)
{
    auto info = InfoFactory::create<AbstractFileInfo>(url);
    if (!TagManager::instance()->canTagFile(info))
        return nullptr;

    return new TagWidget(url);
}

void Tag::installToSideBar()
{
    QMap<QString, QColor> tagsMap = TagManager::instance()->getAllTags();
    auto tagNames = tagsMap.keys();
    auto orders = Application::genericSetting()->value(kSidebarOrder, kTagOrderKey).toStringList();
    for (const auto &item : orders) {
        QUrl u(item);
        auto query = u.query().split("=", QString::SkipEmptyParts);
        if (query.count() == 2 && tagNames.contains(query[1])) {
            auto &&url { TagHelper::instance()->makeTagUrlByTagName(query[1]) };
            auto &&map { TagHelper::instance()->createSidebarItemInfo(query[1]) };
            dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Add", url, map);
            tagNames.removeAll(query[1]);
        }
    }
    for (const auto &tag : tagNames) {   // if tag order is not complete.
        auto &&url { TagHelper::instance()->makeTagUrlByTagName(tag) };
        auto &&map { TagHelper::instance()->createSidebarItemInfo(tag) };
        dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Add", url, map);
    }
}

void Tag::followEvents()
{
    dpfHookSequence->follow(Workspace::EventType::kPaintListItem, TagManager::instance(), &TagManager::paintListTagsHandle);
    dpfHookSequence->follow(Workspace::EventType::kPaintIconItem, TagManager::instance(), &TagManager::paintIconTagsHandle);

    // todo(zy) need to delete
    dpfHookSequence->follow(GlobalEventType::kTempDesktopPaintTag, TagManager::instance(), &TagManager::paintIconTagsHandle);
    // paste
    dpfHookSequence->follow("dfmplugin_workspace", "hook_ShortCut_PasteFiles", TagManager::instance(), &TagManager::pasteHandle);
    dpfHookSequence->follow("dfmplugin_workspace", "hook_FileDrop", TagManager::instance(), &TagManager::fileDropHandle);

    // titlebar crumb
    dpfHookSequence->follow("dfmplugin_titlebar", "hook_Crumb_Seprate", TagManager::instance(), &TagManager::sepateTitlebarCrumb);

    // url trans
    dpfHookSequence->follow("dfmplugin_utils", "hook_UrlsTransform", TagHelper::instance(), &TagHelper::urlsToLocal);

    // file operation
    dpfHookSequence->follow("dfmplugin_fileoperations", "hook_Operation_OpenFileInPlugin", TagFileHelper::instance(), &TagFileHelper::openFileInPlugin);
}

void Tag::bindScene(const QString &parentScene)
{
    if (dfmplugin_menu_util::menuSceneContains(parentScene)) {
        dfmplugin_menu_util::menuSceneBind(TagMenuCreator::name(), parentScene);
    } else {
        menuScenes << parentScene;
        if (!subscribedEvent)
            subscribedEvent = dpfSignalDispatcher->subscribe("dfmplugin_menu", "signal_MenuScene_SceneAdded", this, &Tag::onMenuSceneAdded);
    }
}

void Tag::onMenuSceneAdded(const QString &scene)
{
    if (menuScenes.contains(scene)) {
        menuScenes.remove(scene);
        dfmplugin_menu_util::menuSceneBind(TagMenuCreator::name(), scene);

        if (menuScenes.isEmpty()) {
            dpfSignalDispatcher->unsubscribe("dfmplugin_menu", "signal_MenuScene_SceneAdded", this, &Tag::onMenuSceneAdded);
            subscribedEvent = false;
        }
    }
}

void Tag::bindEvents()
{
    dpfSignalDispatcher->subscribe(GlobalEventType::kChangeCurrentUrl, TagEventReceiver::instance(), &TagEventReceiver::handleWindowUrlChanged);
    dpfSignalDispatcher->subscribe("dfmplugin_sidebar", "signal_Sidebar_Sorted", TagEventReceiver::instance(), &TagEventReceiver::handleSidebarOrderChanged);
}
